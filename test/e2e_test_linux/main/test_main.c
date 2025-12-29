/*
 * ESP32 Vault - Main Test Entry Point for E2E Tests
 * 
 * This file provides the main entry point for running end-to-end integration tests on Linux host.
 */

#include <stdio.h>
#include "unity.h"

// Forward declarations of test functions
extern void test_e2e_signal_to_mqtt(void);
extern void test_e2e_psram_buffer_pressure(void);
extern void test_e2e_mqtt_reconnection(void);
extern void test_e2e_mixed_signals(void);

void app_main(void)
{
    printf("\n");
    printf("===========================================\n");
    printf("ESP32 Vault - End-to-End Integration Tests\n");
    printf("===========================================\n");
    printf("\n");

    UNITY_BEGIN();
    
    // E2E Test Scenarios
    printf("\n--- E2E Test Scenarios ---\n");
    RUN_TEST(test_e2e_signal_to_mqtt);
    RUN_TEST(test_e2e_psram_buffer_pressure);
    RUN_TEST(test_e2e_mqtt_reconnection);
    RUN_TEST(test_e2e_mixed_signals);
    
    UNITY_END();
    
    printf("\n");
    printf("===========================================\n");
    printf("All E2E tests completed!\n");
    printf("===========================================\n");
    printf("\n");
}
