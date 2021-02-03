// Balls_Game Lamp Station 21/05/15 15:34

/* Pinout:
    Arduino Nano 5V: 
    D6-->NeoPIxel DataIn
    A0-->Vraw/3
  
    D9-->CE NRF
    D10-->CSN NRF
    D13 (SCK)-->SCK NRF
    D11 (MOSI)-->MOSI NRF
    D12 (MISO)-->MISO NRF
    D3-->IRQ
    
    Uart Connected to Bluetooth:
*/
/* TO DO List
 Change WINRED, WINGREEN, WINBLUE commands, when there are many balls, they sometimes light up by mistake.
 when collor is losing turn the balls off so there will be indication that the player lost
 improve scanning for more reliable, not sure where the issues are. maybe charnging channle /  addresses will help.
 The issue with communication is straying messages, add another level of identification, Network,Command, talking module.
 add effects.
 add more games.
 
*/

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Adafruit_NeoPixel.h>
#include <stdlib.h> // for the atoi() function

///////////////////////////
// Serial Input Command  //
///////////////////////////
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

byte DataDisp=0;

////////////////////////////
// communication variable //
////////////////////////////
// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

// Radio pipes for Tx and for Rx
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
//const uint64_t Txpipes =0xF3b3b3F5c3LL;
// define number of players, and their addresses
#define NUMPLAYERS 15
const uint64_t Txpipes[NUMPLAYERS] ={0xF3b1b3F5c3LL,0xF3b2b3F5c3LL,0xF3b3b3F5c3LL,0xF3b4b3F5c3LL,0xF3b5b3F5c3LL,0xF3b6b3F5c3LL,0xF3b7b3F5c3LL,0xF3b8b3F5c3LL,0xF3b9b3F5c3LL,0xF3bab3F5c3LL,0xF3bbb3F5c3LL,0xF3bcb3F5c3LL,0xF3bdb3F5c3LL,0xF3beb3F5c3LL,0xF3bfb3F5c3LL};
const uint64_t Rxpipes[2] ={0xb3F3F3b3b5LL,0xb3F3F3b3b7LL};
byte AvlPlayers[NUMPLAYERS]={0};
// define network identifyer variable
#define NTWIDN 0xF3F3F3
#define GAMEMODECOMPLEX 0xF0F0F0
#define GETCOLOR 0xFF00FF
#define WINRED 0xFF0000
#define WINGREEN 0x00FF00
#define WINBLUE 0x0000FF
#define SPARKUP 0xb0b0b0

/////////////
// Battery //
/////////////
double Vbaterry=7.4;
#define VBATTERY_PIN A0
#define ADC2VBAT 65.4 // Convert ADC reading to voltage, VBatt*4.7/14.7=Vref/ADC_Res Vref=5v ADC_Res=1023 
#define BATTERYMINIMUM 6.0 // minimum allowed battery voltage (2s battery) minus schotkky didoe drop
byte BatteryStatus=1; // variable to indicate battery status,0- empty, 1-full
#define BatterySampleDelay 1000 // sample period of the battery voltage
unsigned long VbatSampleMillis=0; // variable to hold millis counter


////////////////
// Neo Pixels //
////////////////
#define NEOPIXELS 74
#define NEODATAPIN 6
// neo Pixel initialize
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXELS, NEODATAPIN, NEO_GRB + NEO_KHZ800);


// [White,Red,Lime,Blue,Yellow,Cyan,Magenta,Maroon,Olive,Green,Purple,Teal,Navy]  from http://www.rapidtables.com/web/color/RGB_Color.htm
#define NUMOFCOLORS 13
uint32_t ColorsArr[NUMOFCOLORS]={0xFFFFFF,0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0x00FFFF,0xFF00FF,0x800000,0x808000,0x008000,0x800080,0x008080,0x000080};
byte CurrColorNum=0;
uint32_t Color=0x000000;
uint32_t ColorMix=0x000000;
#define ColorBlack 0x000000

// declrae RGB Union Type to easily move between fields. 
union ColorTypeUnion
{
  uint32_t RGB;
  uint8_t  CByte[3]; //Blue Green Red
    struct Color
  {
    byte B;
    byte G;
    byte R;  
  }C;
}RGBUnion;

//////////////////  
// Lamp Effects //
//////////////////
// Define Lamp Matrix

