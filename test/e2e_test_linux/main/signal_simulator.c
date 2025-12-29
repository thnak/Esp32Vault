/*
 * ESP32 Vault - Signal Simulator Implementation
 */

#include "signal_simulator.h"
#include <string.h>
#include <stdlib.h>

void signal_simulator_init(signal_simulator_t* sim)
{
    if (!sim) return;
    memset(sim, 0, sizeof(signal_simulator_t));
}

void signal_simulator_generate_square_wave(signal_simulator_t* sim, uint8_t pin,
                                          uint32_t frequency_hz, uint64_t duration_us,
                                          uint64_t start_time_us)
{
    if (!sim || frequency_hz == 0) return;
    
    uint64_t period_us = 1000000ULL / frequency_hz;
    uint64_t half_period_us = period_us / 2;
    
    uint64_t current_time = start_time_us;
    uint64_t end_time = start_time_us + duration_us;
    uint8_t current_value = 0;
    
    while (current_time < end_time && sim->event_count < MAX_SIGNAL_EVENTS) {
        sim_signal_event_t* event = &sim->events[sim->event_count];
        event->type = SIM_EVENT_EDGE_CHANGE;
        event->pin = pin;
        event->value = current_value;
        event->time_us = current_time;
        event->high_us = 0;
        event->low_us = 0;
        
        sim->event_count++;
        
        current_time += half_period_us;
        current_value = 1 - current_value;  // Toggle
    }
}

void signal_simulator_generate_random_edges(signal_simulator_t* sim, uint8_t pin,
                                           uint32_t num_edges, uint32_t min_interval_us,
                                           uint32_t max_interval_us, uint64_t start_time_us)
{
    if (!sim || num_edges == 0) return;
    
    uint64_t current_time = start_time_us;
    uint8_t current_value = 0;
    
    for (uint32_t i = 0; i < num_edges && sim->event_count < MAX_SIGNAL_EVENTS; i++) {
        sim_signal_event_t* event = &sim->events[sim->event_count];
        event->type = SIM_EVENT_EDGE_CHANGE;
        event->pin = pin;
        event->value = current_value;
        event->time_us = current_time;
        event->high_us = 0;
        event->low_us = 0;
        
        sim->event_count++;
        
        // Generate random interval
        uint32_t interval = min_interval_us;
        if (max_interval_us > min_interval_us) {
            interval += rand() % (max_interval_us - min_interval_us);
        }
        current_time += interval;
        current_value = 1 - current_value;  // Toggle
    }
}

void signal_simulator_generate_pulses(signal_simulator_t* sim, uint8_t pin,
                                     uint32_t num_pulses, uint32_t high_us,
                                     uint32_t low_us, uint64_t start_time_us)
{
    if (!sim || num_pulses == 0) return;
    
    uint64_t current_time = start_time_us;
    
    for (uint32_t i = 0; i < num_pulses && sim->event_count < MAX_SIGNAL_EVENTS; i++) {
        sim_signal_event_t* event = &sim->events[sim->event_count];
        event->type = SIM_EVENT_PULSE;
        event->pin = pin;
        event->value = 0;  // Not used for pulse events
        event->time_us = current_time;
        event->high_us = high_us;
        event->low_us = low_us;
        
        sim->event_count++;
        
        current_time += (high_us + low_us);
    }
}

const sim_signal_event_t* signal_simulator_get_next_event(signal_simulator_t* sim, uint32_t* index)
{
    if (!sim || !index) return NULL;
    
    if (*index >= sim->event_count) return NULL;
    
    const sim_signal_event_t* event = &sim->events[*index];
    (*index)++;
    return event;
}

uint32_t signal_simulator_get_event_count(signal_simulator_t* sim)
{
    return sim ? sim->event_count : 0;
}

void signal_simulator_clear(signal_simulator_t* sim)
{
    if (!sim) return;
    sim->event_count = 0;
}
