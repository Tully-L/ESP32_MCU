#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include "SSD1306.h"
#include <BH1750.h>
#include <Preferences.h>
#include <queue>

// 定义引脚
#define LED_Green  14
#define LED_Yellow 12
#define LED_Red    13
#define DHT11_PIN  15

// 定义默认阈值和采集周期
#define SAMPLE_PERIOD 2  // 采集周期(秒)
#define QUEUE_SIZE 30    // 数据队列大小
#define MAX_ALARMS 10    // 最大报警记录数

// 阈值结构体
struct Thresholds {
    float temp_high = 30.0;
    float temp_low = 9.0;
    float humi_high = 95.0;
    float humi_low = 20.0;
    float light_high = 1000.0;
    float light_low = 10.0;
};

// 数据点结构
struct DataPoint {
    float value;
    uint32_t timestamp;
};

// 报警记录结构
struct AlarmRecord {
    float value;
    char type;  // 'T'=温度, 'H'=湿度, 'L'=光照
    uint32_t timestamp;
};

// 显示模式枚举
enum DisplayMode {
    REALTIME_DATA = 0,
    TREND_GRAPH,
    ALARM_DATA,
    PARAMETERS,
    MAX_MODES
};

// 全局变量
Thresholds thresholds;
DisplayMode currentMode = REALTIME_DATA;
bool autoScroll = false;
unsigned long lastScrollTime = 0;
const unsigned long SCROLL_INTERVAL = 3000; // 自动滚动间隔(ms)

// 数据队列
std::queue<DataPoint> tempQueue;
std::queue<DataPoint> humiQueue;
std::queue<DataPoint> lightQueue;

// 报警历史
AlarmRecord alarmHistory[MAX_ALARMS];
int alarmCount = 0;

// 初始化对象
SSD1306 display(0x3c, 5, 4);
DHT dht(DHT11_PIN, DHT11);
BH1750 lightMeter;
Preferences preferences;

// LED控制标志
bool manualControl[3] = {false, false, false}; 
bool manualState[3] = {false, false, false};   

// 加载配置
void loadConfig() {
    preferences.begin("thresholds", true);
    thresholds.temp_high = preferences.getFloat("temp_h", 30.0);
    thresholds.temp_low = preferences.getFloat("temp_l", 9.0);
    thresholds.humi_high = preferences.getFloat("humi_h", 95.0);
    thresholds.humi_low = preferences.getFloat("humi_l", 20.0);
    thresholds.light_high = preferences.getFloat("light_h", 1000.0);
    thresholds.light_low = preferences.getFloat("light_l", 10.0);
    preferences.end();
}

// 保存配置
void saveConfig() {
    preferences.begin("thresholds", false);
    preferences.putFloat("temp_h", thresholds.temp_high);
    preferences.putFloat("temp_l", thresholds.temp_low);
    preferences.putFloat("humi_h", thresholds.humi_high);
    preferences.putFloat("humi_l", thresholds.humi_low);
    preferences.putFloat("light_h", thresholds.light_high);
    preferences.putFloat("light_l", thresholds.light_low);
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
}

// LED闪烁函数
void blinkLED(int pin) {
    for(int i = 0; i < 2; i++) {
        digitalWrite(pin, HIGH);
        delay(200);
        digitalWrite(pin, LOW);
        delay(200);
    }
}

// 更新数据队列
void updateDataQueues(float t, float h, float lux) {
    DataPoint dp;
    dp.timestamp = millis();
    
    // 更新温度队列
    dp.value = t;
    if (tempQueue.size() >= QUEUE_SIZE) {
        tempQueue.pop();
    }
    tempQueue.push(dp);
    
    // 更新湿度队列
    dp.value = h;
    if (humiQueue.size() >= QUEUE_SIZE) {
        humiQueue.pop();
    }
    humiQueue.push(dp);
    
    // 更新光照队列
    dp.value = lux;
    if (lightQueue.size() >= QUEUE_SIZE) {
        lightQueue.pop();
    }
    lightQueue.push(dp);
}


// 在每个显示函数末尾添加页面编号显示
void drawPageNumber() {
    display.setColor(WHITE);  // 使用白色显示页码
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 54, String(currentMode));
}

