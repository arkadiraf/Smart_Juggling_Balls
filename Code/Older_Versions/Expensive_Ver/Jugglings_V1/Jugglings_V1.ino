// Cheap Ver 1 Juggling balls code

/* Pinout:
    Arduino: 5V 
    A5 (SCL)-->SCL
    A4 (SDA)-->SDA
    D2-->INT DRDYG
    D4-->DRDY GY-87 ? not sure what to do with it
    
    D8-->On/Off Pin
    
    D6-->NeoPIxel DataIn 1
    D7-->NeoPIxel DataIn 2
    
    A0-->Vraw
    
    NRF: 
    
    D9-->CE NRF
    D10-->CSN NRF
    D13 (SCK)-->SCK NRF
    D11 (MOSI)-->MOSI NRF
    D12 (MISO)-->MISO NRF
    D3-->IRQ
    
    connector:
    Vcc--> baterry Vcc
    Dtr --> directly
    Tx --> through 1k // in order to protect from shorts
    Rx --> through 1k
    Grnd --> battery GRN
*/

///////////////
// libraries //
///////////////
#include <math.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Adafruit_NeoPixel.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "HMC5883L.h"

///////////////
// Variables //
///////////////

#define rad2deg 57.295
#define ONOFFPIN 8

///////////////
// MPU 6050 //
///////////////

MPU6050 accelgyro;

#define MPUREADDELAY 20
// Single radio pipe address for the 2 nodes to communicate.
unsigned long MpuMillisStamp=0;

const byte INT_MPU_PIN = 2; // INT DRDYG

int16_t ax, ay, az;
int16_t gx, gy, gz;

double AcclValue[3]={0,0,0}; //in G values
double GyroValue[3]={0,0,0}; //in Degrees per second

#define SAMPLE2GACC 2048.0   // +/-16g full scale, 16 bit resolution--> 2^16/32=2048
#define SAMPLE2AGYRO 16.384  // +/-2000 Deg/Sec, 16 but tesolution--> 2^16/4000=16.384
	
//////////////////
// Mag HMC5883L //
//////////////////
HMC5883L mag;
int16_t mx, my, mz;     //To store magnetometer readings
#define SAMPLE2GAUSS 220.0   // +/-8.1gauss full scale
double MagValue[3]={0,0,0}; //in Gauss


////////////////////////////
// communication variable //
////////////////////////////
#define CommunicationDelay 500
// Single radio pipe address for the 2 nodes to communicate.
unsigned long CommunicationMillisStamp=0;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);
// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };



/////////////
// Battery //
/////////////
double Vbaterry=3.7;
#define VBATTERY_PIN A0
#define ADC2VBAT 204.6 // Convert ADC reading to voltage, VBatt=Vref/ADC_Res
#define BATTERYMINIMUM 3.3 // minimum allowed battery voltage
byte BatteryStatus=1; // variable to indicate battery status,0- empty, 1-full
#define BatterySampleDelay 1000 // sample period of the battery voltage
unsigned long VbatSampleMillis=0; // variable to hold millis counter

