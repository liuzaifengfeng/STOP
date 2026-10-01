# 明确列出被控端源码，避免把控制端复制来的单节锂电池 ADC 任务编入固件。
idf_component_register(SRCS
    "main.c"
    "E22-400t22s.c"
    "ble_device_service.c"
    "ble_transport.c"
    "device_config.c"
    "ina226_monitor.c"
    "receiver_safety.c"
    "radio_transport.c"
    "safety_radio_frame.c"
    "status_led.c"
    "vin_monitor.c"
    "ota_service.c"
    "sha256_sw.c"
    "stop_protocol.c")
