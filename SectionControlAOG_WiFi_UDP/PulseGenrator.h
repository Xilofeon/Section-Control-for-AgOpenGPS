#ifndef PULSEGENERATOR_H
#define PULSEGENERATOR_H

#include <Arduino.h>
#include "driver/ledc.h"

// Configuration
#define PIN_PULSE_OUTPUT 15      // Output pin for pulses
#define PULSES_PER_100M 13000
#define SPEED_THRESHOLD 10.0     // Minimum speed threshold in km/h * 10
#define DUTY_CYCLE_PERCENT 20

// Variables
const float pulsesPerMeter = PULSES_PER_100M / 100.0;
float currentSpeed = 0.0;
bool isGenerating = false;

// Configuration timer LEDC
const ledc_channel_t ledcChannel = LEDC_CHANNEL_0;
const ledc_timer_t ledcTimer = LEDC_TIMER_0;
const uint8_t resolution = 15;
const ledc_mode_t speedMode = LEDC_LOW_SPEED_MODE;
const uint32_t max_duty = (1 << resolution) - 1;
const uint32_t duty = (max_duty * DUTY_CYCLE_PERCENT) / 100;

float calculateFrequency(float speedKmh) {
  float speedMs = speedKmh / 36;
  float pulsesPerSecond = speedMs * pulsesPerMeter;
  
  return pulsesPerSecond;
}

void setupPulseGenerator() {
  // Configuration du timer LEDC
  ledc_timer_config_t ledc_timer = {
    .speed_mode = speedMode,
    .duty_resolution = (ledc_timer_bit_t)resolution,
    .timer_num = ledcTimer,
    .freq_hz = 1,
    .clk_cfg = LEDC_AUTO_CLK,
    .deconfigure = false
  };
  ledc_timer_config(&ledc_timer);
  
  ledc_channel_config_t ledc_channel = {
    .gpio_num = PIN_PULSE_OUTPUT,
    .speed_mode = speedMode,
    .channel = ledcChannel,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = ledcTimer,
    .duty = 0,
    .hpoint = 0,
    .flags = 0
  };
  ledc_channel_config(&ledc_channel);
}

void updatePulseSpeed(float speedKmh) {
  currentSpeed = speedKmh;
  
  if (speedKmh <= SPEED_THRESHOLD) {
    if (isGenerating) {
      ledc_set_duty(speedMode, ledcChannel, 0);
      ledc_update_duty(speedMode, ledcChannel);
      isGenerating = false;
    }
    return;
  }
  
  float frequency = calculateFrequency(speedKmh);
  
  if (frequency < 1) frequency = 1;     // Minimum 1 Hz
  else if (frequency > 20000) frequency = 20000;  // max résolution 15Bit
  
  uint32_t freq = (uint32_t)frequency;
  
  esp_err_t freq_result = ledc_set_freq(speedMode, ledcTimer, freq);
  
  if (freq_result != ESP_OK) {
    Serial.print("Frequency change error: ");
    Serial.println(freq_result);
    return;
  }
  
  esp_err_t duty_result = ledc_set_duty(speedMode, ledcChannel, duty);
  if (duty_result == ESP_OK) {
    ledc_update_duty(speedMode, ledcChannel);
    isGenerating = true;
    
    Serial.print("Speed: ");
    Serial.print(speedKmh / 10.0);
    Serial.print(" km/h, Frequency: ");
    Serial.println(frequency);
  } else {
    Serial.print("Duty configuration error: ");
    Serial.println(duty_result);
  }
}

#endif // PULSEGENERATOR
