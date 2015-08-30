/*
 * This class is a keypad emulator for the Digital Security Control's (DSC)
 * PC1550.  The model number of the keypad it emulates is PC1550RK.
 *
 * There are four wires that go to the keypad:
 *    Red    - Voltage.  This should be about 12V.  On my system, a
 *             voltage meter read 13.3V with a brand new battery.  You 
 *             should be able to safely use this as a power supply for the 
 *             Arduino.  Simply connect the red line to the pin labeled Vin.  
 *             The Arduino's internal voltage regulator will take care
 *             of the rest.  Because the PC1550 has a backup battery
 *             supply, your Arduino will continue to be powered even
 *             when the electrity goes out (quite convenient).
 *    Black  - Ground.  If you're using the PC1550 to supply power to
 *             the Arduino, connect this to the GND pin next to the Vin pin
 *    Yellow - Clock.  The PC1550 control panel determines the clock cycle.
 *             So long as the processClockCycle() method on this class is 
 *             called more frequently than half a clock cycle from the PC1550
 *             then we will safely be able to read and write signals from
 *             the PC1550 control panel. The total cycle is about 1500-1600
 *             micro-seconds, on average (roughly 650 Hz).  Because data 
 *             is sent when the clock is low and read when the clock is high,
 *             we must run the processClockCycle() method at least every
 *             800 micro-seconds.  The more frequently the processClockCycle
 *             method is called, the more likely data transmission will
 *             succeed (in both directions) without data loss.  If you're
 *             not sure if you can commit to a calling the processClockCycle()
 *             function frequently enough, then you can use 
 *             processTransmissionCycle() instead which will call 
 *             processClockCycle() and block until a full transmission cycle
 *             is complete.  This could be handy if your program is performing
 *             another task that could use significant clock cycles between
 *             calls to processClockCycle().  On the downside, 
 *             processTransmissionCycle() takes between 57ms and 104ms to
 *             complete.
 *
 *             Connect the clock line up to either a digital or analog pin.
 *             We only read on this line using digitalRead, so either a
 *             digital or analog pin will do.  Analog PIN 4 is the default
 *             but can be overriden via the constructor to this class.
 *
 *    Green  - Data.  The data line is used to send bits to and from the
 *             PC1550 when the clock is low and high, respectively.
 *           
 *             Connect this line to any analog pin.  While we can read
 *             data from the panel using only digital functions, we 
 *             must use analog functions to pull the pin low when we 
 *             want to send data back to the control panel.  Analog pin 3
 *             is the default but can be overridden via the contructor.
 *
 * There is one additional connection that can be made that can provide
 * additinal state information from the alarm controller.
 *
 *    Blue   - PGM.  The PGM terminal on the DSC PC1550 control panel
 *             can be programmed to do a number of things.  One option
 *             is to configure it as a 2nd data line.  The installation
 *             manual refers to the PC16-OUT module.  This module reads
 *             data from the PGM line and we can emulate that module here.
 *             This line does not go to the keypad, and is optional for
 *             use by this library.  See: http://www.alarmhow.net/manuals/
 *                  DSC/Modules/Output%20Modules/PC16-OUT.PDF 
 *             for a listing of options.
 *
 * The PC1550 control panel starts by holding the clock high for
 * roughly 26.5ms.  It then clocks out 16 cycles (one cycle is represented
 * by the clock going low and then returning to a high state).  After
 * 16 clock cycles, the PC1550 holds the clock high for roughly 26.5ms
 * again, which starts the entire cycle over.
 *
 * During the 16 clock cycles data is received when the clock is high:
 *    - The first 8 clock cycles are used to send one octet (byte)
 *      of data to the keypad (one bit per clock cycle).  This byte 
 *      contains information about which zones are currently open 
 *      (what zone lights should display on the keypad).  For this
 *      reason, the first 8 bits are referred to here as "zone bits."
 *
 *      The table below shows how the data is received and interpretted.
 *      Bit 7 is received first, and bit 0 is recieved last.  When bit 7
 *      is on, then the zone 1 light should be on; when bit 6 is on, then
 *      the zone 2 light should be on, etc.  Bits 1 and 0 are not used.
 *
 *          Zone Bit    7    6    5    4    3    2    1    0
 *          Zone        1    2    3    4    5    6  (Not Used)
 *
 *    - The second 8 clock cycles send 8 more bits.  This byte contains
 *      information about the other lights that should be enabled on
 *      the keypad.  These bits represent other states and therefore, 
 *      these bits are referred to here as "state bits."
 *  
 *      The table below shows how each bit is used.  Note that when bit 0
 *      is on, the keypad beep emits a short beep.
 *
 *    State Bit    7       6       5       4       3       2       1        0
 *               Ready   Armed   Memory  Bypass Trouble  --Not Used--     Beep
 *
 * Between receipt of the zone bits, data can be sent back to the control
 * panel when the clock is low.  In other words, the panel sends out its 
 * bits when the clock is high and the keypad sends back its data when the 
 * clock is low.
 *
 * The keypad only sends back 7 bits-- one bit between each of the 8 zone
 * bits received.  These bits represent a button press.  When taking the
 * keypad as a table (rows and columns) of buttons, the first three
 * bits received represent the column of the button pressed.  The last
 * four bits represent the row of the button pressed:
 *
 *           First Three Bits                Last 4 Bits
 *      Column 1      100                    Row 1   0001
 *      Column 2      010                    Row 2   0010
 *      Column 3      001                    Row 3   0100
 *                                           Row 4   1000
 *                                           Row 5   0000
 *
 *   No Key Pressed   000                            0000 
 *
 * Encoding bits in the data line:
 *   When the data line is LOW  the corresponding bit should be ON.
 *   When the data line is HIGH the corresponding bit should be OFF.
 * 
 * If the PGM line is connected, then 16 more bits of data are received
 * on this line if (and ONLY IF) the PGM line is configured in PC-16OUT
 * mode.  Refer to the PC1550 installation manual for instructions on
 * how to configure this mode.  The bits meaning follow:
 *    0 - PGM Output (whatever the PGM is configured for)
 *        (This library assumes PGM terminal has been programmed for
 *         strobe output.  This sets bit 0 to the on position
 *         when the alarm goes off.  And the bit remains set until
 *         the panel is disarmed).
 *    1 - Fire buttom pressed (on for 4 sec)
 *    2 - Aux button pressed (on for 4 sec)
 *    3 - Panic button pressed (on for 4 sec)
 *    4 - Armed
 *    5 - Armed
 *    6 - Armed with bypass (on for 5 sec)
 *    7 - Trouble 
 *    8 - Fire (on when fire alarm is latched in)
 *    9 - Not used
 *   10 - Zone 6 tripped while armed
 *   11 - Zone 5 tripped while armed
 *        ...
 *   15 - Zone 1 tripped while armed
 *
 * For the PGM terminal to work, it will need to be connected to the
 * AUX+ terminal with a 1k Ohm resistor (for PC1550s)
 */