#define LAMPRAWS 8
#define LAMPCOLUMNS 9
//uint8_t LampMatrix[LAMPRAWS][LAMPCOLUMNS][3]={0};

//uint8_t LampMatrix[LAMPRAWS][LAMPCOLUMNS][3]={
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}},
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}},
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}},
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}},
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}},
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}},
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}},
//{{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00}}
//};

uint8_t LampMatrix[LAMPRAWS][LAMPCOLUMNS][3]={
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}},
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}},
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}},
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}},
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}},
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}},
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}},
{{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF},{0x00,0x00,0xFF},{0x00,0x00,0xFF}}
};

// Union Declaration Matrix
//ColorTypeUnion LampMatrixUnion[LAMPRAWS][LAMPCOLUMNS]={0};

// define Light Mode
// 0 - effects; 1 - simple game; 2 - complex game; other: off
int LightMode=0;

// millis delay variables
unsigned long WipeDelay=0;

// variable to scale recieved color values (prolong game)
uint32_t ComplexScale=5;

// variable for different Game modes. 1-SnakeGame
int ComplexModes=1;

// Define Collor sum
int ColorSum[3]={0,0,0};
// Define add color to matrix.
int AddColor[3]={0,0,0};
//for snake game
uint16_t StripPos[3]={0,24,49};


///////////////
// SETUP     //
///////////////

void setup(void)
{
  Serial.begin(115200);
  radio.begin();
  radio.setChannel(79);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(8);
  radio.setRetries(0,0);
  radio.setAutoAck(false);
  radio.openWritingPipe(Txpipes[0]);
  radio.openReadingPipe(1,Rxpipes[0]);
  radio.openReadingPipe(2,Rxpipes[1]);
  radio.startListening();
  
  // neo Pixel initialize
  strip.begin();
  strip.setBrightness(255); //1-255 sets scale, 1 min, 255 max.
  strip.show(); // Initialize all pixels to 'off' 
  
}

///////////////
// MAIN LOOP //
///////////////

