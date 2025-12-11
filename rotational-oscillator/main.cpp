#include <Arduino.h>
#include <vector>
#include <Wire.h> 
#include "SparkFun_BNO08x_Arduino_Library.h"

// Web functionality declarations
void initWebServer();
void handleWebServer();

// Gyro
BNO08x gyro;
float lastPitch = 0.0;
unsigned long lastSensorUpdateTime = 0;

// CONSTANT DECLARATIONS
constexpr int MOTOR1 = 13;
constexpr int MOTOR2 = 12;
constexpr int PWM_CH1 = 0;
constexpr int PWM_CH2 = 1;
constexpr int PWM_FREQ = 50; // Hz control signal
constexpr int PWM_RESOLUTION = 16; // resolution in bits

float gain_p = 0.2;
float gain_d = 0.05;
float gain_i = 0.1;
unsigned long lastLoopTime = 0;
float integralSum = 0.0;
float error = 0.0;
float lastError = 0.0;

// Calibration Variables
float calibMiddle = 0.0;
float calibLowVal = 0.0; 
float calibHighVal = 0.0;

struct DataPoint {
    uint32_t timestamp;
    float sensorValue;
    int selectedSensor;
    float error;
    float controlOutput;
    int strategyUsed;
};

volatile bool running = false;
volatile int selectedStrategy = 0;
volatile int selectedSensor = 0; 
bool wasRunning = false; 

// DATA BUFFER (Used for streaming)
std::vector<DataPoint> dataLog;

// ESC range (microseconds)
constexpr uint32_t ESC_MIN_PULSE = 1000;
constexpr uint32_t ESC_MAX_PULSE = 2000;

// Control strategy output range 
constexpr float STRATEGY_OUTPUT_MIN = 0.0;
constexpr float STRATEGY_OUTPUT_MAX = 1.0;

// Derived constants
constexpr float PERIOD_US = 1000000.0f / PWM_FREQ; 
constexpr uint32_t MAX_DUTY = (1UL << PWM_RESOLUTION) - 1UL;
constexpr uint32_t MIN_DUTY = (uint32_t)((ESC_MIN_PULSE / PERIOD_US) * (float)MAX_DUTY + 0.5f);

// FUNCTIONS

float clamp(float v, float a, float b) {
  if (v < a) return a;
  if (v > b) return b;
  return v;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  float t = (x - in_min) / (in_max - in_min);
  return out_min + t * (out_max - out_min);
}

uint32_t controlToDuty(float controlValue) {
  float v = clamp(controlValue, STRATEGY_OUTPUT_MIN, STRATEGY_OUTPUT_MAX);
  float pulse = mapFloat(v, STRATEGY_OUTPUT_MIN, STRATEGY_OUTPUT_MAX, ESC_MIN_PULSE, ESC_MAX_PULSE);
  float fraction = pulse / PERIOD_US;
  fraction = clamp(fraction, 0.0f, 1.0f);
  uint32_t duty = (uint32_t)(fraction * (float)MAX_DUTY + 0.5f);
  return duty;
}

void setMotorPower(float controlValue) {
  if (controlValue > 0) {
    uint32_t duty = controlToDuty(controlValue);
    ledcWrite(PWM_CH1, duty);
    ledcWrite(PWM_CH2, MIN_DUTY);
  }
  else if (controlValue < 0) {
    uint32_t duty = controlToDuty(-controlValue);
    ledcWrite(PWM_CH1, MIN_DUTY);
    ledcWrite(PWM_CH2, duty);
  }
  else {
    ledcWrite(PWM_CH1, MIN_DUTY);
    ledcWrite(PWM_CH2, MIN_DUTY);
  }
}

float sensorRead(int selectedSensor) {
  for(int i = 0; i < 5; i++) {
    if (gyro.getSensorEvent() && gyro.getSensorEventID() == SENSOR_REPORTID_GAME_ROTATION_VECTOR) {
      lastSensorUpdateTime = millis();
      lastPitch = gyro.getPitch();
    }
    delay(2);
  }
  return lastPitch;
}

