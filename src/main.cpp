#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include "SSD1306.h"

#define LED_Green  14
#define LED_Yellow 12
#define LED_Red    13
#define DHT11_PIN  15

// 初始化OLED (0x3c为I2C地址, GPIO5为SDA, GPIO4为SCL)
SSD1306 display(0x3c, 5, 4);
DHT dht(DHT11_PIN, DHT11);

void setup() {
    Serial.begin(115200);
    
    // LED初始化
    pinMode(LED_Green, OUTPUT);
    pinMode(LED_Yellow, OUTPUT);
    pinMode(LED_Red, OUTPUT);
    
    // DHT11初始化
    dht.begin();
    
    // OLED初始化
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
}

void loop() {
    // LED控制
    digitalWrite(LED_Green, HIGH);
    digitalWrite(LED_Yellow, HIGH);
    digitalWrite(LED_Red, HIGH);

    // 读取温湿度
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // 串口输出
    Serial.printf("Temp:%.1f°C  Humi:%.1f%%\n", t, h);

    // OLED显示
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    // 显示Hello
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Hello");
    
    // 显示温湿度
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 20, "Temp: " + String(t, 1) + "°C");
    display.drawString(0, 35, "Humi: " + String(h, 1) + "%");
    
    display.display();
    
    delay(500);
}