    /* 18/04/2025 - Daniel Desmartins
    *  Connected to the Relay Port in AgOpenGPS
    *  If you find any mistakes or have an idea to improove the code, feel free to contact me. N'hésitez pas à me contacter en cas de problème ou si vous avez une idée d'amélioration.
    */
#define VERSION 2.80

//pins:                                                                                           UPDATE YOUR PINS!!!    //<-
#define NUM_OF_RELAYS 8 //8 relays                                                                  //<-
#define PinWiFiConnected 23 //Pin WiFI Conntected                                                                           //<-
#define PinAogStatus 2 //Pin AOG Conntected                                                                           //<-
#define AutoSwitch 34  //Switch Mode Auto On/Off //Warning!! external pullup! connected this pin to a 10Kohms resistor connected to 3.3v.                                                                        //<-
#define ManualSwitch 35 //Switch Mode Manual On/Off //Warning!! external pullup! connected this pin to a 10Kohms resistor connected to 3.3v.                                                                      //<-
#define WorkWithoutAogSwitch 0 //Switch for work without AOG (optional)                                                 //<-
const uint8_t relayPinArray[] = { 32, 33, 25, 26, 27, 14, 12, 13 };  //Pins for Relays                                //<-
const uint8_t switchPinArray[] = { 4, 16, 17, 5, 18, 19, 21, 22 }; //Pins, Switch activation sections          //<-

//#define WORK_WITHOUT_AOG //Allows to use the box without aog connected (optional)
bool relayIsActive = HIGH; //Replace HIGH with LOW if your relays don't work the way you want

//Variable for speed:
#define PinOutputImpuls 15                                                                                                                                 //<-
#define PULSE_BY_100M 13000

#include <EEPROM.h>
const uint16_t EEPROM_SIZE = 256;
#define EEP_Ident 0x3378 
int16_t EEread = 0;

//Variables for config - 0 is false
struct Config {
  uint8_t raiseTime = 2;
  uint8_t lowerTime = 4;
  uint8_t enableToolLift = 0;
  uint8_t isRelayActiveHigh = 0; //if zero, active low (default)
  
  uint8_t user1 = 0; //user defined values set in machine tab
  uint8_t user2 = 0;
  uint8_t user3 = 0;
  uint8_t user4 = 130;
};  Config aogConfig;   //4 bytes

uint8_t fonction[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };
bool fonctionState[] = { false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false };

//WiFi
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
IPAddress localIP(0, 0, 0, 123);
IPAddress udpAddress(0, 0, 0, 255);
const int udpPort = 9999;
const int udpLocalPort = 8888;
bool WiFiConnected = false;

WiFiUDP udp;
WiFiMulti wifiMulti;

//Variables:
const uint8_t loopTime = 100; //10hz
uint32_t lastTime = loopTime;
uint32_t currentTime = loopTime;
uint32_t lastTimeWifiConnected = loopTime;

//Comm checks
enum { NO_CONNECTED, WIFI_CONNECTED, AOG_CONNECTED, AOG_READY };
uint8_t statusLED = NO_CONNECTED;
uint32_t lastTimeLED = loopTime;
uint8_t watchdogTimer = 12;     //make sure we are talking to AOG
uint8_t wifiResetTimer = 0;     //if Wifi buffer is getting full, empty it

//Communication with AgOpenGPS
uint8_t AOG[] = { 0x80, 0x81, 0x7B, 0xEA, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0xCC };
char udpData[14];

//hello from AgIO
uint8_t helloFromMachine[] = { 128, 129, 123, 123, 5, 0, 0, 0, 0, 0, 71 };

//speed sent as *10
float gpsSpeed = 0, hertz = 0;
uint16_t pulseBy100m = PULSE_BY_100M;

//The variables used for storage
uint8_t sectionLo = 0, sectionHi = 0, tramline = 0, hydLift = 0, geoStop = 0;
uint8_t raiseTimer = 0, lowerTimer = 0, lastTrigger = 0;
bool isRaise = false;
bool isLower = false;
uint8_t count = 0;

bool autoModeIsOn = false;
bool manualModeIsOn = false;
bool aogConnected = false;
bool firstConnection = true;

uint8_t onLo = 0, offLo = 0, onHi = 0, offHi = 0, mainByte = 0;
//End of variables