void loop(void){
  
  // If string is complete then parse string and execute the command.
  if (stringComplete) {
    ParseString();
    ExecuteCommand();
  }

  // update battery voltage
  if ((millis()-VbatSampleMillis)>BatterySampleDelay){
    VbatSampleMillis=millis();
    Vbaterry=((double)analogRead(VBATTERY_PIN))/ADC2VBAT;
    if (Vbaterry>(BATTERYMINIMUM+0.1)){ //add 0.1 in order to remove turning on and off when battery close to value.
	BatteryStatus=1;
    }else if (Vbaterry<BATTERYMINIMUM){
	BatteryStatus=0;
    }
  }
  
  // if battery status is good, then start blinking based on modes. 
 if(BatteryStatus){
  switch (LightMode) {
    case 0: // light effects 
      static unsigned long EffectsMillis=0;
      if (millis()-EffectsMillis<15000){
        strip.setBrightness(150);
        // all effects
        //rainbowRaws(5);
        //rainbowColums(5);
        //rainbowCycleColums(5);
        rainbowCycleRaws(5);
      }else if (millis()-EffectsMillis<30000){
        strip.setBrightness(150);
        rainbowCycleColums(5);
      }else if (millis()-EffectsMillis<45000){
        strip.setBrightness(150);
        rainbowCycleRaws(20);
      }else if (millis()-EffectsMillis<60000){
        strip.setBrightness(150);
        rainbowCycleColums(20);
      }else if (millis()-EffectsMillis<75000){
        strip.setBrightness(150);
        rainbowRaws(20);
      }else if (millis()-EffectsMillis<90000){
        strip.setBrightness(150);
        rainbowColums(20);
      }else{
       EffectsMillis=millis(); // reinitialize cycle
      }
      break;
      
    case 1: // simple game mode
      SimpleGameMode();
      break;
      
    case 2: // complex game mode
      ComplexGameMode();
      break;
      
    default: // turn off
      if ((millis()-WipeDelay>1000)||WipeDelay>millis()){
        WipeDelay=millis();
        strip.setBrightness(255);
        colorWipe(0x000000);
      }
  }
 }else{ /// battery low
   TogglePower(1000); // toggle with the top LEDs
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

/////////////////////////
// Game Mode Functions //
/////////////////////////

// simple game mode
void SimpleGameMode(void){
  uint8_t pipe_number=0;
  uint32_t RcvColor=0; //recive color variable
  uint32_t RcvPacket[2]={0,0};
  if (radio.available(&pipe_number)){ // check what pipe, recieved message
  bool done = false;
    while (!done)
    {
      done=radio.read( &RcvPacket, sizeof(uint32_t)*2 );
      if (RcvPacket[0]==NTWIDN){
        RcvColor=RcvPacket[1];
  //      Serial.print("Pipe: ");
  //      Serial.print(pipe_number);
        if (pipe_number==1){ //data for simple game mode
    //      Serial.print("  RcvColor: ");
    //      Serial.println(RcvColor,HEX);
          if (RcvColor>0xFFFF){
           ColorMix=ColorMix&0x00FFFF;
           ColorMix+=RcvColor;
          }else if(RcvColor>0xFF){
           ColorMix=ColorMix&0xFF00FF;
           ColorMix+=RcvColor;
          }else{
           ColorMix=ColorMix&0xFFFF00;
           ColorMix+=RcvColor;
          }
    //      Serial.print("ColorMix: ");
    //      Serial.println(ColorMix,HEX);
          // limit color power to 2 amps, approximately.
          byte TempRGB[3];
          TempRGB[0]=(ColorMix>>16)&0x0000FF;
          TempRGB[1]=(ColorMix>>8)&0x0000FF;
          TempRGB[2]=(ColorMix>>0)&0x0000FF;
          unsigned int ColorMixSum=TempRGB[0]+TempRGB[1]+TempRGB[2];
          byte brightness=255;
          if (ColorMixSum>255){
            brightness=255-(ColorMixSum-255)/3; // limit to around 2/3 of full power
          }
          strip.setBrightness(brightness);
      
          colorWipe(ColorMix);
      //    Serial.print("ColorMix: ");
      //    Serial.println(ColorMix,HEX);
      //    Serial.print("ColorMixSum: ");
      //    Serial.println(ColorMixSum,HEX);
      //    Serial.print("setBrightness: ");
      //    Serial.println(brightness,DEC);
        }
      }else{
       Serial.println("Invalid Network IDN"); 
      }
    }
  }
} // end simple game mode

//Complex Game Mode
void ComplexGameMode(void){
    uint32_t CMDSend=GETCOLOR;
    uint32_t WritePacket[2]={NTWIDN,CMDSend};
    uint32_t RcvPacket[2]={0,0};
    uint8_t pipe_number=0;
    uint32_t RcvColor=0; //recive color variable
    if(DataDisp==5)Serial.print("Colors: ");
    for (int ii=0; ii<NUMPLAYERS;ii++){
      if (AvlPlayers[ii]){ // if player available send data request
        radio.stopListening();
        radio.openWritingPipe(Txpipes[ii]);
        radio.write( &WritePacket, sizeof(uint32_t)*2 ); // send data request
        radio.startListening();
        unsigned long started_waiting_at = millis();
        bool timeout = false;
        while (!timeout){ // wait for message or time out.
          if (radio.available(&pipe_number)){ // check what pipe, recieved message
              while (!radio.read( &RcvPacket, sizeof(uint32_t)*2 )); // empty buffer
              break;
          }
          if (millis() - started_waiting_at > 50 ){
            timeout = true;
            //Serial.println("Time Out !!!!!!!!!!!!!!");
            AvlPlayers[ii]++;
          }
        }
        if (!timeout){// recived something check what recieved
           //Serial.print("Recieved Data Pipe Number: ");
           //Serial.println(pipe_number); 
           if (RcvPacket[0]==NTWIDN){
             if (pipe_number==2){
               RcvColor=RcvPacket[1];

               // update Matrix With the Recieved Color
               if(DataDisp==5)Serial.print(" ");
               if(DataDisp==5)Serial.print(RcvColor,HEX);
               AddColor[0]+=((RcvColor >> 16)  & 0x0000FF)/ComplexScale;
               AddColor[1]+=((RcvColor >> 8)  & 0x0000FF)/ComplexScale;
               AddColor[2]+=((RcvColor >> 0)  & 0x0000FF)/ComplexScale;
                
               ColorSum[0]+=((RcvColor >> 16)  & 0x0000FF)/ComplexScale;
               ColorSum[1]+=((RcvColor >> 8)  & 0x0000FF)/ComplexScale;
               ColorSum[2]+=((RcvColor >> 0)  & 0x0000FF)/ComplexScale;
               
               AvlPlayers[ii]=1;
             }else{
              Serial.println("Wrong Pipe"); 
             }
           }else{
            Serial.println("Wrong Identification");  
           } 
         }      
        }
        if (AvlPlayers[ii]>50){
          AvlPlayers[ii]=0; // disable player, to many fails
        }
    }
    if(DataDisp==5)Serial.println();
    
    if(DataDisp==2){
      Serial.print("Players: ");
      int TotalPlayers=0;
      for (int ii=0; ii<NUMPLAYERS;ii++){
        Serial.print(AvlPlayers[ii]);
        Serial.print(" ");
        if (AvlPlayers[ii]){
         TotalPlayers++;
        }
      }
      Serial.println();
      Serial.print("TotalPlayers: "); 
      Serial.println(TotalPlayers);
    }

    if(DataDisp==3){
      Serial.print("mills: ");
      Serial.println(millis());
    }
    if(DataDisp==4){  
      Serial.print("AddColor: ");
      Serial.print("Red: ");
      Serial.print(AddColor[0]);
      Serial.print("  Green: ");
      Serial.print(AddColor[1]);
      Serial.print("  Blue: ");
      Serial.println(AddColor[2]);
      
      Serial.print("SumColor: ");
      Serial.print("Red: ");
      Serial.print(ColorSum[0]);
      Serial.print("  Green: ");
      Serial.print(ColorSum[1]);
      Serial.print("  Blue: ");
      Serial.println(ColorSum[2]);
    }
    
    if(DataDisp==1){
      static unsigned long MessageFiltermillis=0;
      if (millis()-MessageFiltermillis>1000){ // show once in a while messages
      MessageFiltermillis=millis();
      uint16_t PlayersBin=0;
      //Serial.print("SumColor: ");
      Serial.println("RGB:");
      Serial.print(ColorSum[0]);
      Serial.print(" ");
      Serial.print(ColorSum[1]);
      Serial.print(" ");
      Serial.println(ColorSum[2]);
      
      //Serial.print("P: ");
      int TotalPlayers=0;
      for (int ii=0; ii<NUMPLAYERS;ii++){
        //Serial.print(AvlPlayers[ii]);
        //Serial.print(" ");
        if (AvlPlayers[ii]){
         TotalPlayers++;
         PlayersBin=PlayersBin+1;
        }
         PlayersBin=PlayersBin<<1;
      }
      //Serial.println();
      Serial.println(PlayersBin,BIN);
      //Serial.print("TP "); 
      //Serial.println(TotalPlayers);
      
      
      }
    }
    
    // update color on lamp based on game mode
    if (ComplexModes==1){
     SnakeGame(); 
    }
}


/////////////////////////////
// Complex Games Functions //
/////////////////////////////
// initialize variable before game starts.
void InitComplexMode(void){
  strip.setBrightness(255);
  colorWipe(0x000000);
  for (int ii=0;ii<3;ii++){
    ColorSum[ii]=0;
    AddColor[ii]=0;
  }
  
  StripPos[0]=0;
  StripPos[1]=24;
  StripPos[2]=49;
}

// snake game mode, update collor until only one color left, or Win function is pressed
void SnakeGame(void){
  for (int ii=0;ii<3;ii++){ // scan all colors
    // first check if the color is in play
    if (StripPos[ii]==NEOPIXELS){
      AddColor[ii]=0;
      ColorSum[ii]=0;
      if (StripPos[(ii+1)%3]==NEOPIXELS){// if the next color is also eaten, than one color is left. send for win.
        Winner();
        // exit game and turn on effects
        LightMode=0;
        
      }
    }else{
      while (AddColor[ii]>0){ // while not finished adding colors
        RGBUnion.RGB=strip.getPixelColor(StripPos[ii]); // get the pixel color of the current position  
        uint32_t AddColorTemp=AddColor[ii]+(uint32_t)RGBUnion.CByte[2-ii];
        if (AddColorTemp<256){
          RGBUnion.CByte[2-ii]=AddColorTemp;
          AddColor[ii]=0;
          // update strip
          strip.setPixelColor(StripPos[ii],RGBUnion.RGB);
        }else{// color is full, move to next pixel
          AddColor[ii]=AddColorTemp-0xFF;
          RGBUnion.CByte[2-ii]=0xFF;
          strip.setPixelColor(StripPos[ii],RGBUnion.RGB);
          StripPos[ii]++; // move to next pixel.
          StripPos[ii]=StripPos[ii]%NEOPIXELS; //start over if at the end of the strip
          
          // clear out next pixel colors
          RGBUnion.RGB=strip.getPixelColor(StripPos[ii]);
          ColorSum[ii]=ColorSum[ii]-RGBUnion.CByte[(2-ii)];
          ColorSum[(ii+1)%3]=ColorSum[(ii+1)%3]-RGBUnion.CByte[((2-ii)+2)%3];
          ColorSum[(ii+2)%3]=ColorSum[(ii+2)%3]-RGBUnion.CByte[((2-ii)+1)%3];
          strip.setPixelColor(StripPos[ii],0x000000);
          
          if (StripPos[ii]==StripPos[(ii+1)%3]){ // color was eaten
          StripPos[(ii+1)%3]=NEOPIXELS;
          }else if(StripPos[ii]==StripPos[(ii+2)%3]){
          StripPos[(ii+2)%3]=NEOPIXELS;
          }
        }
      }
    } 
  }
  
  strip.show();  // Flash color to lamp
} // end Snake Game




// scan for avvailable players
void ScanPlayers(void){
    for (int ii=0; ii<NUMPLAYERS;ii++){ AvlPlayers[ii]=0;}
    uint32_t CMDSend=GAMEMODECOMPLEX;
    uint32_t WritePacket[2]={NTWIDN,CMDSend};
    for (int jj=0; jj<4; jj++){ // resend multiple times to catch all balls.
      for (int ii=0; ii<NUMPLAYERS;ii++){
        radio.stopListening();
        radio.openWritingPipe(Txpipes[ii]);
        radio.write( &WritePacket, sizeof(uint32_t)*2 ); // test transmition
        radio.startListening();
        delay(15); // delay to make sure it will not jump to the next player
        // wait for ack
        unsigned long started_waiting_at = millis();
        bool timeout = false;
        uint8_t pipe_number=0;
        uint32_t RcvPacket[2]={0,0};
        while (!timeout){ // wait for message or time out.
          if (radio.available(&pipe_number)){ // check what pipe, recieved message
              while (!radio.read( &RcvPacket, sizeof(uint32_t)*2 )); // empty buffer
              break;
          }
          if (millis() - started_waiting_at > 100 ){
            timeout = true;
          }
        }
           if ((!timeout)&&(RcvPacket[0]==NTWIDN)){
             if((RcvPacket[1]==GAMEMODECOMPLEX)&&(pipe_number==2)){
                Serial.println("Ack Recieved");
                AvlPlayers[ii]=1;
              }
           }      
      }
    }
    
    Serial.println("Available Players: ");
    int TotalPlayers=0;
    for (int ii=0; ii<NUMPLAYERS;ii++){
      Serial.print(AvlPlayers[ii]);
      Serial.print(" ");
      TotalPlayers+=AvlPlayers[ii];
    }
    Serial.println(); 
    Serial.print("TotalPlayers: "); 
    Serial.println(TotalPlayers);
}


// Declare Winner and Reinitialize variables
void Winner(void){
  uint32_t CMDSend=0x000000; // set winner command
  if ((ColorSum[0]>=ColorSum[1])&&(ColorSum[0]>=ColorSum[1])){
    CMDSend=WINRED;
  }else if((ColorSum[1]>=ColorSum[2])&&(ColorSum[1]>=ColorSum[0])){
    CMDSend=WINGREEN;
  }else{
    CMDSend=WINBLUE;
  }
    radio.stopListening();
    uint32_t WritePacket[2]={NTWIDN,CMDSend};
    for (int jj=0; jj<5;jj++){ // resend multiple times to make sure all balls recieved data.
      for (int ii=0; ii<NUMPLAYERS;ii++){
        if (AvlPlayers[ii]){
        radio.openWritingPipe(Txpipes[ii]);
        delay(5);
        radio.write( &WritePacket, sizeof(uint32_t)*2 ); // update winner
        }    
      }
    }
    radio.startListening();
    Serial.print("Winner Is:  ");
    Serial.println(CMDSend,HEX);
    for (uint8_t ii=0; ii<3; ii++){
      colorWipe(0x000000);
      delay(1000);
      colorWipe(CMDSend);
      delay(1000);
    }
    colorWipe(0x000000);  
}

//Light Up Balls
void SparkBalls(void){
    radio.stopListening();
    uint32_t CMDSend=SPARKUP; // set winner command
    uint32_t WritePacket[2]={NTWIDN,CMDSend};
    for (int jj=0; jj<5;jj++){ // resend multiple times to make sure all balls recieved data.
      for (int ii=0; ii<NUMPLAYERS;ii++){
        if (AvlPlayers[ii]){
        radio.openWritingPipe(Txpipes[ii]);
        delay(5);
        radio.write( &WritePacket, sizeof(uint32_t)*2 ); // update winner
        }    
      }
    }
    radio.startListening();
}
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
      Serial.println("Invalid Value");
      CommandValueString="Inv Val";
      CommandValue=-1;
    }
  }else{
    Serial.println("Invalid CMD");
    Command="Inv CMD";
  }
  
  // Print Command and Value
  Serial.print(Command);
  Serial.print("   ");
  Serial.print(CommandValueString);
  
  Serial.print("   ");
  Serial.println(CommandValue);
  
  // clear the string and restart status:
  inputString = "";
  stringComplete = false;  
}// end parse string


