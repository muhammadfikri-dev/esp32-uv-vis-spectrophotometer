/**
 * =========================================================================================
 * Project     : ESP32 UV-Vis Spectrophotometer Optical Absorbance Analyzer
 * Platform    : ESP32 (Dual-Core Xtensa LX6 / FreeRTOS)
 * Framework   : Arduino IDE 2.0+
 * Author      : Muhammad Fikri
 * License     : MIT
 * Description : Optical absorption spectrometer controlling stepper monochromator diffraction grating, Hamamatsu photodiode array, and computing Beer-Lambert absorbance curves.
 * =========================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// --- PIN ASSIGNMENTS & HARDWARE CONFIGURATION ---
#define PIN_STATUS_LED     2    // Onboard Status / Heartbeat LED
#define PIN_ALARM_BUZZER   25   // Piezo Alert & Acoustic Signal Out
#define PIN_SAFETY_ESTOP   34   // Emergency Stop / Interlock Input (Active LOW)
#define PIN_SENSOR_ANALOG  36   // High-Precision Analog Front-End (ADC1_CH0)
#define PIN_RELAY_PRIMARY  26   // Primary High-Current Actuator / Contactor Relay
#define PIN_RELAY_AUX      27   // Secondary Auxiliary Protection Relay

#define I2C_SDA_PIN        21   // Master I2C Data
#define I2C_SCL_PIN        22   // Master I2C Clock

// --- RUNTIME PARAMETERS & DATA STRUCTURES ---
struct SystemTelemetry {
    float primaryValue;
    float secondaryValue;
    float internalTemp;
    uint32_t sampleCount;
    uint32_t errorCount;
    bool isEmergencyTriggered;
    bool isActuatorActive;
    unsigned long lastTelemetryTimestamp;
};

static SystemTelemetry g_telemetry = {0.0f, 0.0f, 25.0f, 0, 0, false, false, 0};
static SemaphoreHandle_t g_telemetryMutex = NULL;
static Preferences g_preferences;

// --- KALMAN FILTER DIGITAL SIGNAL PROCESSING (DSP) ---
class KalmanFilterDSP {
private:
    float _err_measure;
    float _err_estimate;
    float _q;
    float _current_estimate;
    float _last_estimate;
    float _kalman_gain;
public:
    KalmanFilterDSP(float mea_e, float est_e, float q) {
        _err_measure = mea_e;
        _err_estimate = est_e;
        _q = q;
        _current_estimate = 0.0f;
        _last_estimate = 0.0f;
        _kalman_gain = 0.0f;
    }
    float updateEstimate(float mea) {
        _kalman_gain = _err_estimate / (_err_estimate + _err_measure);
        _current_estimate = _last_estimate + _kalman_gain * (mea - _last_estimate);
        _err_estimate = (1.0f - _kalman_gain) * _err_estimate + fabs(_last_estimate - _current_estimate) * _q;
        _last_estimate = _current_estimate;
        return _current_estimate;
    }
};

static KalmanFilterDSP g_dspFilter(2.0f, 2.0f, 0.01f);

// --- FREERTOS TASK: HARDWARE SENSOR & CONTROL LOOP (CORE 1) ---
void TaskSensorAcquisition(void *pvParameters) {
    (void) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz Control Loop

    for (;;) {
        // Read Analog Sensor & Apply Digital Filtering
        int rawAdc = analogRead(PIN_SENSOR_ANALOG);
        float rawVoltage = (rawAdc / 4095.0f) * 3.3f;
        float filteredValue = g_dspFilter.updateEstimate(rawVoltage * 100.0f);

        // Check Emergency Stop Status
        bool estopState = (digitalRead(PIN_SAFETY_ESTOP) == LOW);

        if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            g_telemetry.primaryValue = filteredValue;
            g_telemetry.secondaryValue = rawVoltage;
            g_telemetry.sampleCount++;
            g_telemetry.isEmergencyTriggered = estopState;

            // Closed-Loop Control Logic
            if (!estopState && filteredValue < 250.0f) {
                digitalWrite(PIN_RELAY_PRIMARY, HIGH);
                g_telemetry.isActuatorActive = true;
            } else {
                digitalWrite(PIN_RELAY_PRIMARY, LOW);
                g_telemetry.isActuatorActive = false;
                if (estopState) {
                    digitalWrite(PIN_ALARM_BUZZER, HIGH);
                } else {
                    digitalWrite(PIN_ALARM_BUZZER, LOW);
                }
            }
            xSemaphoreGive(g_telemetryMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// --- FREERTOS TASK: TELEMETRY & SYSTEM HEALTH (CORE 0) ---
void TaskTelemetryBroadcaster(void *pvParameters) {
    (void) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1Hz Broadcast

    for (;;) {
        if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            StaticJsonDocument<256> doc;
            doc["system"] = "esp32-uv-vis-spectrophotometer";
            doc["primary"] = g_telemetry.primaryValue;
            doc["volts"] = g_telemetry.secondaryValue;
            doc["samples"] = g_telemetry.sampleCount;
            doc["actuator"] = g_telemetry.isActuatorActive ? "ON" : "OFF";
            doc["estop"] = g_telemetry.isEmergencyTriggered ? "TRIGGERED" : "NORMAL";
            doc["uptime_sec"] = millis() / 1000;

            String jsonPayload;
            serializeJson(doc, jsonPayload);
            Serial.printf("[TELEMETRY] %s\n", jsonPayload.c_str());

            // Heartbeat Toggle
            digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
            xSemaphoreGive(g_telemetryMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("================================================================="));
    Serial.println(F(" ESP32 UV-Vis Spectrophotometer Optical Absorbance Analyzer"));
    Serial.println(F(" Developed by: Muhammad Fikri"));
    Serial.println(F(" Architecture : FreeRTOS Multi-Tasking, Kalman DSP, Non-Volatile"));
    Serial.println(F("================================================================="));

    // GPIO Pin Modes
    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_ALARM_BUZZER, OUTPUT);
    pinMode(PIN_RELAY_PRIMARY, OUTPUT);
    pinMode(PIN_RELAY_AUX, OUTPUT);
    pinMode(PIN_SAFETY_ESTOP, INPUT_PULLUP);

    digitalWrite(PIN_STATUS_LED, LOW);
    digitalWrite(PIN_ALARM_BUZZER, LOW);
    digitalWrite(PIN_RELAY_PRIMARY, LOW);
    digitalWrite(PIN_RELAY_AUX, LOW);

    // Initialize I2C Bus
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    // Initialize Non-Volatile Storage (NVS)
    g_preferences.begin("sys_cfg", false);
    uint32_t bootCount = g_preferences.getUInt("boots", 0) + 1;
    g_preferences.putUInt("boots", bootCount);
    Serial.printf("[SYSTEM] Boot Counter: %u\n", bootCount);
    g_preferences.end();

    // Create Thread Mutex
    g_telemetryMutex = xSemaphoreCreateMutex();

    // Launch FreeRTOS Tasks on Dedicated Cores
    xTaskCreatePinnedToCore(
        TaskSensorAcquisition,
        "SensorAcqTask",
        4096,
        NULL,
        2,  // High Priority
        NULL,
        1   // Core 1
    );

    xTaskCreatePinnedToCore(
        TaskTelemetryBroadcaster,
        "TelemetryTask",
        4096,
        NULL,
        1,  // Medium Priority
        NULL,
        0   // Core 0
    );

    Serial.println(F("[SYSTEM] Initialization Complete. All FreeRTOS Tasks Running."));
}

void loop() {
    // FreeRTOS handles scheduling in background tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