#include "PC1550.h"

/* ==================================================================== */
/*       S T A T I C    /    P R I V A T E      H E L P E R S           */
/*   Convert ASCII key values to the byte values for key transmission   */
/* ==================================================================== */
char PC1550::getKeyChar(byte value){
  switch(value)
    {
    case 0b01000001: return '1';
    case 0b00100001: return '2';
    case 0b00010001: return '3';
    case 0b01000010: return '4';
    case 0b00100010: return '5';
    case 0b00010010: return '6';
    case 0b01000100: return '7';
    case 0b00100100: return '8';
    case 0b00010100: return '9';
    case 0b01001000: return '*';
    case 0b00101000: return '0';
    case 0b00011000: return '#';
    case 0b01000000: return 'F';
    case 0b00100000: return 'A';
    case 0b00010000: return 'P';
    default: return '\0';
    }
}

uint8_t PC1550::getKeyValue(char key){
  switch(key)
    {
    case '1': return 0b01000001;
    case '2': return 0b00100001;
    case '3': return 0b00010001;
    case '4': return 0b01000010;
    case '5': return 0b00100010;
    case '6': return 0b00010010;
    case '7': return 0b01000100;
    case '8': return 0b00100100;
    case '9': return 0b00010100;
    case '*': return 0b01001000;
    case '0': return 0b00101000;
    case '#': return 0b00011000;
    case 'F': return 0b01000000;
    case 'A': return 0b00100000;
    case 'P': return 0b00010000;
    default: return 0;
    }
}