/////////////////////////////
// Execute Recieved Command//
/////////////////////////////

void ExecuteCommand(void)
{
  if (Command.equalsIgnoreCase("Battery")){ //Battery Status 
    Serial.print("Vbaterry  ");
    Serial.println(Vbaterry);
    Serial.print("Vbaterry Status  ");
    Serial.println(BatteryStatus);  
  }else if(Command.equalsIgnoreCase("off")){
    Serial.println("Off Mode  ");
    LightMode=-1; 
  }else if(Command.equalsIgnoreCase("Effects")){
    Serial.println("Effects Mode  ");
    LightMode=0;
  }else if(Command.equalsIgnoreCase("Simple")){
    Serial.println("Simple Mode  ");
    LightMode=1;
  }else if(Command.equalsIgnoreCase("Scan")){
    Serial.println("Scanning available players  ");
    ScanPlayers();
    if (CommandValue==1){ // if value=1 than force all balls active
      for (int ii=0; ii<NUMPLAYERS;ii++){ AvlPlayers[ii]=1;} 
    }
  }else if(Command.equalsIgnoreCase("Complex")){
    Serial.println("ComplexGameMode  ");
    if (CommandValue>0){
      ComplexModes=CommandValue;
    }
    InitComplexMode();
    LightMode=2;
  }else if(Command.equalsIgnoreCase("Win")){
    Serial.println("The Winner IS  ");
    Winner();
    LightMode=0;
  }else if(Command.equalsIgnoreCase("Spark")){
    Serial.println("Light Active Balls  ");
    SparkBalls();
  }else if(Command.equalsIgnoreCase("Data")){
    Serial.print("Turn Data On/Off  :");
    DataDisp++;
    DataDisp=DataDisp%6;
    Serial.println(DataDisp);
  }else if(Command.equalsIgnoreCase("Scale")){
    Serial.print("Complex Mode Scale  ");
    Serial.println(CommandValue);
    ComplexScale=CommandValue;
  }
  
  Command=""; // initialize command  
}

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

