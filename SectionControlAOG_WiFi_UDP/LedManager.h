#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include "driver/ledc.h"

#define LED_BRIGHTNESS_PERCENT 8  // LED intensity (1.00-100%)

enum { NO_CONNECTED, WIFI_CONFIG, WIFI_CONNECTED, AOG_CONNECTED, AOG_READY };
uint8_t statusLED = NO_CONNECTED;

const ledc_channel_t ledChannel_WiFi = LEDC_CHANNEL_1;
const ledc_channel_t ledChannel_Aog = LEDC_CHANNEL_2;
const ledc_timer_t ledTimer = LEDC_TIMER_1;
const uint8_t ledResolution = 13;
const ledc_mode_t ledSpeedMode = LEDC_LOW_SPEED_MODE;
const uint32_t ledMaxDuty = (1 << ledResolution) - 1;

uint32_t applyGamma(float brightness) {
  if (brightness < 1) return 1;
  if (brightness >= 100) return ledMaxDuty;
  float corrected = pow(brightness / 100.0, 2.2);
  uint32_t duty = (uint32_t)(corrected * ledMaxDuty);
  if (duty < 1) return 1;
  return duty;
}

const uint32_t ledDutyOn = applyGamma(LED_BRIGHTNESS_PERCENT);
const uint32_t ledDutyOff = 0;

void setupLedPWM() {
  ledc_timer_config_t ledc_timer_led = {
    .speed_mode = ledSpeedMode,
    .duty_resolution = (ledc_timer_bit_t)ledResolution,
    .timer_num = ledTimer,
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK,
    .deconfigure = false
  };
  ledc_timer_config(&ledc_timer_led);
  
  ledc_channel_config_t ledc_channel_wifi = {
    .gpio_num = PinWiFiConnected,
    .speed_mode = ledSpeedMode,
    .channel = ledChannel_WiFi,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = ledTimer,
    .duty = 0,
    .hpoint = 0,
    .flags = 0
  };
  ledc_channel_config(&ledc_channel_wifi);
  
  ledc_channel_config_t ledc_channel_aog = {
    .gpio_num = PinAogStatus,
    .speed_mode = ledSpeedMode,
    .channel = ledChannel_Aog,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = ledTimer,
    .duty = 0,
    .hpoint = 0,
    .flags = 0
  };
  ledc_channel_config(&ledc_channel_aog);
  
  Serial.println("LED PWM initialized (Timer 1, Channels 1-2)");
}

void setLedWiFi(bool state) {
  uint32_t duty = state ? ledDutyOn : ledDutyOff;
  ledc_set_duty(ledSpeedMode, ledChannel_WiFi, duty);
  ledc_update_duty(ledSpeedMode, ledChannel_WiFi);
}

void setLedAog(bool state) {
  uint32_t duty = state ? ledDutyOn : ledDutyOff;
  ledc_set_duty(ledSpeedMode, ledChannel_Aog, duty);
  ledc_update_duty(ledSpeedMode, ledChannel_Aog);
}

void toggleLedWiFi() {
  static bool wifiLedState = false;
  wifiLedState = !wifiLedState;
  setLedWiFi(wifiLedState);
}

void toggleLedAog() {
  static bool aogLedState = false;
  aogLedState = !aogLedState;
  setLedAog(aogLedState);
}

void taskLed(void *parameter) {
  Serial.println("Task LED started");
  
  setupLedPWM();
  
  for (;;) {
    switch (statusLED) {
      case AOG_READY:
        setLedWiFi(true);
        setLedAog(true);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        break;
  
      case AOG_CONNECTED:
        setLedWiFi(true);
        toggleLedAog();
        vTaskDelay(900 / portTICK_PERIOD_MS);
        break;
  
      case WIFI_CONFIG:
        toggleLedWiFi();
        setLedAog(false);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        break;
  
      case WIFI_CONNECTED:
        setLedWiFi(true);
        setLedAog(false);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        break;
  
      case NO_CONNECTED:
        toggleLedWiFi();
        setLedAog(false);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        break;
    }
  }
}

#endif // LEDMANAGER_H
