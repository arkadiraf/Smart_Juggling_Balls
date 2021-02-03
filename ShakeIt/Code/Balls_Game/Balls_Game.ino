// Balls_Game 21/05/2015 1:20

/* Pinout:
    Arduino 3.3V:
    A5 (SCL)-->SCL
    A4 (SDA)-->SDA
    D2-->INT DRDYG
    
    D8-->Reed Switch
    
    D6-->NeoPIxel DataIn
    
    A0-->Vraw/2
    
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
#define BallNumber 13
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

///////////////
// Variables //
///////////////

#define rad2deg 57.295

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
	
////////////////////////////
// communication variable //
////////////////////////////
#define NUMPLAYERS 15
// players addresses
const uint64_t BallsAddresses[NUMPLAYERS] ={0xF3b1b3F5c3LL,0xF3b2b3F5c3LL,0xF3b3b3F5c3LL,0xF3b4b3F5c3LL,0xF3b5b3F5c3LL,0xF3b6b3F5c3LL,0xF3b7b3F5c3LL,0xF3b8b3F5c3LL,0xF3b9b3F5c3LL,0xF3bab3F5c3LL,0xF3bbb3F5c3LL,0xF3bcb3F5c3LL,0xF3bdb3F5c3LL,0xF3beb3F5c3LL,0xF3bfb3F5c3LL};
#define CommunicationDelay 100
// Single radio pipe address for the 2 nodes to communicate.
unsigned long CommunicationMillisStamp=0;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);
// Radio pipe addresses for the 2 nodes to communicate.
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
const uint64_t Txpipes[2] ={0xb3F3F3b3b5LL,0xb3F3F3b3b7LL};
const uint64_t Rxpipes =BallsAddresses[BallNumber];
#define NTWIDN 0xF3F3F3

/////////////
// Battery //
/////////////
double Vbaterry=3.7;
#define VBATTERY_PIN A0
#define ADC2VBAT 163.0 // Convert ADC reading to voltage, VBatt*10/57=Vref/ADC_Res Vref=1.1v ADC_Res=1023 
#define BATTERYMINIMUM 3.0 // minimum allowed battery voltage
byte BatteryStatus=1; // variable to indicate battery status,0- empty, 1-full
#define BatterySampleDelay 1000 // sample period of the battery voltage
unsigned long VbatSampleMillis=0; // variable to hold millis counter

////////////////
// Neo Pixels //
////////////////
#define NEOPIXELS 8
#define NEODATAPIN 6
// neo Pixel initialize
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXELS, NEODATAPIN, NEO_GRB + NEO_KHZ800);


/////////////////////
// Juggling Status //
/////////////////////

double AcclNorm=0;

#define REEDSWITCHPIN 8
int MagStatus=0;
// define colors array
// [White,Red,Lime,Blue,Yellow,Cyan,Magenta,Maroon,Olive,Green,Purple,Teal,Navy]  from http://www.rapidtables.com/web/color/RGB_Color.htm
#define NUMOFCOLORS 13
uint32_t ColorsArr[NUMOFCOLORS]={0xFFFFFF,0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0x00FFFF,0xFF00FF,0x800000,0x808000,0x008000,0x800080,0x008080,0x000080};
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
#define PixelsNumStates 3 // not in use

// set pixel mode:
// 0- off, 1- all colors the same, 2- chasing one color, 3-chasing 2 colors, 4-6-Game Mode
byte PixelsMode=0;
#define PixelsNumModes 7
// variable to store pixel referance for theater chasing mode
int theaterChasePixel=0;

unsigned long theaterChaseMillis=0; // timer counter
#define theaterChaseDelay 100

// color update rate
unsigned long ColorUpdate=0;


///////////////
// Game Mode //
///////////////
// variable to define game mode status
byte GameModeSimple=1; // 1- simple game, 0 complex game
#define GAMEMODECOMPLEX 0xF0F0F0
#define GETCOLOR 0xFF00FF
#define WINRED 0xFF0000
#define WINGREEN 0x00FF00
#define WINBLUE 0x0000FF
#define SPARKUP 0xb0b0b0
uint32_t GameColor=0x000000;

// color update rate
unsigned long GameColorUpdate=0;

// color value converter
#define ACC2COLOR  35.0// 8 G is scaled to 255
double ColorGameValue=0;
byte ColorGameByte=0;
// LPF (Low Pass Filter) Coef for Acceleration value
double AcclNormLPF=0;
#define LPFACC 0.98

///////////////
// SETUP	 //
///////////////
void setup(){
      // join I2C bus (I2Cdev library doesn't do this automatically)
    Wire.begin();
    // initialize serial communication
    Serial.begin(57600);
  // neo Pixel initialize
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  // Nrf initialize
  radio.begin();
  radio.setChannel(79);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(8);
  radio.setRetries(0,0);
  radio.setAutoAck(false);
  radio.openWritingPipe(Txpipes[0]);
  radio.openReadingPipe(1,Rxpipes);
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
  
  // set battery pin as input  with internal referense of 1.1v and read voltage
  analogReference(INTERNAL);
  delay(10);// settle down
  pinMode(VBATTERY_PIN, INPUT);
  Vbaterry=((double)analogRead(VBATTERY_PIN))/ADC2VBAT;
  
  // Set pin modes
  pinMode(REEDSWITCHPIN, INPUT);
  // interrupt pins
  pinMode(2, INPUT);
  pinMode(3, INPUT);

}

///////////////
// MAIN LOOP //
///////////////
void loop(){
  //Serial.println(millis());
	
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
                AcclValue[0]=((double)ax)/SAMPLE2GACC;
                AcclValue[1]=((double)ay)/SAMPLE2GACC;
                AcclValue[2]=((double)az)/SAMPLE2GACC;
                
		AcclNorm=AcclValue[0]*AcclValue[0]+AcclValue[1]*AcclValue[1]+AcclValue[2]*AcclValue[2];
		AcclNorm=sqrt(AcclNorm);

                // calculate LPF value of the acceleration
                AcclNormLPF=AcclNormLPF*(LPFACC) + (AcclNorm-1)*(1-LPFACC); // normalized without gravity (more or less).
                
                // Calculate Color Game Value
                ColorGameValue=(AcclNormLPF*ACC2COLOR);
                if (ColorGameValue>255){ // set max value
                  ColorGameByte=255;
                }else if (ColorGameValue<5){ // set threshhold value
                  ColorGameByte=5;
                }else{
                 ColorGameByte=(byte)ColorGameValue;
                }
                
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


            	// update ReedSwitch status
                MagStatus=digitalRead(REEDSWITCHPIN);
            
		for (int ii=2; ii>0; ii--){
			MagStatusArr[ii]=MagStatusArr[ii-1];
		}
                MagStatusArr[0]=MagStatus;
		
		if ((MagStatusArr[0])&&(MagStatusArr[1])&&(!MagStatusArr[2])){ // magnet detected, switch between modes
			PixelsMode++;
			PixelsMode=PixelsMode%PixelsNumModes;
		}

	
	}
 
  
  
  
        // simple game mode
        if (GameModeSimple){
            unsigned long TimeMillis=millis();
          if (((TimeMillis-CommunicationMillisStamp)>CommunicationDelay)||(CommunicationMillisStamp>TimeMillis)){
            CommunicationMillisStamp=TimeMillis;
            radio.stopListening(); 
            if ((PixelsMode==4)||(PixelsMode==5)||(PixelsMode==6)){ // game modes
            uint32_t WritePacket[2]={NTWIDN,GameColor};
            radio.write( &WritePacket, sizeof(uint32_t)*2 );
            Serial.println(GameColor,HEX);
            }
            radio.startListening();    
            
          }
        }
        //check available data:
        if (radio.available()){
          uint32_t RcvPacket[2]={0,0};
            while (!radio.read( &RcvPacket, sizeof(uint32_t)*2 )); // empty buffer
            if (RcvPacket[0]==NTWIDN){
              if (RcvPacket[1]==GAMEMODECOMPLEX){
                radio.stopListening(); 
                if (GameModeSimple){  // change writing pipe, to complex game pipe
                radio.openWritingPipe(Txpipes[1]);
                GameModeSimple=0;
                }
                //delay(1);
                uint32_t WritePacket[2]={NTWIDN,GAMEMODECOMPLEX}; // send Ack
                radio.write( &WritePacket, sizeof(uint32_t)*2 );
                radio.startListening();
              }else if ((RcvPacket[1]==GETCOLOR)&&(!GameModeSimple)){  
                radio.stopListening(); 
                if ((PixelsMode==4)||(PixelsMode==5)||(PixelsMode==6)){ // game modes
                uint32_t WritePacket[2]={NTWIDN,GameColor};
                radio.write( &WritePacket, sizeof(uint32_t)*2 );
                Serial.println(GameColor,HEX);
                }
                radio.startListening(); 
              }else if((RcvPacket[1]==WINRED)&&((PixelsMode==4))){
               colorWipe(WINRED);
               delay(2000);
               rainbow(10);
               colorWipe(WINRED);
               delay(2000);
              }else if((RcvPacket[1]==WINGREEN)&&((PixelsMode==5))){
               colorWipe(WINGREEN);
               delay(2000);
               rainbow(10);
               colorWipe(WINGREEN);
               delay(2000);
              }else if((RcvPacket[1]==WINBLUE)&&((PixelsMode==6))){
               colorWipe(WINBLUE);
               delay(2000);
               rainbow(10);
               colorWipe(WINBLUE);
               delay(2000);
              }else if(RcvPacket[1]==SPARKUP){
               colorWipe(0xFF0000);
               delay(500);
               colorWipe(0x00FF00);
               delay(500);
               colorWipe(0x0000FF);
               delay(500);
               rainbow(10);
              }
            }
        } 

	// neopixels:
	if((BatteryStatus)&&(PixelsMode)){ //lets play
		
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
		if (PixelsMode==4){ //game mode
                  if (millis()-GameColorUpdate>100){
                    GameColorUpdate=millis();
                    // update color value
                    GameColor=strip.Color(ColorGameByte,0,0);
                    Serial.print(AcclNormLPF);
                    Serial.print("  ");
                    Serial.println(GameColor,HEX);
                  
                  colorWipe(GameColor);
                  }
		}
		if (PixelsMode==5){ //game mode
                  if (millis()-GameColorUpdate>100){
                    GameColorUpdate=millis();
                    // update color value
                    GameColor=strip.Color(0,ColorGameByte,0);
                    Serial.print(AcclNormLPF);
                    Serial.print("  ");
                    Serial.println(GameColor,HEX);
                  
                  colorWipe(GameColor);
                  }
		}
		if (PixelsMode==6){ //game mode
                  if (millis()-GameColorUpdate>100){
                    GameColorUpdate=millis();
                    // update color value
                    GameColor=strip.Color(0,0,ColorGameByte);
                    Serial.print(AcclNormLPF);
                    Serial.print("  ");
                    Serial.println(GameColor,HEX);
                  
                  colorWipe(GameColor);
                  }
		}
		
		
	}else{	
                if (millis()-ColorUpdate>1000){
                  if (!GameModeSimple){// reset game mode
                    radio.stopListening();
                    radio.openWritingPipe(Txpipes[0]);
                    radio.startListening();
                    GameModeSimple=1; 
                  }
                    ColorUpdate=millis();
                   static byte statetoggle=0;
                   statetoggle++;	
    		//turn pixels off except one
    		for(uint16_t i=0; i<strip.numPixels(); i++) {
    			strip.setPixelColor(i, ColorBlack);
    		}
                  if (statetoggle%2){
    		    strip.setPixelColor(0,0x100000);
                    }
                    else{
                    strip.setPixelColor(0,0x000000);
                    }
    		strip.show();
                }
  
	}			

}// end main



/////////////////////////
// Neo Pixels Functions//
/////////////////////////

// Wipe color Color
void colorWipe(uint32_t c) {
	for(uint16_t i=0; i<strip.numPixels(); i++) {
		strip.setPixelColor(i, c);
	}
	strip.show();
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c) {
	theaterChasePixel++;
	theaterChasePixel=theaterChasePixel%3;
	// set all pixels off
	for (int i=0; i<strip.numPixels();i++){
		strip.setPixelColor(i,ColorBlack);
	}
	//turn every third pixel on starting at the theaterChasePixel
	for (int i=theaterChasePixel; i<strip.numPixels();i=i+3){
		strip.setPixelColor(i, c);    
	}
	strip.show();
}

//Theatre-style crawling lights multi color.
void theaterChaseMultiColor(uint32_t c1, uint32_t c2, uint32_t c3) {
	theaterChasePixel++;
	theaterChasePixel=theaterChasePixel%3;
	// color every third pixel	
	for (int i=0; i<strip.numPixels();i=i+3){ 
		strip.setPixelColor((i+theaterChasePixel)%strip.numPixels(),c1);
	}
	// color every third pixel
	for (int i=0; i<strip.numPixels();i=i+3){
		strip.setPixelColor((i+theaterChasePixel+1)%strip.numPixels(),c2);
	}
	// color every third pixel
	for (int i=0; i<strip.numPixels();i=i+3){
		strip.setPixelColor((i+theaterChasePixel+2)%strip.numPixels(),c3);
	}
	strip.show();
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}




// calculate heading
float calcHeading(float hx, float hy)
{
	if (hy > 0)
	{
		return 90 - (atan(hx / hy) * 180 / PI);
	}
	else if (hy < 0)
	{
		return 270 - (atan(hx / hy) * 180 / PI);
	}
	else // hy = 0
	{
		if (hx < 0) return 180;
		else return 0;
	}
}
