// =============================================================================
//
// Copyright (c) 2017 Pascal Krumme
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// =============================================================================
// Requires CRC32 Library by Christopher Baker 
// Requires Keypad Library by Mark Stanley and Alexander Brevig
// Available through the Arduino Library Manager

#include <CRC32.h>
#include <Keypad.h>

//DATA PD6 -> D6
//CLOCK PB0 -> D8
//RESET PD7 -> D7

#define SR_RESET 4
#define SR_DATA 11
#define SR_CLOCK 12

#define CHANGEMOVE 0x11
#define XPLUS 0x12
#define EPLUS 0x13
#define ZPLUS 0x14
#define YMINUS 0x21
#define HOMEXY 0x22
#define YPLUS 0x23
#define HOMEZ 0x24
#define CHANGETOOL 0x31
#define XMINUS 0x32
#define RETRACT 0x33
#define ZMINUS 0x34
#define STOP 0x41
#define PAUSE 0x42
#define PLAY 0x43
#define USER2 0x44
#define USER3 0x51
#define USER4 0x52
#define USER5 0x53
#define NOTPOPULATED 0x54

const byte ROWS = 5; //five rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {PLAY,PAUSE,STOP,HOMEXY},
  {USER5,HOMEZ,ZMINUS,ZPLUS},
  {USER4,RETRACT,EPLUS,CHANGETOOL},
  {USER3,XMINUS,YPLUS,YMINUS},
  {USER2,XPLUS,CHANGEMOVE,NOTPOPULATED}
};
/*      
 *       
 *       ----
 *      |R5C2|
 *  ----------------
 *  |R4C4|R5C3|R5C1|
 *  ----------------
 *  |R3C4|R4C2|R5C1|
 *  ----------------
 *  |R2C4|R3C3|R4C1|
 *  ----------------
 *  |R2C3|R3C2|R3C1|
 *  ----------------
 *  |R1C4|R2C2|R2C1|
 *  ----------------
 *  |R1C3|R1C2|R1C1|
 *  ----------------
 * 
 */

byte rowPins[ROWS] = {6, 5, 10, A3 , A5}; //connect to the row pinouts of the keypad PD4 , PD5, PB1, PC1, PC3
byte colPins[COLS] = {A4, 9 , A2, 8}; //connect to the column pinouts of the keypad PC2 , PD3, PC0, PD2

#define TRANSMISSION_HEADER 0x80
#define COMMAND_KEYPRESSED 0x10
#define COMMAND_KEYRELEASED 0x11
#define COMMAND_KEYHOLD 0x12
#define COMMAND_SETLED 0x20

#define COMMAND_ACK 0x01
#define COMMAND_NACK 0x02

//initialize an instance of class NewKeypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 
CRC32 crc;

char ackPending = false;
long lastSendTime = 0;
char lastSendKey = 0;
char lastSendCMD = 0;
uint8_t retryCounter = 0;


void writeByteToSR(byte data){
  for(int i = 0 ; i < 8 ; i++){
    PORTD &= ~(1<<PD6);
    
    if( (data) & (1<<i) ){
      PORTB |= (1 << PB7);
    }else{
      PORTB &= ~(1<<PB7);
    }
    
    PORTD |= (1<<PD6);
    
   }
}


void setup(){
  Serial.begin(115200);
  
  
  
  customKeypad.addEventListener(keypadEvent);

  pinMode(SR_RESET,OUTPUT);
  pinMode(SR_DATA,OUTPUT);
  pinMode(SR_CLOCK,OUTPUT);

  digitalWrite(SR_RESET,LOW);
  
  digitalWrite(SR_DATA,LOW);
  digitalWrite(SR_CLOCK,LOW);
  digitalWrite(SR_RESET,HIGH);
  for(int i = 0 ; i < 8 ; i++){
    writeByteToSR(1<<i);
    delay(100);
  }
  setLeds(0);
}



void keypadEvent(KeypadEvent key){
    switch (customKeypad.getState()){
    case PRESSED:
        sendKeyToHost(key,COMMAND_KEYPRESSED);
        break;

    case RELEASED:
        sendKeyToHost(key,COMMAND_KEYRELEASED);
        break;

    case HOLD:
        sendKeyToHost(key,COMMAND_KEYHOLD);
        break;
    }
}

long lastcall = 0L;
unsigned char ledIndex = 0;

unsigned char telegramLength = 0;
unsigned char telegramCommand = 0;
unsigned char payload[256];
unsigned char payloadIndex = 0;
unsigned char parsingState = 0;
uint32_t telegramCRC = 0;

  
void loop(){
  char key = customKeypad.getKey();
  if(ackPending && (millis()- lastSendTime > 2000) && retryCounter < 3){
    resendLastCommand();
    retryCounter++;
  }
  handleSerial();
}

int row1 = 0;
int row2 = 0;

void setLeds(byte b){
  byte ledOut = 0;

  switch(b){
    case 0:
      row1 = 1<<4;
      break;
    case 1:
      row1 = 1<<6;
      break;
    case 2:
      row1 = 1<<0;
      break;
    case 3:
      row1 = 1<<2;
      break;
    case 4:
    row2 = 1<<5;
    break;
    case 5:
    row2 = 1<<7;
    break;
    case 6:
    row2 = 1<<1;
    break;
    case 7:
    row2 = 1<<3;
    break;
    default:
    break;
  }
  writeByteToSR(row1|row2);
  
}



