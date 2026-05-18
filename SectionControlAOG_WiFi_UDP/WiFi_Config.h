#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>

#define AP_SERVER_IP      "192.168.1.1"
#define AP_SERVER_GW      "192.168.1.1"
#define AP_SERVER_SUBNET  "255.255.255.0"

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
#define EEP_WIFI_ID 0xC1FA

struct APConfig {
  bool    enabled;
  char    ssid[32];
  char    pass[64];
};

#define AP_EEPROM_START  (WIFI_EEPROM_START + WIFI_EEPROM_SIZE)
#define AP_EEPROM_HEADER sizeof(uint16_t)
#define AP_EEPROM_SIZE   (AP_EEPROM_HEADER + sizeof(APConfig))
#define EEP_AP_ID        EEP_WIFI_ID

// EEPROM.begin(EEPROM_SIZE + WIFI_EEPROM_SIZE + AP_EEPROM_SIZE)
#define TOTAL_EEPROM_SIZE (EEPROM_SIZE + WIFI_EEPROM_SIZE + AP_EEPROM_SIZE)

bool WiFiConfig = false;
bool WiFiAPServer = false;
uint8_t configuredNetworks = MAX_SSID;
unsigned long configStartTime = 0;
bool pageVisited = false;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 1000;
unsigned long wifiDisconnectedSince = 0;

#define MAX_AUTH_FAIL_COUNT 5
struct FailedAuth { uint8_t mac[6]; uint8_t count; };
FailedAuth failedClients[4];
uint8_t failedClientCount = 0;
bool forceConfigMode = false;

// Helper function to escape special characters for HTML output
String escapeHTML(String text) {
  text.replace("&", "&amp;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  text.replace("\"", "&quot;");
  text.replace("'", "&#039;");
  return text;
}

void loadNetworks(WiFiNetwork networks[], uint8_t& count) {
  uint16_t ident = 0;
  EEPROM.get(WIFI_EEPROM_START, ident);
  if (ident != EEP_WIFI_ID) { count = 0; return; }
  EEPROM.get(WIFI_EEPROM_START + sizeof(uint16_t), count);
  if (count > MAX_SSID) count = MAX_SSID;
  int addr = WIFI_EEPROM_START + WIFI_EEPROM_HEADER;
  for (uint8_t i = 0; i < count; i++) {
    EEPROM.get(addr, networks[i]);
    addr += sizeof(WiFiNetwork);
  }
}

void saveNetworks(WiFiNetwork networks[], uint8_t count) {
  EEPROM.put(WIFI_EEPROM_START, (uint16_t)EEP_WIFI_ID);
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
  int addr = WIFI_EEPROM_START + WIFI_EEPROM_HEADER;
  for (uint8_t i = 0; i < MAX_SSID; i++) {
    WiFiNetwork empty = { "", "" };
    EEPROM.put(addr, empty);
    addr += sizeof(WiFiNetwork);
  }
  EEPROM.commit();
}

void loadAPConfig(APConfig& cfg) {
  uint16_t ident = 0;
  EEPROM.get(AP_EEPROM_START, ident);
  if (ident != EEP_AP_ID) {
    cfg.enabled = false;
    strncpy(cfg.ssid, "SectionControl_AP", sizeof(cfg.ssid));
    strncpy(cfg.pass, "", sizeof(cfg.pass));
    return;
  }
  EEPROM.get(AP_EEPROM_START + AP_EEPROM_HEADER, cfg);
}

void saveAPConfig(APConfig& cfg) {
  EEPROM.put(AP_EEPROM_START, (uint16_t)EEP_AP_ID);
  EEPROM.put(AP_EEPROM_START + AP_EEPROM_HEADER, cfg);
  EEPROM.commit();
}

