#include <Arduino.h>
#include "SystemController.h"
#include "WebUI.h"

SystemController controller;
WebUI* webUI = nullptr;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=================================");
    Serial.println("  CNC Stop Block — Starting Up");
    Serial.println("=================================");

    controller.begin();

    webUI = new WebUI(&controller);
    webUI->begin();
}

void loop() {
    controller.update();
    webUI->update();
}
