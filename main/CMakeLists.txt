set(srcs "main.c" "pressure_sensor.c" "st7789.c" "fontx.c")

# 1. Сначала ВСЕГДА регистрируем компонент
idf_component_register(SRCS ${srcs} 
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_uart esp_driver_spi esp_timer spiffs)

# 2. Только ПОСЛЕ регистрации отключаем ошибку missing-field-initializers
component_compile_options("-Wno-error=missing-field-initializers")
