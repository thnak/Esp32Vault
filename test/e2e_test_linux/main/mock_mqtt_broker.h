/*
 * ESP32 Vault - Mock MQTT Broker for E2E Testing
 * 
 * Provides a simple in-memory MQTT broker simulation for testing
 * publish/subscribe operations without requiring network I/O.
 */

#ifndef MOCK_MQTT_BROKER_H
#define MOCK_MQTT_BROKER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of topics and subscribers
#define MAX_MQTT_TOPICS 32
#define MAX_MQTT_SUBSCRIBERS 16
#define MAX_MQTT_TOPIC_LEN 128
#define MAX_MQTT_PAYLOAD_SIZE 4096

// Message structure for published messages
typedef struct {
    char topic[MAX_MQTT_TOPIC_LEN];
    uint8_t payload[MAX_MQTT_PAYLOAD_SIZE];
    size_t payload_len;
    uint64_t timestamp_us;
    bool is_binary;
} mqtt_message_t;

// Callback for message reception
typedef void (*mqtt_message_callback_t)(const char* topic, const uint8_t* payload, size_t payload_len, void* user_data);

// Subscriber structure
typedef struct {
    char topic[MAX_MQTT_TOPIC_LEN];
    mqtt_message_callback_t callback;
    void* user_data;
    bool active;
} mqtt_subscriber_t;

// Mock MQTT broker state
typedef struct {
    mqtt_subscriber_t subscribers[MAX_MQTT_SUBSCRIBERS];
    mqtt_message_t message_log[256];  // Log of all published messages
    uint32_t message_count;
    bool connected;
    bool slow_mode;  // Simulate slow/blocked publishing
    uint32_t publish_delay_us;
} mock_mqtt_broker_t;

/**
 * Initialize the mock MQTT broker
 */
void mock_mqtt_broker_init(mock_mqtt_broker_t* broker);

/**
 * Connect to the mock broker
 */
bool mock_mqtt_broker_connect(mock_mqtt_broker_t* broker);

/**
 * Disconnect from the mock broker
 */
void mock_mqtt_broker_disconnect(mock_mqtt_broker_t* broker);

/**
 * Check if connected to broker
 */
bool mock_mqtt_broker_is_connected(mock_mqtt_broker_t* broker);

/**
 * Subscribe to a topic with callback
 * Returns true if subscription successful
 */
bool mock_mqtt_broker_subscribe(mock_mqtt_broker_t* broker, const char* topic, 
                                mqtt_message_callback_t callback, void* user_data);

/**
 * Unsubscribe from a topic
 */
void mock_mqtt_broker_unsubscribe(mock_mqtt_broker_t* broker, const char* topic);

/**
 * Publish a message to a topic
 * Returns true if message published successfully
 */
bool mock_mqtt_broker_publish(mock_mqtt_broker_t* broker, const char* topic, 
                              const uint8_t* payload, size_t payload_len, bool is_binary);

/**
 * Process pending messages (delivers to subscribers)
 */
void mock_mqtt_broker_process(mock_mqtt_broker_t* broker);

/**
 * Get the number of messages published
 */
uint32_t mock_mqtt_broker_get_message_count(mock_mqtt_broker_t* broker);

/**
 * Get a specific message from the log
 */
const mqtt_message_t* mock_mqtt_broker_get_message(mock_mqtt_broker_t* broker, uint32_t index);

/**
 * Clear all messages from the log
 */
void mock_mqtt_broker_clear_messages(mock_mqtt_broker_t* broker);

/**
 * Set slow mode (simulates slow/blocked network)
 */
void mock_mqtt_broker_set_slow_mode(mock_mqtt_broker_t* broker, bool enabled, uint32_t delay_us);

/**
 * Get messages matching a topic pattern
 * Returns number of matching messages copied to output array
 */
uint32_t mock_mqtt_broker_get_messages_by_topic(mock_mqtt_broker_t* broker, const char* topic_pattern,
                                                 mqtt_message_t* output, uint32_t max_output);

#ifdef __cplusplus
}
#endif

#endif // MOCK_MQTT_BROKER_H
