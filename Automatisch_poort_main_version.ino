/*
* Copyright 2018, Janik Van Holderbeke
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
* 
*/

// If needed add debug output


//LIBRARIES
 
  #include <SimpleTimer.h>    // load the SimpleTimer library to make timers, instead of delays & too many millis statements
  #include <SoftwareSerial.h>


//PINS
  //OUTPUT
  #define LDR_INPUT       A2 // photocell - pin A2
  #define PWM_B_OUT       11 // speed motor B - pin 11
  #define BRAKE_B_OUT     8  // brake motor B - pin 8
  #define DIR_B_OUT       13 // direction motor B - pin 13
  #define TX              10
  //INPUT
  #define BOT_GATE_SWITCH 4  // bottom REED sensor - pin 4
  #define TOP_GATE_SWITCH 5  // top REED sensor - pin 5
  #define TOGGLE_SWITCH   7  // automatic/manual switch - pin 7
  #define PUSH_PIN_OPEN   2  // manual open door - pin 2
  #define PUSH_PIN_CLOSE  3  // manual close door - pin 3
  #define RX              9


//CONSTANTS
  //LvL0 == DARK, values for bottom and top LDR output comparison
  #define LDR_LvL0_Base     0
  #define LDR_LvL0_TOP      85
  //LvL1 == TWILLIGHT, values for bottom and top LDR output comparison
  #define LDR_LvL1_Base     86
  #define LDR_LvL1_TOP      500
  //LvL2 == LIGHT, value for bottom LDR output comparison/top not used in this program
  #define LDR_LvL2_Base     501
  //Indication of ligth intensity divided in three levels
  #define LIGHT       3
  #define TWILIGHT    2
  #define DARK        1
  //defines for easier coding
  #define UP 1
  #define DOWN 0
  #define BRAKE 1
  #define RUN 0
  #define CLOSED 0
  #define OPEN 1

//GLOBAL VARIABLES
// Photocell
  int photocellReading;   // analog reading of the photocel
  int lightILevel;        // photocel reading levels (dark, twilight, light)
// Debounce
  int debStartValue;           // switch var for reading the pin status
  int debDelayValue;          // switch var for reading the pin delay/debounce status
// Switch INPUT
  int bottomGateStatus;     // bottom switch var status
  int topGateStatus;        // top switch var status
  int programSwitchStatus;   // master switch var status
  int manualOpenStatus;  // open door switch var status
  int manualCloseStatus; // close door switch var status
// 
  int halfDay;                           // check for 12 hours
  int wasNight;                          
  int timerID1;
  int timerID2;

  // Boolean for Serial Communication with Arduino
  boolean newData = false;
  const byte numChars = 32;
  char receivedChars[numChars];
  
  #ifdef DEBUG
  bool programChange;
  #endif
// just need 1 SimpleTimer object
  SimpleTimer coopTimer;
  SoftwareSerial mySerial(RX, TX);
//=======================================================
/*
 *
 *    FUNCTIONS
 *
 */