void calibrateLowStep(int selectedSensor) {
    float sum = 0.0;
    for(int i = 0; i < 200; i++) { sum += sensorRead(selectedSensor); delay(10); }
    calibLowVal = sum / 200.0;
    calibMiddle = (calibLowVal + calibHighVal) / 2.0;
}

void calibrateHighStep(int selectedSensor) {
    float sum = 0.0;
    for(int i = 0; i < 200; i++) { sum += sensorRead(selectedSensor); delay(10); }
    calibHighVal = sum / 200.0;
    calibMiddle = (calibLowVal + calibHighVal) / 2.0;
}

float runControl(float sensorValue, int selectedStrategy) {
  unsigned long now = micros();
  float dt = (now - lastLoopTime) / 1000000.0;
  lastLoopTime = now;
  lastError = error;
  error = calibMiddle - sensorValue;
  
  switch(selectedStrategy) {
    default: return 0.0;
    case 1: return gain_p * error;
    case 2: { 
      if (error > 0) return 1.0; 
      else if (error < 0) return -1.0; 
      else return 0.0; 
    }
    case 3: { 
      // Prevent Integral Windup
      integralSum += error * dt;
      float derivative = (error - lastError) / dt;
      return gain_p * error + gain_i * integralSum + gain_d * derivative;
    }
  }
}

// Data streaming helper (too large to keep in esp32 memory)
String getBufferCSV() {
    String csv = "";
    if (dataLog.empty()) return "";
    
    // Convert buffer to string
    for (const auto& dp : dataLog) {
        csv += String(dp.timestamp) + "," +
               String(dp.sensorValue, 9) + "," +
               String(dp.selectedSensor) + "," +
               String(dp.error, 9) + "," +
               String(dp.controlOutput, 9) + "," +
               String(dp.strategyUsed) + "\n";
    }
    // Clear the buffer so it can fill up again
    dataLog.clear(); 
    return csv;
}

// SETUP AND LOOP

void setup() {
  delay(7000);
  Serial.begin(115200);
  Serial.println("Booting ESP32...");
  
  dataLog.reserve(1500); // Reserve memory

  Wire.begin(21, 22); 
  Wire.setClock(100000); // 100kHz

  gyro.begin(0x4B, Wire, -1, -1); 
  delay(500);
  
  gyro.enableGameRotationVector(20);
  
  ledcSetup(PWM_CH1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CH2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR1, PWM_CH1);
  ledcAttachPin(MOTOR2, PWM_CH2);

  Serial.println("Starting web server...");
  initWebServer();
  Serial.println("Web server started.");

  ledcWrite(PWM_CH1, MIN_DUTY);
  ledcWrite(PWM_CH2, MIN_DUTY);
  delay(2000);
}

void loop() {
  handleWebServer();
  if (gyro.getSensorEvent()) {
      if (gyro.getSensorEventID() == SENSOR_REPORTID_GAME_ROTATION_VECTOR) {
        lastSensorUpdateTime = millis();
        lastPitch = gyro.getPitch();
      }
  }

  if (running && (millis() - lastSensorUpdateTime) < 100) {
    if (!wasRunning) {
      dataLog.clear();
      wasRunning = true;
      integralSum = 0.0;
      lastLoopTime = micros();
    }
    
    int currentSensor = selectedSensor;
    int currentStrategy = selectedStrategy;

    float controlOutput = runControl(lastPitch, currentStrategy);
    setMotorPower(controlOutput);

    if (dataLog.size() < 1500) {
        DataPoint dp;
        dp.timestamp = micros();
        dp.sensorValue = lastPitch;
        dp.selectedSensor = selectedSensor;
        dp.error = error;
        dp.controlOutput = controlOutput;
        dp.strategyUsed = selectedStrategy;
        dataLog.push_back(dp);
    } 
  } 
  else {
    setMotorPower(0.0);
    if (wasRunning) wasRunning = false;
  }
}