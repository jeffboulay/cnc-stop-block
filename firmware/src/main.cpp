#include <Arduino.h>
#include "SystemController.h"
#include "WebAPI.h"

SystemController controller;
WebAPI* webAPI = nullptr;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=================================");
    Serial.println("  CNC Stop Block — Starting Up");
    Serial.println("=================================");

    controller.begin();

    webAPI = new WebAPI(&controller);
    webAPI->begin();
}

void loop() {
    controller.update();
    webAPI->update();
}
