# pc1550-interface
An interface and keypad emulator for Digital Security Control's PC1550 
Alarm Panel

This class is a keypad emulator for the Digital Security Control's (DSC)
PC1550.  The model number of the keypad it emulates is PC1550RK.  It
should work with any ATmega chipset, and was tested using an Arduino UNO.

With this programmatic interface, you could emulate your own keypad on 
the device (iPhone maybe?) of your choice.  Or you could setup your own
monitoring/alerting system (maybe send yourself a text message) when certain
alarm events occur.

Hardware Connections
----------------------------------------------------------------------------

There are four wires that go to the keypad:

#### Red      

            Voltage.  This should be about 12V.  On my system, a
            voltage meter read 13.3V with a brand new battery.  You 
            should be able to safely use this as a power supply for the 
            Arduino.  Simply connect the red line to the pin labeled Vin.  
            The Arduino's internal voltage regulator will take care
            of the rest.  Because the PC1550 has a backup battery
            supply, your Arduino will continue to be powered even
            when the electrity goes out (quite convenient).

#### Black   
 
            Ground.  If you're using the PC1550 to supply power to
            the Arduino, connect this to the GND pin next to the Vin pin

#### Yellow  

            Clock.  The PC1550 control panel determines the clock cycle.
            So long as the processClockCycle() method on this class is 
            called more frequently than half a clock cycle from the PC1550
            then we will safely be able to read and write signals from
            the PC1550 control panel. The total cycle is about 1500-1600
            micro-seconds, on average (roughly 650 Hz).  Because data 
            is sent when the clock is low and read when the clock is high,
            we must run the processClockCycle() method at least every
            800 micro-seconds.  The more frequently the processClockCycle
            method is called, the more likely data transmission will
            succeed (in both directions) without data loss.  If you're
            not sure if you can commit to a calling the processClockCycle()
            function frequently enough, then you can use 
            processTransmissionCycle() instead which will call 
            processClockCycle() and block until a full transmission cycle
            is complete.  This could be handy if your program is performing
            another task that could use significant clock cycles between
            calls to processClockCycle().  On the downside, 
            processTransmissionCycle() takes between 57ms and 104ms to
            complete.

            Connect the clock line up to either a digital or analog pin.
            We only read on this line using digitalRead, so either a
            digital or analog pin will do.  Analog PIN 4 is the default
            but can be overriden via the constructor to this class.

#### Green  

            Data.  The data line is used to send bits to and from the
            PC1550 when the clock is low and high, respectively.
          
            Connect this line to any analog pin.  While we can read
            data from the panel using only digital functions, we 
            must use analog functions to pull the pin low when we 
            want to send data back to the control panel.  Analog pin 3
            is the default but can be overridden via the contructor.

There is one additional connection that can be made that can provide
additinal state information from the alarm controller.

#### Blue   

            PGM.  The PGM terminal on the DSC PC1550 control panel
            can be programmed to do a number of things.  One option
            is to configure it as a 2nd data line.  The installation
            manual refers to the PC16-OUT module.  This module reads
            data from the PGM line and we can emulate that module here.
            This line does not go to the keypad, and is optional for
            use by this library.  

            For the PGM terminal to work, it will need to be connected to the
            AUX+ terminal with a 1k Ohm resistor (for PC1550s)

See http://www.alarmhow.net/manuals/DSC/Modules/Output%20Modules/PC16-OUT.PDF 
for a full listing of PGM options.

PC1550 Interface Specification
----------------------------------------------------------------------------
The PC1550 control panel starts by holding the clock high for
roughly 26.5ms.  It then clocks out 16 cycles (one cycle is represented
by the clock going low and then returning to a high state).  After
16 clock cycles, the PC1550 holds the clock high for roughly 26.5ms
again, which starts the entire cycle over.

During the 16 clock cycles data is received when the clock is high:
   - The first 8 clock cycles are used to send one octet (byte)
     of data to the keypad (one bit per clock cycle).  This byte 
     contains information about which zones are currently open 
     (what zone lights should display on the keypad).  For this
     reason, the first 8 bits are referred to here as "zone bits."

     The table below shows how the data is received and interpretted.
     Bit 7 is received first, and bit 0 is recieved last.  When bit 7
     is on, then the zone 1 light should be on; when bit 6 is on, then
     the zone 2 light should be on, etc.  Bits 1 and 0 are not used.

         Zone Bit    7    6    5    4    3    2    1    0
         Zone        1    2    3    4    5    6  (Not Used)

   - The second 8 clock cycles send 8 more bits.  This byte contains
     information about the other lights that should be enabled on
     the keypad.  These bits represent other states and therefore, 
     these bits are referred to here as "state bits."
 
     The table below shows how each bit is used.  Note that when bit 0
     is on, the keypad beep emits a short beep.

   State Bit  7       6       5       4       3       2       1       0
              Ready   Armed   Memory  Bypass  Trouble X       X       Beep

