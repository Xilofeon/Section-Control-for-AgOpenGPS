    /* V1.9.1 - 23/11/2022 - Daniel Desmartins
    *  Connected to the Relay Port in AgOpenGPS
    *  If you find any mistakes or have an idea to improove the code, feel free to contact me. N'hésitez pas à me contacter en cas de problème ou si vous avez une idée d'amélioration.
    */

//pins:                                                                                                                                 UPDATE YOUR PINS!!! //<-
#define NUM_OF_RELAYS 7 //7 relays max for Arduino Nano                                                                                                     //<-
const uint8_t relayPinArray[] = {2, 3, 4, 5, 6, 7, 8, 255, 255, 255, 255, 255, 255, 255, 255, 255};  //Pins, Relays, D2 à D8                                //<-
#define PinAogConnected 9 //Pin AOG Conntected                                                                                                              //<- 
#define AutoSwitch 10  //Switch Mode Auto On/Off                                                                                                            //<-
#define ManuelSwitch 11 //Switch Mode Manuel On/Off                                                                                                         //<-
const uint8_t switchPinArray[] = {A5, A4, A3, A2, A1, A0, 12, 255, 255, 255, 255, 255, 255, 255, 255, 255}; //Pins, Switch activation sections A5 à A0 et D1//<-
#define OUTPUT_LED_NORMAL //comment out if use relay for switch leds On/AogConnected
//#define EEPROM_USE //comment out if not use EEPROM and AOG config machine
//#define WORK_WITHOUT_AOG //Permet d'utiliser le boitier sans aog connecté
#define PULSE_BY_100M 13000

#ifdef EEPROM_USE
#include <EEPROM.h>
#define EEP_Ident 0x5400

//Program counter reset
void(* resetFunc) (void) = 0;

//Variables for config - 0 is false
struct Config {
  uint8_t user1 = 0; //user defined values set in machine tab
  uint8_t user2 = 0;
  uint8_t user3 = 0;
  uint8_t user4 = 130;
};  Config aogConfig;   //4 bytes
#endif

//Variables:
const uint8_t loopTime = 100; //10hz
uint32_t lastTime = loopTime;
uint32_t currentTime = loopTime;

//Comm checks
uint8_t watchdogTimer = 12;      //make sure we are talking to AOG
uint8_t serialResetTimer = 0;   //if serial buffer is getting full, empty it

//Communication with AgOpenGPS
int16_t EEread = 0;

//speed sent as *10
float gpsSpeed = 0, hertz = 0;

//Parsing PGN
bool isPGNFound = false, isHeaderFound = false;
uint8_t pgn = 0, dataLength = 0;
int16_t tempHeader = 0;

//show life in AgIO
uint8_t helloAgIO[] = {0x80, 0x81, 0x7F, 0xED, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0x74 };
uint8_t helloCounter=0;
  
uint8_t AOG[] = {0x80, 0x81, 0x7F, 0xED, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0xCC };

//The variables used for storage
uint8_t relayHi=0, relayLo = 0; 

uint8_t count = 0;

boolean autoModeIsOn = false;
boolean manuelModeIsOn = false;
boolean aogConnected = false;
boolean firstConnection = true;
boolean relayIsActive = LOW;

uint8_t onLo = 0, offLo = 0, onHi = 0, offHi = 0, mainByte = 0;
//End of variables

void setup() {  
  delay(200); //wait for IO chips to get ready
  
  #ifdef EEPROM_USE
  EEPROM.get(0, EEread);              // read identifier
    
  if (EEread != EEP_Ident) {   // check on first start and write EEPROM
    EEPROM.put(0, EEP_Ident);
    EEPROM.put(6, aogConfig);
  } else { 
    EEPROM.get(6, aogConfig);
  }
  if (aogConfig.isRelayActiveHigh) { relayIsActive = HIGH; }
  #endif
  
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(relayPinArray[count], OUTPUT);
  }  
  pinMode(PinAogConnected, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(AutoSwitch, INPUT_PULLUP);  //INPUT_PULLUP: no external Resistor to GND or to PINx is needed, PULLUP: HIGH state if Switch is open! Connect to GND and D0/PD0/RXD
  pinMode(ManuelSwitch, INPUT_PULLUP);
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(switchPinArray[count], INPUT_PULLUP);
  }

  switchRelaisOff();
  
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(PinAogConnected, HIGH);

  Serial.begin(38400);  //set up communication
  while (!Serial) {
    // wait for serial port to connect. Needed for native USB
  }
} //end of setup

