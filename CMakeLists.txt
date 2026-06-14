cmake_minimum_required(VERSION 3.16)

# Подключаем систему сборки ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Регистрируем имя проекта
project(ESP32_S2_DISP_KRUG)

# Отключаем ошибку missing-field-initializers (баг в ESP-IDF 6.0.1 с GCC)
add_compile_options("-Wno-error=missing-field-initializers")

# Дополнительно: снимаем флаг -Werror для missing-field-initializers на уровне всего проекта
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=missing-field-initializers")
