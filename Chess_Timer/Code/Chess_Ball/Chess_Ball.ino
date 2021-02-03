// Chess Balls 18/08/2015 11:37

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
#define BallNumber 4
#define SendAddress 1
#define NUMPLAYERS 15
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
#define REEDSWITCHPIN 8

///////////////
// MPU 6050 //
///////////////

MPU6050 accelgyro;

#define MPUREADDELAY 10
unsigned long MPU_Millis=0;

const byte INT_MPU_PIN = 2; // INT DRDYG

int16_t ax, ay, az;
int16_t gx, gy, gz;

double AcclValue[3]={0,0,0}; //in G values
double GyroValue[3]={0,0,0}; //in Degrees per second

#define SAMPLE2GACC 2048.0   // +/-16g full scale, 16 bit resolution--> 2^16/32=2048
#define SAMPLE2AGYRO 16.384  // +/-2000 Deg/Sec, 16 but tesolution--> 2^16/4000=16.384

// Calculate norm, HPF, LPF
double AcclNorm=0;	
// LPF (Low Pass Filter) Coef for Acceleration value
double AcclNormLPF=0;
#define LPFACC 0.98
double AcclNormHPF=0;
//Record Tap Millis
unsigned long TapMillis=0;
unsigned long LastTapMillis=0;
#define DOUBLETAPTHRESH 350
//Accelerometer states, for tap detection
int AcclState[2]={0,0};
#define TAPTHRESH 3.0
////////////////////////////
// communication variable //
////////////////////////////
// players addresses
const uint64_t BallsAddresses[NUMPLAYERS] ={0xF3b1b3F5c3LL,0xF3b2b3F5c3LL,0xF3b3b3F5c3LL,0xF3b4b3F5c3LL,0xF3b5b3F5c3LL,0xF3b6b3F5c3LL,0xF3b7b3F5c3LL,0xF3b8b3F5c3LL,0xF3b9b3F5c3LL,0xF3bab3F5c3LL,0xF3bbb3F5c3LL,0xF3bcb3F5c3LL,0xF3bdb3F5c3LL,0xF3beb3F5c3LL,0xF3bfb3F5c3LL};
// pipe addresses
const uint64_t Txpipes[2] ={0xb3F3d3c3d2LL,0xb3F3d3c3b4LL};
const uint64_t Rxpipes =BallsAddresses[BallNumber];
// communication time delay
unsigned long CommunicationMillisStamp=0;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

/////////////
// Battery //
/////////////
double Vbaterry=3.7;
#define VBATTERY_PIN A0
#define ADC2VBAT 163.0 // Convert ADC reading to voltage, VBatt*10/57=Vref/ADC_Res Vref=1.1v ADC_Res=1023 
#define BATTERYMINIMUM 3.3 // minimum allowed battery voltage
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


///////////
// SETUP //
///////////
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
  radio.setChannel(52);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_2MBPS);
  radio.setPayloadSize(1);
  radio.setRetries(5,15);
  radio.openWritingPipe(Txpipes[SendAddress]);
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
      Serial.println(Vbaterry);
    }
      //Serial.println(Vbaterry);
  }
  if (BatteryStatus){
    // accelerometer data read
    if ((millis()-MPU_Millis)>MPUREADDELAY){
      MPU_Millis=millis();
      // read raw accel/gyro measurements from device
      accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
      AcclValue[0]=((double)ax)/SAMPLE2GACC;
      AcclValue[1]=((double)ay)/SAMPLE2GACC;
      AcclValue[2]=((double)az)/SAMPLE2GACC;
      
      AcclNorm=AcclValue[0]*AcclValue[0]+AcclValue[1]*AcclValue[1]+AcclValue[2]*AcclValue[2];
  	    AcclNorm=sqrt(AcclNorm);
      // calculate HPF value of the acceleration
      AcclNormLPF=AcclNormLPF*(LPFACC) + (AcclNorm)*(1-LPFACC); 
      AcclNormHPF=AcclNorm-AcclNormLPF;
      
      // tap detect
      //Store Last States
      AcclState[1]=AcclState[0];
      if (AcclNormHPF>TAPTHRESH){
        AcclState[0]=1;
        //Serial.println("TAP");
      }else{
        AcclState[0]=0;
      }
      // tap if last state is 0 and the current is high.
      if ((AcclState[0])&&(!AcclState[1])){
        LastTapMillis=TapMillis;
        TapMillis=millis();
        Serial.println("TAP");
        // Double Tap Detection:
        if ((TapMillis-LastTapMillis)<DOUBLETAPTHRESH){
          Serial.println("Double TAP");
          // send update to the Timer that double tap was performed
          radio.stopListening();
          // send twice, sometimes its not arriving (some bug?)
          byte address=SendAddress;
          bool ok = radio.write( &address, 1 );
          //ok = radio.write( &address, 1 );
          if (ok) Serial.println("ok..."); else Serial.println("failed");
          radio.startListening();
          for(int i=0;i<NEOPIXELS;i++){
            // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
            strip.setPixelColor(i, strip.Color(0,150,0)); // Moderately bright green color.
          }
          strip.show(); // This sends the updated pixel color to the hardware.
        }
      }
    }
    if (radio.available()){
      bool done = false;
      while (!done){
        byte address=0; 
        done = radio.read( &address, 1 ); 
        Serial.println(address);
          for(int i=0;i<NEOPIXELS;i++){
            // Set Color based on player
            if (SendAddress) strip.setPixelColor(i, strip.Color(255,0,0));
            else strip.setPixelColor(i, strip.Color(0,0,255));
          }
          strip.show(); // This sends the updated pixel color to the hardware.
       }
     }
  }else{ // battery empty
    for(int i=0;i<NEOPIXELS;i++){
      strip.setPixelColor(i, strip.Color(0,0,0));
    }
    strip.show();
    delay(1000);
  }    
}// end main
