/*
///////////////////////////////////////////////
//        Chess Timer V_1 19/8/15            //
// Arkadi Rafalovich - Arkadiraf@gmail.com   //
///////////////////////////////////////////////
    Setup:
    Arduino nano
    Bluetooth HC-06
    Addressable 16 LED`s two rings
    NRF Module

    Pinout:
    Arduino Nano 5V: 
    D9-->CE NRF
    D10-->CSN NRF
    D13 (SCK)-->SCK NRF
    D11 (MOSI)-->MOSI NRF
    D12 (MISO)-->MISO NRF
    D3-->IRQ
    
    D6-->NeoPIxel DataIn 1
    D7-->NeoPIxel DataIn 2
    
    Command Protocol:
    <EndGame>\r\n                       Turn Effects On
    <Brightness><BrightnessNUM>\r\n     Set Brightness
    <SetTime><TimeSeconds>\r\n          Set Game Time, (Seconds)
    <Restart>\r\n                       Restart Game
    <Data><DataNum>\r\n                 Set Data on off
    <AddTime><AddTimeNum>\r\n           Add Time in seconds after each move
*/
#define BallNumber1 3
#define BallNumber2 4
#define NUMPLAYERS 15
///////////////
// libraries //
///////////////
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Adafruit_NeoPixel.h>
///////////////
// Variables //
///////////////

#define SAMPLE2GACC 2048.0   // +/-16g full scale, 16 bit resolution--> 2^16/32=2048
#define SAMPLE2AGYRO 16.384  // +/-2000 Deg/Sec, 16 but tesolution--> 2^16/4000=16.384

///////////////////////////
// Time Keeper Variables //
///////////////////////////
// players time
long SetPlayersTime=15000;         // start with 5 minuts default (5*60*1000 = 300000)
long Player1millis=SetPlayersTime; 
long Player2millis=SetPlayersTime;

// Variable to add time after each move
long IncreaseTime=1000; // variable in millis
//TimeStamp for time counter
long TimeStampCounter=0;
//check timout
unsigned long CheckTimeoutMillis=0;
#define TIMEOUTINTERVAL 100

// variable for the current player
int SetPlayer=0;



////////////////////////////
// communication variable //
////////////////////////////
// players addresses
const uint64_t BallsAddresses[NUMPLAYERS] ={0xF3b1b3F5c3LL,0xF3b2b3F5c3LL,0xF3b3b3F5c3LL,0xF3b4b3F5c3LL,0xF3b5b3F5c3LL,0xF3b6b3F5c3LL,0xF3b7b3F5c3LL,0xF3b8b3F5c3LL,0xF3b9b3F5c3LL,0xF3bab3F5c3LL,0xF3bbb3F5c3LL,0xF3bcb3F5c3LL,0xF3bdb3F5c3LL,0xF3beb3F5c3LL,0xF3bfb3F5c3LL};
// pipe addresses
const uint64_t Rxpipes[2] ={0xb3F3d3c3d2LL,0xb3F3d3c3b4LL};
const uint64_t Txpipes =BallsAddresses[BallNumber1];
uint8_t pipe_num=0;
// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

///////////////////////////
// Serial Input Command  //
///////////////////////////
// communication protocol is based on my own open source protocol implemented in other open source projects

String inputString = "                              ";         // a string to hold incoming data (30 chars length)
boolean stringComplete = false;  // whether the string is complete
int stringCMDStart=-1; // Command String Start
int stringCMDEnd=-1; // Command String Ends
int stringCMDValueStart=-1; // Command Value String Start
int stringCMDValueEnd=-1; // Command Value String Ends
// Split the inputString into Command and Value

String Command="                    "; // The Commands (20 chars length)
String CommandValueString="          "; // The Values (10 chars length)
char CommandValueChar[10]={"         "};
int CommandValue=0; // The Values in Int

