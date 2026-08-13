#define VERSION 1.60
/*  13/08/2026 - Daniel Desmartins
 *  Connected to the Relay Port in AgOpenGPS
 *  If you find any mistakes or have an idea to improove the code, feel free to contact me. N'hésitez pas à me contacter en cas de problème ou si vous avez une idée d'amélioration.
 */

//pins:
#define NUM_OF_RELAYS 8
#define PinSC_Ready 2
#define PinAogStatus 23  //Pin AOG Conntected
const uint8_t relayPinArray[] = {32, 33, 25, 26, 27, 14, 12, 13};
#define AutoSwitch 34  //Switch Mode Auto On/Off //Warning!! external pullup! connected this pin to a 10Kohms resistor connected to 3.3v.                                                                        //<-
#define ManualSwitch 35 //Switch Mode Manual On/Off //Warning!! external pullup! connected this pin to a 10Kohms resistor connected to 3.3v.                                                                      //<-
const uint8_t switchPinArray[] = {4, 16, 17, 5, 18, 19, 21, 22};
#define WorkWithoutAogSwitch 0 //Switch for work without AOG (optional). For use, connect to GND within 5s after turning on the box, but must not be at GND when turning on! (the ESP will remain frozen in boot mode)
//#define NO_REMOTE_MODE

//Options:
bool relayIsActive = HIGH; //Replace LOW with HIGH if your relays don't work the way you want
bool readyIsActive = LOW;

#define BT //comment to use a serial link
#ifdef BT
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
#else
#define SerialBT Serial
#endif //BT

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

//hello from AgIO
uint8_t helloFromMachine[] = { 128, 129, 123, 123, 5, 0, 0, 0, 0, 0, 71 };
bool helloUDP = false;
//show life in AgIO
uint8_t helloAgIO[] = { 0x80, 0x81, 0x7B, 0xEA, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0x6D };
uint8_t helloCounter = 0;

uint8_t AOG[] = { 0x80, 0x81, 0x7B, 0xEA, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0xCC };

//The variables used for storage
uint8_t relayLo = 0, relayHi = 0;

uint8_t count = 0;

bool autoModeIsOn = false;
bool manualModeIsOn = false;
bool aogConnected = false;
bool firstConnection = true;

uint8_t onLo = 0, offLo = 0, mainByte = 0;

//whitout AOG
bool lastManualMode = false;
bool workWithoutAog = false;
bool initWorkWithoutAog = false;
uint8_t countManualMode = 0;
uint32_t lastTimeManualMode = loopTime;
//End of variables

#include "LedManager.h"
#include "PulseGenrator.h"

void setup() {
  //Pin Initialization
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(relayPinArray[count], OUTPUT);
  }
  pinMode(PinSC_Ready, OUTPUT);
  analogWrite(PinSC_Ready, 25);
  pinMode(PinAogStatus, OUTPUT);
  pinMode(AutoSwitch, INPUT_PULLUP);  //INPUT_PULLUP: no external Resistor to GND or to PINx is needed, PULLUP: HIGH state if Switch is open! Connect to GND
  pinMode(ManualSwitch, INPUT_PULLUP);
  pinMode(WorkWithoutAogSwitch, INPUT_PULLUP);
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(switchPinArray[count], INPUT_PULLUP);
  }
  
  switchRelaisOff(); //All relays off!
  
  Serial.begin(38400);  //set up communication
  while (!Serial) {
    // wait for serial port to connect. Needed for native USB
  }
  Serial.println("\r\n.");
  Serial.println(".\r\n");
  Serial.println("Firmware : SectionControl BT");
  Serial.print("Version : ");
  Serial.println(VERSION);
  #ifdef BT
  SerialBT.begin("SectionControl");
  #endif
  
  xTaskCreate( taskLed, "LED Task", 2048, NULL, 1, NULL );
  setupPulseGenerator();
} //end of setup

