#pragma once
#include <time.h>

void nvs_init();
time_t load_time_from_nvs();
void save_time_to_nvs(time_t time);