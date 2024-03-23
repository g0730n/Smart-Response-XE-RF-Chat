/* RadioFunctions.h
  A handful of sending and receiving functions for the 
  ATmega128RFA1.
  by: Jim Lindblom
      SparkFun Electronics
  date: July 8, 2013
  license: Beerware. Use, distribut, and modify this code freely
  however you please. If you find it useful, you can buy me a beer
  some day.
  
  Functions in here:
    rfBegin(uint8_t channel) - Initializes the radio on a channel
                                between 11 and 26.
    rfWrite(uint8_t b) - Sends a byte over the radio.
    rfPrint(String toPrint) - Sends a string over the radio.
    int rfAvailable() - Returns number of characters currently
                        in receive buffer, ready to be read.
    char rfRead() - Reads a character out of the buffer.
    
  Interrupt Sub-Routines (ISRs) in here:
    TRX24_TX_END_vect - End of radio transfer.
    TRX24_RX_START_vect - Beginning of radio receive.
    TRX24_RX_END_vect - End of radio receive. Characters are 
                        collected into a buffer here.

  ---------------------edits by g0730n----------------------
  3/19/2024: 
  I've removed everything to do with LED's from
  original code as SRXE does not have these LED. 
  As well as added some code for waiting for
  a reply from receiver and retrying to send message if we
  dont get it. That code was inspired by looking at Larry
  Bank's wireless bootloader for Smart Response XE.
*/

#define MAX_RF_WAIT 300000L
#define MAX_RETRY 10
#define RF_BUFFER_SIZE 128

//Global variables for keeping track of things between ISR's and functions
uint8_t rssiRaw;          // Global variable shared between RX ISRs
uint8_t retryCount;       //keeping track of how many times we tried sending a message
uint32_t sendTimeout;     //timeout variable for sending a message
bool rfReceived;          //flag if a valid message was received
bool rfSent;              //flag if we successfully sent a message and received confirmation back
bool rfFailed;            //flag if we tried sending message and never received confirmation back
bool txBusy;
uint8_t wakeOnReceive;    //flag for setting device to wake when msg received

// A buffer to maintain data being received by radio.
struct ringBuffer
{
  unsigned char buffer[RF_BUFFER_SIZE];
  volatile unsigned int head;
  volatile unsigned int tail;
} radioRXBuffer;

//function to turn off radio before going to sleep (this must be disabled in "SmartResponseXE.h" sleep ruitine)
uint8_t rfOFF(){

  //not sure if needed but lets reset our RXbuffer read positions
  radioRXBuffer.tail = 0;
  radioRXBuffer.head = 0;

  /* //code for setting registers if device is to be woken from RF
  if (wakeOnReceive){
    
  }
  */

  TRXPR |= (1<<TRXRST);   // TRXRST = 1 (Reset state, resets all registers)

  // Transceiver Interrupt Enable Mask - IRQ_MASK
  // This register disables/enables individual radio interrupts.
  // First, we'll disable IRQ and clear any pending IRQ's
  IRQ_MASK = 0;  // Disable all IRQs
  
  // Transceiver State Control Register -- TRX_STATE
  // This regiseter controls the states of the radio.
  // First, we'll set it to the TRX_OFF state.
  TRX_STATE = (TRX_STATE & 0xE0) | TRX_OFF;  // Set to TRX_OFF state
  delay(1);

  if ((TRX_STATUS & 0x1F) != TRX_OFF) // Check to make sure state is correct
    return 0;	// Error, TRX isn't off

  SRXEWriteString(129,60, "RADIO OFF!", FONT_LARGE, 3, 0);
  return 1;
}