////////////////
// Neo Pixels //
////////////////
#define NEODATAPIN1 6
#define NEODATAPIN2 7
#define NEOPIXELS 26
// neo Pixel initialize
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(26, NEODATAPIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(26, NEODATAPIN2, NEO_GRB + NEO_KHZ800);

/////////////////////
// Juggling Status //
/////////////////////

double AcclNorm=0;
double MagNorm=0;
#define REEDSWITCHPIN 8
int MagStatus=0;
// define colors array
// [White,Red,Lime,Blue,Yellow,Cyan,Magenta,Maroon,Olive,Green,Purple,Teal,Navy]  from http://www.rapidtables.com/web/color/RGB_Color.htm
#define NUMOFCOLORS 13
long ColorsArr[NUMOFCOLORS]={0xFFFFFF,0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0x00FFFF,0xFF00FF,0x800000,0x808000,0x008000,0x800080,0x008080,0x000080};
byte CurrColorNum=0;
uint32_t Color=0x000000;
#define ColorBlack 0x000000

// Status based on accelerometer
// 0- free fall, 1- 1G, 2- bump or anything else
byte AcclStatusArr[3]={1,1,1};

// Mode based on Magnetometer
// 0- No Magnet, 1- Magnet,
byte MagStatusArr[3]={1,1,1};
// set pixel state
// 0- do nothing, 1- change color, 2- 1G be green
byte PixelState=0;
#define PixelsNumStates 4
// 0- off, 1- all colors the same, 2- chasing one color, 3-chasing 2 colors,
byte PixelsMode=0;
#define PixelsNumModes 5
// variable to store pixel referance for theater chasing mode
int theaterChasePixel=0;

unsigned long theaterChaseMillis=0; // timer counter
#define theaterChaseDelay 50


///////////////
// SETUP	 //
///////////////
void setup(){
    // turn system on after boot up
    pinMode(ONOFFPIN,OUTPUT);
    digitalWrite(ONOFFPIN,HIGH);
    
    // join I2C bus (I2Cdev library doesn't do this automatically)
    Wire.begin();
    // initialize serial communication
    Serial.begin(57600);
    // neo Pixel initialize
    strip1.begin();
    strip1.show(); // Initialize all pixels to 'off'
    strip2.begin();
    strip2.show(); // Initialize all pixels to 'off'
  
    // Nrf initialize
    radio.begin();
    radio.setRetries(15,15);
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  
    radio.startListening();
  
    //MPU6050 Settings
    // initialize device
    accelgyro.initialize();

    // verify connection
    Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

    // change accel/gyro scale values
    // set accelerometer to 16G and gyro to 2000 deg/sec
    accelgyro.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);
    accelgyro.setFullScaleGyroRange(MPU6050_GYRO_FS_2000);
  
    // set battery pin as input and read voltage
    pinMode(VBATTERY_PIN, INPUT);
    Vbaterry=((double)analogRead(VBATTERY_PIN))/ADC2VBAT;
  
    /////////////////////////////////
    // Magnetometer configuration ///
    
    // set MPU I2C mode to Mag
    accelgyro.setI2CMasterModeEnabled(0);
    accelgyro.setI2CBypassEnabled(1);

  // Initialize Mag Using Library functions
    mag.initialize();
    mag.setMode(HMC5883L_MODE_CONTINUOUS);
    mag.setGain(HMC5883L_GAIN_220); // set gain to +/- 8.1 Ga 
    mag.setDataRate(HMC5883L_RATE_75); 
  
    // End I2C Bypassing  
    accelgyro.setI2CBypassEnabled(0);
  
    // Define Slave address to read values of the Mag. 
    // X axis word
    accelgyro.setSlaveAddress(0, HMC5883L_DEFAULT_ADDRESS | 0x80); // 0x80 turns 7th bit ON, according to datasheet, 7th bit controls Read/Write direction
    accelgyro.setSlaveRegister(0, HMC5883L_RA_DATAX_H);
    accelgyro.setSlaveEnabled(0, true);
    accelgyro.setSlaveWordByteSwap(0, false);
    accelgyro.setSlaveWriteMode(0, false);
    accelgyro.setSlaveWordGroupOffset(0, false);
    accelgyro.setSlaveDataLength(0, 2);
  
    // Y axis word
    accelgyro.setSlaveAddress(1, HMC5883L_DEFAULT_ADDRESS | 0x80);
    accelgyro.setSlaveRegister(1, HMC5883L_RA_DATAY_H);
    accelgyro.setSlaveEnabled(1, true);
    accelgyro.setSlaveWordByteSwap(1, false);
    accelgyro.setSlaveWriteMode(1, false);
    accelgyro.setSlaveWordGroupOffset(1, false);
    accelgyro.setSlaveDataLength(1, 2);
  
    // Z axis word
    accelgyro.setSlaveAddress(2, HMC5883L_DEFAULT_ADDRESS | 0x80);
    accelgyro.setSlaveRegister(2, HMC5883L_RA_DATAZ_H);
    accelgyro.setSlaveEnabled(2, true);
    accelgyro.setSlaveWordByteSwap(2, false);
    accelgyro.setSlaveWriteMode(2, false);
    accelgyro.setSlaveWordGroupOffset(2, false);
    accelgyro.setSlaveDataLength(2, 2);
  
    accelgyro.setI2CMasterModeEnabled(1);
  
    // interrupt pins
    pinMode(2, INPUT);
    pinMode(3, INPUT);

}