//=======================================================
//Software Reset Function
/*
void(* Reset) (void) = 0;
*/
//=======================================================
// photocel to read levels of exterior light
// function to be called repeatedly - per cooptimer set in setup
void read_photocell() { 
  photocellReading = analogRead(LDR_INPUT);
  #ifdef DEBUG
  Serial.println("---------------------");
  Serial.print("Photocel Analog Value =");
  // Serial.print("\t");
  Serial.print(photocellReading);
  Serial.print(" , Level =");
  //Photocel threshholds
  #endif
  if (photocellReading >= LDR_LvL0_Base && photocellReading <= LDR_LvL0_TOP) {
    lightILevel = DARK;
    #ifdef DEBUG
    Serial.println("Dark");
    Serial.println("---------------------");
    #endif
  } else if (photocellReading  >= LDR_LvL1_Base && photocellReading <= LDR_LvL1_TOP){
    lightILevel = TWILIGHT;
    #ifdef DEBUG
    Serial.println("Twilight");
    Serial.println("---------------------");
    #endif
  } else if (photocellReading  >= LDR_LvL2_Base ) {
    lightILevel = LIGHT;
    #ifdef DEBUG
    Serial.println("Light");
    Serial.println("---------------------");
    #endif
  } else {
    #ifdef DEBUG
    Serial.println(" NO LEVEL DETECTED _ ANALOG VALUE OUT OF BOUNDS");
    Serial.println("---------------------");
    #endif
  }
}
//=======================================================
void set_half_day(){
  halfDay = 1;
  #ifdef DEBUG
  Serial.print(" 12 Hours have past");
  #endif
}
//=======================================================
//debounce input from arduino
//blocking function, but not critical for this program
//Crude, but works fine
void debounce(int PIN, int *VAL){
  debStartValue = digitalRead(PIN);
  delay(10);
  debDelayValue = digitalRead(PIN);
  
  if(debStartValue == debDelayValue){
    *VAL = debStartValue;
         #ifdef DEBUG
         if(*VAL != debStartValue){
           Serial.print("-  PIN=");
           Serial.print(PIN);
           Serial.print(", Value=");
           Serial.println(*VAL);
         } else {
          
         }
         #endif
  }
}
//=======================================================
// stop the coop door motor
void stop_gate(){
  analogWrite (PWM_B_OUT, 0);         // 0 speed
  digitalWrite (BRAKE_B_OUT, BRAKE);  // turn on motor brake
  digitalWrite (DIR_B_OUT, LOW);    // turn off motor direction
}

 //=======================================================