/* ==================================================================== */
/*                S T A T E    I N F O    A N D    M G M T              */
/* ==================================================================== */
bool PC1550::keypadStateChanged(){
  return this->bStateChanged;
}

char PC1550::keyPressed(){
  if (!bKeyPressed)
    return '\0';
  return getKeyChar(this->available_keypad_data);
}

char PC1550::keyReleased(){
  if (key_released_data == 0)
    return '\0';
  return getKeyChar(this->key_released_data);
}

bool PC1550::Zone1Light(){
  return (available_controller_data >> 15);
}

bool PC1550::Zone2Light(){
  return (available_controller_data << 1 >> 15);
}

bool PC1550::Zone3Light(){
  return (available_controller_data << 2 >> 15);
}

bool PC1550::Zone4Light(){
  return (available_controller_data << 3 >> 15);
}

bool PC1550::Zone5Light(){
  return (available_controller_data << 4 >> 15);
}

bool PC1550::Zone6Light(){
  return (available_controller_data << 5 >> 15);
}

bool PC1550::ReadyLight(){
  return (available_controller_data << 8  >> 15);
}

bool PC1550::ArmedLight(){
  return (available_controller_data << 9 >> 15);
}

bool PC1550::MemoryLight(){
  return (available_controller_data << 10 >> 15);
}

bool PC1550::BypassLight(){
  return (available_controller_data << 11 >> 15);
}

bool PC1550::TroubleLight(){
  return (available_controller_data << 12 >> 15);
}

bool PC1550::Beep(){
  return (available_controller_data << 15 >> 15);
}

uint16_t PC1550::consecutiveBeeps(){
  return iConsecutiveBeeps;
}

bool PC1550::atTransmissionEnd(){
  return bTransmissionEnd;
}

uint16_t PC1550::consecutiveKeyPresses(){
  return iConsecutiveKeyPressCycles;
}

bool PC1550::readyForKeyPress(){
  if (keyHoldCycles > 0 || cyclesWithoutKey < 1){
    return false;
  }
  return true;
}

bool PC1550::sendKey(char c, uint8_t holdCycles){
  if (!readyForKeyPress())
    return false;
  
  uint8_t keyval = getKeyValue(c);
  if (keyval == 0) {return false; }

  this->key_to_send = keyval;
  this->keyHoldCycles = holdCycles;
  return true;
}


//configured based on programming of PC1550
bool PC1550::PGMOutput(){
  return (available_pc16out_data & 0b0000000000000001) > 0;
}

//active for 4 seconds after button is held for 20 clock cycles
bool PC1550::fireButtonTripped(){
  return (available_pc16out_data & 0b0000000000000010) > 0;
}

//active for 4 seconds after button is held for 20 clock cycles
bool PC1550::auxButtonTripped(){
  return (available_pc16out_data & 0b0000000000000100) > 0;
}

//active for 4 seconds after button is held for 20 clock cycles
bool PC1550::panicButtonTripped(){
  return (available_pc16out_data & 0b0000000000001000) > 0;
}

//constantly on whenever system is armed
bool PC1550::systemArmed(){
  return (available_pc16out_data & 0b0000000000110000) > 0;
}

//pulses for 5 seconds when the system is armed with bypass
bool PC1550::armedWithByPass(){
  return (available_pc16out_data & 0b0000000001000000) > 0;
}

//active during a trouble condition
bool PC1550::systemTrouble(){
  return (available_pc16out_data & 0b0000000010000000) > 0;
}

//active so long as fire alarm is latched in
bool PC1550::fireAlarmTripped(){
  return (available_pc16out_data & 0b0000000100000000) > 0;
}

//will only return true when system is armed
bool PC1550::Zone6Tripped(){
  return (available_pc16out_data & 0b0000010000000000) > 0;
}

//will only return true when system is armed
bool PC1550::Zone5Tripped(){
  return (available_pc16out_data & 0b0000100000000000) > 0;
}

//will only return true when system is armed
bool PC1550::Zone4Tripped(){
  return (available_pc16out_data & 0b0001000000000000) > 0;
}

