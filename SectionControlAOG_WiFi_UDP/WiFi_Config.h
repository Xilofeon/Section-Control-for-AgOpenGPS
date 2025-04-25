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
  int addr = WIFI_EEPROM_START + WIFI_EEPROM_HEADER;
  for (uint8_t i = 0; i < MAX_SSID; i++) {
    WiFiNetwork empty = { "", "" };
    EEPROM.put(addr, empty);
    addr += sizeof(WiFiNetwork);
  }

  EEPROM.commit();
}

void initWebConfig() {
  // Start AP and captive DNS
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Section Control WiFi Config");
  IPAddress ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ip);

  Serial.println("==> Configuration mode enabled");
  Serial.print("IP : "); Serial.println(ip);

  server.on("/", HTTP_GET, []() {
    pageVisited = true;
    
    // Scan for available networks
    uint8_t n = WiFi.scanNetworks();
    // Load any previously saved networks
    WiFiNetwork networks[MAX_SSID];
    uint8_t savedCount = 0;
    loadNetworks(networks, savedCount);

    // If reset, ensure at least one slot and clear all network entries
    if (savedCount == 0) {
      savedCount = 1;
      for (int i = 0; i < MAX_SSID; i++) {
        networks[i].ssid[0] = '\0';
        networks[i].pass[0] = '\0';
      }
    }

    // Build HTML
    String html = R"rawliteral(
      <!DOCTYPE html><html><head>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>WiFi Configuration</title>
      <style>
        body { font-family: sans-serif; max-width: 600px; margin: auto; padding: 20px; }
        input, select, button { width: 100%; padding: 8px; margin: 6px 0; }
        .net { border: 1px solid #ccc; border-radius: 6px; padding: 10px; margin: 8px 0; position: relative; }
        ul { list-style: none; padding: 0; }
        .btn-save { background: #28a745; color: white; }
        .slot-select { 
          width: auto; 
          min-width: 120px;
          position: absolute; 
          top: 10px; 
          right: 10px; 
          z-index: 100;
          background-color: white;
          border: 1px solid #007bff;
        }
      </style>
      <script>
        const MAX_SSID = )rawliteral" + String(MAX_SSID) + R"rawliteral(;
        let savedCount = )rawliteral" + String(savedCount) + R"rawliteral(;

        function updateNetworkFields(count) {
          for (let i = 0; i < MAX_SSID; i++) {
            document.getElementById('row' + i).style.display = i < count ? 'block' : 'none';
          }
          savedCount = count;
        }

        function chooseNetworkSlot(ssid, container) {
          // Remove any existing selector
          const old = document.querySelector('.slot-select');
          if (old) old.remove();
        
          // Build dropdown
          let select = document.createElement('select');
          select.className = 'slot-select';
          select.autofocus = true;
          
          // Add Cancel option first
          select.add(new Option('Cancel', -1));
          
          // Then add the slots
          for (let i = 0; i < savedCount; i++) {
            select.add(new Option('Slot ' + (i+1), i));
          }
        
          // Set Cancel as the default selected option
          select.selectedIndex = 0;
        
          // Handle selection
          select.addEventListener('change', function() {
            const idx = parseInt(this.value);
            if (idx >= 0 && idx < MAX_SSID) {
              document.getElementById("ssid" + idx).value = ssid;
              document.getElementById("pass" + idx).value = "";
              document.getElementById("pass" + idx).focus();
            }
            this.remove();
          });
          
          // Add to container and open immediately
          container.appendChild(select);
          select.focus();
          
          // Try multiple methods to open the dropdown
          try {
            // Try the click method
            select.click();
            
            // Alternative method for some browsers
            var event = new MouseEvent('mousedown', {
              view: window,
              bubbles: true,
              cancelable: true
            });
            select.dispatchEvent(event);
          } catch(e) {
            console.log("Could not auto-open dropdown:", e);
          }
        }
        
        window.onload = function() {
          document.getElementById('nb').addEventListener('change', function() {
            updateNetworkFields(parseInt(this.value));
          });
        };
      </script>
      </head><body>
      <h2>WiFi Configuration</h2>
      <form method="POST" action="/save">
        <label>Number of networks:</label>
        <select name="count" id="nb">)rawliteral";

    // Options for number of networks
    for (int i = 1; i <= MAX_SSID; i++) {
      html += "<option value='" + String(i) + "'" + (i == savedCount ? " selected" : "") + ">"
              + String(i) + "</option>";
    }
    
    html += R"rawliteral(
        </select>)rawliteral";

    // Input fields for each slot - Escape special characters in saved values
    for (int i = 0; i < MAX_SSID; i++) {
      html += "<div id='row" + String(i) + "' style='display:" + ((i < savedCount) ? "block" : "none") + ";'>";
      html += "SSID " + String(i+1) + ": <input id='ssid" + String(i) + "' name='ssid" + String(i) +
              "' value='" + escapeHTML(String(networks[i].ssid)) + "'>";
      html += "Password: <input id='pass" + String(i) + "' name='pass" + String(i) +
              "' type='password' value='" + escapeHTML(String(networks[i].pass)) + "'></div>";
    }
    
    html += R"rawliteral(
        <button type="submit" class="btn-save">Save</button>
      </form>
      <form method="POST" action="/reset">
        <button style="background:#dc3545; color:#fff;" type="submit">Reset WiFi</button>
      </form>
      <hr>
      <h3>Available Networks</h3>
      <ul>)rawliteral";

    // List scanned networks - Escape special characters in SSIDs
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      String escapedSSID = escapeHTML(ssid);
      int rssi = WiFi.RSSI(i);
      html += "<li class='net'>";
      html += "<strong>" + escapedSSID + "</strong> (" + String(rssi) + " dBm)";
      html += "<br><button onclick=\"chooseNetworkSlot('" + escapedSSID + "', this.parentNode)\">"
              "Select Network</button></li>";
    }
    if (n == 0) {
      html += "<li>No networks found.</li>";
    }

    html += R"rawliteral(
      </ul>
      </body></html>)rawliteral";

    server.send(200, "text/html", html);
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
      
      if (s.length() >= sizeof(networks[i].ssid)) {
        s = s.substring(0, sizeof(networks[i].ssid) - 1);
      }
      if (p.length() >= sizeof(networks[i].pass)) {
        p = p.substring(0, sizeof(networks[i].pass) - 1);
      }
      
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