void loop() {
  currentTime = millis();
  if (currentTime - lastTime >= loopTime) {  //start timed loop
    lastTime = currentTime;

    whitoutAogMode();
        
    //clean out serial buffer to prevent buffer overflow:
    if (serialResetTimer++ > 20) {
      updatePulseSpeed(0);
      while (SerialBT.available() > 0) SerialBT.read();
      serialResetTimer = 0;
      statusLED = NO_CONNECTED;
    }
    
    //avoid overflow of watchdogTimer:
    if (watchdogTimer++ > 250) watchdogTimer = 12;
    
    if (aogConnected && watchdogTimer > 60) {
      aogConnected = false;
      firstConnection = true;
      statusLED = AOG_CONNECTED;
    }
    
    //emergency off:
    if (watchdogTimer > 10) {
      switchRelaisOff();
      
      //show life in AgIO
      if (++helloCounter > 10 && !helloUDP) {
        SerialBT.write(helloAgIO, sizeof(helloAgIO));
        helloCounter = 0;
      }
    }
    #ifdef NO_REMOTE_MODE
    else if (!digitalRead(ManualSwitch)) {
      //show life in AgIO
      if (++helloCounter > 10 && !helloUDP) {
        SerialBT.write(helloAgIO, sizeof(helloAgIO));
        helloCounter = 0;
      }

      if (mainByte != 0) {
        mainByte = 0;
        offLo = 0;
        sendToAOG();
      }

      for (count = 0; count < NUM_OF_RELAYS; count++) {
        if (count < 8) {
          digitalWrite(relayPinArray[count], (bitRead(relayLo, count) == relayIsActive)); //Open or Close relayLo by AOG
        }
      }
    } else {
      switchRelaisOff();
      //Send to AOG All Off!
      mainByte = 2;

      sendToAOG();
      //if (helloUDP) delay(10);
    }
    #else // NO_REMOTE_MODE
    else {
      //check Switch if Auto/Manual:
      autoModeIsOn = !digitalRead(AutoSwitch); //Switch has to close for autoModeOn, Switch closes ==> LOW state ==> ! makes it to true
      if (autoModeIsOn) {
        mainByte = 1;
      } else {
        mainByte = 2;
        manualModeIsOn = !digitalRead(ManualSwitch);
        if (!manualModeIsOn) firstConnection = false;
      }
      
      if (!autoModeIsOn) {
        if(manualModeIsOn && !firstConnection) { //Mode Manual
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
      
      sendToAOG();
    }
    #endif
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
      statusLED = AOG_CONNECTED;
      watchdogTimer = 12;
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
          aogConnected = true;
          firstConnection = true;
      }
      statusLED = AOG_READY;
    }
    else if (pgn == 200) // Hello from AgIO
    {
      helloUDP = true;
      
      SerialBT.read(); //Version
      SerialBT.read();
      
      if (SerialBT.read())
      {
        relayLo -= 255;
        relayHi -= 255;
        watchdogTimer = 0;
      }
    
      //crc
      SerialBT.read();
      
      helloFromMachine[5] = relayLo;
      helloFromMachine[6] = relayHi;

      delay(10); //delay for USR modules which can be grouped into packages (readable for AGIO)
      SerialBT.write(helloFromMachine, sizeof(helloFromMachine));
      delay(10); //delay for USR modules which can be grouped into packages (readable for AGIO)
      
      if (statusLED != AOG_READY) statusLED = AOG_CONNECTED;
      
      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn = dataLength = 0;
    }
    else if (pgn == 202)
    {
      while (SerialBT.available() > 0) SerialBT.read();
      uint8_t scanReply[] = { 128, 129, 123, 203, 7, 
                    192, 168, 1, 123,
                    192, 168, 1, 23 };
      
      //checksum
      int16_t CK_A = 0;
      for (uint8_t i = 2; i < sizeof(scanReply) - 1; i++)
      {
        CK_A = (CK_A + scanReply[i]);
      }
      scanReply[sizeof(scanReply) - 1] = CK_A;
      
      //off to AOG
      delay(10); //delay for USR modules which can be grouped into packages (readable for AGIO)
      SerialBT.write(scanReply, sizeof(scanReply));
      delay(10); //delay for USR modules which can be grouped into packages (readable for AGIO)
      
      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn = dataLength = 0;
    }
    else if (pgn == 254)
    {
      float gpsSpeed = ((float)(Serial.read() | Serial.read() << 8)); // = Speed * 10
      updatePulseSpeed(gpsSpeed);

      Serial.read();
      Serial.read();
      Serial.read();
      Serial.read();
      Serial.read();
      Serial.read();

      //Bit 13 CRC
      Serial.read();
        
      //Reset serial Watchdog
      serialResetTimer = 0;

      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn = dataLength = 0;      
    }
    else { //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn = dataLength = 0;
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

void sendToAOG() {
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

void whitoutAogMode() {
  if (Serial.available()) {
    initWorkWithoutAog = false;
    countManualMode = 0;
    return;
  }

  manualModeIsOn = digitalRead(ManualSwitch);
  if (manualModeIsOn == HIGH && lastManualMode == LOW)
  {
    if (lastTimeManualMode < currentTime + 5000) {
      if (countManualMode++ > 4) {
        initWorkWithoutAog = true;
        watchdogTimer = 12;
      }
    } else {
      countManualMode = 0;
      initWorkWithoutAog = false;
      lastTimeManualMode = currentTime;
    }
  }
  lastManualMode = manualModeIsOn;
  
  if (initWorkWithoutAog/* || !digitalRead(PinWorkWithoutAOG)*/) {
    if (!(watchdogTimer % 6)) digitalWrite(PinAogStatus, !digitalRead(PinAogStatus));
    if (!(watchdogTimer % 8)) digitalWrite(PinSC_Ready, !digitalRead(PinSC_Ready));
    
    if (watchdogTimer > 100) {
      initWorkWithoutAog = false;
      workWithoutAog = true;
      countManualMode = 0;
      digitalWrite(PinSC_Ready, !readyIsActive);
      digitalWrite(PinAogStatus, readyIsActive);
    }
  }
  
  while (workWithoutAog) {
    for (count = 0; count < NUM_OF_RELAYS; count++) {
      if (digitalRead(switchPinArray[count]) || (digitalRead(AutoSwitch) && digitalRead(ManualSwitch))) {
        digitalWrite(relayPinArray[count], !relayIsActive); //Relay OFF
      } else {
        digitalWrite(relayPinArray[count], relayIsActive); //Relay ON
      }
    }
    delay(100);
    if (Serial.available()) {
      workWithoutAog = false;
      digitalWrite(PinAogStatus, !readyIsActive);
    }
  }
}
