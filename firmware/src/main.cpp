#include <Arduino.h>
#include <esp_task_wdt.h>
#include "SystemController.h"
#include "WebAPI.h"

// Watchdog timeout: 10 seconds. If loop() stalls, the ESP32 reboots.
constexpr int WDT_TIMEOUT_S = 10;

SystemController controller;
WebAPI* webAPI = nullptr;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=================================");
    Serial.println("  CNC Stop Block — Starting Up");
    Serial.println("=================================");

    // #12: Initialize task watchdog timer
    // esp_task_wdt_config_t / struct API was added in IDF v5.
    // Use the IDF v4-compatible signature here (timeout_s, panic_on_timeout).
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    {
        esp_task_wdt_config_t wdt_cfg = {
            .timeout_ms  = (uint32_t)(WDT_TIMEOUT_S * 1000),
            .idle_core_mask = 0,
            .trigger_panic  = true
        };
        esp_task_wdt_init(&wdt_cfg);
    }
#else
    esp_task_wdt_init(WDT_TIMEOUT_S, /*panic=*/true);
#endif
    esp_task_wdt_add(NULL); // Subscribe current task (loopTask)

    controller.begin();

    webAPI = new WebAPI(&controller);
    webAPI->begin();
}

void loop() {
    esp_task_wdt_reset(); // Feed the watchdog
    controller.update();
    webAPI->update();
}