////////////////
// Neo Pixels //
////////////////
#define NEODATAPIN1 6
#define NEODATAPIN2 7
#define NEOPIXELS 16
// neo Pixel initialize
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(26, NEODATAPIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(26, NEODATAPIN2, NEO_GRB + NEO_KHZ800);

// Define how much "units" of color available on a strip
#define AMOUNTOFCOLOR 4080.0   //16 LED 255 Strengh units. total 4080

// Brightness value
int LEDBrightness=50;

// Variable to show data to pc.
int DataNum=1;


////////////
// SETUP  //
////////////
void setup(){
    // initialize serial communication
    Serial.begin(57600);
  
  // Nrf initialize
  radio.begin();
  radio.setChannel(52);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_2MBPS);
  radio.setPayloadSize(1);
  radio.setRetries(5,15);
  radio.openWritingPipe(Txpipes);
  radio.openReadingPipe(1,Rxpipes[0]);
  radio.openReadingPipe(2,Rxpipes[1]);
  radio.startListening();

  // interrupt pins
  pinMode(3, INPUT); 
 
   // neo Pixel initialize
  strip1.begin();
  strip1.show(); // Initialize all pixels to 'off'
  strip2.begin();
  strip2.show(); // Initialize all pixels to 'off' 
  // Set pixel Brightness
  strip1.setBrightness(LEDBrightness);
  strip2.setBrightness(LEDBrightness);
}// End Setup

///////////////
// MAIN LOOP //
///////////////
void loop(){
  // If string is complete then parse string and execute the command. (From Bluetooth)
  if (stringComplete) {
    ParseString();
    ExecuteCommand();
  }
  
 // receive message
 if (radio.available(&pipe_num)){  
  if (TimeStampCounter==0){TimeStampCounter=millis();} //first round test:
    bool done = false;
    while (!done){
      byte address=0; 
      done = radio.read( &address, 1 );
      //Serial.println(address);
      //Serial.println(pipe_num);
        
        // message from ball 1 
        if (pipe_num==1){ 
         radio.stopListening();
         radio.openWritingPipe(BallsAddresses[BallNumber2]);
         // send twice, sometimes its not arriving (some bug?)
          byte MsgSend=BallNumber2;
          bool ok = radio.write( &MsgSend, 1 );
          ok = radio.write( &MsgSend, 1 );
          //if (ok) Serial.println("ok..."); else Serial.println("failed");
         radio.startListening();
         // check if its a player switch or the same player
         if (SetPlayer!=pipe_num){
           // player switched:
           SetPlayer=pipe_num;
           // reduce the other players time
           Player1millis=Player1millis-(millis()-TimeStampCounter)+IncreaseTime;
           // update ring with the results
           long ColorsLeft=(long)((((double)Player1millis)/((double)SetPlayersTime))*AMOUNTOFCOLOR);
           //Serial.println(ColorsLeft); 
           FillStrip(3,ColorsLeft,&strip1);
           //Serial.println("Player1millis");
           //Serial.println(Player1millis);
           TimeStampCounter=millis(); // take a time stamp
         }
         
         // message from ball 2 
        }else if(pipe_num==2){
         radio.stopListening();
         radio.openWritingPipe(BallsAddresses[BallNumber1]);
         // send twice, sometimes its not arriving (some bug?)
          byte MsgSend=BallNumber1;
          bool ok = radio.write( &MsgSend, 1 );
          ok = radio.write( &MsgSend, 1 );
          //if (ok) Serial.println("ok..."); else Serial.println("failed");
         radio.startListening();
         
         // check if its a player switch or the same player
         if (SetPlayer!=pipe_num){
           // player switched:
           SetPlayer=pipe_num;
           // reduce the other players time
           Player2millis=Player2millis-(millis()-TimeStampCounter)+IncreaseTime;
           // Update Strip with the results
           long ColorsLeft=(long)((((double)Player2millis)/((double)SetPlayersTime))*AMOUNTOFCOLOR);
           //Serial.println(ColorsLeft); 
           FillStrip(1,ColorsLeft,&strip2);
           //Serial.println("Player2millis");
           //Serial.println(Player2millis);
           TimeStampCounter=millis(); // take a time stamp
         }
          
        }
      }
 
  }
 
 // check time status and update leds:
 if ((millis()-CheckTimeoutMillis)>TIMEOUTINTERVAL){
   CheckTimeoutMillis=millis();
   // check players time status.
   if (SetPlayer==2){
     long TimeLeftMillis=Player1millis-(long)(millis()-TimeStampCounter);
     if (TimeLeftMillis<0) TimeLeftMillis=0;
       if (DataNum==1){
         static unsigned int ShowNum=0;
         ShowNum++;
         if (ShowNum%5==0){ // Send update at TIMEOUTINTERVAL/5
           Serial.print("Player1 Time:  ");
           Serial.println(TimeLeftMillis);
         }
       }
     long ColorsLeft=(long)((((double)TimeLeftMillis)/((double)SetPlayersTime))*AMOUNTOFCOLOR);
     //Serial.println(ColorsLeft); 
     FillStrip(3,ColorsLeft,&strip1);
       if (TimeLeftMillis<=0){
         Serial.println("Player1 Timeout");
         SetPlayer=0;
         // do something about it
         for (int i=0; i<5; i++){ // flash 5 times
         colorWipeStrip(0x000000,&strip1);
         delay(500);
         colorWipeStrip(0x0000FF,&strip1);
         delay(500);
         }
         EndOfGame(); // throw in some effects
          
       }
   }else if (SetPlayer==1){
     long TimeLeftMillis=Player2millis-(long)(millis()-TimeStampCounter);
     if (TimeLeftMillis<0) TimeLeftMillis=0;
       if (DataNum==1){
        static unsigned int ShowNum=0;
         ShowNum++;
         if (ShowNum%5==0){ // Send update at TIMEOUTINTERVAL/5
           Serial.print("Player2 Time:  ");
           Serial.println(TimeLeftMillis);
         }
       }
     long ColorsLeft=(long)((((double)TimeLeftMillis)/((double)SetPlayersTime))*AMOUNTOFCOLOR);
     //Serial.println(ColorsLeft);
     FillStrip(1,ColorsLeft,&strip2);
       if (TimeLeftMillis<=0){
         Serial.println("Player2 Timeout");
         SetPlayer=0;
         // do something about it
         for (int i=0; i<5; i++){ // flash 5 times
         colorWipeStrip(0x000000,&strip2);
         delay(500);
         colorWipeStrip(0xFF0000,&strip2);
         delay(500);
         }
         EndOfGame(); // throw in some effects
         
       }
   }else{ // Game didn`t start yet, initialize parameter color the strips
     Player1millis=SetPlayersTime;
     Player2millis=SetPlayersTime;
     TimeStampCounter=0;
     
     colorWipeStrip(0xFF0000,&strip2);
     colorWipeStrip(0x0000FF,&strip1);
   }
 }
 
          
}// end main

