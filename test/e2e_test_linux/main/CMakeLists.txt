idf_component_register(
    SRCS "mock_mqtt_broker.c"
         "signal_simulator.c"
         "test_e2e.c"
         "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES unity
)
