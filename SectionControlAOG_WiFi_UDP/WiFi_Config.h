#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>

IPAddress myIP(192, 168, 1, 123);
IPAddress udpAddress(0, 0, 0, 255);
const int udpPort = 9999;
const int udpLocalPort = 8888;

WiFiMulti wifiMulti;
WiFiUDP udp;
WebServer server(80);
DNSServer dnsServer;

#define MAX_SSID 4
struct WiFiNetwork { char ssid[32]; char pass[64]; };

#define WIFI_EEPROM_START EEPROM_SIZE
#define WIFI_EEPROM_HEADER (sizeof(uint16_t) + sizeof(uint8_t))
#define WIFI_EEPROM_SIZE (WIFI_EEPROM_HEADER + MAX_SSID * sizeof(WiFiNetwork))
#define EEP_WIFI_ID 0xC0FF

bool WiFiConfig = false;
uint8_t configuredNetworks = MAX_SSID;
unsigned long configStartTime = 0;
bool pageVisited = false;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 1000;
unsigned long wifiDisconnectedSince = 0;

void loadNetworks(WiFiNetwork networks[], uint8_t& count) {
  uint16_t ident = 0;
  EEPROM.get(WIFI_EEPROM_START, ident);
  if (ident != EEP_WIFI_ID) {
    count = 0;
    return;
  }
  EEPROM.get(WIFI_EEPROM_START + sizeof(uint16_t), count);
  if (count > MAX_SSID) count = MAX_SSID;
  int addr = WIFI_EEPROM_START + WIFI_EEPROM_HEADER;
  for (uint8_t i = 0; i < count; i++) {
    EEPROM.get(addr, networks[i]);
    addr += sizeof(WiFiNetwork);
  }
}

void saveNetworks(WiFiNetwork networks[], uint8_t count) {
  EEPROM.put(WIFI_EEPROM_START, EEP_WIFI_ID);
  EEPROM.put(WIFI_EEPROM_START + sizeof(uint16_t), count);
  int addr = WIFI_EEPROM_START + WIFI_EEPROM_HEADER;
  for (uint8_t i = 0; i < count; i++) {
    EEPROM.put(addr, networks[i]);
    addr += sizeof(WiFiNetwork);
  }

  for (uint8_t i = count; i < MAX_SSID; i++) {
    WiFiNetwork empty = { "", "" };
    EEPROM.put(addr, empty);
    addr += sizeof(WiFiNetwork);
  }
  
  EEPROM.commit();
}

void clearNetworks() {
  EEPROM.put(WIFI_EEPROM_START, (uint16_t)0);
  EEPROM.commit();
}