void serveConfigPage() {
  pageVisited = true;

  uint8_t n = WiFi.scanNetworks();

  WiFiNetwork networks[MAX_SSID];
  uint8_t savedCount = 0;
  loadNetworks(networks, savedCount);
  if (savedCount == 0) {
    savedCount = 1;
    for (int i = 0; i < MAX_SSID; i++) {
      networks[i].ssid[0] = '\0';
      networks[i].pass[0] = '\0';
    }
  }

  APConfig apCfg;
  loadAPConfig(apCfg);

  String checkedAP   = apCfg.enabled ? "checked" : "";
  String apSSID      = escapeHTML(String(apCfg.ssid));
  String apPass      = escapeHTML(String(apCfg.pass));
  String modeLabel   = WiFiAPServer ? " [MODE SERVEUR AP ACTIF]" : "";

  String html = R"rawliteral(
    <!DOCTYPE html><html><head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Configuration</title>
    <style>
      body { font-family: sans-serif; max-width: 600px; margin: auto; padding: 20px; }
      input, select, button { width: 100%; padding: 8px; margin: 6px 0; box-sizing: border-box; }
      input[type=checkbox] { width: auto; margin-right: 8px; }
      .net { border: 1px solid #ccc; border-radius: 6px; padding: 10px; margin: 8px 0; position: relative; }
      ul { list-style: none; padding: 0; }
      .btn-save  { background: #28a745; color: white; }
      .btn-reset { background: #dc3545; color: white; }
      .section-ap { border: 2px solid #007bff; border-radius: 8px; padding: 12px; margin: 16px 0; }
      .section-ap h3 { color: #007bff; margin-top: 0; }
      .ap-fields { display: none; }
      .badge { background:#007bff; color:white; padding:3px 8px; border-radius:4px; font-size:0.8em; }
      .slot-select {
        width: auto; min-width: 120px;
        position: absolute; top: 10px; right: 10px; z-index: 100;
        background-color: white; border: 1px solid #007bff;
      }
    </style>
    <script>
      const MAX_SSID = )rawliteral" + String(MAX_SSID) + R"rawliteral(;
      let savedCount = )rawliteral" + String(savedCount) + R"rawliteral(;

      function updateNetworkFields(count) {
        for (let i = 0; i < MAX_SSID; i++)
          document.getElementById('row' + i).style.display = i < count ? 'block' : 'none';
        savedCount = count;
      }

      function toggleAPFields() {
        const cb = document.getElementById('apEnabled');
        document.getElementById('apFields').style.display = cb.checked ? 'block' : 'none';
      }

      function chooseNetworkSlot(ssid, container) {
        const old = document.querySelector('.slot-select');
        if (old) old.remove();
        let select = document.createElement('select');
        select.className = 'slot-select';
        select.add(new Option('Cancel', -1));
        for (let i = 0; i < savedCount; i++)
          select.add(new Option('Slot ' + (i+1), i));
        select.selectedIndex = 0;
        select.addEventListener('change', function() {
          const idx = parseInt(this.value);
          if (idx >= 0 && idx < MAX_SSID) {
            document.getElementById("ssid" + idx).value = ssid;
            document.getElementById("pass" + idx).value = "";
            document.getElementById("pass" + idx).focus();
          }
          this.remove();
        });
        container.appendChild(select);
        select.focus();
        try { select.click(); } catch(e) {}
      }

      window.onload = function() {
        document.getElementById('nb').addEventListener('change', function() {
          updateNetworkFields(parseInt(this.value));
        });
        toggleAPFields();
      };
    </script>
    </head><body>
    <h2>WiFi Configuration )rawliteral" + modeLabel + R"rawliteral(</h2>
    <form method="POST" action="/save">
      <label>Number of networks:</label>
      <select name="count" id="nb">)rawliteral";

  for (int i = 1; i <= MAX_SSID; i++) {
    html += "<option value='" + String(i) + "'" + (i == savedCount ? " selected" : "") + ">"
            + String(i) + "</option>";
  }

  html += R"rawliteral(</select>)rawliteral";

  for (int i = 0; i < MAX_SSID; i++) {
    html += "<div id='row" + String(i) + "' style='display:" + (i < savedCount ? "block" : "none") + ";'>";
    html += "SSID " + String(i+1) + ": <input id='ssid" + String(i) + "' name='ssid" + String(i)
          + "' value='" + escapeHTML(String(networks[i].ssid)) + "'>";
    html += "Password : <input id='pass" + String(i) + "' name='pass" + String(i)
          + "' type='password' value='" + escapeHTML(String(networks[i].pass)) + "'></div>";
  }

  html += R"rawliteral(
      <button type="submit" class="btn-save">Save</button>
    </form>

    <div class="section-ap">
      <h3>&#128225; Mode Serveur AP</h3>
      <p style="font-size:0.9em;color:#555;">
        The ESP32 can create its own WiFi network.<br>
        ESP32 IP address:<strong>)rawliteral" + String(AP_SERVER_IP) + R"rawliteral(</strong>
      </p>
      <form method="POST" action="/saveap">
        <label>
          <input type="checkbox" id="apEnabled" name="apEnabled" value="1" )rawliteral"
          + checkedAP + R"rawliteral( onchange="toggleAPFields()">
          Enable AP Server Mode
        </label>
        <div id="apFields" class="ap-fields">
          Network name (SSID):
          <input type="text" name="apSsid" maxlength="31" value=")rawliteral" + apSSID + R"rawliteral(">
          Password (minimum 8 characters, empty = open network):
          <input type="password" name="apPass" maxlength="63" value=")rawliteral" + apPass + R"rawliteral(">
        </div>
        <button type="submit" class="btn-save">Save in AP mode</button>
      </form>
    </div>

    <!-- -- Reset -- -->
    <form method="POST" action="/reset">
      <button class="btn-reset" type="submit">Full WiFi Reset</button>
    </form>
    <hr>
    <h3>Available Networks</h3>
    <ul>)rawliteral";

  for (int i = 0; i < n; i++) {
    String ssid        = WiFi.SSID(i);
    String escapedSSID = escapeHTML(ssid);
    int    rssi        = WiFi.RSSI(i);
    html += "<li class='net'><strong>" + escapedSSID + "</strong> (" + String(rssi) + " dBm)";
    html += "<br><button onclick=\"chooseNetworkSlot('" + escapedSSID + "', this.parentNode)\">Select Network</button></li>";
  }
  if (n == 0) html += "<li>No networks found.</li>";

  html += R"rawliteral(</ul></body></html>)rawliteral";

  server.send(200, "text/html", html);
}

void initWebConfig() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Section Control WiFi Config");
  IPAddress ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ip);
  
  Serial.println("Mode Configuration");
  Serial.print("IP : "); Serial.println(ip);
  
  server.on("/",       HTTP_GET,  []() { serveConfigPage(); });
  server.on("/config", HTTP_GET,  []() { serveConfigPage(); });
  
  server.on("/save", HTTP_POST, []() {
    WiFiNetwork networks[MAX_SSID];
    
    if (server.hasArg("count")) {
      configuredNetworks = server.arg("count").toInt();
      if (configuredNetworks == 0) configuredNetworks = 1;
      if (configuredNetworks > MAX_SSID) configuredNetworks = MAX_SSID;
    }
    
    for (uint8_t i = 0; i < configuredNetworks; i++) {
      String s = server.arg("ssid" + String(i));
      String p = server.arg("pass" + String(i));
      if (s.length() >= sizeof(networks[i].ssid)) s = s.substring(0, sizeof(networks[i].ssid) - 1);
      if (p.length() >= sizeof(networks[i].pass)) p = p.substring(0, sizeof(networks[i].pass) - 1);
      s.toCharArray(networks[i].ssid, sizeof(networks[i].ssid));
      p.toCharArray(networks[i].pass, sizeof(networks[i].pass));
    }
    
    saveNetworks(networks, configuredNetworks);
    server.send(200, "text/html", "<html><body><h2>Saved. Restarting...</h2></body></html>");
    delay(1500);
    ESP.restart();
  });
  
  server.on("/saveap", HTTP_POST, []() {
    APConfig cfg;
    cfg.enabled = server.hasArg("apEnabled") && server.arg("apEnabled") == "1";
    String s = server.arg("apSsid");
    String p = server.arg("apPass");
    if (s.length() == 0) s = "SectionControl_AP";
    if (s.length() >= sizeof(cfg.ssid)) s = s.substring(0, sizeof(cfg.ssid) - 1);
    if (p.length() >= sizeof(cfg.pass)) p = p.substring(0, sizeof(cfg.pass) - 1);
    s.toCharArray(cfg.ssid, sizeof(cfg.ssid));
    p.toCharArray(cfg.pass, sizeof(cfg.pass));
    saveAPConfig(cfg);
    server.send(200, "text/html", "<html><body><h2>AP mode saved. Restart...</h2></body></html>");
    delay(1500);
    ESP.restart();
  });
  
  server.on("/reset", HTTP_POST, []() {
    clearNetworks();
    APConfig empty = { false, "SectionControl_AP", "" };
    saveAPConfig(empty);
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

void onApAuthFail(WiFiEvent_t event, WiFiEventInfo_t info) {
  uint8_t reason = info.wifi_ap_stadisconnected.reason;

  if (reason != WIFI_REASON_AUTH_FAIL && reason != WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) return;

  uint8_t* mac = info.wifi_ap_stadisconnected.mac;

  Serial.printf("Auth fail (reason %d) from %02X:%02X:%02X:%02X:%02X:%02X\n",
    reason, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  for (uint8_t i = 0; i < failedClientCount; i++) {
    if (memcmp(failedClients[i].mac, mac, 6) == 0) {
      failedClients[i].count++;
      Serial.printf("Client known : %d/%d fails\n", failedClients[i].count, MAX_AUTH_FAIL_COUNT);
      if (failedClients[i].count >= MAX_AUTH_FAIL_COUNT) {
        forceConfigMode = true;
      }
      return;
    }
  }

  if (failedClientCount < 4) {
    memcpy(failedClients[failedClientCount].mac, mac, 6);
    failedClients[failedClientCount].count = 1;
    failedClientCount++;
    Serial.println("New client tracked (1 fail)");
  }
}

void initAPServer(APConfig& cfg) {
  IPAddress apIP, apGW, apSN;
  apIP.fromString(AP_SERVER_IP);
  apGW.fromString(AP_SERVER_GW);
  apSN.fromString(AP_SERVER_SUBNET);

  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apGW, apSN);

  if (strlen(cfg.pass) >= 8) {
    WiFi.softAP(cfg.ssid, cfg.pass);
  } else {
    WiFi.softAP(cfg.ssid);
  }

  Serial.println("==> Active Server AP Mode");
  Serial.print("SSID : "); Serial.println(cfg.ssid);
  Serial.print("IP   : "); Serial.println(WiFi.softAPIP());

  myIP = apIP;
  myIP[3] = 1;
  udpAddress    = apIP;
  udpAddress[3] = 255;

  udp.begin(udpLocalPort);

  server.on("/",       HTTP_GET,  []() { serveConfigPage(); });
  server.on("/config", HTTP_GET,  []() { serveConfigPage(); });

  server.on("/saveap", HTTP_POST, []() {
    APConfig cfg2;
    cfg2.enabled = server.hasArg("apEnabled") && server.arg("apEnabled") == "1";
    String s = server.arg("apSsid");
    String p = server.arg("apPass");
    if (s.length() == 0) s = "SectionControl_AP";
    if (s.length() >= sizeof(cfg2.ssid)) s = s.substring(0, sizeof(cfg2.ssid) - 1);
    if (p.length() >= sizeof(cfg2.pass)) p = p.substring(0, sizeof(cfg2.pass) - 1);
    s.toCharArray(cfg2.ssid, sizeof(cfg2.ssid));
    p.toCharArray(cfg2.pass, sizeof(cfg2.pass));
    saveAPConfig(cfg2);
    server.send(200, "text/html", "<html><body><h2>AP mode saved. Restart...</h2></body></html>");
    delay(1500); ESP.restart();
  });

  server.on("/save", HTTP_POST, []() {
    WiFiNetwork networks[MAX_SSID];
    if (server.hasArg("count")) {
      configuredNetworks = server.arg("count").toInt();
      if (configuredNetworks == 0) configuredNetworks = 1;
      if (configuredNetworks > MAX_SSID) configuredNetworks = MAX_SSID;
    }
    for (uint8_t i = 0; i < configuredNetworks; i++) {
      String s = server.arg("ssid" + String(i));
      String p = server.arg("pass" + String(i));
      if (s.length() >= sizeof(networks[i].ssid)) s = s.substring(0, sizeof(networks[i].ssid) - 1);
      if (p.length() >= sizeof(networks[i].pass)) p = p.substring(0, sizeof(networks[i].pass) - 1);
      s.toCharArray(networks[i].ssid, sizeof(networks[i].ssid));
      p.toCharArray(networks[i].pass, sizeof(networks[i].pass));
    }
    saveNetworks(networks, configuredNetworks);
    server.send(200, "text/html", "<html><body><h2>Save. Restart...</h2></body></html>");
    delay(1500); ESP.restart();
  });

  server.on("/reset", HTTP_POST, []() {
    clearNetworks();
    APConfig empty = { false, "SectionControl_AP", "agopengps" };
    saveAPConfig(empty);
    server.send(200, "text/html", "<html><body><h2>Reset. Restart...</h2></body></html>");
    delay(1500); ESP.restart();
  });
  
  WiFi.onEvent(onApAuthFail, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
  
  server.begin();
}

bool connectFromEEPROM() {
  WiFiNetwork networks[MAX_SSID];
  uint8_t count = 0;
  loadNetworks(networks, count);
  if (count == 0) return false;

  for (uint8_t i = 0; i < count; i++)
    wifiMulti.addAP(networks[i].ssid, networks[i].pass);
  
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
  APConfig apCfg;
  loadAPConfig(apCfg);
  
  if (apCfg.enabled) {
    WiFiAPServer = true;
    statusLED    = WIFI_CONFIG;
    initAPServer(apCfg);
  } else if (connectFromEEPROM()) {
    Serial.print("IP adress : ");
    myIP = WiFi.localIP();
    Serial.println(myIP);
    udpAddress = myIP;
    udpAddress[3] = 255;
    udp.begin(udpLocalPort);
  } else {
    Serial.println("No connection possible. Booting in config mode.");
    WiFiConfig = true;
    statusLED = WIFI_CONFIG;
    initWebConfig();
  }
  pinMode(25, OUTPUT); //Wifi disables this pin on startup
}

void handleWebConfig() {
  dnsServer.processNextRequest();
  server.handleClient();
}

bool loopWiFi() {
  if (WiFiConfig) {
    handleWebConfig();
    if (!pageVisited && (millis() - configStartTime > 120000)) ESP.restart();
    delay(10);
    return true;
  }
  
  if (WiFiAPServer) {
    if (forceConfigMode) {
      Serial.println("Too many auth failures -> switching to config mode");
      forceConfigMode   = false;
      failedClientCount = 0;
      server.stop();
      WiFi.removeEvent(ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
      WiFi.softAPdisconnect(true);
      delay(100);
      WiFiAPServer = false;
      WiFiConfig   = true;
      pageVisited  = false;
      statusLED    = WIFI_CONFIG;
      initWebConfig();
      return true;
    }

    if (statusLED < AOG_CONNECTED) {
        server.handleClient();
    }
    if (millis() - lastWiFiCheck > wifiCheckInterval) {
      lastWiFiCheck = millis();
      uint8_t clients = WiFi.softAPgetStationNum();
      if (clients == 0) {
        statusLED = NO_CONNECTED;
      } else if (clients > 0 && statusLED < WIFI_CONNECTED) {
        statusLED = WIFI_CONNECTED;
      }
    }
    return false;
  }
  
  if (millis() - lastWiFiCheck > wifiCheckInterval) {
    lastWiFiCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      wifiDisconnectedSince = 0;
      if (statusLED < WIFI_CONNECTED)
        statusLED = WIFI_CONNECTED;
    } else {
      if (wifiDisconnectedSince == 0) {
        wifiDisconnectedSince = millis();
      } else if (millis() - wifiDisconnectedSince > 30000) {
        ESP.restart();
      } else {
        statusLED = NO_CONNECTED;
      }
    }
  }
  return false;
}

#endif // WIFICONFIG_H