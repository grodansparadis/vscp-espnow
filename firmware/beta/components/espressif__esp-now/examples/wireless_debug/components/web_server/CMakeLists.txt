
idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "include"
                    REQUIRES "spi_flash" "console" "nvs_flash" "fatfs" "esp-now" "spiffs" "mdns" "esp_http_server" "json")

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/front")

if(EXISTS ${WEB_SRC_DIR})
    spiffs_create_partition_image(www ${WEB_SRC_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-pointer-sign -Wno-format)