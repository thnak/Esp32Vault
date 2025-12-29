#ifndef TEST_SEEDER_H
#define TEST_SEEDER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "MQTTManager.h"
#include "SignalTelemetry.h"

/**
 * TestSeeder - Generates synthetic telemetry data for firmware stability testing
 * 
 * When enabled via CONFIG_ENABLE_TEST_SEEDER build flag, this component
 * generates and publishes test telemetry data every second:
 * - Raw edge packets with simulated GPIO changes
 * - Pulse width measurements with varying timings
 * - Diagnostic data showing system health
 * 
 * This allows validation of:
 * - MQTT connectivity and throughput
 * - Binary packet serialization
 * - Network stability under load
 * - PSRAM buffer behavior
 * - Overall firmware stability
 */
class TestSeeder {
private:
    MQTTManager* mqttManager;
    std::string macAddress;
    TaskHandle_t seederTaskHandle;
    
    uint32_t seq;           // Sequence counter
    uint32_t iteration;     // Iteration counter
    
    static void seederTaskFunction(void* parameter);
    void generateAndPublishData();
    void publishRawPacket();
    void publishPulsePacket();
    void publishDiagPacket();
    
public:
    TestSeeder();
    ~TestSeeder();
    
    void begin(MQTTManager* mqtt, const std::string& mac);
    void stop();
};

#endif // TEST_SEEDER_H
