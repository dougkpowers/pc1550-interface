#ifndef DSC_PC1550_H
#define DSC_PC1550_H

#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <Wprogram.h> // Arduino 0022
#endif

#include <stdint.h>

class PC1550 {

  uint8_t datapin;
  uint8_t clockpin;
  uint8_t pgmpin;

  //set to true when we are synchronized with the controller
  bool synchronized;

  //the number of bits successfully read from the controller 
  //since the start of the last transmission cycle
  uint8_t controller_bits_read;

  //the controller bits that have been read so far in the transmission cycle
  uint16_t controller_data;

  //the last set of data completely received from the controller
  uint16_t available_controller_data;

  //indicates the available controller_data was changed from the last
  //transmission cycle.  Can be set to false until new data is received
  //by calling stateHandled();
  bool bStateChanged;

  //the number of bits successfully read from other keypads (or our own
  //emulated keypad since the start of the last transmission cycle).
  uint8_t keypad_bits_read;

  //the keypad bits that have been read so far in the transmission cycle
  uint8_t keypad_data;

  //the last set of keypad data completely read
  uint8_t available_keypad_data;

  //the pc16out (PGM) bits that have been read so far in the tx cycle
  uint16_t pc16out_data;

  //the pc16out (PGM) bits that have been completely read
  uint16_t available_pc16out_data;

  //the last key released (in keypad_data format)
  uint8_t key_released_data;

  //indicates the key was pressed on this cycle
  bool bKeyPressed;

  //indicates that a key was released on this cycle
  bool bKeyReleased;

  //the consecutive transmission cycles of the key being held
  uint8_t iConsecutiveKeyPressCycles;
  
  //the last time the clock was low
  unsigned long last_read;

  //the last time we checked, was the clock high or low?
  bool last_clock;

  //number of cycles without a keypress
  uint8_t cyclesWithoutKey;

  //whether or not we should be transmitting key bits
  bool transmitting;

  //sometimes we want to simulate a press and hold
  //the Fire key (F), for example, only works if held for a few seconds
  //this variable indicates for how many more cycles we should
  //send the key
  uint8_t keyHoldCycles;

  //the next byte of data we should transmit to the control panel
  //the value is set to zero once succesfully sent
  uint8_t key_to_send;

  //the number of bits we have sent to the control panel from keyCodeToSend
  uint8_t keypad_bits_sent;

  //the number of consecutive transmission cycles that have signaled a beep
  int iConsecutiveBeeps;

  //true for one execution of processClockCycle
  //and only when finished reading all 16 bits
  bool bTransmissionEnd;

  static char getKeyChar(uint8_t value);
  uint8_t getKeyValue(char key);

 public:
  PC1550(uint8_t datapin = A3, uint8_t clockpin = A4, uint8_t pgmpin = A1);
  void processClockCycle();
  void processTransmissionCycle();

  //keypad emulation and status
  bool keypadStateChanged();
  char keyPressed();
  char keyReleased();
  bool Zone1Light();
  bool Zone2Light();
  bool Zone3Light();
  bool Zone4Light();
  bool Zone5Light();
  bool Zone6Light();
  bool ReadyLight();
  bool ArmedLight();
  bool MemoryLight();
  bool BypassLight();
  bool TroubleLight();
  bool Beep();
  uint16_t consecutiveBeeps();
  uint16_t consecutiveKeyPresses();
  bool atTransmissionEnd();
  bool readyForKeyPress();
  bool sendKey(char c, uint8_t holdCycles = 1);

  //the following are available only when the PC16OUT is enabled
  //and the PGM terminal from the panel is connected
  bool PGMOutput();
  bool fireButtonTripped();
  bool auxButtonTripped();
  bool panicButtonTripped();
  bool systemArmed();
  bool armedWithByPass();
  bool systemTrouble();
  bool fireAlarmTripped();
  bool Zone1Tripped();
  bool Zone2Tripped();
  bool Zone3Tripped();
  bool Zone4Tripped();
  bool Zone5Tripped();
  bool Zone6Tripped();
  bool AlarmTripped();
 
};

#endif
