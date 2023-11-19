#define VERSION 2.52
    /* 19/11/2023 - Daniel Desmartins
    *  Connected to the Relay Port in AgOpenGPS
    *  If you find any mistakes or have an idea to improove the code, feel free to contact me. N'hésitez pas à me contacter en cas de problème ou si vous avez une idée d'amélioration.
    */

//pins:
#define NUM_OF_RELAYS 7 //7 relays max for Arduino Nano with LED. If 8 sections are used the control LEDs are deactivated.
const uint8_t relayPinArray[] = {2, 3, 4, 5, 6, 7, 8, 9};  //Pins, Relays, D2 à D9
#define PinAogReady 9 //Pin AOG Conntected
#define AutoSwitch 10  //Switch Mode Auto On/Off
#define ManuelSwitch 11 //Switch Mode Manuel On/Off
const uint8_t switchPinArray[] = {A5, A4, A3, A2, A1, A0, 12, 13}; //Pins, Switch activation sections A5 to A0 and D12, D13
boolean relayIsActive = LOW; //Replace LOW with HIGH if your relays don't work the way you want
boolean readyIsActive = HIGH;

//Variables:
const uint8_t loopTime = 100; //10hz
uint32_t lastTime = loopTime;
uint32_t currentTime = loopTime;

//Comm checks
uint8_t watchdogTimer = 12;      //make sure we are talking to AOG
uint8_t serialResetTimer = 0;   //if serial buffer is getting full, empty it

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

boolean autoModeIsOn = false;
boolean manuelModeIsOn = false;
boolean aogConnected = false;
boolean firstConnection = true;
boolean initWorkWithoutAog = false;
boolean workWithoutAog = false;

uint8_t onLo = 0, offLo = 0, onHi = 0, offHi = 0, mainByte = 0;
//End of variables

void setup() {
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(relayPinArray[count], OUTPUT);
  }
  #if NUM_OF_RELAYS < 8
  pinMode(PinAogReady, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  #endif
  pinMode(AutoSwitch, INPUT_PULLUP);  //INPUT_PULLUP: no external Resistor to GND or to PINx is needed, PULLUP: HIGH state if Switch is open! Connect to GND and D0/PD0/RXD
  pinMode(ManuelSwitch, INPUT_PULLUP);
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(switchPinArray[count], INPUT_PULLUP);
  }
  
  switchRelaisOff();

  #if NUM_OF_RELAYS < 8
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(PinAogReady, !readyIsActive);
  #endif
  
  delay(100); //wait for IO chips to get ready
  
  Serial.begin(38400);  //set up communication
  while (!Serial) {
    // wait for serial port to connect. Needed for native USB
  }
  Serial.println("Firmware : SectionControlAOG");
  Serial.print("Version : ");
  Serial.println(VERSION);

  //code for whitout AOG
  if (!digitalRead(ManuelSwitch)) {
    initWorkWithoutAog = true;
    for (count = 0; count < NUM_OF_RELAYS; count++) {
      if (!digitalRead(switchPinArray[count])) {
        initWorkWithoutAog = false;
      }
    }
  }//end code for whitout AOG
} //end of setup

void loop() {
  currentTime = millis();
  if (currentTime - lastTime >= loopTime) {  //start timed loop
    lastTime = currentTime;
    
    //code for whitout AOG
    if (initWorkWithoutAog) {
      if (Serial.available() || digitalRead(ManuelSwitch)) initWorkWithoutAog = false;
      for (count = 0; count < NUM_OF_RELAYS; count++) {
        if (!digitalRead(switchPinArray[count])) {
          initWorkWithoutAog = false;
        }
      }
      
      if (!(watchdogTimer % 7)) digitalWrite(PinAogReady, !digitalRead(PinAogReady));
      
      if (watchdogTimer > 245) {
        initWorkWithoutAog = false;
        workWithoutAog = true;
        #if NUM_OF_RELAYS < 8
        digitalWrite(PinAogReady, readyIsActive);
        #endif
      }
    }
    
    while (workWithoutAog) {
      for (count = 0; count < NUM_OF_RELAYS; count++) {
        if (digitalRead(switchPinArray[count]) || (digitalRead(AutoSwitch) && digitalRead(ManuelSwitch))) {
          digitalWrite(relayPinArray[count], !relayIsActive); //Relay OFF
        } else {
          digitalWrite(relayPinArray[count], relayIsActive); //Relay ON
        }
      }
      delay(20);
      if (Serial.available()) workWithoutAog = false;
    }//end code for whitout AOG
    
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
        #if NUM_OF_RELAYS < 8
        digitalWrite(LED_BUILTIN, LOW);
        digitalWrite(PinAogReady, !readyIsActive);
      } else if (watchdogTimer > 240) {
        digitalWrite(LED_BUILTIN, LOW);
        #endif
      }
    }
    
    //emergency off:
    if (watchdogTimer > 10) {
      switchRelaisOff();
      
      //show life in AgIO
      if (++helloCounter > 10 && !helloUDP) {
        Serial.write(helloAgIO, sizeof(helloAgIO));
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
        } else { //Mode off
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
              digitalWrite(relayPinArray[count], (bitRead(relayLo, count) == relayIsActive)); //Open or Close relayLo if AOG requests it in auto mode
            } else {
              bitClear(offHi, count-8); 
              digitalWrite(relayPinArray[count], (bitRead(relayHi, count - 8) == relayIsActive)); //Open or Close relayHi if AOG requests it in auto mode
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
      
      Serial.write(AOG, sizeof(AOG));
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
      #if NUM_OF_RELAYS < 8
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
        #if NUM_OF_RELAYS < 8
        digitalWrite(PinAogReady, readyIsActive);
        #endif
        aogConnected = true;
      }
    }
    else if (pgn == 200) // Hello from AgIO
    {
      helloUDP = true;
      
      Serial.read(); //Version
      Serial.read();
      
      if (Serial.read())
      {
        relayLo -= 255;
        relayHi -= 255;
        watchdogTimer = 0;
      }
	  
      //crc
      Serial.read();
      
      helloFromMachine[5] = relayLo;
      helloFromMachine[6] = relayHi;
      
      Serial.write(helloFromMachine, sizeof(helloFromMachine));
      
      //reset for next pgn sentence
      isHeaderFound = isPGNFound = false;
      pgn = dataLength = 0;
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
  onLo = onHi = 0;
  offLo = offHi = 0b11111111;
}
