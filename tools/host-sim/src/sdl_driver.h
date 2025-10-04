#pragma once

#include <lvgl.h>

namespace host_sim {

bool init(int width, int height);
void shutdown();

lv_disp_t* register_display(int width, int height);
lv_indev_t* register_pointer();

void pump_events(bool& should_quit);
void delay(uint32_t ms);

}  // namespace host_sim