/////////////////
// SerialEvent //
/////////////////
/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs.
*/
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    } 
  }
} // end serial event

/////////////////////////////
// Parse Recieved Command  //
/////////////////////////////
void ParseString(void)
{
  stringCMDStart=inputString.indexOf('<');
  stringCMDEnd=inputString.indexOf('>',stringCMDStart+1);
  stringCMDValueStart=inputString.indexOf('<',stringCMDEnd+1);
  stringCMDValueEnd=inputString.indexOf('>',stringCMDValueStart+1);
  
  // seperate command and value
  if (stringCMDStart!=-1&&stringCMDEnd!=-1){
    Command=inputString.substring(stringCMDStart+1,stringCMDEnd);
    if (stringCMDValueStart!=-1&&stringCMDValueEnd!=-1){
      CommandValueString=inputString.substring(stringCMDValueStart+1,stringCMDValueEnd);
      CommandValueString.toCharArray(CommandValueChar, 10);
      CommandValue=atoi(CommandValueChar);
    }else{
//      Serial.println("Invalid Value");
      CommandValueString="Inv Val";
      CommandValue=-1;
    }
  }else{
//    Serial.println("Invalid CMD");
    Command="Inv CMD";
  }
  
  // Print Command and Value
  Serial.print(Command);
  Serial.print(",");
  Serial.println(CommandValue);
  
  // clear the string and restart status:
  inputString = "";
  stringComplete = false;  
}// end parse string

/////////////////////////////
// Execute Recieved Command//
/////////////////////////////
//    Command Protocol:
//    <EndGame>\r\n                       Turn Effects On
//    <Brightness><BrightnessNUM>\r\n     Set Brightness
//    <SetTime><TimeSeconds>\r\n          Set Game Time, (Seconds)
//    <Restart>\r\n                       Restart Game
//    <Data><DataNum>\r\n                 Set Data on off
//    <AddTime><AddTimeNum>\r\n           Add Time in seconds after each move
void ExecuteCommand(void)
{
  if (Command.equalsIgnoreCase("EndGame")){
    EndOfGame();
  }else if(Command.equalsIgnoreCase("Restart")){
    // initialize parameters
    Player1millis=SetPlayersTime;
    Player2millis=SetPlayersTime;
    SetPlayer=0;
    TimeStampCounter=0;
  }else if(Command.equalsIgnoreCase("SetTime")){
    SetPlayersTime=CommandValue;
    SetPlayersTime=SetPlayersTime*1000;
    Player1millis=SetPlayersTime;
    Player2millis=SetPlayersTime;
    SetPlayer=0;
    TimeStampCounter=0;
  }else if(Command.equalsIgnoreCase("Brightness")){
    LEDBrightness=CommandValue;
    strip1.setBrightness(LEDBrightness);
    strip2.setBrightness(LEDBrightness);
  }else if(Command.equalsIgnoreCase("Data")){
    DataNum=CommandValue;
  }else if(Command.equalsIgnoreCase("AddTime")){
    IncreaseTime=CommandValue;
    IncreaseTime=IncreaseTime*1000;
  }
  Command=""; // initialize command  
}