// Toggle with 2 LEDs on low power
void TogglePower(unsigned int ToggleDelay){
   static unsigned long ToggleMillis=0;
   if ((millis()-ToggleMillis>ToggleDelay)||ToggleMillis>(millis())){
     ToggleMillis=millis();
    static byte statetoggle=0;
    statetoggle++;	
  	//turn pixels off except one
  	for(byte i=0; i<strip.numPixels(); i++) {
  	  strip.setPixelColor(i, ColorBlack);
  	}
      if (statetoggle%2){
  	strip.setPixelColor(72,0xFF0000);
        strip.setPixelColor(73,0x000000);
      }
      else{
        strip.setPixelColor(72,0x000000);
        strip.setPixelColor(73,0x00FF00);
      }
  	 strip.show();     
   }
}


// flash matrix to LED`s 
void Flash_Matrix(void){
      for (int icol=0; icol<LAMPCOLUMNS; icol++){
        for (int iraw=0; iraw<LAMPRAWS; iraw++){
          if (icol%2==0){ // raws going down
          uint32_t ColorFlash=strip.Color(LampMatrix[iraw][icol][0], LampMatrix[iraw][icol][1], LampMatrix[iraw][icol][2]);
          strip.setPixelColor((LAMPRAWS-1)-iraw+icol*LAMPRAWS,ColorFlash);
          }else{ // raws going up
          uint32_t ColorFlash=strip.Color(LampMatrix[iraw][icol][0], LampMatrix[iraw][icol][1], LampMatrix[iraw][icol][2]);
          strip.setPixelColor(iraw+icol*LAMPRAWS,ColorFlash);
          }
//          Serial.print(LampMatrix[iraw][icol]);
//          Serial.print(" ");
        }
//        Serial.println();
      }
      strip.show();
}