// Receive Serial message and remove '<' and '>' from message
void receive_serial_data() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
 
 // if (mySerial.available() > 0) {
    while (mySerial.available() > 0 && newData == false) {
        rc = mySerial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

//=======================================================
// This is to read Serial Data from Arduino Serial interface to take action with,
// report the status of the arduino, send logging data to database
void process_serial_data(){
  if (newData == true) {
    String rdcommand = String(receivedChars);
    if(rdcommand.startsWith("DATA")){
      rdcommand.remove(0,6);
      //set_timing(rdcommand.toInt());
    }
    else if (rdcommand.startsWith("STATUS")){
      rdcommand.remove(0,4);
      //set_pump_run(rdcommand.toInt());
    }
    else if (rdcommand.startsWith("START")){
      
    }
    else if (rdcommand.startsWith("STOP")){
      
    } else {
        Serial.println("<Invalid command>");
    }
    //Serial.println(rdcommand);
    newData = false;
  }
}

//=======================================================
// close the coop door motor (motor dir close = down)
void close_gate() {
  debounce(BOT_GATE_SWITCH, &bottomGateStatus);                    // read and debounce the switches
  debounce(TOP_GATE_SWITCH, &topGateStatus);
  if (bottomGateStatus == OPEN){
    digitalWrite(BRAKE_B_OUT, RUN);
    digitalWrite(DIR_B_OUT, DOWN);
    analogWrite(PWM_B_OUT, 255);
  } else {
    stop_gate();
    #ifdef DEBUG
    Serial.println(" Coop Door Closed - no danger");
    #endif
    wasNight = 1;
  }
}
//=======================================================
// open the coop door (motor dir open = up)
void open_gate() {
  debounce(BOT_GATE_SWITCH, &bottomGateStatus);    // read and debounce the switches
  debounce(TOP_GATE_SWITCH, &topGateStatus);
  if (topGateStatus == OPEN){
    digitalWrite(BRAKE_B_OUT, RUN);
    digitalWrite(DIR_B_OUT, UP);
    analogWrite(PWM_B_OUT, 255);
  } else {
    stop_gate();
    //#ifdef DEBUG
    //Serial.println(" Coop Door open - danger!");
    //#endif
  }
}
//=======================================================
// main function to check what needs to be done with the gate
void auto_gate_check(){
//  switch(lightILevel){
//    case DARK:  //If it is night time
//      if (lightILevel != LIGHT && halfDay == 1) {
//        close_gate();
//      }
//      break;
//    case LIGHT: //
//      if (lightILevel != DARK && wasNight == 1) {            // if it's not dark (depricated, but still not wrong ;-)
//        halfDay = 0;                      // reset halfDay
//        wasNight = 0;                     // reset wasNight
//        coopTimer.restartTimer(timerID2); // restart Timer for 12 hours just in case
//      }
//      open_gate();                   // Open the door
//      break;
//    case TWILIGHT:
//      stop_gate();
//      break;
//    default:
//      stop_gate();
//      break;
//  }
  
  
  if (lightILevel  == DARK && lightILevel != TWILIGHT) {    // if it's dark and not twilight
    if (lightILevel != LIGHT && halfDay == 1) {             // if it's not light and 12 hours have past
      close_gate();                                    // close the door
    }
  }
  if (lightILevel  == LIGHT && lightILevel != TWILIGHT) {   // if it's light and not twilight
    if (lightILevel != DARK) {                              // if it's not dark
      if(wasNight == 1){
        halfDay = 0;                                        // reset halfDay
        wasNight = 0;                                       // reset wasNight
        coopTimer.restartTimer(timerID2);                   // restart Timer for 12 hours just in case
      }
      open_gate();                                     // Open the door
    }
  }
  if (lightILevel == TWILIGHT){
    stop_gate();
  }
}
//=======================================================
/*
 *
 *    SETUP
 *
 */
//=======================================================
void setup(void) {
  mySerial.begin(9600);
  message = "";
  #ifdef DEBUG
  Serial.begin(9600);
  programChange = true;
// welcome message
  Serial.println(" Checking auto_gate_check: every 10 minutes for light levels to open or close door");
  Serial.println();
  #endif
  
// setup coop door motor
  pinMode (PWM_B_OUT, OUTPUT);    // speed motor pin = output (LOW = UP, HIGH = DOWN) (quote this can change on how you connect your motor and/or how he is installed)
  pinMode (BRAKE_B_OUT, OUTPUT);  // brake motor pin = output
  pinMode (DIR_B_OUT, OUTPUT);    // direction motor pin = output
 
// set up coop door switches
  pinMode(BOT_GATE_SWITCH, INPUT);        // set bottom switch pin as input
  //digitalWrite(BOT_GATE_SWITCH, HIGH);  // activate bottom switch resistor, not needed as this will alter the read out (situational)
  pinMode(TOP_GATE_SWITCH, INPUT);        // set top switch pin as input
  //digitalWrite(TOP_GATE_SWITCH, HIGH);  // activate top switch resistor, not needed as this will alter the read out (situational)
  pinMode(TOGGLE_SWITCH, INPUT);
  pinMode(PUSH_PIN_OPEN, INPUT);
  pinMode(PUSH_PIN_CLOSE, INPUT);
 
// simple timer setup
  timerID1 = coopTimer.setInterval(2000, read_photocell);      // read the photocell every 10 minutes
  timerID2 = coopTimer.setInterval(43200000, set_half_day);       // set halfDay variable to 1 after 12 hours (43200000 millis)
  
//Set the check variables to Night mode (gate will be closed when DARK is detected, if this is done with daylight you'll have to wait 12 hours from starting to get the gate closed)  
  halfDay = 1;
  wasNight = 0;
  newData = false;
}
//===============================================================================================================
/****************************************************************************************************************
 ****************************************************************************************************************
 *    MAIN LOOP
 ****************************************************************************************************************
 ****************************************************************************************************************/
//===============================================================================================================
void loop() {
  //boolean for debug message loop
  #ifdef DEBUG
  boolean debugmessage;
  #endif
  //Read Serial and decrypt message from ESP8266
  receive_serial_data();
  process_serial_data();

  // debounce program switch
  debounce(TOGGLE_SWITCH, &programSwitchStatus);
  //  polling occurs
  coopTimer.run();
  /*
  switch (programSwitchStatus){
  case 0:
    auto_gate_check();
    #ifdef DEBUG
    if(programChange == true){
      Serial.println("**********************************************");
      Serial.println("AUTOMATIC PROGRAM");
      Serial.println("**********************************************");
      programChange = false;
    }
    #endif
    //Secret Reset
    //Press both Open & Close switch while program has started to reset to Dark
    debounce(PUSH_PIN_OPEN, &manualOpenStatus);
    debounce(PUSH_PIN_CLOSE, &manualCloseStatus);
    if (manualOpenStatus == 1 && manualCloseStatus == 1){
      Reset();                  //Reset the arduino software in style & in secret (only perform this in dark times before the sun comes up, its a bit vampiric ;-) )
      #ifdef DEBUG
      Serial.println("---------------------");
      Serial.println("Resetting");
      Serial.println("---------------------");
      #endif
    } else if (manualOpenStatus != 1 && manualCloseStatus == 1) {    //Bypass for 12 hour timer
      halfDay = 1;
      #ifdef DEBUG
      Serial.println("---------------------");
      Serial.println("12 Hours bypassed, no more waiting!!");
      Serial.println("---------------------");
      #endif
    }
    break;
  case 1:
    #ifdef DEBUG
      if(programChange == false){
      Serial.println("**********************************************");
      Serial.println("MANUAL PROGRAM");
      Serial.println("**********************************************");
      programChange = true;
    }
    #endif
    debounce(PUSH_PIN_OPEN, &manualOpenStatus);
    debounce(PUSH_PIN_CLOSE, &manualCloseStatus);
    
    if(manualOpenStatus == 1 && manualCloseStatus != 1) {
      //open door
      open_gate();
    } else if (manualOpenStatus != 1 && manualCloseStatus == 1) {
      //close door
      close_gate();
    } else {
      //stop motor
      stop_gate();
    }
    break;
  default:
    #ifdef DEBUG
    Serial.println("**********************************************");
    Serial.println("UNDEFINED PROGRAM STATE");
    Serial.println("**********************************************");
    #endif
    stop_gate();
    break;
  }*/
  
  if (programSwitchStatus == 0) {
    auto_gate_check();
//          #ifdef DEBUG
//          if(debugmessage == false){
//            Serial.println("----------------------------------------------");
//            Serial.println("Automatic program started");
//            Serial.println("----------------------------------------------");
//            debugmessage = true;
//          } else if(debugmessage == true){
//            debugmessage = true;
//          } else {
//            //debugmessage = false;
//          }
//          #endif
    //Secret Reset
    //Press both Open & Close switch while program has started to reset to Dark
    debounce(PUSH_PIN_OPEN, &manualOpenStatus);
    debounce(PUSH_PIN_CLOSE, &manualCloseStatus);
    if (manualOpenStatus == CLOSED && manualCloseStatus == CLOSED){
          #ifdef DEBUG
          Serial.println("----------------------------------------------");
          Serial.println("Resetting");
          Serial.println("----------------------------------------------");
          #endif
      //Reset();                  //Reset the arduino software in style & in secret (only perform this in dark times before the sun comes up, its a bit vampiric ;-) )
    } else if (manualOpenStatus == OPEN && manualCloseStatus == CLOSED) {    //Bypass for 12 hour timer
        halfDay = 1;
          #ifdef DEBUG
          Serial.println("----------------------------------------------");
          Serial.println("12 Hours bypassed, no more waiting!!");
          Serial.println("----------------------------------------------");
          #endif
    }
  } else if (programSwitchStatus == 1) {
          #ifdef DEBUG
          if(debugmessage == true){
            Serial.println("**********************************************");
            Serial.println("Manual labor is required");
            Serial.println("**********************************************");
            debugmessage = false;
          } else if(debugmessage == false){
            debugmessage = false;
          } else {
            debugmessage = true;
          }
          #endif
    debounce(PUSH_PIN_OPEN, &manualOpenStatus);
    debounce(PUSH_PIN_CLOSE, &manualCloseStatus);
    
    if(manualOpenStatus == CLOSED && manualCloseStatus == OPEN) {
      //open door
      open_gate();
    } else if (manualOpenStatus == OPEN && manualCloseStatus == CLOSED) {
      //close door
      close_gate();
    } else {
      //stop motor
      stop_gate();
    }
  }
}
