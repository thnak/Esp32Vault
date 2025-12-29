/*
 * ESP32 Vault - Mock MQTT Broker Implementation
 */

#include "mock_mqtt_broker.h"
#include <string.h>
#include <stdio.h>

// Helper function to match topic with potential wildcards
static bool topic_matches(const char* pattern, const char* topic)
{
    // Simple implementation: exact match or # wildcard
    if (strcmp(pattern, "#") == 0) {
        return true;  // Match all
    }
    
    // Check for trailing # wildcard
    size_t pattern_len = strlen(pattern);
    if (pattern_len > 0 && pattern[pattern_len - 1] == '#') {
        // Match prefix
        size_t prefix_len = pattern_len - 1;
        if (prefix_len > 0 && pattern[prefix_len - 1] == '/') {
            prefix_len--;
        }
        // Only compare if there's a prefix to compare
        if (prefix_len > 0) {
            return strncmp(pattern, topic, prefix_len) == 0;
        } else {
            // Pattern is just "#" or "/#", match all
            return true;
        }
    }
    
    // Exact match
    return strcmp(pattern, topic) == 0;
}

void mock_mqtt_broker_init(mock_mqtt_broker_t* broker)
{
    if (!broker) return;
    
    memset(broker, 0, sizeof(mock_mqtt_broker_t));
    broker->connected = false;
    broker->slow_mode = false;
    broker->publish_delay_us = 0;
    broker->message_count = 0;
    
    // Initialize all subscribers as inactive
    for (int i = 0; i < MAX_MQTT_SUBSCRIBERS; i++) {
        broker->subscribers[i].active = false;
    }
}

bool mock_mqtt_broker_connect(mock_mqtt_broker_t* broker)
{
    if (!broker) return false;
    broker->connected = true;
    return true;
}

void mock_mqtt_broker_disconnect(mock_mqtt_broker_t* broker)
{
    if (!broker) return;
    broker->connected = false;
}

bool mock_mqtt_broker_is_connected(mock_mqtt_broker_t* broker)
{
    return broker ? broker->connected : false;
}

bool mock_mqtt_broker_subscribe(mock_mqtt_broker_t* broker, const char* topic,
                                mqtt_message_callback_t callback, void* user_data)
{
    if (!broker || !topic || !callback) return false;
    
    // Find an available subscriber slot
    for (int i = 0; i < MAX_MQTT_SUBSCRIBERS; i++) {
        if (!broker->subscribers[i].active) {
            strncpy(broker->subscribers[i].topic, topic, MAX_MQTT_TOPIC_LEN - 1);
            broker->subscribers[i].topic[MAX_MQTT_TOPIC_LEN - 1] = '\0';
            broker->subscribers[i].callback = callback;
            broker->subscribers[i].user_data = user_data;
            broker->subscribers[i].active = true;
            return true;
        }
    }
    
    return false;  // No available slots
}

void mock_mqtt_broker_unsubscribe(mock_mqtt_broker_t* broker, const char* topic)
{
    if (!broker || !topic) return;
    
    for (int i = 0; i < MAX_MQTT_SUBSCRIBERS; i++) {
        if (broker->subscribers[i].active && strcmp(broker->subscribers[i].topic, topic) == 0) {
            broker->subscribers[i].active = false;
        }
    }
}

bool mock_mqtt_broker_publish(mock_mqtt_broker_t* broker, const char* topic,
                              const uint8_t* payload, size_t payload_len, bool is_binary)
{
    if (!broker || !topic || !payload) return false;
    if (!broker->connected) return false;
    
    // Simulate slow mode delay
    if (broker->slow_mode && broker->publish_delay_us > 0) {
        // In real implementation, this would block or defer
        // For testing, we just note the delay
    }
    
    // Check message log size
    if (broker->message_count >= MAX_MQTT_MESSAGE_LOG_SIZE) {
        return false;  // Log full
    }
    
    // Store message in log
    mqtt_message_t* msg = &broker->message_log[broker->message_count];
    strncpy(msg->topic, topic, MAX_MQTT_TOPIC_LEN - 1);
    msg->topic[MAX_MQTT_TOPIC_LEN - 1] = '\0';
    
    size_t copy_len = (payload_len < MAX_MQTT_PAYLOAD_SIZE) ? payload_len : MAX_MQTT_PAYLOAD_SIZE;
    memcpy(msg->payload, payload, copy_len);
    msg->payload_len = copy_len;
    msg->is_binary = is_binary;
    msg->timestamp_us = 0;  // Would use actual timestamp in real implementation
    
    broker->message_count++;
    
    // Deliver to matching subscribers
    for (int i = 0; i < MAX_MQTT_SUBSCRIBERS; i++) {
        if (broker->subscribers[i].active && 
            topic_matches(broker->subscribers[i].topic, topic) &&
            broker->subscribers[i].callback != NULL) {
            broker->subscribers[i].callback(topic, payload, payload_len, 
                                           broker->subscribers[i].user_data);
        }
    }
    
    return true;
}

void mock_mqtt_broker_process(mock_mqtt_broker_t* broker)
{
    // In this simple implementation, messages are delivered immediately in publish()
    // This function is here for API compatibility if we need async processing later
    (void)broker;
}

uint32_t mock_mqtt_broker_get_message_count(mock_mqtt_broker_t* broker)
{
    return broker ? broker->message_count : 0;
}

const mqtt_message_t* mock_mqtt_broker_get_message(mock_mqtt_broker_t* broker, uint32_t index)
{
    if (!broker || index >= broker->message_count) return NULL;
    return &broker->message_log[index];
}

void mock_mqtt_broker_clear_messages(mock_mqtt_broker_t* broker)
{
    if (!broker) return;
    broker->message_count = 0;
}

void mock_mqtt_broker_set_slow_mode(mock_mqtt_broker_t* broker, bool enabled, uint32_t delay_us)
{
    if (!broker) return;
    broker->slow_mode = enabled;
    broker->publish_delay_us = delay_us;
}

uint32_t mock_mqtt_broker_get_messages_by_topic(mock_mqtt_broker_t* broker, const char* topic_pattern,
                                                 mqtt_message_t* output, uint32_t max_output)
{
    if (!broker || !topic_pattern || !output || max_output == 0) return 0;
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < broker->message_count && count < max_output; i++) {
        if (topic_matches(topic_pattern, broker->message_log[i].topic)) {
            memcpy(&output[count], &broker->message_log[i], sizeof(mqtt_message_t));
            count++;
        }
    }
    
    return count;
}
