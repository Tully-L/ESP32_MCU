#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include "SSD1306.h"
#include <BH1750.h>
#include <Preferences.h>

// 定义引脚
#define LED_Green  14
#define LED_Yellow 12
#define LED_Red    13
#define DHT11_PIN  15

// 定义默认阈值
float LIGHT_THRESHOLD = 50.0;  
float TEMP_THRESHOLD = 16.0;   
float HUMI_THRESHOLD = 80.0;   

// 初始化对象
SSD1306 display(0x3c, 5, 4);
DHT dht(DHT11_PIN, DHT11);
BH1750 lightMeter;
Preferences preferences;

// LED控制标志
bool manualControl[3] = {false, false, false}; 
bool manualState[3] = {false, false, false};   

// 报警记录结构
struct AlarmRecord {
    float value;
    char type;  // 'T'=温度, 'H'=湿度, 'L'=光照
    uint32_t timestamp;
};

#define MAX_ALARMS 10
AlarmRecord alarmHistory[MAX_ALARMS];
int alarmCount = 0;

// 加载配置
void loadConfig() {
    preferences.begin("sensor-cfg", true);
    LIGHT_THRESHOLD = preferences.getFloat("light", 50.0);
    TEMP_THRESHOLD = preferences.getFloat("temp", 16.0);
    HUMI_THRESHOLD = preferences.getFloat("humi", 80.0);
    preferences.end();
}

// 保存配置
void saveConfig() {
    preferences.begin("sensor-cfg", false);
    preferences.putFloat("light", LIGHT_THRESHOLD);
    preferences.putFloat("temp", TEMP_THRESHOLD);
    preferences.putFloat("humi", HUMI_THRESHOLD);
    preferences.end();
}

// 加载报警历史
void loadAlarmHistory() {
    preferences.begin("alarm-hist", true);
    alarmCount = preferences.getInt("count", 0);
    if(alarmCount > 0) {
        preferences.getBytes("alarms", alarmHistory, sizeof(AlarmRecord) * alarmCount);
    }
    preferences.end();
}

// 添加报警记录
void addAlarmRecord(float value, char type) {
    // 移动现有记录
    for(int i = MAX_ALARMS-1; i > 0; i--) {
        alarmHistory[i] = alarmHistory[i-1];
    }
    
    // 添加新记录
    alarmHistory[0].value = value;
    alarmHistory[0].type = type;
    alarmHistory[0].timestamp = millis() / 1000;
    
    if(alarmCount < MAX_ALARMS) alarmCount++;
    
    // 保存到Flash
    preferences.begin("alarm-hist", false);
    preferences.putInt("count", alarmCount);
    preferences.putBytes("alarms", alarmHistory, sizeof(AlarmRecord) * alarmCount);
    preferences.end();
    
    // 打印报警信息
    Serial.printf("Alarm: %c=%.1f at %lu sec\n", type, value, alarmHistory[0].timestamp);
}

// LED闪烁
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
        
        if (cmd == "history") {  // 打印报警历史
            Serial.println("\nAlarm History:");
            for(int i = 0; i < alarmCount; i++) {
                Serial.printf("%d: %c=%.1f at %lu sec\n", 
                            i+1, 
                            alarmHistory[i].type,
                            alarmHistory[i].value,
                            alarmHistory[i].timestamp);
            }
        }
        else if (cmd.startsWith("L")) {
            float newValue = cmd.substring(1).toFloat();
            if (newValue > 0) {
                LIGHT_THRESHOLD = newValue;
                saveConfig();
                Serial.printf("Light threshold set to: %.1f\n", LIGHT_THRESHOLD);
            }
        }
        else if (cmd.startsWith("T")) {
            float newValue = cmd.substring(1).toFloat();
            if (newValue > 0) {
                TEMP_THRESHOLD = newValue;
                saveConfig();
                Serial.printf("Temperature threshold set to: %.1f\n", TEMP_THRESHOLD);
            }
        }
        else if (cmd.startsWith("H")) {
            float newValue = cmd.substring(1).toFloat();
            if (newValue > 0) {
                HUMI_THRESHOLD = newValue;
                saveConfig();
                Serial.printf("Humidity threshold set to: %.1f\n", HUMI_THRESHOLD);
            }
        }
        else if (cmd.length() == 2) {
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
    
    // 加载配置和报警历史
    loadConfig();
    loadAlarmHistory();
    
    ledInitAnimation();
    
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
    
    // 检查传感器数据并记录报警
    if(t > TEMP_THRESHOLD) {
        addAlarmRecord(t, 'T');
    }
    if(h > HUMI_THRESHOLD) {
        addAlarmRecord(h, 'H');
    }
    if(lux < LIGHT_THRESHOLD) {
        addAlarmRecord(lux, 'L');
    }
    
    // 自动控制逻辑
    bool autoGreen = (t > TEMP_THRESHOLD);
    bool autoYellow = (h > HUMI_THRESHOLD);
    bool autoRed = (lux < LIGHT_THRESHOLD);
    
    // LED控制逻辑
    if (manualControl[0]) {
        if (manualState[0] != autoGreen) {
            blinkLED(LED_Green);
            manualControl[0] = false;
        } else {
            digitalWrite(LED_Green, manualState[0] ? HIGH : LOW);
        }
    } else {
        digitalWrite(LED_Green, autoGreen ? HIGH : LOW);
    }
    
    if (manualControl[1]) {
        if (manualState[1] != autoYellow) {
            blinkLED(LED_Yellow);
            manualControl[1] = false;
        } else {
            digitalWrite(LED_Yellow, manualState[1] ? HIGH : LOW);
        }
    } else {
        digitalWrite(LED_Yellow, autoYellow ? HIGH : LOW);
    }
    
    if (manualControl[2]) {
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