#define VERSION 1.22
/* 26/03/2024 - Daniel Desmartins
 *  Connected to the Relay Port in AgOpenGPS  
 */

//pins:
#define NUM_OF_RELAYS 7
#define PinSC_Ready 23
#define PinAogReady 35 //Pin AOG Conntected
const uint8_t relayPinArray[] = {32, 33, 25, 26, 27, 14, 12, 13};
#define AutoSwitch 15  //Switch Mode Auto On/Off
#define ManuelSwitch 2 //Switch Mode Manuel On/Off
const uint8_t switchPinArray[] = {4, 16, 17, 5, 18, 19, 21, 22};
bool relayIsActive = HIGH; //Replace LOW with HIGH if your relays don't work the way you want
bool readyIsActive = LOW;

#define LED_CONNECTED 0
#define LED_READY 1
#define LED_ON 138

#define BT //comment to use a serial link
#ifdef BT
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
#else
#define SerialBT Serial
#endif BT

//Variables:
const uint8_t loopTime = 100; //10hz
uint32_t lastTime = loopTime;
uint32_t currentTime = loopTime;

//Comm checks
uint8_t watchdogTimer = 12;      //make sure we are talking to AOG
uint8_t serialResetTimer = 0;    //if serial buffer is getting full, empty it

//Parsing PGN
bool isPGNFound = false, isHeaderFound = false;
uint8_t pgn = 0, dataLength = 0;
int16_t tempHeader = 0;

//show life in AgIO
uint8_t helloAgIO[] = { 0x80, 0x81, 0x7B, 0xEA, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0x6D };
uint8_t helloCounter = 0;
uint8_t AOG[] = { 0x80, 0x81, 0x7B, 0xEA, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0xCC };

uint8_t pwm_LED = 0;
uint8_t LED_Increment = LED_ON/10;

//The variables used for storage
uint8_t relayLo = 0, relayHi = 0;

uint8_t count = 0;

bool autoModeIsOn = false;
bool manuelModeIsOn = false;
bool aogConnected = false;
bool firstConnection = true;

uint8_t onLo = 0, offLo = 0, mainByte = 0;
//End of variables

void setup() {
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(relayPinArray[count], OUTPUT);
  }  
  pinMode(AutoSwitch, INPUT_PULLUP);  //INPUT_PULLUP: no external Resistor to GND or to PINx is needed, PULLUP: HIGH state if Switch is open! Connect to GND and D0/PD0/RXD
  pinMode(ManuelSwitch, INPUT_PULLUP);
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(switchPinArray[count], INPUT_PULLUP);
  }

  ledcSetup(LED_CONNECTED, 5000, 8);
  ledcSetup(LED_READY, 5000, 8);
  ledcAttachPin(PinAogReady, LED_CONNECTED);
  ledcAttachPin(PinSC_Ready, LED_READY);
  
  switchRelaisOff();
  
  ledcWrite(LED_CONNECTED, 0);
  ledcWrite(LED_READY, 0);
  
  delay(100); //wait for IO chips to get ready
  
  Serial.begin(38400);  //set up communication
  while (!Serial) {
    // wait for serial port to connect. Needed for native USB
  }
  Serial.println("");
  Serial.println("Firmware : SectionControl BT");
  Serial.print("Version : ");
  Serial.println(VERSION);
  #ifdef BT
  SerialBT.begin("SectionControl");
  #endif
} //end of setup