//will only return true when system is armed
bool PC1550::Zone3Tripped(){
  return (available_pc16out_data & 0b0010000000000000) > 0;
}

//will only return true when system is armed
bool PC1550::Zone2Tripped(){
  return (available_pc16out_data & 0b0100000000000000) > 0;
}

//will only return true when system is armed
bool PC1550::Zone1Tripped(){
  return (available_pc16out_data & 0b1000000000000000) > 0;
}

//returns true of any of the zones are tripped when system is armed
bool PC1550::AlarmTripped(){
  return (available_pc16out_data & 0b1111110000000000) > 0;
}

/* ==================================================================== */
/*        C O N S T R U C T I O N     A N D    P R O C E S S I N G      */
/* ==================================================================== */

PC1550::PC1550(uint8_t datapin, uint8_t clockpin, uint8_t pgmpin){
  this->datapin = datapin;
  this->clockpin = clockpin;
  this->pgmpin = pgmpin;

  pinMode(datapin,INPUT);
  pinMode(clockpin,INPUT);
  pinMode(pgmpin,INPUT);

  //when nothing else is driving the data pin, we want
  //it to remain in the low state.  This sets us up to (by default)
  //send "No Key Press" to the controller.
  digitalWrite(datapin,LOW); 

  //set our control and state variables up
  synchronized = false;
  last_read = micros();
  last_clock = HIGH;
  key_to_send = 0;
  keyHoldCycles = 0;
  bKeyPressed = false;
  key_released_data = 0;
  cyclesWithoutKey = 0;
  transmitting = false;
  controller_bits_read = 0;
  controller_data = 0;
  available_controller_data = 0;
  keypad_bits_read = 0;
  keypad_data = 0;
  available_keypad_data = 0;
  keypad_bits_sent = 0;
  iConsecutiveBeeps = 0;
  iConsecutiveKeyPressCycles = 0;
  bStateChanged = false;
  bTransmissionEnd = false;
  pc16out_data = 0;
  available_pc16out_data = 0;
}

//this calls processClockCycle() until a full 16 bits are read and processed
//this takes (at a minimum) 57ms.  If out of synchronization, this could
//take twice as long (104ms).
void PC1550::processTransmissionCycle(){
  do{
    processClockCycle();
  }
  while (!atTransmissionEnd());
}