void setup() {
  //Pin Initialization
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    pinMode(relayPinArray[count], OUTPUT);
  }
  pinMode(PinWiFiConnected, OUTPUT);
  pinMode(PinAogStatus, OUTPUT);
  pinMode(PinOutputImpuls, OUTPUT);
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
  Serial.println("");
  Serial.println("");
  Serial.println("Firmware : SectionControlAOG_WiFi_UDP");
  Serial.print("Version : ");
  Serial.println(VERSION);
  
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, EEread);              // read identifier

  if (EEread != EEP_Ident) {   // check on first start and write EEPROM
    EEPROM.put(0, EEP_Ident);
    EEPROM.put(6, aogConfig);
    EEPROM.put(20, fonction);
    EEPROM.put(50, localIP);
    EEPROM.commit();
  } else {
    EEPROM.get(6, aogConfig);
    EEPROM.get(20, fonction);
    EEPROM.get(50, localIP);
  }
  
  //WiFi Setup
  wifiMulti.addAP("ssid1", "password1"); // <-- enter your login/password
  wifiMulti.addAP("ssid2", "password2");
  wifiMulti.addAP("ssid3", "password3");
  wifiMulti.addAP("ssid4", "password4");
  WiFi.mode(WIFI_STA);
  WiFi.config(localIP);
  
  if (aogConfig.isRelayActiveHigh) { relayIsActive = HIGH; }
  pulseBy100m = aogConfig.user4 * 100;

  currentTime = millis();
} //end of setup