// 绘制实时数据界面
void drawRealtimeData(float t, float h, float lux) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    
    // 标题使用白色
    display.setColor(WHITE);
    display.drawString(0, 0, "Real-time Data");
    
    // 绘制大边框
    display.drawRect(0, 14, 100, 48);  // 从y=14开始，高度48像素的大边框
    
    // 温度显示
    display.setColor(WHITE);
    display.drawString(5, 16, "T: " + String(t, 1) + " C");
    
    // 湿度显示
    display.drawString(5, 32, "H: " + String(h, 1) + " %");
    
    // 光照显示
    display.drawString(5, 48, "L: " + String(lux, 1) + " lx");
    
    // 显示页码（右下角）
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 54, String(currentMode));
    
    display.display();
}

void drawTrendGraph() {
    display.clear();
    display.setColor(WHITE);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Trend");
    
    // 添加图例
    display.drawString(40, 0, "T-");
    display.drawString(70, 0, "H:");
    display.drawString(100, 0, "L.");
    
    // 绘制坐标轴和刻度
    display.drawHorizontalLine(10, 60, 118); // X轴
    display.drawVerticalLine(10, 10, 50);    // Y轴
    
    // 绘制刻度
    for(int i = 0; i < 5; i++) {
        display.drawHorizontalLine(8, 10 + i * 10, 4);  // Y轴刻度
        display.drawVerticalLine(10 + i * 25, 58, 4);   // X轴刻度
    }
    
    // 绘制曲线
    std::queue<DataPoint> tempQueueCopy = tempQueue;
    int x = 10;
    int lastY_T = 0, lastY_H = 0, lastY_L = 0;
    bool first = true;
    
    std::queue<DataPoint> humiQueueCopy = humiQueue;
    std::queue<DataPoint> lightQueueCopy = lightQueue;
    
    while (!tempQueueCopy.empty() && x < 128) {
        DataPoint dp_t = tempQueueCopy.front();
        DataPoint dp_h = humiQueueCopy.front();
        DataPoint dp_l = lightQueueCopy.front();
        
        tempQueueCopy.pop();
        humiQueueCopy.pop();
        lightQueueCopy.pop();
        
        int y_t = map(dp_t.value, thresholds.temp_low, thresholds.temp_high, 55, 15);
        int y_h = map(dp_h.value, thresholds.humi_low, thresholds.humi_high, 55, 15);
        int y_l = map(dp_l.value, thresholds.light_low, thresholds.light_high, 55, 15);
        
        if (!first) {
            // 温度曲线（实线）
            display.drawLine(x-2, lastY_T, x, y_t);  // x间距减半
            display.fillCircle(x, y_t, 1);
            
            // 湿度曲线（虚线）
            if ((x/2) % 2 == 0) {  // 虚线间距调整
                display.drawLine(x-2, lastY_H, x, y_h);
            }
            display.fillCircle(x, y_h, 1);
            
            // 光照曲线（点线）
            display.drawLine(x-2, lastY_L, x, y_l);
            display.fillCircle(x, y_l, 1);
        }
        
        lastY_T = y_t;
        lastY_H = y_h;
        lastY_L = y_l;
        x += 2; 
        first = false;
    }
    
    drawPageNumber();
    display.display();
}

// 绘制报警数据
void drawAlarmData() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    // 标题
    display.drawString(0, 0, "Alarm Records");
    
    // 绘制表格边框
    display.drawRect(0, 12, 130, 38);  // 主表格框
    
    // 绘制表头分隔线
    display.drawHorizontalLine(0, 25, 130);
    
    // 表头
    display.drawString(5, 13, "Type");
    display.drawString(35, 13, "Value");
    display.drawString(85, 13, "Time(m)");
    
    // 显示报警记录
    int y = 24;
    for (int i = 0; i < min(3, alarmCount); i++) {
        display.drawString(5, y, String(alarmHistory[i].type));
        display.drawString(35, y, String(alarmHistory[i].value, 1));
        display.drawString(85, y, String(alarmHistory[i].timestamp/60));
        y += 12;
    }
    
    drawPageNumber();
    display.display();  // 确保显示更新
}

// 绘制参数设置
void drawParameters() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    // 标题
    display.drawString(0, 0, "Parameters");
    
    // 采集周期
    display.drawString(0, 12, "Sample: " + String(SAMPLE_PERIOD) + " s");
    
    // 温度阈值
    display.drawString(0, 24, "Temp:");
    display.drawString(40, 24, String(thresholds.temp_low, 1) + "-" + 
                              String(thresholds.temp_high, 1) + " C");
    
    // 湿度阈值
    display.drawString(0, 36, "Humi:");
    display.drawString(40, 36, String(thresholds.humi_low, 1) + "-" + 
                              String(thresholds.humi_high, 1) + " %");
    
    // 光照阈值
    display.drawString(0, 48, "Light:");
    display.drawString(40, 48, String(thresholds.light_low, 1) + "-" + 
                              String(thresholds.light_high, 1) + " lx");
    
    drawPageNumber();
    display.display();  // 确保显示更新
}