//// flash matrix to LED`s using Union
//void Flash_Matrix(void){
//      for (int icol=0; icol<LAMPCOLUMNS; icol++){
//        for (int iraw=0; iraw<LAMPRAWS; iraw++){
//          if (icol%2==0){ // raws going down
//          uint32_t ColorFlash=strip.Color(Lamp[iraw][icol].C.R, Lamp[iraw][icol].C.G, Lamp[iraw][icol].C.B);
//          strip.setPixelColor((LAMPRAWS-1)-iraw+icol*LAMPRAWS, ColorFlash);
//          }else{ // raws going up
//          uint32_t ColorFlash=strip.Color(Lamp[iraw][icol].C.R, Lamp[iraw][icol].C.G, Lamp[iraw][icol].C.B);
//          strip.setPixelColor(iraw+icol*LAMPRAWS,ColorFlash);
//          }
////          Serial.print(LampMatrix[iraw][icol]);
////          Serial.print(" ");
//        }
////        Serial.println();
//      }
//      strip.show();
//}



// pulse matrix power
void PulseMatrix(uint16_t PulseDelay){
   static unsigned long PulseMillis=0;
   static int PowerChange=2;
   static int SetLedPower=250;
   static byte SetLedPowerByte=250;
   if ((millis()-PulseMillis>PulseDelay)||PulseMillis>(millis())){
     PulseMillis=millis();
       if (SetLedPower<25){
          PowerChange=2;
       }if (SetLedPower>250){
          PowerChange=-2;
       }
     SetLedPower=SetLedPower+PowerChange;
     SetLedPowerByte=(byte)SetLedPower; 
     strip.setBrightness(SetLedPowerByte);
     Flash_Matrix();     
   }
}