void initWebConfig() {
  WiFi.softAP("Section Control WiFi Config");
  IPAddress ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ip);

  Serial.println("==> Configuration mode enabled");
  Serial.print("IP : "); Serial.println(ip);

  server.on("/", HTTP_GET, []() {
    pageVisited = true;
    WiFiNetwork networks[MAX_SSID];
    uint8_t count = 0;
    loadNetworks(networks, count);

    String page = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>";
    page += "<title>Config WiFi</title><style>body{font-family:sans-serif;max-width:600px;margin:auto;padding:20px;}input,select{width:100%;margin-bottom:10px;padding:8px;}button{padding:10px;margin-top:10px;width:100%;}</style>";
    page += "<script>function setFields(){const n=document.getElementById('nb').value;for(let i=0;i<5;i++){document.getElementById('row'+i).style.display=(i<n)?'block':'none';}}</script>";
    page += "</head><body><h2>WiFi Configuration</h2><form method='POST' action='/save'>";
    page += "<label>Number of networks :</label><select name='count' id='nb' onchange='setFields()'>";
    for (int i = 1; i <= MAX_SSID; i++) {
      page += "<option value='" + String(i) + "'" + (i == count ? " selected" : "") + ">" + String(i) + "</option>";
    }
    page += "</select>";

    for (int i = 0; i < MAX_SSID; i++) {
      String ssid = (i < count) ? networks[i].ssid : "";
      String pass = (i < count) ? networks[i].pass : "";
      page += "<div id='row" + String(i) + "' style='display:" + (i < count ? "block" : "none") + ";'>";
      page += "SSID " + String(i + 1) + ": <input name='ssid" + String(i) + "' value='" + ssid + "'>";
      page += "Password: <input name='pass" + String(i) + "' value='" + pass + "' type='password'></div>";
    }
    page += "<button type='submit'>Save</button></form>";
    page += "<form action='/reset' method='POST'><button style='background:#c00;color:#fff;' type='submit'>Reset WiFi</button></form>";
    page += "</body><script>setFields();</script></html>";
    server.send(200, "text/html", page);
  });

  server.on("/save", HTTP_POST, []() {
    WiFiNetwork networks[MAX_SSID];
    
    if (server.hasArg("count")) {
      configuredNetworks = server.arg("count").toInt();
      if (configuredNetworks > MAX_SSID) configuredNetworks = MAX_SSID;
    }
    
    for (uint8_t i = 0; i < configuredNetworks; i++) {
      String s = server.arg("ssid" + String(i));
      String p = server.arg("pass" + String(i));
      s.toCharArray(networks[i].ssid, sizeof(networks[i].ssid));
      p.toCharArray(networks[i].pass, sizeof(networks[i].pass));
    }

    saveNetworks(networks, configuredNetworks);
    server.send(200, "text/html", "<html><body><h2>Saved. Restarting...</h2></body></html>");
    delay(1500);
    ESP.restart();
  });

  server.on("/reset", HTTP_POST, []() {
    clearNetworks();
    server.send(200, "text/html", "<html><body><h2>WiFi reset. Rebooting...</h2></body></html>");
    delay(1500);
    ESP.restart();
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='0; url=/'></head><body>Redirection...</body></html>");
  });

  server.begin();
  configStartTime = millis();
}

void handleWebConfig() {
  dnsServer.processNextRequest();
  server.handleClient();
}

bool connectFromEEPROM() {
  WiFiNetwork networks[MAX_SSID];
  uint8_t count = 0;
  loadNetworks(networks, count);

  if (count == 0) return false;
  for (uint8_t i = 0; i < count; i++) {
    wifiMulti.addAP(networks[i].ssid, networks[i].pass);
  }

  Serial.print("WiFi connection attempt...");
  unsigned long startAttempt = millis();
  WiFi.mode(WIFI_STA);
  WiFi.config(myIP);
  while (millis() - startAttempt < 120000) {
    if (wifiMulti.run() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected to " + WiFi.SSID());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nLogin failed.");
  return false;
}

void setupWiFi() {
  if (connectFromEEPROM()) {
    Serial.print("IP adress : ");
    myIP = WiFi.localIP();
    Serial.println(myIP);
    udpAddress = myIP;
    udpAddress[3] = 255;
    udp.begin(udpLocalPort);
    statusLED = WIFI_CONNECTED;
  } else {
    Serial.println("No connection possible. Booting in config mode.");
    WiFiConfig = true;
    statusLED = WIFI_CONFIG;
    initWebConfig();
  }
  pinMode(25, OUTPUT); //Wifi disables this pin on startup
}

bool loopWiFi() {
  if (WiFiConfig) {
    handleWebConfig();
    if (!pageVisited && (millis() - configStartTime > 120000)) ESP.restart();
    delay(10);
    return true;
  }
  if (millis() - lastWiFiCheck > wifiCheckInterval) {
    lastWiFiCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      wifiDisconnectedSince = 0;
    } else {
      if (wifiDisconnectedSince == 0) {
        wifiDisconnectedSince = millis();
      } else if (millis() - wifiDisconnectedSince > 60000) {
        ESP.restart();
      }
    }
  }
  return false;
}
#endif // WIFICONFIG_H
