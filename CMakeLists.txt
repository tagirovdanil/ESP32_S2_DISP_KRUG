cmake_minimum_required(VERSION 3.16)

# Обязательно сначала подключаем сам ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Регистрируем имя проекта
project(ESP32_S2_DISP_KRUG)

# Отключаем ошибку missing-field-initializers для ВСЕХ компонентов проекта
component_compile_options("-Wno-error=missing-field-initializers")