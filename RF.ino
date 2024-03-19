/*
RF chat for Smart Response XE

"rf.h" was derived from the Sparkfun "RadioFunctions.h" library,
found here:
https://github.com/sparkfun/ATmega128RFA1_Dev

"SmartResponseXE.h" Library creeated by Larry Bank,
https://github.com/bitbank2
I also used some concepts from his wireless bootloader program for the
Smart Response XE on transferring data.

This code has no warranty, you may use it and modify it however you like,
I would like to continue improving it, so any changes you make, feel free
to create pull requests so I can update this repository.

I plan on cleaning the code up and adding better comments.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <SmartResponseXE.h>
#include "rf.h"

#define BUFF_LENGTH 26
#define NAME_LENGTH 5
#define LINE_NUM 7

char sBuf[LINE_NUM][BUFF_LENGTH]; //buffer for screen lines
char keyBuffer[BUFF_LENGTH];      //buffer for input keys
char buffer[BUFF_LENGTH];         //buffer for incoming Data processing
char tmp[BUFF_LENGTH];            //temporary buffer for something?
char name[NAME_LENGTH];           //username

byte key;               //variable to store pressed keys
bool start;             //if true, asks to set username
int msgSent;            //counter for messages sent
int msgRecv;            //counter for messages received
uint8_t buffPos;        //variable to keep track of where we are at in the buffer

//function to clear the data from screenbuffer
void clearScreen(){
  for (int i=0; i < LINE_NUM; i++){
    for (int j=0; j < BUFF_LENGTH; j++){
      sBuf[i][j] = 0;
    }
  }
}

//displays a message on first boot or when waking from sleep
void welcomeMsg(){
  SRXEWriteString(0,60,"Welcome ", FONT_LARGE, 3, 0);
  snprintf ( name, NAME_LENGTH, name);
  SRXEWriteString(129,60, name, FONT_LARGE, 3, 0);
  SRXEWriteString(198,60, "!", FONT_LARGE, 3, 0);
  delay(1000);
  clearLine(0,60);
}

//function to run on first boot or wake
void initDevice(){
  buffPos = 0;
  msgSent = 0;
  msgRecv = 0;
  SRXEInit(0xe7, 0xd6, 0xa2);
  if (start == true){
    for (int i=0; i<NAME_LENGTH; i++){
      name[i] = 0;
    }
    setName();
  }
  clearScreen();
  welcomeMsg();
  clearKeyBuffer();
  updateKeyBuffer();
  rfBegin(26);
}

//function to create or change username
void setName(){

  SRXEWriteString(0,120,"Enter name:", FONT_LARGE, 3, 0);

  //check if name is empty and update buffer position
  if (name[3] != 0){
    buffPos = NAME_LENGTH-1;
  }else{
    buffPos = 0;
  }

  //only run if start flag is true
  while (start == true) {
    key = SRXEGetKey();
    if (key != 0){

      if (key == 8 && buffPos > 0){
        buffPos--;
        name[buffPos] = 0;
        
      }
      if (buffPos != NAME_LENGTH-1 && key != 0xd && key != 8){
        name[buffPos] = key;
        buffPos++;
      }
      if (key == 0xd && buffPos == NAME_LENGTH-1){
        buffPos = NAME_LENGTH;
        start = false;
      }
      clearLine(0,120);
      snprintf ( name, NAME_LENGTH, name);
      SRXEWriteString(0,120,"Enter name:", FONT_LARGE, 3, 0);
      SRXEWriteString(180,120,name, FONT_LARGE, 3, 0);

    }
  }
  clearLine(0,120);
}

// function for clearing a line on screen
void clearLine(int x, int y) {
  SRXEWriteString(x,y, "                         ", FONT_LARGE, 3, 0);
}

//function to print lines on screen
void printScreen(int dir){ //dir 0 is for printing from receiving buffer, dir 1 is for printing from keybuffer
  int next = 1;
  for (int i=0; i < LINE_NUM;i++){

    next += i;

    for (int j = 0; j<BUFF_LENGTH-1; j++){
      if (i > (LINE_NUM - 2)){
        if (dir == 1){
          sBuf[i][j] = keyBuffer[j];
        }else{
          sBuf[i][j] = buffer[j];
        }
        tmp[j] = sBuf[i][j];
      }else{
        sBuf[i][j] = sBuf[next][j];
        tmp[j] = sBuf[i][j];
      }
    }

    snprintf ( tmp, BUFF_LENGTH, tmp);
    clearLine(0,i*16);
    SRXEWriteString(0,i*16, tmp, FONT_LARGE, 3, 0);
    next = 1;
  }
}

//function to clear keybuffer and prepend "NAME:" at beginning
void clearKeyBuffer(){
  for (int i=0; i < BUFF_LENGTH-1; i++){
    if (i == 0){
      keyBuffer[i] = name[0];
    }
    else if (i == 1){
      keyBuffer[i] = name[1];
    }
    else if (i == 2){
      keyBuffer[i] = name[2];
    }
    else if (i == 3){
      keyBuffer[i] = name[3];
    }
    else if (i == 4){
      keyBuffer[i] = ':';
    }
    else { keyBuffer[i] = 0;
    }
  }
  buffPos = NAME_LENGTH;
}

//used after valid key is pressed, updates the keybuffer line on screen
void updateKeyBuffer(){
  clearLine(0,120);
  snprintf ( keyBuffer, BUFF_LENGTH, keyBuffer);
  SRXEWriteString(0,120, keyBuffer, FONT_LARGE, 3, 0);
}

//updates messages sent/received status line on screen
void updateStatus(){
  clearLine(0,120);
  snprintf ( tmp, BUFF_LENGTH, "Sent: %d Received: %d", msgSent, msgRecv );
  SRXEWriteString(0,120,tmp, FONT_LARGE, 3, 0);
}

void setup() {
  start = true;
  initDevice();
}

void loop() {

  key = SRXEGetKey(); //get a key if it's pressed

  if (key != 0){ //if a key is pressed:

    if (key != 0xd){ //if key isn't ENTER key
      if (key == 8 && buffPos > NAME_LENGTH){ //if key is delete key, and we are at start of text buffer:
        //delete character from buffer
        buffPos--;
        keyBuffer[buffPos] = 0;
      }else{
        if (buffPos != BUFF_LENGTH && key != 8){ //if we are not at end of keybuffer and key isn't delete key
          //update keybuffer and advance in buffer position
          keyBuffer[buffPos] = key;
          buffPos++;
        }
      }
      //show changes to screen and reset key
      updateKeyBuffer();
      key = 0;
    }
    if (key == 0xd && buffPos != NAME_LENGTH){ //if key is ENTER key and the text buffer isn't empty

      key = 0; //reset the key

      //if textbuffer == "bye"
      if (keyBuffer[5] == 'b' && keyBuffer[6] == 'y' && keyBuffer[7] == 'e' && keyBuffer[8] == 0){
        //send "bye" msg over radio so other users know we are leaving
        rfPrint(keyBuffer);
        delay(20);
        //run function to turn off radio, and check if it's off
        if(rfOFF() == true){
          clearLine(0,120);
          SRXEWriteString(0,120,"Radio off. Sleeping...", FONT_LARGE, 3, 0);
          delay(500);
          //go to sleep
          SRXESleep();
          //setup device again after sleep
          initDevice();
        }
      }
      else if (keyBuffer[5] == '_' && keyBuffer[6] == 'n') { //if text buffer = "_n" we can change username
        start = true; //enable start flag which is needed to run setName() function
        clearLine(0,120);
        setName(); //change the username

      } 
      else {
        clearLine(0,120);
        SRXEWriteString(0,120,"Sending msg...", FONT_LARGE, 3, 0);
        //attempt to send message
        rfPrint(keyBuffer);
        if (rfFailed == false){ //if success, increment message sent counter.
          msgSent++;
          printScreen(1);
        }

      }
      clearKeyBuffer();
      clearLine(0,120);
      if (rfFailed == true){ //if we didn't send the message print error
        snprintf ( tmp, BUFF_LENGTH, "Sending failed!");
      }else{ //otherwise update the messages sent/received status line
        snprintf ( tmp, BUFF_LENGTH, "Sent: %d Received: %d", msgSent, msgRecv );
      }

      SRXEWriteString(0,120,tmp, FONT_LARGE, 3, 0);

    }
  }

  if (rfReceived == true){ // rfReceived is a flag that is set from the end-packet RF-rx interrupt in rf.h
    clearLine(0,120);
    SRXEWriteString(0,120,"Receiving msg...", FONT_LARGE, 3, 0); //display a message that we have received a message
    delay(500);
    byte trash; //probably unneeded, used for eating first byte of RXbuffer

    for (int i=0; i < BUFF_LENGTH+1; i++){ //loop through the RXbuffer
      if (rfAvailable() < 1){ //as long as there are bytes to read keep reading
        trash = rfRead();
        buffer[i] = ' '; //if we have reached the end of the actual message, lets just write spaces
      }else{ 
        buffer[i] = rfRead(); //if we are still reading from RXbuffer, add to our buffer.
     }
    }
    printScreen(0); //print the received message
    rfReceived = false; //reset the flag
    msgRecv++;
    updateStatus();
  }
}