///////////////
// MAIN LOOP //
///////////////
void loop(){
	
	// update battery voltage
	if ((millis()-VbatSampleMillis)>BatterySampleDelay){
		VbatSampleMillis=millis();
		Vbaterry=((double)analogRead(VBATTERY_PIN))/ADC2VBAT;
		if (Vbaterry>(BATTERYMINIMUM+0.1)){ //add 0.1 in order to remove turning on and off when battery close to value.
			BatteryStatus=1;
		}else if (Vbaterry<BATTERYMINIMUM){
			BatteryStatus=0;
		}
		//Serial.println(Vbaterry);
	}
 
	 // accelerometer interrupt ready
	 if ((millis()-MpuMillisStamp)>MPUREADDELAY){
                MpuMillisStamp=millis();
                // read raw accel/gyro measurements from device
                accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
                
                //Read magnetometer measures
                mx=accelgyro.getExternalSensorWord(0);
                my=accelgyro.getExternalSensorWord(2);
                mz=accelgyro.getExternalSensorWord(4);
    
                // convert to G scale
                AcclValue[0]=((double)ax)/SAMPLE2GACC;
                AcclValue[1]=((double)ay)/SAMPLE2GACC;
                AcclValue[2]=((double)az)/SAMPLE2GACC;
                
                // convert to Gauss scale
                MagValue[0]=mx/SAMPLE2GAUSS;
		MagValue[1]=my/SAMPLE2GAUSS;
		MagValue[2]=mz/SAMPLE2GAUSS;
                
                // calculate Norm Value
		AcclNorm=AcclValue[0]*AcclValue[0]+AcclValue[1]*AcclValue[1]+AcclValue[2]*AcclValue[2];
		AcclNorm=sqrt(AcclNorm);

                MagNorm=MagValue[0]*MagValue[0]+MagValue[1]*MagValue[1]+MagValue[2]*MagValue[2];
		MagNorm=sqrt(MagNorm);

		// update accelerometer status
		for (int ii=2; ii>0; ii--){
			AcclStatusArr[ii]=AcclStatusArr[ii-1];
		}
		if (AcclNorm<0.4){
			AcclStatusArr[0]=0;	// free fall
		} else if(AcclNorm<1.5){
			AcclStatusArr[0]=1;	// Gravity
		}else{
			AcclStatusArr[0]=2;	// bump or anything else
		}
		
		// change color based on start of free fall
		if (AcclStatusArr[0]==0){
			if (AcclStatusArr[1]==0){
				if (AcclStatusArr[2]!=0){
					PixelState=1; //change color
				}
			}
		}
		
		// One color when in hand
		if (AcclStatusArr[0]==1){
			if (AcclStatusArr[1]==1){
				if (AcclStatusArr[2]==1){
					PixelState=2; //Set color
				}
			}
		}
		
		//Serial.print("A: ");
		//// millis
		//Serial.print(millis());
		//Serial.print(", ");
		//// Using the calcAccel helper function, we can get the
		//// accelerometer readings in g's.
		//Serial.print(AcclValue[0],4);
		//Serial.print(", ");
		//Serial.print(AcclValue[1],4);
		//Serial.print(", ");
		//Serial.println(AcclValue[2],4);
		
		//Serial.println(AcclStatus);


            	// update Magnetometer status
            
		for (int ii=2; ii>0; ii--){
			MagStatusArr[ii]=MagStatusArr[ii-1];
		}
                if (MagNorm<1){
			MagStatusArr[0]=0;	// no magnet
		} else{
			MagStatusArr[0]=1;	// magnet
		}
		
		if ((MagStatusArr[0])&&(MagStatusArr[1])&&(!MagStatusArr[2])){ // magnet detected, switch between modes
			PixelsMode++;
			PixelsMode=PixelsMode%PixelsNumModes;
		}

	
	}
 
   unsigned long TimeMillis=millis();
  if (((TimeMillis-CommunicationMillisStamp)>CommunicationDelay)||(CommunicationMillisStamp>TimeMillis)){
    CommunicationMillisStamp=TimeMillis;
  
    radio.stopListening();  
    int ok = radio.write( &Vbaterry, sizeof(double) );
    if(ok){Serial.println("ok");}else{Serial.println("not ok");}
    Serial.println(Vbaterry);
    radio.startListening();    
    
  }


	// neopixels:
	if(BatteryStatus){ //lets play
		if (PixelsMode==0){
  		      //turn pixels off
  		         for(uint16_t i=0; i<strip1.numPixels(); i++) {
  			  strip1.setPixelColor(i, ColorBlack);
                          strip2.setPixelColor(i, ColorBlack);
  		         }
                      strip1.setPixelColor(1, 0xFF0000);
  		      strip1.show();
                      strip2.show();
		}

		
		if (PixelState==1){ //change color
			PixelState=0;
			CurrColorNum++;
			CurrColorNum=CurrColorNum%NUMOFCOLORS;
			Color=ColorsArr[CurrColorNum];
			if (PixelsMode==1){
				colorWipe(Color);
			}
		}
		
		if (PixelState==2){ //be green when in hand
			PixelState=0;
			Color=0x00FF00;
			if (PixelsMode==1){
				colorWipe(Color);
			}
		}
		
		if (PixelsMode==2){
			if ((millis()-theaterChaseMillis)>theaterChaseDelay){
			theaterChaseMillis=millis();
			theaterChase(Color);
			}
		}
		if (PixelsMode==3){
			if ((millis()-theaterChaseMillis)>theaterChaseDelay){
				theaterChaseMillis=millis();
				uint32_t c1=ColorsArr[CurrColorNum] ;
				uint32_t c2=ColorsArr[((CurrColorNum+1)%NUMOFCOLORS)];
				uint32_t c3=ColorsArr[((CurrColorNum+2)%NUMOFCOLORS)];
				theaterChaseMultiColor(c1,c2,c3);
			}
		}
		if (PixelsMode==4){
  		      //turn pixels 
  		         for(uint16_t i=0; i<strip1.numPixels(); i++) {
  			  strip1.setPixelColor(i, ColorBlack);
                          strip2.setPixelColor(i, ColorBlack);
  		         }
  		      strip1.show();
                      strip2.show();
                      // turn off module
                      digitalWrite(ONOFFPIN,LOW);
                      delay(100);
		}

		
		
	}else{		
		//turn pixels
		for(uint16_t i=0; i<strip1.numPixels(); i++) {
			strip1.setPixelColor(i, ColorBlack);
                        strip2.setPixelColor(i, ColorBlack);
		}
		strip1.show();
                strip2.show();
                // turn off module
                digitalWrite(ONOFFPIN,LOW);
	}			

}// end main