//////////////////////////////
//modified adafruit effects //
//////////////////////////////

//RainBow Effect on colums
void rainbowColums(uint8_t PulseDelay) {
  static unsigned long PulseMillis=0;
  if ((millis()-PulseMillis>PulseDelay)||PulseMillis>(millis())){
    static byte ColorCounter=0;
    uint32_t ColorWheel=0;
    PulseMillis=millis();
    ColorCounter++;
    byte RedMat,GreenMat,BlueMat;

    for(uint8_t i=0; i<LAMPCOLUMNS; i++) {
      ColorWheel=Wheel((i+ColorCounter) & 255);
      RedMat=((ColorWheel >> 16)  & 0x0000FF);
      GreenMat=((ColorWheel >> 8)  & 0x0000FF);
      BlueMat=((ColorWheel >> 0)  & 0x0000FF);
      for (uint8_t k=0; k<LAMPRAWS; k++){
        LampMatrix[k][i][0]=RedMat;
        LampMatrix[k][i][1]=GreenMat;
        LampMatrix[k][i][2]=BlueMat;
      }
    }
    
    Flash_Matrix(); 
  }
}


//RainBow Effect on Raws
void rainbowRaws(uint8_t PulseDelay) {
  static unsigned long PulseMillis=0;
  if ((millis()-PulseMillis>PulseDelay)||PulseMillis>(millis())){
    static byte ColorCounter=0;
    uint32_t ColorWheel=0;
    PulseMillis=millis();
    ColorCounter++;
    byte RedMat,GreenMat,BlueMat;
    for(uint8_t i=0; i<LAMPRAWS; i++) {
      ColorWheel=Wheel((i+ColorCounter) & 255);
      RedMat=((ColorWheel >> 16)  & 0x0000FF);
      GreenMat=((ColorWheel >> 8)  & 0x0000FF);
      BlueMat=((ColorWheel >> 0)  & 0x0000FF);
      for (uint8_t k=0; k<LAMPCOLUMNS; k++){
        LampMatrix[i][k][0]=RedMat;
        LampMatrix[i][k][1]=GreenMat;
        LampMatrix[i][k][2]=BlueMat;
      }
    }
    
    Flash_Matrix(); 
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
// RainBow Effect on Raws
void rainbowCycleRaws(uint8_t PulseDelay) {
  static unsigned long PulseMillis=0;
  if ((millis()-PulseMillis>PulseDelay)||PulseMillis>(millis())){
    static byte ColorCounter=0;
    uint32_t ColorWheel=0;
    PulseMillis=millis();
    ColorCounter++;
    byte RedMat,GreenMat,BlueMat;
    for(uint8_t i=0; i<LAMPRAWS; i++) {
      ColorWheel=Wheel(((i * 256 / strip.numPixels()) + ColorCounter) & 255);
      RedMat=((ColorWheel >> 16)  & 0x0000FF);
      GreenMat=((ColorWheel >> 8)  & 0x0000FF);
      BlueMat=((ColorWheel >> 0)  & 0x0000FF);
      for (uint8_t k=0; k<LAMPCOLUMNS; k++){
        LampMatrix[i][k][0]=RedMat;
        LampMatrix[i][k][1]=GreenMat;
        LampMatrix[i][k][2]=BlueMat;
      }
    }
    
    Flash_Matrix(); 
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
// RainBow Effect on Raws
void rainbowCycleColums(uint8_t PulseDelay) {
  static unsigned long PulseMillis=0;
  if ((millis()-PulseMillis>PulseDelay)||PulseMillis>(millis())){
    static byte ColorCounter=0;
    uint32_t ColorWheel=0;
    PulseMillis=millis();
    ColorCounter++;
    byte RedMat,GreenMat,BlueMat;
    for(uint8_t i=0; i<LAMPCOLUMNS; i++) {
      ColorWheel=Wheel(((i * 256 / strip.numPixels()) + ColorCounter) & 255);
      RedMat=((ColorWheel >> 16)  & 0x0000FF);
      GreenMat=((ColorWheel >> 8)  & 0x0000FF);
      BlueMat=((ColorWheel >> 0)  & 0x0000FF);
      for (uint8_t k=0; k<LAMPRAWS; k++){
        LampMatrix[k][i][0]=RedMat;
        LampMatrix[k][i][1]=GreenMat;
        LampMatrix[k][i][2]=BlueMat;
      }
    }
    
    Flash_Matrix(); 
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


////Theatre-style crawling lights.
//void theaterChase(uint32_t c) {
//	theaterChasePixel++;
//	theaterChasePixel=theaterChasePixel%3;
//	// set all pixels off
//	for (int i=0; i<strip.numPixels();i++){
//		strip.setPixelColor(i,ColorBlack);
//	}
//	//turn every third pixel on starting at the theaterChasePixel
//	for (int i=theaterChasePixel; i<strip.numPixels();i=i+3){
//		strip.setPixelColor(i, c);    
//	}
//	strip.show();
//}
//
////Theatre-style crawling lights multi color.
//void theaterChaseMultiColor(uint32_t c1, uint32_t c2, uint32_t c3) {
//	theaterChasePixel++;
//	theaterChasePixel=theaterChasePixel%3;
//	// color every third pixel	
//	for (int i=0; i<strip.numPixels();i=i+3){ 
//		strip.setPixelColor((i+theaterChasePixel)%strip.numPixels(),c1);
//	}
//	// color every third pixel
//	for (int i=0; i<strip.numPixels();i=i+3){
//		strip.setPixelColor((i+theaterChasePixel+1)%strip.numPixels(),c2);
//	}
//	// color every third pixel
//	for (int i=0; i<strip.numPixels();i=i+3){
//		strip.setPixelColor((i+theaterChasePixel+2)%strip.numPixels(),c3);
//	}
//	strip.show();
//}