// Initialize the RFA1's low-power 2.4GHz transciever.
// Sets up the state machine, and gets the radio into
// the RX_ON state. Interrupts are enabled for RX
// begin and end, as well as TX end.
uint8_t rfBegin(uint8_t channel)
{
  for (int i=0; i<RF_BUFFER_SIZE; i++)
  {
    radioRXBuffer.buffer[i] = 0;
  }

  radioRXBuffer.tail = 0;
  radioRXBuffer.head = 0;

  //initialize our global variables
  retryCount = 0;
  sendTimeout = 0;
  rfReceived = false;
  rfSent = false;
  rfFailed = false;
  txBusy = false;

  // Transceiver Pin Register -- TRXPR.
  // This register can be used to reset the transceiver, without
  // resetting the MCU.
  TRXPR |= (1<<TRXRST);   // TRXRST = 1 (Reset state, resets all registers)

  // Transceiver Interrupt Enable Mask - IRQ_MASK
  // This register disables/enables individual radio interrupts.
  // First, we'll disable IRQ and clear any pending IRQ's
  IRQ_MASK = 0;  // Disable all IRQs
  
  // Transceiver State Control Register -- TRX_STATE
  // This regiseter controls the states of the radio.
  // First, we'll set it to the TRX_OFF state.
  //TRX_STATE = (TRX_STATE & 0xE0) | TRX_OFF;  // Set to TRX_OFF state
  delay(1);
  
  // Transceiver Status Register -- TRX_STATUS
  // This read-only register contains the present state of the radio transceiver.
  // After telling it to go to the TRX_OFF state, we'll make sure it's actually
  // there.
  if ((TRX_STATUS & 0x1F) != TRX_OFF) // Check to make sure state is correct
    return 0;	// Error, TRX isn't off

  // Transceiver Control Register 1 - TRX_CTRL_1
  // We'll use this register to turn on automatic CRC calculations.
  TRX_CTRL_1 |= (1<<TX_AUTO_CRC_ON);  // Enable automatic CRC calc. 
  
  // Enable RX start/end and TX end interrupts
  IRQ_MASK = (1<<RX_START_EN) | (1<<RX_END_EN) | (1<<TX_END_EN);
  
  // Transceiver Clear Channel Assessment (CCA) -- PHY_CC_CCA
  // This register is used to set the channel. CCA_MODE should default
  // to Energy Above Threshold Mode.
  // Channel should be between 11 and 26 (2405 MHz to 2480 MHz)
  if ((channel < 11) || (channel > 26)) channel = 11;  
  PHY_CC_CCA = (PHY_CC_CCA & 0xE0) | channel; // Set the channel
  
  // Finally, we'll enter into the RX_ON state. Now waiting for radio RX's, unless
  // we go into a transmitting state.
  TRX_STATE = (TRX_STATE & 0xE0) | RX_ON; // Default to receiver
  //if everything has worked print a message
  SRXEWriteString(129,60, "RADIO ON!", FONT_LARGE, 3, 0);
  return 1;
}

// This function sends a string of characters out of the radio.
// Given a string, it'll format a frame, and send it out.
void rfPrint(String toPrint)
{
  //set our flags
  rfSent = false;
  rfFailed = false;

  uint8_t frame[RF_BUFFER_SIZE];  // We'll need to turn the string into an arry
  int length = toPrint.length();  // Get the length of the string

  for (int i=0; i<length; i++)  // Fill our array with bytes in the string
  {
    frame[i] = toPrint.charAt(i);
  }
  
  //if we haven't tried sending the message maximum times, and the sent flag isn't set
  while (retryCount <= MAX_RETRY && rfSent == false){

    // Transceiver State Control Register -- TRX_STATE
    // This regiseter controls the states of the radio.
    // Set to the PLL_ON state - this state begins the TX.
    TRX_STATE = (TRX_STATE & 0xE0) | PLL_ON;  // Set to TX start state
    while (!(TRX_STATUS & PLL_ON))
      ; // wait for PLL to lock
    // Start of frame buffer - TRXFBST
    // This is the first byte of the 128 byte frame. It should contain
    // the length of the transmission.
    TRXFBST = length + 2;
    memcpy((void *)(&TRXFBST + 1), frame, length);
    // Transceiver Pin Register -- TRXPR.
    // From the PLL_ON state, setting SLPTR high will initiate the TX.
    TRXPR |= (1 << SLPTR);   // SLPTR high
    TRXPR &= ~(1 << SLPTR);  // SLPTR low

    // After sending the byte, set the radio back into the RX waiting state.
    TRX_STATE = (TRX_STATE & 0xE0) | RX_ON;

    sendTimeout = 0;  // reset our timeout counter

    //if we don't get the confirmation byte "0x10", wait until timeout
    while (radioRXBuffer.buffer[radioRXBuffer.tail] != 0x10 && sendTimeout < MAX_RF_WAIT) {
      sendTimeout++;
    }
    //timed out, increment retry counter to try again
    if (sendTimeout >= MAX_RF_WAIT){
      retryCount++;
      delay(100);
      continue;  // try sending it again
    }
    // we received the confirmation from receiver
    if (radioRXBuffer.buffer[radioRXBuffer.tail] == 0x10){
      radioRXBuffer.tail = (unsigned int)(radioRXBuffer.tail + 1) % RF_BUFFER_SIZE; //increment the RXBuffer tail
      rfSent = true; //set the sent flag
      continue;
    }
  }
  TRX_STATE = (TRX_STATE & 0xE0) | RX_ON; //switch back to reveiving mode

  //if we tried sending max times without reply, set failed flag
  if (retryCount >= MAX_RETRY && rfSent == false){
    rfFailed = true;
  }
  //reset these variables
  retryCount = 0;
  rfSent = false; 
  //reset our readbuffer (not sure if needed)
  //radioRXBuffer.tail = 0;
  //radioRXBuffer.head = 0;
}

