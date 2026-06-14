set(srcs "main.c")

# Регистрируем компонент
idf_component_register(SRCS ${srcs} 
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio)

component_compile_options("-Wno-error=missing-field-initializers")
