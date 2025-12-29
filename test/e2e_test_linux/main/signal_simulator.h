/*
 * ESP32 Vault - Signal Simulator for E2E Testing
 * 
 * Simulates GPIO pin state changes for testing signal capture.
 */

#ifndef SIGNAL_SIMULATOR_H
#define SIGNAL_SIMULATOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Signal event types
typedef enum {
    SIM_EVENT_EDGE_CHANGE,
    SIM_EVENT_PULSE
} sim_event_type_t;

// Simulated signal event
typedef struct {
    sim_event_type_t type;
    uint8_t pin;
    uint8_t value;        // For edge changes: 0 or 1
    uint64_t time_us;     // Timestamp in microseconds
    uint32_t high_us;     // For pulse events: high duration
    uint32_t low_us;      // For pulse events: low duration
} sim_signal_event_t;

// Signal simulator state
typedef struct {
    sim_signal_event_t events[1000];
    uint32_t event_count;
    uint64_t current_time_us;
} signal_simulator_t;

/**
 * Initialize signal simulator
 */
void signal_simulator_init(signal_simulator_t* sim);

/**
 * Generate a square wave on a pin
 * @param sim Simulator instance
 * @param pin Pin number
 * @param frequency_hz Frequency in Hz
 * @param duration_us Duration to simulate in microseconds
 * @param start_time_us Starting timestamp
 */
void signal_simulator_generate_square_wave(signal_simulator_t* sim, uint8_t pin,
                                          uint32_t frequency_hz, uint64_t duration_us,
                                          uint64_t start_time_us);

/**
 * Generate random edge changes
 * @param sim Simulator instance
 * @param pin Pin number
 * @param num_edges Number of edges to generate
 * @param min_interval_us Minimum time between edges
 * @param max_interval_us Maximum time between edges
 * @param start_time_us Starting timestamp
 */
void signal_simulator_generate_random_edges(signal_simulator_t* sim, uint8_t pin,
                                           uint32_t num_edges, uint32_t min_interval_us,
                                           uint32_t max_interval_us, uint64_t start_time_us);

/**
 * Generate pulse width events
 * @param sim Simulator instance
 * @param pin Pin number
 * @param num_pulses Number of pulses to generate
 * @param high_us High duration in microseconds
 * @param low_us Low duration in microseconds
 * @param start_time_us Starting timestamp
 */
void signal_simulator_generate_pulses(signal_simulator_t* sim, uint8_t pin,
                                     uint32_t num_pulses, uint32_t high_us,
                                     uint32_t low_us, uint64_t start_time_us);

/**
 * Get the next event from the simulator
 * Returns NULL if no more events
 */
const sim_signal_event_t* signal_simulator_get_next_event(signal_simulator_t* sim, uint32_t* index);

/**
 * Get event count
 */
uint32_t signal_simulator_get_event_count(signal_simulator_t* sim);

/**
 * Clear all events
 */
void signal_simulator_clear(signal_simulator_t* sim);

#ifdef __cplusplus
}
#endif

#endif // SIGNAL_SIMULATOR_H