// 处理串口命令
void processSerialCommand() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.startsWith("page")) {  // 新增page命令处理
            int page = cmd.substring(4).toInt();
            if (page >= 0 && page < MAX_MODES) {
                currentMode = (DisplayMode)page;
                autoScroll = false;  // 切换到手动模式
                Serial.printf("Switched to page %d\n", page);
            }
        }
        else if (cmd == "home") {
            currentMode = REALTIME_DATA;
            autoScroll = false;
            Serial.println("Returned to home screen (page 0)");
        }
        else if (cmd == "scroll") {
            autoScroll = !autoScroll;
            // 从当前页面开始滚动
            lastScrollTime = millis();  // 重置滚动计时器
            Serial.printf("Auto scroll %s from page %d\n", 
                        autoScroll ? "enabled" : "disabled", 
                        currentMode);
        }
        else if (cmd.startsWith("TH")) {
            thresholds.temp_high = cmd.substring(2).toFloat();
            saveConfig();
            Serial.printf("Temperature high threshold set to: %.1f\n", thresholds.temp_high);
        }
        else if (cmd.startsWith("TL")) {
            thresholds.temp_low = cmd.substring(2).toFloat();
            saveConfig();
            Serial.printf("Temperature low threshold set to: %.1f\n", thresholds.temp_low);
        }
        else if (cmd.startsWith("HH")) {
            thresholds.humi_high = cmd.substring(2).toFloat();
            saveConfig();
            Serial.printf("Humidity high threshold set to: %.1f\n", thresholds.humi_high);
        }
        else if (cmd.startsWith("HL")) {
            thresholds.humi_low = cmd.substring(2).toFloat();
            saveConfig();
            Serial.printf("Humidity low threshold set to: %.1f\n", thresholds.humi_low);
        }
        else if (cmd.startsWith("LH")) {
            thresholds.light_high = cmd.substring(2).toFloat();
            saveConfig();
            Serial.printf("Light high threshold set to: %.1f\n", thresholds.light_high);
        }
        else if (cmd.startsWith("LL")) {
            thresholds.light_low = cmd.substring(2).toFloat();
            saveConfig();
            Serial.printf("Light low threshold set to: %.1f\n", thresholds.light_low);
        }
        else if (cmd == "history") {
            Serial.println("\nAlarm History:");
            for(int i = 0; i < alarmCount; i++) {
                Serial.printf("%d: %c=%.1f at %lu sec\n", 
                            i+1, 
                            alarmHistory[i].type,
                            alarmHistory[i].value,
                            alarmHistory[i].timestamp);
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

// LED初始化动画函数
void ledInitAnimation() {
    // 依次点亮每个LED
    digitalWrite(LED_Green, HIGH);
    delay(200);
    digitalWrite(LED_Green, LOW);
    
    digitalWrite(LED_Yellow, HIGH);
    delay(200);
    digitalWrite(LED_Yellow, LOW);
    
    digitalWrite(LED_Red, HIGH);
    delay(200);
    digitalWrite(LED_Red, LOW);
    
    // 全部同时闪烁一次
    digitalWrite(LED_Green, HIGH);
    digitalWrite(LED_Yellow, HIGH);
    digitalWrite(LED_Red, HIGH);
    delay(500);
    digitalWrite(LED_Green, LOW);
    digitalWrite(LED_Yellow, LOW);
    digitalWrite(LED_Red, LOW);
}

void displayInitAnimation() {
    // 趣味动画：简单的加载动画
    for(int i = 0; i < 3; i++) {  // 3秒动画
        display.clear();
        display.setFont(ArialMT_Plain_10);
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        
        // 显示加载点
        String dots = "";
        for(int j = 0; j <= i % 3; j++) {
            dots += ".";
        }
        
        display.drawString(64, 20, "Loading" + dots);
        display.drawProgressBar(10, 32, 108, 8, (i * 33)); // 进度条
        display.display();
        delay(1000);
    }
    
    // 显示Tully
    display.clear();
    display.setFont(ArialMT_Plain_24); // 使用大字体
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 20, "Tully");
    display.display();
    delay(2000);
}

void setup() {
    Serial.begin(115200);
    
    // 串口显示初始化信息
    Serial.println("\nSystem Initializing...");
    
    // I2C初始化
    Wire.begin(5, 4);
    Serial.println("I2C: Initialized (SDA:5, SCL:4)");
    
    // OLED初始化
    if(display.init()) {
        Serial.println("OLED: Initialized successfully");
        Serial.println("OLED Mode: " + String(autoScroll ? "Auto Scroll" : "Single Screen"));
    } else {
        Serial.println("OLED: Initialization failed!");
    }
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    
    // DHT11初始化
    dht.begin();
    Serial.println("DHT11: Sensor started");
    
    // BH1750初始化
    if(lightMeter.begin()) {
        Serial.println("BH1750: Light sensor started");
    } else {
        Serial.println("BH1750: Error initializing sensor!");
    }
    
    // LED初始化
    pinMode(LED_Green, OUTPUT);
    pinMode(LED_Yellow, OUTPUT);
    pinMode(LED_Red, OUTPUT);
    Serial.println("LEDs: Pins configured");
    
    // 配置加载
    loadConfig();
    loadAlarmHistory();
    Serial.println("Config: Settings loaded");
    
    // LED初始化动画
    Serial.println("Running LED test sequence...");
    ledInitAnimation();
    
    // OLED初始化动画和显示
    Serial.println("Starting display animation...");
    displayInitAnimation();
    
    // 显示当前配置
    Serial.println("\nCurrent Configuration:");
    Serial.printf("Temperature thresholds: %.1f-%.1f°C\n", thresholds.temp_low, thresholds.temp_high);
    Serial.printf("Humidity thresholds: %.1f-%.1f%%\n", thresholds.humi_low, thresholds.humi_high);
    Serial.printf("Light thresholds: %.1f-%.1f lx\n", thresholds.light_low, thresholds.light_high);
    Serial.printf("Sample period: %d seconds\n", SAMPLE_PERIOD);
    Serial.printf("Display mode: %s\n", autoScroll ? "Auto Scroll" : "Single Screen");
    
    Serial.println("\nSystem Ready!");
}

void loop() {
    processSerialCommand();
    
    static unsigned long lastSampleTime = 0;
    unsigned long currentTime = millis();
    
    // 定时采样
    if (currentTime - lastSampleTime >= SAMPLE_PERIOD * 1000) {
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        float lux = lightMeter.readLightLevel();
        
        updateDataQueues(t, h, lux);
        lastSampleTime = currentTime;
        
        // 检查报警条件
        if (t > thresholds.temp_high || t < thresholds.temp_low) {
            addAlarmRecord(t, 'T');
        }
        if (h > thresholds.humi_high || h < thresholds.humi_low) {
            addAlarmRecord(h, 'H');
        }
        if (lux > thresholds.light_high || lux < thresholds.light_low) {
            addAlarmRecord(lux, 'L');
        }
        
        // LED控制逻辑
        bool autoGreen = (t > thresholds.temp_high || t < thresholds.temp_low);
        bool autoYellow = (h > thresholds.humi_high || h < thresholds.humi_low);
        bool autoRed = (lux > thresholds.light_high || lux < thresholds.light_low);
        
        // LED控制
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
    }
    

    display.setColor(WHITE);  // 重置颜色
    
     // 显示处理
    if (autoScroll && (currentTime - lastScrollTime >= SCROLL_INTERVAL)) {
        currentMode = (DisplayMode)((currentMode + 1) % MAX_MODES);
        lastScrollTime = currentTime;
    }
    
    static DisplayMode lastMode = MAX_MODES;  // 添加模式变化检测
    if (lastMode != currentMode) {  // 仅在模式改变时更新显示
        float t = tempQueue.empty() ? 0 : tempQueue.back().value;
        float h = humiQueue.empty() ? 0 : humiQueue.back().value;
        float lux = lightQueue.empty() ? 0 : lightQueue.back().value;

   switch (currentMode) {
            case REALTIME_DATA:
                drawRealtimeData(t, h, lux);
                break;
            case TREND_GRAPH:
                drawTrendGraph();
                break;
            case ALARM_DATA:
                drawAlarmData();
                break;
            case PARAMETERS:
                drawParameters();
                break;
        }
        lastMode = currentMode;
    }
    delay(100);
}