#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include "SSD1306.h"
#include <BH1750.h>

// 定义引脚
#define LED_Green  14
#define LED_Yellow 12
#define LED_Red    13
#define DHT11_PIN  15

// 定义默认阈值
float LIGHT_THRESHOLD = 50.0;  // 光照阈值
float TEMP_THRESHOLD = 16.0;   // 温度阈值
float HUMI_THRESHOLD = 80.0;   // 湿度阈值

// 初始化对象
SSD1306 display(0x3c, 5, 4);
DHT dht(DHT11_PIN, DHT11);
BH1750 lightMeter;

// LED手动控制标志
bool manualControl[3] = {false, false, false}; // 绿、黄、红
bool manualState[3] = {false, false, false};   // 绿、黄、红

// LED闪烁函数
void blinkLED(int pin) {
    for(int i = 0; i < 2; i++) {
        digitalWrite(pin, HIGH);
        delay(200);
        digitalWrite(pin, LOW);
        delay(200);
    }
}

// 处理串口命令
void processSerialCommand() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.startsWith("L")) {  // 光照阈值设置
            float newValue = cmd.substring(1).toFloat();
            if (newValue > 0) {
                LIGHT_THRESHOLD = newValue;
                Serial.printf("Light threshold set to: %.1f\n", LIGHT_THRESHOLD);
            }
        }
        else if (cmd.startsWith("T")) {  // 温度阈值设置
            float newValue = cmd.substring(1).toFloat();
            if (newValue > 0) {
                TEMP_THRESHOLD = newValue;
                Serial.printf("Temperature threshold set to: %.1f\n", TEMP_THRESHOLD);
            }
        }
        else if (cmd.startsWith("H")) {  // 湿度阈值设置
            float newValue = cmd.substring(1).toFloat();
            if (newValue > 0) {
                HUMI_THRESHOLD = newValue;
                Serial.printf("Humidity threshold set to: %.1f\n", HUMI_THRESHOLD);
            }
        }
        else if (cmd.length() == 2) {  // LED控制命令
            int ledIndex = cmd.charAt(0) - '0';
            int state = cmd.charAt(1) - '0';
            
            if (ledIndex >= 0 && ledIndex <= 2 && (state == 0 || state == 1)) {
                manualControl[ledIndex] = true;
                manualState[ledIndex] = (state == 1);
                
                const char* ledNames[] = {"Green", "Yellow", "Red"};
                Serial.printf("Manual control: %s LED %s\n", 
                            ledNames[ledIndex], 
                            state ? "ON" : "OFF");
            }
        }
    }
}

// LED初始化动画
void ledInitAnimation() {
    digitalWrite(LED_Red, HIGH);
    delay(500);
    digitalWrite(LED_Red, LOW);
    
    digitalWrite(LED_Yellow, HIGH);
    delay(500);
    digitalWrite(LED_Yellow, LOW);
    
    digitalWrite(LED_Green, HIGH);
    delay(500);
    digitalWrite(LED_Green, LOW);
}

void setup() {
    Serial.begin(115200);
    Wire.begin(5, 4);
    
    pinMode(LED_Green, OUTPUT);
    pinMode(LED_Yellow, OUTPUT);
    pinMode(LED_Red, OUTPUT);
    
    digitalWrite(LED_Green, LOW);
    digitalWrite(LED_Yellow, LOW);
    digitalWrite(LED_Red, LOW);
    
    dht.begin();
    lightMeter.begin();
    
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    
    ledInitAnimation();
    
    // 打印初始阈值
    Serial.println("\nCurrent thresholds:");
    Serial.printf("Light: %.1f lx\n", LIGHT_THRESHOLD);
    Serial.printf("Temperature: %.1f°C\n", TEMP_THRESHOLD);
    Serial.printf("Humidity: %.1f%%\n", HUMI_THRESHOLD);
}

void loop() {
    processSerialCommand();
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float lux = lightMeter.readLightLevel();
    
    // 自动控制逻辑
    bool autoGreen = (t > TEMP_THRESHOLD);
    bool autoYellow = (h > HUMI_THRESHOLD);
    bool autoRed = (lux < LIGHT_THRESHOLD);
    
    // LED控制逻辑
    if (manualControl[0]) {  // 绿灯
        if (manualState[0] != autoGreen) {
            blinkLED(LED_Green);
            manualControl[0] = false;
        } else {
            digitalWrite(LED_Green, manualState[0] ? HIGH : LOW);
        }
    } else {
        digitalWrite(LED_Green, autoGreen ? HIGH : LOW);
    }
    
    if (manualControl[1]) {  // 黄灯
        if (manualState[1] != autoYellow) {
            blinkLED(LED_Yellow);
            manualControl[1] = false;
        } else {
            digitalWrite(LED_Yellow, manualState[1] ? HIGH : LOW);
        }
    } else {
        digitalWrite(LED_Yellow, autoYellow ? HIGH : LOW);
    }
    
    if (manualControl[2]) {  // 红灯
        if (manualState[2] != autoRed) {
            blinkLED(LED_Red);
            manualControl[2] = false;
        } else {
            digitalWrite(LED_Red, manualState[2] ? HIGH : LOW);
        }
    } else {
        digitalWrite(LED_Red, autoRed ? HIGH : LOW);
    }

    Serial.printf("Temp:%.1f°C  Humi:%.1f%%  Light:%.1f lx\n", t, h, lux);

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Sensor Data");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 20, "Temp: " + String(t, 1) + "°C");
    display.drawString(0, 35, "Humi: " + String(h, 1) + "%");
    display.drawString(0, 50, "Light: " + String(lux, 1) + " lx");
    display.display();
    
    delay(500);
}