void loop() {
  currentTime = millis();
  if (currentTime - lastTime >= loopTime) {  //start timed loop
    lastTime = currentTime;
    
    #ifdef WORK_WITHOUT_AOG
    while (!analogRead(WorkWithoutAogSwitch)) {
      for (count = 0; count < NUM_OF_RELAYS; count++) {
        if (digitalRead(switchPinArray[count]) || (digitalRead(AutoSwitch) && digitalRead(ManualSwitch))) {
          fonctionState[count] = false; //Section OFF
        } else {
          fonctionState[count] =  true; //Section ON
        }
      }
      if (serialResetTimer < 100) watchdogTimer = serialResetTimer = 100;
      delay(20);
    }
    #endif
    
    //WiFi Run
    startWiFi();
    
    //clean out WiFi buffer to prevent buffer overflow:
    if (wifiResetTimer++ > 20) {
      while (udp.available() > 0) udp.read();
      wifiResetTimer = 0;
      noTone(PinOutputImpuls);
    }
    
    //avoid overflow of watchdogTimer:
    if (watchdogTimer++ > 250) watchdogTimer = 12;
        
    if (watchdogTimer > 20) {
      if (aogConnected && watchdogTimer > 60) {
        aogConnected = false;
        firstConnection = true;
        statusLED = AOG_CONNECTED;
      }
    }
    
    //emergency off:
    if (watchdogTimer > 10) {
      switchRelaisOff(); //All relays off!
    } else {
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
              } else {
                bitClear(offHi, count-8);
                bitSet(onHi, count-8);
              }
              fonctionState[count] =  true; //Section ON
            } else {
              if (count < 8) {
                bitSet(offLo, count);
                bitClear(onLo, count);
              } else {
                bitSet(offHi, count-8);
                bitClear(onHi, count-8);
              }
              fonctionState[count] = false; //Section OFF
            }
          }
        } else { //Mode off
          switchRelaisOff(); //All relays off!
        }
      } else if (!firstConnection) { //Mode Auto
        onLo = onHi = 0;
        for (count = 0; count < NUM_OF_RELAYS; count++) {
          if (digitalRead(switchPinArray[count])) {
            digitalWrite(relayPinArray[count], !relayIsActive); //Close the relay
            if (count < 8) {
              bitSet(offLo, count); //Info for AOG switch OFF
            } else {
              bitSet(offHi, count-8); //Info for AOG switch OFF
            }
          } else { //Signal LOW ==> switch is closed
            if (count < 8) {
              bitClear(offLo, count);
              fonctionState[count] = bitRead(sectionLo, count); //Open or Close sectionLo if AOG requests it in auto mode
            } else {
              bitClear(offHi, count - 8);
              fonctionState[count] = bitRead(sectionHi, count-8); //Open or Close  le sectionHi if AOG requests it in auto mode
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
      
      udp.beginPacket(udpAddress, udpPort);
      udp.write(AOG, sizeof(AOG));
      udp.endPacket();
      udp.flush();
    }
    //hydraulic lift

    if (hydLift != lastTrigger && (hydLift == 1 || hydLift == 2))
    {
        lastTrigger = hydLift;
        lowerTimer = 0;
        raiseTimer = 0;

        //100 msec per frame so 10 per second
        switch (hydLift)
        {
            //lower
        case 1:
            lowerTimer = aogConfig.lowerTime * 10;
            break;

            //raise
        case 2:
            raiseTimer = aogConfig.raiseTime * 10;
            break;
        }
    }

    //countdown if not zero, make sure up only
    if (raiseTimer)
    {
        raiseTimer--;
        lowerTimer = 0;
    }
    if (lowerTimer) lowerTimer--;

    //if anything wrong, shut off hydraulics, reset last
    if ((hydLift != 1 && hydLift != 2) || watchdogTimer > 10) //|| gpsSpeed < 2)
    {
        lowerTimer = 0;
        raiseTimer = 0;
        lastTrigger = 0;
    }

    if (aogConfig.isRelayActiveHigh)
    {
        isLower = isRaise = false;
        if (lowerTimer) isLower = true;
        if (raiseTimer) isRaise = true;
    }
    else
    {
        isLower = isRaise = true;
        if (lowerTimer) isLower = false;
        if (raiseTimer) isRaise = false;
    }

    //set sections
    setSection();
    //set LEDs
    setLed();
  }
  
  //UDP Receive
  udp.parsePacket();   // Size of packet to receive
  if (udp.read(udpData, 14)) {
    if (udpData[0] == 0x80 && udpData[1] == 0x81 && udpData[2] == 0x7F) //Data
    {
      if (udpData[3] == 239)  //machine data
      {            
        hydLift = udpData[7];
        tramline = udpData[8];  //bit 0 is right bit 1 is left
        geoStop = udpData[9];

        sectionLo = udpData[11];          // read relay control from AgOpenGPS
        sectionHi = udpData[12];

        if (aogConfig.isRelayActiveHigh)
        {
            tramline = 255 - tramline;
            sectionLo = 255 - sectionLo;
            sectionHi = 255 - sectionHi;
        }
        
        //reset watchdog
        watchdogTimer = 0;
        
        //Reset WiFi Watchdog
        wifiResetTimer = 0;
        
        if (!aogConnected) {
          statusLED = AOG_READY;
          aogConnected = true;
        }
      }
      else if (udpData[3] == 200) // Hello from AgIO
      {
        if (udpData[7] == 1)
        {
          sectionLo -= 255;
          sectionHi -= 255;
          watchdogTimer = 12;
        }
        
        helloFromMachine[5] = sectionLo;
        helloFromMachine[6] = sectionHi;
        
        udp.beginPacket(udpAddress, udpPort);
        udp.write(helloFromMachine, sizeof(helloFromMachine));
        udp.endPacket();
        udp.flush();

        if (statusLED != AOG_READY) statusLED = AOG_CONNECTED;
      }
      else if (udpData[3] == 201)
      {
          //make really sure this is the subnet pgn
          if (udpData[4] == 5 && udpData[5] == 201 && udpData[6] == 201)
          {
              localIP[0] = udpData[7];
              localIP[1] = udpData[8];
              localIP[2] = udpData[9];
              
              Serial.print("\r\n localIP Changed to: ");
              Serial.println(localIP);

              //save in EEPROM and restart
              EEPROM.put(50, localIP);
              EEPROM.commit();
              esp_restart();
          }
      }
      else if (udpData[3] == 202)
      {
        //make really sure this is the subnet pgn
        if (udpData[4] == 3 && udpData[5] == 202 && udpData[6] == 202)
        {
          //hello from AgIO
          uint8_t scanReply[] = { 128, 129, 123, 203, 7, 
                        localIP[0], localIP[1], localIP[2], localIP[3],
                        localIP[0], localIP[1], localIP[2], 23 };
          
          //checksum
          int16_t CK_A = 0;
          for (uint8_t i = 2; i < sizeof(scanReply) - 1; i++)
          {
            CK_A = (CK_A + scanReply[i]);
          }
          scanReply[sizeof(scanReply) - 1] = CK_A;
          
          static uint8_t ipDest[] = { 255,255,255,255 };
          
          //off to AOG
          udp.beginPacket(ipDest, udpPort);
          udp.write(scanReply, sizeof(scanReply));
          udp.endPacket();
          udp.flush();
        }
      }
      else if (udpData[3] == 254)
      {
        gpsSpeed = ((float)(udpData[5] | udpData[6] << 8 )); // = Vitesse * 10
        hertz = (gpsSpeed * pulseBy100m) / 60 / 60; // = (pulsation par H) / min / s = Hertz
        if (hertz > 39) tone(PinOutputImpuls, hertz);
        else noTone(PinOutputImpuls);
        
        //Reset WiFi Watchdog
        wifiResetTimer = 0;
      }
      else if (udpData[3] == 238)
      {        
        //set1 
        uint8_t sett = udpData[8];  //setting0     
        if (bitRead(sett, 0)) aogConfig.isRelayActiveHigh = 1; else aogConfig.isRelayActiveHigh = 0;
        
        aogConfig.user1 = udpData[9];
        aogConfig.user2 = udpData[10];
        aogConfig.user3 = udpData[11];
        aogConfig.user4 = udpData[12];
        
        //save in EEPROM and restart
        EEPROM.put(6, aogConfig);
        EEPROM.commit();
        
        //Reset WiFi Watchdog
        wifiResetTimer = 0;
      }
      else if (udpData[3] == 236) //Sections Settings 
      {
          for (uint8_t i = 0; i < 16; i++)
          {
              fonction[i] = udpData[i + 5];
          }

          //save in EEPROM and restart
          EEPROM.put(20, fonction);
          EEPROM.commit();
      }
    }
  }
} //end of main loop

void switchRelaisOff() {  //that are the relais, switch all off
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    fonctionState[count] = false;
  }
  onLo = onHi = 0;
  offLo = offHi = 0b11111111;
}