//This processes every clock cycle from the PC1550 control panel.
//This should be called within the Arduino loop() function at least
//every 800us.  If you're not sure you can commit to that frequency
//then call processTransmissionCycle() which will hold control for
//at least one full transmission cycle
void PC1550::processClockCycle(){
  
  //clear the bTransmissionEnd flag
  bTransmissionEnd = false;

  //read our clock and data values
  boolean clock = digitalRead(clockpin);
  boolean data = digitalRead(datapin);
  boolean pgmData = digitalRead(pgmpin);

  //how long has it been since we last read a byte?
  unsigned long time_since_last_read = micros() - last_read;
  
  //if the clock is hanging (clock line is remains HIGH so clock
  //variable remains false for an extended period) then the controller
  //is telling us that it is done with its last transmission cycle.
  //we can now enter a synchronized state
  if (!clock && time_since_last_read > 25000 && time_since_last_read < 28000){

    //at this point we should be synchronized
    synchronized = true;
    
    //reset the cycle
    controller_bits_read = 0;
    controller_data = 0;
    pc16out_data = 0;
    keypad_bits_read = 0;
    keypad_data = 0;
    keypad_bits_sent = 0;
  }
  
  //key press bits are transmitted with the clock is HIGH
  //a HIGH clock line means the clock variable will be false
  if (!clock && last_clock != clock){
    
    //we read key presses via the 7 bits transmitted BETWEEN
    //the first 8 bits received from the control plannel
    if (controller_bits_read > 0 && controller_bits_read < 8){
      
      //The Atmega on the Arduino has one ADC that is multiplexed for all the 
      //analog pins.  When we do an analogRead(), a multiplexer connects the 
      //pin we are reading to the ADC. This works fine for low impedance 
      //voltage sources but it takes time for a high impedance sensor to 
      //change the  voltage at the ADC after this switch of pins.
      
      //so we read once to switch pins
      digitalRead(datapin);
      
      //wait 100 microseconds to ensure the ADC has the right voltage
      delayMicroseconds(100);
      
      //then we read again
      uint8_t bitValue = (uint8_t)(!digitalRead(datapin));
      
      //update our keypad_data field and increment the count of bits read
      keypad_data |= (bitValue << (6 - keypad_bits_read));
      keypad_bits_read++;     
    }//end if between bits 1 and 8

  }//end if clock is HIGH/OFF

  //we read all other bits on a LOW clock line
  //a LOW clock line will mean the clock variable will be true
  if (clock && last_clock != clock){

    //update the last time we read a bit
    last_read = micros();
    
    //store the bit read (controller data)
    uint16_t dataValue = ((uint16_t)data) << (15 - controller_bits_read);
    controller_data |= dataValue;
    
    //store the bit read (pc16out data)
    dataValue = ((uint16_t)pgmData) << (15 - controller_bits_read);
    pc16out_data |= dataValue;

    //update the number of bits read
    controller_bits_read++;
    
    //if this is the first bit, see if we should be sending a key
    if (synchronized && controller_bits_read == 1){
      if (key_to_send != 0){
	cyclesWithoutKey = 0;
	transmitting = true;
      }
      else
	cyclesWithoutKey++;
    }

    //sanity check
    if (controller_bits_read >= 8)
      transmitting = false;

    //this will let the voltage float to whatever the panel is driving
    //which will allow other keypads to drive the line and for us to
    //see what other keypads are driving between receive bits
    //if no other keypad is driving, the line will pull high since
    //the dsc 1550 panel is pulling high...so the line should go back to 5v
    pinMode(datapin,INPUT);
    
    //if in transmit mode
    if (transmitting){
      //it is going to pull HIGH by default, so we only drive
      //low if the bit for this place in the sequence is set
      if (((key_to_send >> (7-controller_bits_read)) & 0x01)){
	//this will effectively pull the voltage low on the line
	//getting it close to zero (if not zero)
	pinMode(datapin,OUTPUT);
      }
      
      //if we've sent the last bit, we can clear our key sending fields
      if (controller_bits_read == 7){
	if (keyHoldCycles > 0) keyHoldCycles--;
	if (keyHoldCycles == 0){
	  key_to_send = 0;
	}
      }
	
    }//end if we have a key to write
  }

  //if we successfully received 16 bits, then update available data
  if (synchronized && controller_bits_read == 16){
    if (this->available_controller_data != controller_data)
      this->bStateChanged = true;
    else
      this->bStateChanged = false;
    
    //if we received keypad_data
    if (keypad_data != 0){
      //if it's the exact same as last time
      if (this->available_keypad_data == keypad_data){
	bKeyPressed = false;
	key_released_data = 0;
	iConsecutiveKeyPressCycles++;
      }
      else if (this->available_keypad_data == 0){
	bKeyPressed = true;
	key_released_data = 0;
	iConsecutiveKeyPressCycles = 1;
      }
      else if (this->available_keypad_data != 0){
	//this is a highly unlikely state
	//where two different key-presses occur in back to back cycles
	//the PC1550 controller does require a cycle of no transmission
	//between each key-press so this *should* never occur
	bKeyPressed = true;
	iConsecutiveKeyPressCycles = 1;
	this->key_released_data = available_keypad_data;
      }
    }
    //if keypad_data == 0
    else{
      if (this->available_keypad_data != 0){
	bKeyPressed = false;
	this->key_released_data = available_keypad_data;
      }
      else if (this->available_keypad_data == 0){
	this->key_released_data = 0;
	bKeyPressed = false;
	iConsecutiveKeyPressCycles = 0;	
      }
    }

    this->available_keypad_data = keypad_data;
    this->available_controller_data = controller_data;
    this->available_pc16out_data = this->pc16out_data;

    //update iConsecutiveBeeps
    if (!Beep()) iConsecutiveBeeps = 0;
    else iConsecutiveBeeps++;

    //indicate we are at the end of our transmission cycle
    bTransmissionEnd = true;

    //just in case the next call to processClockCycle is delayed
    //let's assume that we lose our synchronization
    synchronized = false;
    controller_bits_read = 0;
  }

  last_clock = clock;
}//end processClockCycle()