void loop() {
  currentTime = millis();
  if (currentTime - lastTime >= loopTime) {  //start timed loop
    lastTime = currentTime;
    
    //avoid overflow of watchdogTimer:
    if (watchdogTimer++ > 250) watchdogTimer = 241;
    
    //clean out serial buffer to prevent buffer overflow:
    if (serialResetTimer++ > 20) {
      while (SerialBT.available() > 0) SerialBT.read();
      serialResetTimer = 0;
    }
    
    if ((watchdogTimer > 20)) {
      if (aogConnected) {
        if (watchdogTimer > 60) {
          aogConnected = false;
          firstConnection = true;
          ledcWrite(LED_CONNECTED, 0);
          ledcWrite(LED_READY, 0);
        }
      } else {
        if (watchdogTimer > 240) {
          ledcWrite(LED_CONNECTED, 0);
          pwm_LED = 0;
        } else {
          pwm_LED += LED_Increment;
          if (pwm_LED > LED_ON) pwm_LED = 0;
          ledcWrite(LED_CONNECTED, pwm_LED);
        }
      }
    }
    
    //emergency off:
    if (watchdogTimer > 10) {
      switchRelaisOff();
      
      //show life in AgIO
      if (++helloCounter > 10) {
        SerialBT.write(helloAgIO, sizeof(helloAgIO));
        helloCounter = 0;
      }
    } else {
      //check Switch if Auto/Manuel:
      autoModeIsOn = !digitalRead(AutoSwitch); //Switch has to close for autoModeOn, Switch closes ==> LOW state ==> ! makes it to true
      if (autoModeIsOn) {
        mainByte = 1;
      } else {
        mainByte = 2;
        manuelModeIsOn = !digitalRead(ManuelSwitch);
        if (!manuelModeIsOn) firstConnection = false;
      }
      
      if (!autoModeIsOn) {
        if(manuelModeIsOn && !firstConnection) { //Mode Manuel
          for (count = 0; count < NUM_OF_RELAYS; count++) {
            if (!digitalRead(switchPinArray[count])) { //Signal LOW ==> switch is closed
              if (count < 8) {
                bitClear(offLo, count);
                bitSet(onLo, count);
              }
              digitalWrite(relayPinArray[count], relayIsActive); //Relay ON
            } else {
              if (count < 8) {
                bitSet(offLo, count);
                bitClear(onLo, count);
              }
              digitalWrite(relayPinArray[count], !relayIsActive); //Relay OFF
            }
          }
        } else { //Mode off
          switchRelaisOff(); //All relays off!
        }
      } else if (!firstConnection) { //Mode Auto
        onLo = 0;
        for (count = 0; count < NUM_OF_RELAYS; count++) {
          if (digitalRead(switchPinArray[count])) {
            if (count < 8) {
              bitSet(offLo, count); //Info for AOG switch OFF
            }
            digitalWrite(relayPinArray[count], !relayIsActive); //Close the relay
          } else { //Signal LOW ==> switch is closed
            if (count < 8) {
              bitClear(offLo, count);
              digitalWrite(relayPinArray[count], (bitRead(relayLo, count) == relayIsActive)); //Open or Close relayLo if AOG requests it in auto mode
            }
          }
        }
      } else { //FirstConnection
        switchRelaisOff(); //All relays off!
        mainByte = 2;
      }
      
      //Send to AOG
      AOG[5] = (uint8_t)mainByte;
      AOG[9] = (uint8_t)onLo;
      AOG[10] = (uint8_t)offLo;
      
      //add the checksum
      int16_t CK_A = 0;
      for (uint8_t i = 2; i < sizeof(AOG)-1; i++)
      {
        CK_A = (CK_A + AOG[i]);
      }
      AOG[sizeof(AOG)-1] = CK_A;
      
      SerialBT.write(AOG, sizeof(AOG));
    }
  }
  
  // Serial Receive
  //Do we have a match with 0x8081?    
  if (SerialBT.available() > 4 && !isHeaderFound && !isPGNFound) 
  {
    uint8_t temp = SerialBT.read();
    if (tempHeader == 0x80 && temp == 0x81)
    {
      isHeaderFound = true;
      tempHeader = 0;
    }
    else
    {
      tempHeader = temp;     //save for next time
      return;
    }
  }

  //Find Source, PGN, and Length
  if (SerialBT.available() > 2 && isHeaderFound && !isPGNFound)
  {
    SerialBT.read(); //The 7F or less
    pgn = SerialBT.read();
    dataLength = SerialBT.read();
    isPGNFound = true;
    
    if (!aogConnected) {
      watchdogTimer = 12;
      ledcWrite(LED_CONNECTED, LED_ON);
    }
  }
  
  //The data package
  if (SerialBT.available() > dataLength && isHeaderFound && isPGNFound)
  {
    if (pgn == 239) // EF Machine Data
    {
      SerialBT.read();
      SerialBT.read();
      SerialBT.read();
      SerialBT.read();
      SerialBT.read();   //high,low bytes
      SerialBT.read();
      
      relayLo = SerialBT.read();          // read relay control from AgOpenGPS
      relayHi = SerialBT.read();
      
      //Bit 13 CRC
      SerialBT.read();
      
      //reset watchdog
      watchdogTimer = 0;
  
      //Reset serial Watchdog
      serialResetTimer = 0;

      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn=dataLength=0;
      
      if (!aogConnected) {
        ledcWrite(LED_READY, LED_ON);
        aogConnected = true;
        pwm_LED = 0;
      }
    }
    else { //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn=dataLength=0;
    }
  }
} //end of main loop

void switchRelaisOff() {  //that are the relais, switch all off
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    digitalWrite(relayPinArray[count], !relayIsActive);
  }
  onLo = 0;
  offLo = 0b11111111;
}
