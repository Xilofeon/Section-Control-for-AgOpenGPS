#ifndef PULSEGENERATOR_H
#define PULSEGENERATOR_H

#include <Arduino.h>
#include <esp_arduino_version.h>

#define PIN_PULSE_OUTPUT    15
#define PULSES_PER_100M     13000    // 13000 Norme ISO 11786
#define SPEED_THRESHOLD     1.0      // km/h
#define DUTY_CYCLE_PERCENT  50       // 20 à 80% Norme ISO 11786

static const float pulsesPerMeter = PULSES_PER_100M / 100.0;
static hw_timer_t* pulseTimer = NULL;
static volatile bool pulseState = false;
static bool isGenerating = false;

void IRAM_ATTR onPulseTimer() {
  pulseState = !pulseState;
  digitalWrite(PIN_PULSE_OUTPUT, pulseState);
}

static float calculateFrequency(float speedKmh) {
  return (speedKmh / 3.6) * pulsesPerMeter;
}

void setupPulseGenerator() {
  pinMode(PIN_PULSE_OUTPUT, OUTPUT);
  digitalWrite(PIN_PULSE_OUTPUT, LOW);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  pulseTimer = timerBegin(1000000);
  timerAttachInterrupt(pulseTimer, &onPulseTimer);
  timerStop(pulseTimer);
#else
  pulseTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(pulseTimer, &onPulseTimer, true);
  timerAlarmDisable(pulseTimer);
#endif
}

void updatePulseSpeed(float speedKmh10) {
  float speedKmh = speedKmh10 / 10.0;

  if (speedKmh < SPEED_THRESHOLD) {
    if (isGenerating) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
      timerStop(pulseTimer);
#else
      timerAlarmDisable(pulseTimer);
#endif
      digitalWrite(PIN_PULSE_OUTPUT, LOW);
      pulseState   = false;
      isGenerating = false;
    }
    return;
  }

  float freq = calculateFrequency(speedKmh);
  if (freq < 1.0) freq = 1.0;
  uint64_t halfPeriodUs = (uint64_t)(500000.0 / freq);
  if (halfPeriodUs < 10) halfPeriodUs = 10;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  timerAlarm(pulseTimer, halfPeriodUs, true, 0);
  if (!isGenerating) timerStart(pulseTimer);
#else
  timerAlarmWrite(pulseTimer, halfPeriodUs, true);
  if (!isGenerating) timerAlarmEnable(pulseTimer);
#endif

  isGenerating = true;
}

#endif // PULSEGENERATOR_H