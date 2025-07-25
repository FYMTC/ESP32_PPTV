file(GLOB_RECURSE SRCS "*.cpp" "*.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "drivers" "storage" "services"
    REQUIRES 
        # Core ESP-IDF components
        driver nvs_flash esp_adc esp_wifi esp_event
        # File system components  
        fatfs spiffs
        # External libraries
        XPowersLib cst128 LovyanGFX lvgl espressif__mpu6050
)
