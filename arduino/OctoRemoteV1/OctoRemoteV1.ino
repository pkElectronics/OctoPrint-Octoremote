#include <CRC32.h>

#include <Keypad.h>

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {0x11,0x12,0x13,0x14},
  {0x21,0x22,0x23,0x24},
  {0x31,0x32,0x33,0x34},
  {0x41,0x42,0x43,0x44}
};
byte rowPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {A0, A1, A2, A3}; //connect to the column pinouts of the keypad

#define LED_G1 10
#define LED_G2 11
#define LED_G3 12
#define LED_G4 13

#define LED_R1 6
#define LED_R2 7
#define LED_R3 8
#define LED_R4 9

#define TRANSMISSION_HEADER 0x80
#define COMMAND_KEYPRESSED 0x10
#define COMMAND_KEYRELEASED 0x11
#define COMMAND_KEYHOLD 0x12
#define COMMAND_SETLED 0x20

#define COMMAND_ACK 0x01
#define COMMAND_NACK 0x02

byte ledMapRed[4] = {LED_R1,LED_R2,LED_R3,LED_R4};
byte ledMapGreen[4] = {LED_G1,LED_G2,LED_G3,LED_G4};

//initialize an instance of class NewKeypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 
CRC32 crc;

char ackPending = false;
long lastSendTime = 0;
char lastSendKey = 0;
char lastSendCMD = 0;
void setup(){
  Serial.begin(9600);
  customKeypad.addEventListener(keypadEvent);
  pinMode(LED_G1,OUTPUT);
  pinMode(LED_G2,OUTPUT);
  pinMode(LED_G3,OUTPUT);
  pinMode(LED_G4,OUTPUT);

  pinMode(LED_R1,OUTPUT);
  pinMode(LED_R2,OUTPUT);
  pinMode(LED_R3,OUTPUT);
  pinMode(LED_R4,OUTPUT);

  digitalWrite(LED_G1,HIGH);
  digitalWrite(LED_G2,HIGH);
  digitalWrite(LED_G3,HIGH);
  digitalWrite(LED_G4,HIGH);

  digitalWrite(LED_R1,HIGH);
  digitalWrite(LED_R2,HIGH);
  digitalWrite(LED_R3,HIGH);
  digitalWrite(LED_R4,HIGH);
}

void sendKeyToHost(char key,char cmd){
  crc.reset();
  crc.update(TRANSMISSION_HEADER);
  crc.update(8);
  crc.update(cmd);
  crc.update(key);
  uint32_t checksum = crc.finalize();
  Serial.write(TRANSMISSION_HEADER); //STX
  Serial.write(8); //LENGTH
  Serial.write(cmd); //Command (Key Pressed)
  Serial.write(key);
  for(int i = 0; i < 4 ; i++){
    Serial.write( (checksum>>(8*i) & 0xFF));
  }
  ackPending = true;
  lastSendKey = key;
  lastSendCMD = cmd;
  lastSendTime = millis();
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
  if(ackPending && millis()- lastSendTime > 2000){
    resendLastCommand();
  }
  if(Serial.available() > 0){
    unsigned char readbyte = Serial.read();

    switch(parsingState){
      case 0: //STX
        if(readbyte == 0x80) parsingState++;
        payloadIndex = 0;
        telegramLength = 0;
        telegramCommand = 0;
        break;
      case 1: //LENGTH
        telegramLength = readbyte;
        parsingState++;
        break;
      case 2: //COMMAND
        telegramCommand = readbyte;
        parsingState++;
        if(telegramLength == 7) parsingState++; //telegram ohne payload 
        break;
      case 3: //payload
        payload[payloadIndex] = readbyte;
        payloadIndex++;
        if( (telegramLength - 4 - 3 - payloadIndex)   == 0){
          parsingState++;
        }
      break;

       case 4: //CRC
       case 5:
       case 6:
        telegramCRC += readbyte << (8*(parsingState-4));
        break;
       case 7:
        telegramCRC += readbyte << (8*(parsingState-4));
        crc.reset();
        crc.update(0x80);
        crc.update(telegramLength);
        crc.update(telegramCommand);
        for(int i = 0; i < payloadIndex; i++){
          crc.update(payload[i]);
        }
        if(crc.finalize() == telegramCRC){
          sendACK();
          parsingState = 0;

          switch(telegramCommand){
            case COMMAND_NACK:
              resendLastCommand();
            break;

            case COMMAND_ACK:
              ackPending = false;
            break;

            case COMMAND_SETLED:
              if(payload[0] >= 4){
                for(int i = 0; i < 3 ; i++){
                  if(payload[0] - 4 == i) digitalWrite(ledMapRed[i],HIGH);
                  else digitalWrite(ledMapRed[i],LOW); 
                }
              }else{
                for(int i = 0; i < 3 ; i++){
                  if(payload[0] == i) digitalWrite(ledMapGreen[i],HIGH);
                  else digitalWrite(ledMapGreen[i],LOW); 
                }
              }
            break;
          }
          
        }else{
          sendNACK();
        }
       break;
    }
      
  }
}

void resendLastCommand(){
  sendKeyToHost(lastSendKey,lastSendCMD);
}


void sendSingleCMD(char command){
   crc.reset();
  crc.update(TRANSMISSION_HEADER);
  crc.update(7);
  crc.update(command);
  uint32_t checksum = crc.finalize();
  Serial.write(TRANSMISSION_HEADER); //STX
  Serial.write(7); //LENGTH
  Serial.write(command); //Command (Key Pressed)
  for(int i = 0; i < 4 ; i++){
    Serial.write( (checksum>>(8*i) & 0xFF));
  }
}
void sendACK(){
 sendSingleCMD(COMMAND_ACK); 
}
void sendNACK(){
  sendSingleCMD(COMMAND_NACK); 
}



