#pragma once
#include "page.hpp"
class MainPage : public Page {
public:
    lv_obj_t* onCreate() override;
    void onDestroy() override;
};