void setSection() {
  fonctionState[16] = isLower;
  fonctionState[17] = isRaise;
  
  //Tram
  fonctionState[18] = bitRead(tramline, 0); //right
  fonctionState[19] = bitRead(tramline, 1); //left
  
  //GeoStop
  fonctionState[20] =  geoStop;
  
  for (count = 0; count < NUM_OF_RELAYS; count++) {
    digitalWrite(relayPinArray[count], (fonctionState[fonction[count] - 1] == relayIsActive));
  }
}


void setLed() { //NO_CONNECTED, WIFI_CONNECTED, AOG_CONNECTED, AOG_READY  
  switch (statusLED) {
    case AOG_READY:
      digitalWrite(PinWiFiConnected, HIGH);
      digitalWrite(PinAogStatus, HIGH);
      break;

    case AOG_CONNECTED:
      digitalWrite(PinWiFiConnected, HIGH);
      if (currentTime - lastTimeLED >= 500) {
        lastTimeLED = currentTime;
        digitalWrite(PinAogStatus, !digitalRead(PinAogStatus));
      }
      break;

    case WIFI_CONNECTED:
      digitalWrite(PinWiFiConnected, HIGH);
      digitalWrite(PinAogStatus, LOW);
      break;

    case NO_CONNECTED:
    //case default:
      if (currentTime - lastTimeLED >= 500) {
        lastTimeLED = currentTime;
        digitalWrite(PinWiFiConnected, !digitalRead(PinWiFiConnected));
      }
      digitalWrite(PinAogStatus, LOW);
      break;
  }
}

void startWiFi() {
  if ((WiFi.status() != WL_CONNECTED) && (currentTime - lastTimeWifiConnected >= 10000)) {
    lastTimeWifiConnected = currentTime;

    if (WiFiConnected) esp_restart();
    
    Serial.println("");
    Serial.println("WiFi connection...");

    if (wifiMulti.run() == WL_CONNECTED)
    {
      Serial.println("");
      Serial.println("Wi-Fi connected");
      Serial.print("IP adress : ");
      localIP = WiFi.localIP();
      Serial.println(localIP);
      udpAddress = localIP;
      udpAddress[3] = 255;
      udp.begin(udpLocalPort);
      statusLED = WIFI_CONNECTED;
      WiFiConnected = true;
      pinMode(25, OUTPUT); //Wifi disables this pin on startup
    }
  }
}
