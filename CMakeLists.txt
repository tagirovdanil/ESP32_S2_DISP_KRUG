cmake_minimum_required(VERSION 3.16)

# Подключаем систему сборки ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Регистрируем имя проекта
project(ESP32_S2_DISP_KRUG)

add_compile_options("-Wno-error=missing-field-initializers")