/////////////////////////
// Neo Pixels Functions//
/////////////////////////

// Wipe color Color
void colorWipe(uint32_t c) {
  for(uint16_t i=0; i<strip1.numPixels(); i++) {
      strip1.setPixelColor(i, c);
      strip1.show();
  }
    for(uint16_t i=0; i<strip2.numPixels(); i++) {
      strip2.setPixelColor(i, c);
      strip2.show();
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c) {
	theaterChasePixel++;
	theaterChasePixel=theaterChasePixel%3;
	// set all pixels off
	for (int i=0; i<strip1.numPixels();i++){
		strip1.setPixelColor(i,ColorBlack);
	}
	//turn every third pixel on starting at the theaterChasePixel
	for (int i=theaterChasePixel; i<strip1.numPixels();i=i+3){
		strip1.setPixelColor(i, c);    
	}
	strip1.show();

	for (int i=0; i<strip2.numPixels();i++){
		strip2.setPixelColor(i,ColorBlack);
	}
	//turn every third pixel on starting at the theaterChasePixel
	for (int i=theaterChasePixel; i<strip2.numPixels();i=i+3){
		strip2.setPixelColor(i, c);    
	}
	strip2.show();
}

//Theatre-style crawling lights multi color.
void theaterChaseMultiColor(uint32_t c1, uint32_t c2, uint32_t c3) {
	theaterChasePixel++;
	theaterChasePixel=theaterChasePixel%3;
	// color every third pixel	
	for (int i=0; i<strip1.numPixels();i=i+3){ 
		strip1.setPixelColor((i+theaterChasePixel)%strip1.numPixels(),c1);
	}
	// color every third pixel
	for (int i=0; i<strip1.numPixels();i=i+3){
		strip1.setPixelColor((i+theaterChasePixel+1)%strip1.numPixels(),c2);
	}
	// color every third pixel
	for (int i=0; i<strip1.numPixels();i=i+3){
		strip1.setPixelColor((i+theaterChasePixel+2)%strip1.numPixels(),c3);
	}
	strip1.show();

	// color every third pixel	
	for (int i=0; i<strip2.numPixels();i=i+3){ 
		strip2.setPixelColor((i+theaterChasePixel)%strip2.numPixels(),c1);
	}
	// color every third pixel
	for (int i=0; i<strip2.numPixels();i=i+3){
		strip2.setPixelColor((i+theaterChasePixel+1)%strip2.numPixels(),c2);
	}
	// color every third pixel
	for (int i=0; i<strip2.numPixels();i=i+3){
		strip2.setPixelColor((i+theaterChasePixel+2)%strip2.numPixels(),c3);
	}
	strip2.show();
}

