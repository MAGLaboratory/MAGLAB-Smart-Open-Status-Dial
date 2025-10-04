#include "sdl_driver.h"

#include <SDL.h>

#include <vector>

#include "esp_log.h"

namespace host_sim {

namespace {

constexpr const char* TAG = "SDLDriver";

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
int g_width = 0;
int g_height = 0;

std::vector<uint32_t> g_framebuffer;
std::vector<lv_color_t> g_draw_buffer_storage;
lv_disp_draw_buf_t g_draw_buf;

lv_disp_drv_t g_disp_drv;
lv_disp_t* g_display = nullptr;

lv_indev_drv_t g_pointer_drv;
lv_indev_t* g_pointer = nullptr;

struct MouseState {
    int16_t x = 0;
    int16_t y = 0;
    bool pressed = false;
};

MouseState g_mouse;

void display_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    if (g_texture == nullptr) {
        lv_disp_flush_ready(disp);
        return;
    }

    const int32_t x1 = area->x1;
    const int32_t y1 = area->y1;
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;

    for (int32_t row = 0; row < height; ++row) {
        const lv_color_t* src_row = color_p + row * width;
        uint32_t* dst_row = g_framebuffer.data() + (y1 + row) * g_width + x1;
        for (int32_t col = 0; col < width; ++col) {
            const lv_color_t pixel = src_row[col];
            const uint32_t rgb = lv_color_to32(pixel) & 0x00FFFFFFu;
            dst_row[col] = 0xFF000000u | rgb;
        }
    }

    SDL_UpdateTexture(g_texture, nullptr, g_framebuffer.data(), g_width * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
    SDL_RenderPresent(g_renderer);

    lv_disp_flush_ready(disp);
}

void pointer_read(lv_indev_drv_t*, lv_indev_data_t* data) {
    data->point.x = g_mouse.x;
    data->point.y = g_mouse.y;
    data->state = g_mouse.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

}  // namespace

bool init(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        ESP_LOGE(TAG, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    g_width = width;
    g_height = height;

    g_window = SDL_CreateWindow("M5 Dial UI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_window) {
        ESP_LOGE(TAG, "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        ESP_LOGW(TAG, "Falling back to software renderer");
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        ESP_LOGE(TAG, "SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!g_texture) {
        ESP_LOGE(TAG, "SDL_CreateTexture failed: %s", SDL_GetError());
        return false;
    }

    g_framebuffer.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0xFF000000u);
    return true;
}

lv_disp_t* register_display(int width, int height) {
    if (g_display) {
        return g_display;
    }

    g_draw_buffer_storage.assign(static_cast<size_t>(width) * static_cast<size_t>(height), lv_color_black());
    lv_disp_draw_buf_init(&g_draw_buf, g_draw_buffer_storage.data(), nullptr,
                          static_cast<uint32_t>(g_draw_buffer_storage.size()));

    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res = width;
    g_disp_drv.ver_res = height;
    g_disp_drv.draw_buf = &g_draw_buf;
    g_disp_drv.flush_cb = display_flush;

    g_display = lv_disp_drv_register(&g_disp_drv);
    return g_display;
}

lv_indev_t* register_pointer() {
    if (g_pointer) {
        return g_pointer;
    }

    lv_indev_drv_init(&g_pointer_drv);
    g_pointer_drv.type = LV_INDEV_TYPE_POINTER;
    g_pointer_drv.read_cb = pointer_read;
    g_pointer = lv_indev_drv_register(&g_pointer_drv);
    return g_pointer;
}

void pump_events(bool& should_quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                should_quit = true;
                break;
            case SDL_MOUSEMOTION:
                g_mouse.x = static_cast<int16_t>(event.motion.x);
                g_mouse.y = static_cast<int16_t>(event.motion.y);
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    g_mouse.pressed = true;
                    g_mouse.x = static_cast<int16_t>(event.button.x);
                    g_mouse.y = static_cast<int16_t>(event.button.y);
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    g_mouse.pressed = false;
                    g_mouse.x = static_cast<int16_t>(event.button.x);
                    g_mouse.y = static_cast<int16_t>(event.button.y);
                }
                break;
            default:
                break;
        }
    }
}

void delay(uint32_t ms) {
    SDL_Delay(ms);
}

void shutdown() {
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = nullptr;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }
    SDL_Quit();
}

}  // namespace host_sim
