#include "services/state_persistence.h"

#include <esp_log.h>
#include <esp_check.h>
#include <nvs.h>
#include <nvs_flash.h>

namespace dial::persistence {

namespace {
constexpr const char* TAG = "StatePersist";
constexpr const char* kNamespace = "timer";
constexpr const char* kKeyState = "state";
constexpr const char* kKeySetpoint = "setpoint";
constexpr const char* kKeyRemaining = "remain";

nvs_handle_t g_handle = 0;
bool g_initialised = false;

uint8_t encode_state(TimerState state) {
    return static_cast<uint8_t>(state);
}

TimerState decode_state(uint8_t raw) {
    if (raw > static_cast<uint8_t>(TimerState::Finished)) {
        return TimerState::Idle;
    }
    return static_cast<TimerState>(raw);
}

}  // namespace

esp_err_t init() {
    if (g_initialised) {
        return ESP_OK;
    }

    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &g_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to open NVS handle");

    g_initialised = true;
    return ESP_OK;
}

esp_err_t save(const TimerSnapshot& snapshot) {
    if (!g_initialised) {
        ESP_RETURN_ON_ERROR(init(), TAG, "NVS init failed");
    }

    ESP_RETURN_ON_ERROR(nvs_set_u8(g_handle, kKeyState, encode_state(snapshot.state)), TAG, "set state failed");
    ESP_RETURN_ON_ERROR(nvs_set_u32(g_handle, kKeySetpoint, snapshot.setpoint_seconds), TAG, "set setpoint failed");
    ESP_RETURN_ON_ERROR(nvs_set_u32(g_handle, kKeyRemaining, snapshot.remaining_ms), TAG, "set remaining failed");
    return nvs_commit(g_handle);
}

esp_err_t load(RestoredState* out) {
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = RestoredState{};
    if (!g_initialised) {
        auto err = init();
        if (err != ESP_OK) {
            return err;
        }
    }

    uint8_t state_raw = 0;
    uint32_t setpoint = 0;
    uint32_t remaining = 0;

    esp_err_t err = nvs_get_u8(g_handle, kKeyState, &state_raw);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_u32(g_handle, kKeySetpoint, &setpoint);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_u32(g_handle, kKeyRemaining, &remaining);
    if (err != ESP_OK) {
        return err;
    }

    out->state = decode_state(state_raw);
    out->setpoint_seconds = setpoint;
    out->remaining_ms = remaining;
    out->valid = true;
    return ESP_OK;
}

}  // namespace dial::persistence
