# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/components/config.html HTML_CONTENT)
string(REPLACE "\"" "\\\"" ESCAPED_HTML "${HTML_CONTENT}")
string(REPLACE "\n" "\\n\"\n\"" FORMATED_HTML "${ESCAPED_HTML}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config_html.h 
    "const char *config_html = \"${FORMATED_HTML}\";")


project(air-quality-alarmer)

list(APPEND EXTRA_COMPONENT_DIRS components/u8g2)