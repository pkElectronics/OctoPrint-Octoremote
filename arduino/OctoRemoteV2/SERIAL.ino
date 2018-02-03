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
  retryCounter= 0;
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
    Serial.write( ((checksum>>(8*i)) & 0xFF));
  }
}
void sendACK(){
 sendSingleCMD(COMMAND_ACK); 
}
void sendNACK(){
  sendSingleCMD(COMMAND_NACK); 
}
void handleSerial(){
  
  if(Serial.available() > 0){
    unsigned char readbyte = Serial.read();
    unsigned long b = readbyte;
    
    switch(parsingState){
      case 0: //STX
        if(readbyte == 0x80) parsingState++;
        payloadIndex = 0;
        telegramLength = 0;
        telegramCommand = 0;
        telegramCRC = 0;
        break;
      case 1: //LENGTH
        telegramLength = readbyte;
        //payloadIndex = telegramLength -7;
        parsingState++;
        break;
      case 2: //COMMAND
        telegramCommand = readbyte;
        parsingState++;
        if(telegramLength == 7)parsingState++; //telegram ohne payload 
        break;
      case 3: //payload
        payload[payloadIndex] = readbyte;
        
        payloadIndex++;
        if( telegramLength ==  (payloadIndex + 7)){
          parsingState++;
        }
        break;

       case 4: //CRC
       case 5:
       case 6:
        telegramCRC |= b << (8*(parsingState-4));
        parsingState++;
        break;
       case 7:
        
        telegramCRC |= b << (8*(parsingState-4));
        crc.reset();
        crc.update(0x80);
        
        crc.update(telegramLength);
        
        crc.update(telegramCommand);
        
        for(int i = 0; i < payloadIndex; i++){
          crc.update(payload[i]);
          
        }
        parsingState = 0;
        if(crc.finalize() == telegramCRC){
          switch(telegramCommand){
            case COMMAND_NACK:
              resendLastCommand();
            break;

            case COMMAND_ACK:
              ackPending = false;
            break;

            case COMMAND_SETLED:
              ackPending=false;
              if(telegramLength == 8){
                sendACK();
              setLeds(payload[0]);  
              }else{
                sendNACK();
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



