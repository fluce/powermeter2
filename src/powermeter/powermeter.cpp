//#ifdef ESPHOME
#include "esphome/core/log.h"
#include "powermeter.h"

extern void internal_setup();
extern uint8_t internal_loop();

namespace esphome {
namespace powermeter_sensor {

static const char *TAG = "powermeter";

void PowerMeterComponent::setup() {
    ESP_LOGI(TAG, "PowerMeter component setup");
    internal_setup();
}

void PowerMeterComponent::update() {
    ESP_LOGI(TAG, "PowerMeter component update");
    if (result_mask) {
        for (int i = 0; i < CHANNELS; i++) {
            if (result_mask & (1 << i)) {
                this->internal_channels[i].publish_state();
                result_mask &= ~(1 << i);
            }
        }
    }
}

void PowerMeterComponent::loop() {
    uint8_t ret=internal_loop();
    result_mask |= ret;    
    /*try {
        internal_loop();
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Exception in loop: %s", e.what());
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception in loop");
    }*/
}

void PowerMeterComponent::dump_config(){
    ESP_LOGCONFIG(TAG, "PowerMeter component");
}

}
}
//#endif