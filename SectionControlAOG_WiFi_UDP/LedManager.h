enum { NO_CONNECTED, WIFI_CONFIG, WIFI_CONNECTED, AOG_CONNECTED, AOG_READY };
uint8_t statusLED = NO_CONNECTED;

void taskLed(void *parameter) {
  Serial.println("Task LED started");
  for (;;) {
    switch (statusLED) {
      case AOG_READY:
        digitalWrite(PinWiFiConnected, HIGH);
        digitalWrite(PinAogStatus, HIGH);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        break;
  
      case AOG_CONNECTED:
        digitalWrite(PinWiFiConnected, HIGH);
        digitalWrite(PinAogStatus, !digitalRead(PinAogStatus));
        vTaskDelay(500 / portTICK_PERIOD_MS);
        break;
  
      case WIFI_CONFIG:
        digitalWrite(PinWiFiConnected, !digitalRead(PinWiFiConnected));
        digitalWrite(PinAogStatus, LOW);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        break;
  
      case WIFI_CONNECTED:
        digitalWrite(PinWiFiConnected, HIGH);
        digitalWrite(PinAogStatus, LOW);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        break;
  
      case NO_CONNECTED:
        digitalWrite(PinWiFiConnected, !digitalRead(PinWiFiConnected));
        digitalWrite(PinAogStatus, LOW);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        break;
    }
  }
}
