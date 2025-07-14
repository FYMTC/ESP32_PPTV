#pragma once
#include "page.hpp"
class SecondPage : public Page {
public:
    lv_obj_t* onCreate() override;
    void onDestroy() override;
};
