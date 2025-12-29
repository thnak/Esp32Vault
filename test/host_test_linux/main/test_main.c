/*
 * ESP32 Vault - Main Test Entry Point for Linux Host Tests
 * 
 * This file provides the main entry point for running unit tests on Linux host.
 */

#include <stdio.h>
#include "unity.h"

// Forward declarations of test functions
extern void test_signal_telemetry_packet_serialization(void);
extern void test_raw_packet_structure(void);
extern void test_pulse_packet_structure(void);
extern void test_diag_packet_structure(void);
extern void test_psram_buffer_basic(void);
extern void test_psram_buffer_overflow(void);

void app_main(void)
{
    printf("\n");
    printf("===========================================\n");
    printf("ESP32 Vault - Linux Host Unit Tests\n");
    printf("===========================================\n");
    printf("\n");

    UNITY_BEGIN();
    
    // Signal Telemetry Tests
    printf("\n--- Signal Telemetry Tests ---\n");
    RUN_TEST(test_signal_telemetry_packet_serialization);
    RUN_TEST(test_raw_packet_structure);
    RUN_TEST(test_pulse_packet_structure);
    RUN_TEST(test_diag_packet_structure);
    
    // PSRAM Buffer Tests
    printf("\n--- PSRAM Buffer Tests ---\n");
    RUN_TEST(test_psram_buffer_basic);
    RUN_TEST(test_psram_buffer_overflow);
    
    UNITY_END();
    
    printf("\n");
    printf("===========================================\n");
    printf("All tests completed!\n");
    printf("===========================================\n");
    printf("\n");
}