Between receipt of the zone bits, data can be sent back to the control
panel when the clock is low.  In other words, the panel sends out its 
bits when the clock is high and the keypad sends back its data when the 
clock is low.

The keypad only sends back 7 bits-- one bit between each of the 8 zone
bits received.  These bits represent a button press.  When taking the
keypad as a table (rows and columns) of buttons, the first three
bits received represent the column of the button pressed.  The last
four bits represent the row of the button pressed:

                   First Three Bits               Last 4 Bits
     Column 1      100                    Row 1   0001
     Column 2      010                    Row 2   0010
     Column 3      001                    Row 3   0100
                                          Row 4   1000
                                          Row 5   0000
     No Key        000                            0000 

Encoding bits in the data line:
  When the data line is LOW  the corresponding bit should be ON.
  When the data line is HIGH the corresponding bit should be OFF.

If the PGM line is connected, then 16 more bits of data are received
on this line if (and ONLY IF) the PGM line is configured in PC-16OUT
mode.  Refer to the PC1550 installation manual for instructions on
how to configure this mode.  The bits meaning follow:


*   0 --  PGM Output (whatever the PGM is configured for)
...       (This library assumes PGM terminal has been programmed for
...       strobe output.  This sets bit 0 to the on position
...       when the alarm goes off.  And the bit remains set until
...       the panel is disarmed).
*   1 --  Fire buttom pressed (on for 4 sec)
*   2 --  Aux button pressed (on for 4 sec)
*   3 --  Panic button pressed (on for 4 sec)
*   4 --  Armed
*   5 --  Armed
*   6 --  Armed with bypass (on for 5 sec)
*   7 --  Trouble 
*   8 --  Fire (on when fire alarm is latched in)
*   9 --  Not used
*  10 --  Zone 6 tripped while armed
*  11 --  Zone 5 tripped while armed
*  12 --  Zone 4 tripped while armed
*  13 --  Zone 3 tripped while armed
*  14 --  Zone 2 tripped while armed
*  15 --  Zone 1 tripped while armed

For the PGM terminal to work, it will need to be connected to the
AUX+ terminal with a 1k Ohm resistor (for PC1550s)

Usage
----------------------------------------------------------------------------
To use the library, simply do three things:

    (1) Include PC1550.h in your Arduino program
    (2) Instantiate an instance of the PC1550 instance
    (3) call processTransmissionCycle() on the object in each loop

The PC1550 object has a number of methods you may call to determine the
state of the lights on the keypad.  All the following methods return a boolean 
value:

      ReadyLight()   -- Indicates whether the keypad's ready light is on
      ArmedLight()   -- Indicates whether the keypad's armed light is on
      MemoryLight()  -- Indicates whether the keypad's memory light is on
      BypassLight()  -- Indicates whether the keypad's bypass light is on
      TroubleLight() -- Indicates whether the keypad's trouble light is on
      Zone1Light()   -- Indicates whether the keypad's zone 1 light is on
      Zone2Light()   -- Indicates whether the keypad's zone 2 light is on
      Zone3Light()   -- Indicates whether the keypad's zone 3 light is on
      Zone4Light()   -- Indicates whether the keypad's zone 4 light is on
      Zone5Light()   -- Indicates whether the keypad's zone 5 light is on
      Zone6Light()   -- Indicates whether the keypad's zone 6 light is on
      Beep()         -- Indicates whether the keypad is beeping

