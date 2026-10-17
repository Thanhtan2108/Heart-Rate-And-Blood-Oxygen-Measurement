#include <Arduino.h>

// Chu kỳ in thông báo (ms)
static const uint32_t kPrintIntervalMs = 1000;

static uint32_t lastPrintMs = 0;
static uint32_t bootCounter = 0;

void setup() {
  Serial.begin(115200);
  // Chờ cầu USB-UART ổn định để không mất dòng đầu tiên
  delay(500);
  Serial.println();
  Serial.println(F("=== Boot test: ESP32 DOIT DEVKIT V1 ==="));
}

void loop() {
  const uint32_t now = millis();
  // Dùng millis() thay vì delay() để làm quen kiểu non-blocking
  if (now - lastPrintMs >= kPrintIntervalMs) {
    lastPrintMs = now;
    bootCounter++;
    Serial.printf("boot OK #%lu | uptime %lu ms\n", (unsigned long)bootCounter, (unsigned long)now);
  }
}