////////////////
// End OF GAME//
////////////////
void EndOfGame(void){
  // Adafruit Effects
  colorWipe(strip1.Color(0, 255, 0), 50); // Green
  colorWipe(strip1.Color(255, 0, 0), 50); // Red
  colorWipe(strip1.Color(0, 0, 255), 50); // Blue
  //rainbow(10);
  rainbowCycle(10);
  //theaterChaseRainbow(25);
  theaterChase(strip1.Color(0, 127, 0), 50); // Green
  theaterChase(strip1.Color(127,   0,   0), 50); // Red
  theaterChase(strip1.Color(  0,   0, 127), 50); // Blue
  
  // Due to delays caused by the effects, dump all data in NRF buffer:
  if (radio.available(&pipe_num)){  
    bool done = false;
    while (!done){
      byte address=0; 
      done = radio.read( &address, 1 );
    }
  }
  // initialize parameters
  Player1millis=SetPlayersTime;
  Player2millis=SetPlayersTime;
  SetPlayer=0;
  TimeStampCounter=0;

} // End End Of Game

/////////////////////////
// Neo Pixels Functions//
/////////////////////////

// Wipe color
void colorWipe(uint32_t c) {
  for(uint16_t i=0; i<strip1.numPixels(); i++) {
      strip1.setPixelColor(i, c);
  }
  strip1.show();
    for(uint16_t i=0; i<strip2.numPixels(); i++) {
      strip2.setPixelColor(i, c);
  }
  strip2.show();
}
// Wipe color of a specific strip
void colorWipeStrip(uint32_t c,Adafruit_NeoPixel *Strip) {
  for(uint16_t i=0; i<Strip->numPixels(); i++) {
      Strip->setPixelColor(i, c);
  }
  Strip->show();
}

// Fill Up Strip based on time left
void FillStrip(int FillCollor,long ColorLeft, Adafruit_NeoPixel *Strip){
 int SetColor=0;
 uint32_t c=0; 
  for(uint16_t i=0; i<Strip->numPixels(); i++) {
    if (ColorLeft<=255){
       SetColor=ColorLeft;
    }else{
      SetColor=255;
    }
    ColorLeft=ColorLeft-SetColor;
    // update color based on the chosen color
    if (FillCollor==1){
      c=Strip->Color(SetColor, 0, 0);
    }else if (FillCollor==2){
      c=Strip->Color(0, SetColor, 0);
    }else if (FillCollor==3){
      c=Strip->Color(0, 0, SetColor);
    }
    Strip->setPixelColor(i, c);
  }
  Strip->show();
}

///////////////////////////////////////
// neo pixel example library Effects //
///////////////////////////////////////

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip1.numPixels(); i++) {
      strip1.setPixelColor(i, c);
      strip2.setPixelColor(i, c);
      strip1.show();
      strip2.show();
      delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip1.numPixels(); i++) {
      strip1.setPixelColor(i, Wheel((i+j) & 255));
      strip2.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip1.show();
    strip2.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip1.numPixels(); i++) {
      strip1.setPixelColor(i, Wheel(((i * 256 / strip1.numPixels()) + j) & 255));
      strip2.setPixelColor(i, Wheel(((i * 256 / strip1.numPixels()) + j) & 255));
    }
    strip1.show();
    strip2.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip1.numPixels(); i=i+3) {
        strip1.setPixelColor(i+q, c);    //turn every third pixel on
        strip2.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip1.show();
      strip2.show();
      delay(wait);
     
      for (int i=0; i < strip1.numPixels(); i=i+3) {
        strip1.setPixelColor(i+q, 0);        //turn every third pixel off
        strip2.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
        for (int i=0; i < strip1.numPixels(); i=i+3) {
          strip1.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
          strip2.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
        }
        strip1.show();
        strip2.show();
        delay(wait);
       
        for (int i=0; i < strip1.numPixels(); i=i+3) {
          strip1.setPixelColor(i+q, 0);        //turn every third pixel off
          strip2.setPixelColor(i+q, 0);        //turn every third pixel off
        }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip1.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip1.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip1.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}


