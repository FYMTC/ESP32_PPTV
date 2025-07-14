idf_component_register(
	SRCS "init.cpp" "drivers/lv_port_disp.cpp" "drivers/lv_port_indev.cpp"
	INCLUDE_DIRS "." "drivers/." "storage/."
	REQUIRES driver nvs_flash fatfs spiffs XPowersLib cst128 LovyanGFX lvgl esp_adc
)