The PC1550 object has a number of methods you may call to determine the
state of the alarm based on the PGM output terminal.  All the following 
methods return a boolean value:

      PGMOutput()          -- Indicates whether there is PGM output
      fireButtonTripped()  -- Indicates the fire button on the keypad caused
                              the fire alert to be tripped
      auxButtonTripped()   -- Indicates the aux button on the keypad caused 
                              the aux alert to be tripped
      panicButtonTripped() -- Indicates the panic button on the keypad caused
                              the panic alert to be tripped
      systemArmed()        -- Indicates the system is currently armed
      armedWithBypass()    -- Indicates the system is armed in bypass mode
      systemTrouble()      -- Indicates a trouble situation.  Use the 
                              keypad light methods to deduce the actual 
                              problem
      fireAlarmTripped()   -- Indicates the fire alarm has been tripped
      AlarmTripped()       -- Indicaets the security alarm has been tripped
      Zone1Tripped()       -- Indicates Zone1 tripped since system was armed
      Zone2Tripped()       -- Indicates Zone2 tripped since system was armed
      Zone3Tripped()       -- Indicates Zone3 tripped since system was armed
      Zone4Tripped()       -- Indicates Zone4 tripped since system was armed
      Zone5Tripped()       -- Indicates Zone5 tripped since system was armed
      Zone6Tripped()       -- Indicates Zone6 tripped since system was armed

      keypadStateChanged() -- Indicates the state of the keypad has changed
                              since the last call to processTransmissionCycle()

There are a number of misc methods to provide additional details on
the state of the system:
      
       consecutiveBeeps()      -- The number of clock cycles where a beep has
        		          sounded from the keypad
       consecutiveKeyPresses() -- The number of clock cycles where the same
       			          key has been pressed (and held down)
       atTransmissionEnd()     -- Indicates a full transmission (across 16
                                  clock cycles) has been read from the panel
       readyforKeyPress()      -- Indicates the panel is ready for the next
                                  keypress

You can simluate a keypress with the following method:

       bool sendKey(char c, int holdCycles)
           where 
              char is the character to send.  Valid values are 
                '1' through '9',
                '#'
                '*'
                'F','A', and 'P'

              holdCycles is the number of cycles across which to simulate
              holding of the key

           returns false if the panel is not ready for a keypress or
           if the character code is not valid.

To read from the panel, call one of the following methods:

        processTransmissionCycle() -- blocks until a full transmission has
                                      been read across 16 clock cycles
        processClockCycle()        -- reads a single clock cycle and release
                                      control so that you may perform other
                                      tasks before the next clock cycle.  You
                                      must call this method every 800us or
                                      data receipt will not be reliable.  If
                                      you are unsure, call 
                                      processTransmissionCycle() instead.


Example
----------------------------------------------------------------------------
```c++

#include <PC1550.h>

PC1550 alarm = PC1550();

void setup() {
   Serial.begin(115200);
}

void loop() {

  //process a full transmisison cycle with the PC1550 controller  
  alarm.processTransmissionCycle();

  //print the state of the keypad and and PGM output to the Serial console
  if (alarm.keypadStateChanged())
     printState();

}


void printState(){
      Serial.print("\n|");
      alarm.ReadyLight() ? Serial.print("R") : Serial.print(" ");
      alarm.ArmedLight() ? Serial.print("A") : Serial.print(" ");
      alarm.MemoryLight() ? Serial.print("M") : Serial.print(" ");
      alarm.BypassLight() ? Serial.print("B") : Serial.print(" ");
      alarm.TroubleLight() ? Serial.print("T|") : Serial.print(" |");
      alarm.Zone1Light() ? Serial.print("1") : Serial.print(" ");
      alarm.Zone2Light() ? Serial.print("2") : Serial.print(" ");
      alarm.Zone3Light() ? Serial.print("3") : Serial.print(" ");
      alarm.Zone4Light() ? Serial.print("4") : Serial.print(" ");
      alarm.Zone5Light() ? Serial.print("5") : Serial.print(" ");
      alarm.Zone6Light() ? Serial.print("6|") : Serial.print(" |");
      alarm.Beep() ? Serial.print("B| ") : Serial.print(" | ");

      alarm.fireButtonTripped()  ? Serial.print("F"):Serial.print(" ");
      alarm.auxButtonTripped()   ? Serial.print("A"):Serial.print(" ");
      alarm.panicButtonTripped() ? Serial.print("P|"):Serial.print(" |");
      alarm.systemArmed() ? Serial.print("Armed   |") : Serial.print("Disarmed|");
      alarm.Zone1Tripped() ? Serial.print("1") : Serial.print(" ");
      alarm.Zone2Tripped() ? Serial.print("2") : Serial.print(" ");
      alarm.Zone3Tripped() ? Serial.print("3") : Serial.print(" ");
      alarm.Zone4Tripped() ? Serial.print("4") : Serial.print(" ");
      alarm.Zone5Tripped() ? Serial.print("5") : Serial.print(" ");
      alarm.Zone6Tripped() ? Serial.print("6") : Serial.print(" ");
      alarm.AlarmTripped() ? Serial.print("|A|") : Serial.print("| |");
 
}
```