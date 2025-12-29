idf_component_register(
    SRCS "test_signal_telemetry.c"
         "test_psram_buffer.c"
         "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES unity
)