void loop() {
  currentTime = millis();
  if (currentTime - lastTime >= loopTime) {  //start timed loop
    lastTime = currentTime;
    
    #ifdef WORK_WITHOUT_AOG
    while (!digitalRead(ManuelSwitch)) {
      for (count = 0; count < NUM_OF_RELAYS; count++) {
        if (digitalRead(switchPinArray[count])) {
          digitalWrite(relayPinArray[count], !relayIsActive); //Relay OFF
        } else {
          digitalWrite(relayPinArray[count], relayIsActive); //Relay ON
        }
      }
      if (serialResetTimer < 100) watchdogTimer = serialResetTimer = 100;
      delay(20);
    }
    #endif
    
    //avoid overflow of watchdogTimer:
    if (watchdogTimer++ > 250) watchdogTimer = 12;
    
    //clean out serial buffer to prevent buffer overflow:
    if (serialResetTimer++ > 20) {
      while (Serial.available() > 0) Serial.read();
      serialResetTimer = 0;
    }
    
    if ((watchdogTimer > 20)) {
      if (aogConnected && watchdogTimer > 60) {
        aogConnected = false;
        firstConnection = true;
        #ifdef OUTPUT_LED_NORMAL
        digitalWrite(LED_BUILTIN, HIGH);
        #endif
        digitalWrite(PinAogConnected, HIGH);
      } else if (watchdogTimer > 240) digitalWrite(LED_BUILTIN, LOW);
    }
    
    //emergency off:
    if (watchdogTimer > 10) {
      switchRelaisOff();
      
      //show life in AgIO
      if (++helloCounter > 10) {
        Serial.write(helloAgIO,sizeof(helloAgIO));
        Serial.flush();   // flush out buffer
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
              } else {
                bitClear(offHi, count-8);
                bitSet(onHi, count-8);
              }
              digitalWrite(relayPinArray[count], relayIsActive); //Relay ON
            } else {
              if (count < 8) {
                bitSet(offLo, count);
                bitClear(onLo, count);
              } else {
                bitSet(offHi, count-8);
                bitClear(onHi, count-8);
              }
              digitalWrite(relayPinArray[count], !relayIsActive); //Relay OFF
            }
          }
        } else {//Mode off
          switchRelaisOff(); //All relays off!
        }
      } else if (!firstConnection) { //Mode Auto
        onLo = onHi = 0;
        for (count = 0; count < NUM_OF_RELAYS; count++) {
          if (digitalRead(switchPinArray[count])) {
            if (count < 8) {
              bitSet(offLo, count); //Info for AOG switch OFF
            } else {
              bitSet(offHi, count-8); //Info for AOG switch OFF
            }
            digitalWrite(relayPinArray[count], !relayIsActive); //Close the relay
          } else { //Signal LOW ==> switch is closed
            if (count < 8) {
              bitClear(offLo, count);
              digitalWrite(relayPinArray[count], !bitRead(relayLo, count)); //Open or Close relayLo if AOG requests it in auto mode
            } else {
              bitClear(offHi, count-8); 
              digitalWrite(relayPinArray[count], !bitRead(relayHi, count-8)); //Open or Close  le relayHi if AOG requests it in auto mode
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
      AOG[11] = (uint8_t)onHi;
      AOG[12] = (uint8_t)offHi;
      
      //add the checksum
      int16_t CK_A = 0;
      for (uint8_t i = 2; i < sizeof(AOG)-1; i++)
      {
        CK_A = (CK_A + AOG[i]);
      }
      AOG[sizeof(AOG)-1] = CK_A;
      
      Serial.write(AOG,sizeof(AOG));
      Serial.flush();   // flush out buffer
    }
  }

  // Serial Receive
  //Do we have a match with 0x8081?    
  if (Serial.available() > 4 && !isHeaderFound && !isPGNFound) 
  {
    uint8_t temp = Serial.read();
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
  if (Serial.available() > 2 && isHeaderFound && !isPGNFound)
  {
    Serial.read(); //The 7F or less
    pgn = Serial.read();
    dataLength = Serial.read();
    isPGNFound = true;
    
    if (!aogConnected) {
      watchdogTimer = 12;
      #ifdef OUTPUT_LED_NORMAL
      digitalWrite(LED_BUILTIN, LOW);
      #else
      digitalWrite(LED_BUILTIN, HIGH);
      #endif
    }
  }

  //The data package
  if (Serial.available() > dataLength && isHeaderFound && isPGNFound)
  {
    if (pgn == 239) // EF Machine Data
    {
      Serial.read();
      Serial.read();
      Serial.read();
      Serial.read();
      Serial.read();   //high,low bytes
      Serial.read();
      
      relayLo = Serial.read();          // read relay control from AgOpenGPS
      relayHi = Serial.read();
      
      //Bit 13 CRC
      Serial.read();
      
      //reset watchdog
      watchdogTimer = 0;
  
      //Reset serial Watchdog
      serialResetTimer = 0;

      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn=dataLength=0;
      
      if (!aogConnected) {
        digitalWrite(PinAogConnected, LOW);
        aogConnected = true;
      }
    }
    /*else if (pgn == 254) {
      //bit 5,6
      gpsSpeed = ((float)(Serial.read()| Serial.read() << 8 )); // = Vitesse * 10
	  hertz = (gpsSpeed * PULSE_BY_100M) / 60 / 60; // = (pulsation par H) / min / s = Hertz
      
      //bit 7,8,9
      Serial.read();
      Serial.read();
      Serial.read();
      
      //Bit 10 Tram 
      Serial.read();
      
      //Bit 11 section 1 to 8
      //Bit 12 section 9 to 16
      relayLo = Serial.read();          // read relay control from AgOpenGPS
      relayHi = Serial.read();

      //Bit 13 CRC
      Serial.read();

      //Reset serial Watchdog
      serialResetTimer = 0;
      
      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn=dataLength=0;
    }*/
    #ifdef EEPROM_USE
    else if (pgn==238) { //EE Machine Settings
      Serial.read();
      Serial.read();
      Serial.read();
      Serial.read();
      
      aogConfig.user1 = Serial.read();
      aogConfig.user2 = Serial.read();
      aogConfig.user3 = Serial.read();
      aogConfig.user4 = Serial.read();
      
      //crc
      Serial.read();
  
      //save in EEPROM and restart
      EEPROM.put(6, aogConfig);
      resetFunc();

      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn = dataLength = 0;
    }
    #endif
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
  onLo = onHi = 0;
  offLo = offHi = 0b11111111;
}
