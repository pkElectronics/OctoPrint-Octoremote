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

#define SR_RESET 7
#define SR_DATA 6
#define SR_CLOCK 8

const byte ROWS = 5; //five rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {0x43,0x42,0x41,0x22},
  {0x24,0x51,0x34,0x14},
  {0x31,0x33,0x13,0x52},
  {0x41,0x32,0x23,0x21},
  {0x53,0x12,0x11,0x54}
};
byte rowPins[ROWS] = {4, 5, 9, A1 , A3}; //connect to the row pinouts of the keypad PD4 , PD5, PB1, PC1, PC3
byte colPins[COLS] = {A2, 3 , A0, 2}; //connect to the column pinouts of the keypad PC2 , PD3, PC0, PD2

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
    PORTB &= ~(1<<PB0);
    if( (data >> i ) & 1 == 1){
      PORTD |= 1 << PD6;
    }else{
      PORTD &= ~(1<<PD6);
    }
    PORTB |= (1<<PB0);
   }
}


void setup(){
  Serial.begin(115200);
  customKeypad.addEventListener(keypadEvent);

  pinMode(SR_RESET,OUTPUT);
  pinMode(SR_DATA,OUTPUT);
  pinMode(SR_CLOCK,OUTPUT);

  digitalWrite(SR_RESET,HIGH);

   pinMode(SR_RESET,OUTPUT);
  pinMode(SR_DATA,OUTPUT);
  pinMode(SR_CLOCK,OUTPUT);

  digitalWrite(SR_RESET,HIGH);
  for(int i = 0 ; i < 8 ; i++){
    setLeds(1<<i);
    delay(300);
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



