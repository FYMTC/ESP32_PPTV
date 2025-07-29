#include "tests.h"

#include <esp_log.h>
#include "wifi_manager.h"
void test001()
{
    ESP_LOGI("TEST", "Test 001 started");
	//vTaskDelay(pdMS_TO_TICKS(10000));
    wifi_manager_enable();



    ESP_LOGI("TEST", "Test 001 completed");
}