// This function will transmit a single byte out of the radio.
void rfWrite(uint8_t b)
{
  uint8_t length = 3;

  // Transceiver State Control Register -- TRX_STATE
  // This regiseter controls the states of the radio.
  // Set to the PLL_ON state - this state begins the TX.
  TRX_STATE = (TRX_STATE & 0xE0) | PLL_ON;  // Set to TX start state
  while(!(TRX_STATUS & PLL_ON))
    ;  // Wait for PLL to lock

  while (txBusy == true) { };

  // Start of frame buffer - TRXFBST
  // This is the first byte of the 128 byte frame. It should contain
  // the length of the transmission.
  TRXFBST = length;
  // Now copy the byte-to-send into the address directly after TRXFBST.
  memcpy((void *)(&TRXFBST+1), &b, 1);

  // Transceiver Pin Register -- TRXPR.
  // From the PLL_ON state, setting SLPTR high will initiate the TX.
  TRXPR |= (1<<SLPTR);   // SLPTR = 1
  TRXPR &= ~(1<<SLPTR);  // SLPTR = 0  // Then bring it back low

  txBusy = true; //txBusy flag (we are sending the byte!)

  // After sending the byte, set the radio back into the RX waiting state.
  TRX_STATE = (TRX_STATE & 0xE0) | RX_ON;
}

// Returns how many unread bytes remain in the radio RX buffer.
// 0 means the buffer is empty. Maxes out at RF_BUFFER_SIZE.

unsigned int rfAvailable()
{
  return (unsigned int)(RF_BUFFER_SIZE + radioRXBuffer.head - radioRXBuffer.tail) % RF_BUFFER_SIZE;
}


// This function reads the oldest data in the radio RX buffer.
// If the buffer is emtpy, it'll return a 255.
char rfRead()
{
  if (radioRXBuffer.head == radioRXBuffer.tail)
  {
    return -1;
  }
  else
  {
    // Read from the buffer tail, and update the tail pointer.
    char c = radioRXBuffer.buffer[radioRXBuffer.tail];
    radioRXBuffer.tail = (unsigned int)(radioRXBuffer.tail + 1) % RF_BUFFER_SIZE;
    return c;
  }
}

// This interrupt is called when radio TX is complete.
ISR(TRX24_TX_END_vect)
{
  txBusy = false;
}

// This interrupt is called the moment data is received by the radio.
// We'll use it to gather information about RSSI -- signal strength --
ISR(TRX24_RX_START_vect)
{
  rssiRaw = PHY_RSSI;  // Read in the received signal strength
}

// This interrupt is called at the end of data receipt. Here we'll gather
// up the data received. And store it into a global variable.
ISR(TRX24_RX_END_vect)
{
  uint8_t length;
  // Maximum transmission is 128 bytes
  uint8_t tempFrame[RF_BUFFER_SIZE];

  // The received signal must be above a certain threshold.
  if (rssiRaw & RX_CRC_VALID)
  {
    // The length of the message will be the first byte received.
    length = TST_RX_LENGTH;
    // The remaining bytes will be our received data.
    memcpy(&tempFrame[0], (void*)&TRXFBST, length);
    
    // Now we need to collect the frame into our receive buffer.
    //  k will be used to make sure we don't go above the length
    //  i will make sure we don't overflow our buffer.
    unsigned int k = 0;
    unsigned int i = (radioRXBuffer.head + 1) % RF_BUFFER_SIZE; // Read buffer head pos and increment;
    while ((i != radioRXBuffer.tail) && (k < length-2))
    {
      // First, we update the buffer with the first byte in the frame
      radioRXBuffer.buffer[radioRXBuffer.head] = tempFrame[k++];
      radioRXBuffer.head = i; // Update the head
      i = (i + 1) % RF_BUFFER_SIZE; // Increment i % BUFFER_SIZE
    }
    //check the last byte received
      char c = radioRXBuffer.buffer[radioRXBuffer.tail];
      //if it's not equal to our confirmation byte, it means we just
      //received a message, so we need to send the confirmation byte
      if (c != 0x10){
        //send confirmation
        rfWrite(0x10);
        //set the received flag to true so our programs will know to check for RX data
        rfReceived = true;
      }
  }
  else{
    //reset our RXbuffer read locations (unneeded?)
    //radioRXBuffer.tail = 0;
    //radioRXBuffer.head = 0;
  }
}
