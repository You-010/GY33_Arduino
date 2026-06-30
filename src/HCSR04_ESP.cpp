#include "HCSR04_ESP.h"
#include <algorithm>
#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_rom_gpio.h"
#include "soc/mcpwm_periph.h"
#include "soc/gpio_sig_map.h"
#include <soc/gpio_struct.h>

ActiveChannelTrigger HCSR04::_globalTriggerRegistry[HCSR04::MAX_SYSTEM_OBJECTS] = {};
uint8_t HCSR04::_registryCount = 0;

mcpwm_cap_timer_handle_t HCSR04::_globalGroupTimers[SOC_MCPWM_GROUPS] = { nullptr };
int HCSR04::_globalGroupTimerRefs[SOC_MCPWM_GROUPS] = { 0 };

static bool IRAM_ATTR hc_sr04_echo_callback(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data) {
    if (!user_data) return false;
    void** context = static_cast<void**>(user_data);
    TaskHandle_t task_to_notify = static_cast<TaskHandle_t>(context[0]);
    uint32_t* start_ticks       = static_cast<uint32_t*>(context[1]);
    
    BaseType_t high_task_wakeup = pdFALSE;

    if (edata->cap_edge == MCPWM_CAP_EDGE_POS) {
        *start_ticks = edata->cap_value;
    } else {
        uint32_t delta = edata->cap_value - (*start_ticks);
        if (task_to_notify) {
            xTaskNotifyFromISR(task_to_notify, delta, eSetValueWithOverwrite, &high_task_wakeup);
        }
    }
    return high_task_wakeup == pdTRUE;
}

bool HCSR04::registerTriggerUse(void* owner, uint8_t pin) {
    if (pin >= 64) { DEBUG_PRINT("REGISTRY ERROR: Pin %d exceeds 64-bit mask boundaries!\n", pin); return false; }

    uint64_t pin_bit = (1ULL << pin);
    int owner_index = -1;

    for (uint8_t i = 0; i < _registryCount; i++) {
        if (_globalTriggerRegistry[i].triggered_pins_mask & pin_bit) {
            if (_globalTriggerRegistry[i].owner_handle == owner) { 
                return true;
            } else { DEBUG_PRINT("REGISTRY CONFLICT: Trigger Pin %d is busy. Stolen by another object!\n", pin); return false; }
        }
        if (_globalTriggerRegistry[i].owner_handle == owner) {
            owner_index = i;
        }
    } 
    if (owner_index != -1) {
        _globalTriggerRegistry[owner_index].triggered_pins_mask |= pin_bit; 
        return true; 
    }
    if (_registryCount >= MAX_SYSTEM_OBJECTS) { DEBUG_PRINT("REGISTRY ERROR: Maximum hardware allocation channels exhausted!\n"); return false; }
    
    _globalTriggerRegistry[_registryCount].owner_handle = owner;
    _globalTriggerRegistry[_registryCount].triggered_pins_mask = pin_bit;
    _registryCount++;
    return true;
}

void HCSR04::unregisterTriggerUse(void* owner) {
    for (uint8_t i = 0; i < _registryCount; i++) {
        if (_globalTriggerRegistry[i].owner_handle == owner) {
            for (uint8_t j = i; j < _registryCount - 1; j++) {
                _globalTriggerRegistry[j] = _globalTriggerRegistry[j + 1];
            }
            _registryCount--;
            break;
        }
    }
}

int HCSR04::allocateChannel(uint8_t echoPin) {
    esp_err_t err = ESP_FAIL;

    for (int g = 0; g < SOC_MCPWM_GROUPS; g++) {
        bool newlyCreated = false;
        if (_globalGroupTimers[g] != nullptr) {
            _capTimer = _globalGroupTimers[g];
            err = ESP_OK;
        } 
        else {
            mcpwm_capture_timer_config_t timer_conf = { 
                .group_id = g, 
                .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT 
            };
            err = mcpwm_new_capture_timer(&timer_conf, &_capTimer);
            if (err == ESP_OK) {
                newlyCreated = true;
            }
        }
        if (err == ESP_OK) {
            mcpwm_capture_channel_config_t chan_conf = {
                .gpio_num = (gpio_num_t)echoPin, 
                .prescale = 1,
                .flags = { .pos_edge = true, .neg_edge = true }
            };
            err = mcpwm_new_capture_channel(_capTimer, &chan_conf, &_capChannel);
            if (err == ESP_OK) {
                _timerGroup = g;
                if (newlyCreated) {
                    _globalGroupTimers[g] = _capTimer;
                    mcpwm_capture_timer_enable(_capTimer);
                    mcpwm_capture_timer_start(_capTimer);
                }
                _globalGroupTimerRefs[g]++;
                break;
            } else {
                if (newlyCreated) { mcpwm_del_capture_timer(_capTimer); }
                _capTimer = nullptr;
            }
        }
    }
    if (_timerGroup == -1) { DEBUG_PRINT("[ERROR] Failed to allocate MCPWM hardware! All groups/channels full.\n"); return -1; }

    int channelIdx = -1;
    for (int ch = 0; ch < SOC_MCPWM_CAPTURE_CHANNELS_PER_TIMER; ch++) {
        int signal_id = mcpwm_periph_signals.groups[_timerGroup].captures[ch].cap_sig;
        if (GPIO.func_in_sel_cfg[signal_id].func_sel == echoPin) {
            channelIdx = ch;
            break;
        }
    }
    if (channelIdx == -1) { DEBUG_PRINT("[ERROR] Critical Matrix Mapping Fault!\n"); return -1; }
    int signalIdx = mcpwm_periph_signals.groups[_timerGroup].captures[channelIdx].cap_sig;
    DEBUG_PRINT("[ROUTING] Echo Pin %d safely verified on Hardware Group %d, Channel %d (Signal Index: %d)\n", 
                  echoPin, _timerGroup, channelIdx, signalIdx);
    mcpwm_capture_timer_get_resolution(_capTimer, &_timerClkHz);
    return signalIdx;
}

void HCSR04::releaseHardware() {
    unregisterTriggerUse(this);
    if (_taskHandle) { 
        vTaskDelete(_taskHandle); _taskHandle = nullptr; }

    if (_capChannel) { 
        mcpwm_capture_channel_disable(_capChannel); 
        mcpwm_del_capture_channel(_capChannel); _capChannel = nullptr; }

    if (_timerGroup >= 0 && _timerGroup < SOC_MCPWM_GROUPS) {
        _globalGroupTimerRefs[_timerGroup]--;
        if (_globalGroupTimerRefs[_timerGroup] <= 0) {
            mcpwm_capture_timer_disable(_globalGroupTimers[_timerGroup]);
            mcpwm_del_capture_timer(_globalGroupTimers[_timerGroup]);
            _globalGroupTimers[_timerGroup] = nullptr;
            _globalGroupTimerRefs[_timerGroup] = 0;
        }
        _timerGroup = -1;
    }
    _capTimer = nullptr;
}

void HCSR04::stop() {
    if (!_isActive) return;
    releaseHardware();
    _isActive = false; 
}

HCSR04Sensor::HCSR04Sensor(uint8_t trigPin, uint8_t echoPin, float minCm, float maxCm,
                           uint8_t sampleSize, uint32_t timeoutMs, uint32_t burstDelayMs) :
    HCSR04(burstDelayMs), 
    _trigPin(trigPin), _echoPin(echoPin), _minCm(minCm), _maxCm(maxCm),
    _sampleSize(sampleSize), _timeoutMs(timeoutMs), 
    _durationUs(0), _medianUs(0), _isManaged(false) {}

HCSR04Sensor::~HCSR04Sensor() {}

bool HCSR04Sensor::begin() {
    _lastError = OK;
    if (_isManaged || _isActive) return true; 
    if (!registerTriggerUse(this, _trigPin)) { _lastError = ERR_PIN_CONFLICT; return false; }
    gpio_reset_pin((gpio_num_t)_trigPin);
    gpio_set_direction((gpio_num_t)_trigPin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_trigPin, 0);
    if (allocateChannel(_echoPin) == -1) { _lastError = ERR_NO_CHANNELS; return false; }

    if (xTaskCreatePinnedToCore(
        [](void* p) {
            HCSR04Sensor* inst = static_cast<HCSR04Sensor*>(p);
            while (true) {
                inst->processSensorLoop();
                vTaskDelay(pdMS_TO_TICKS(inst->_burstDelayMs));
            }
        }, "stdAloneSensor", 3072, this, 1, &_taskHandle, 1
    ) != pdPASS) { _lastError = ERR_TASK_CREATION; return false; }

    _isrContext[0] = (void*)_taskHandle; 
    _isrContext[1] = (void*)&_isrStartTicks;
    mcpwm_capture_event_callbacks_t cbs = { .on_cap = hc_sr04_echo_callback };
    mcpwm_capture_channel_register_event_callbacks(_capChannel, &cbs, _isrContext);

    mcpwm_capture_channel_enable(_capChannel);
    _isActive = true;
    return true;
}

void HCSR04Sensor::processSensorLoop() {
    pingMedian();
}

bool HCSR04Sensor::pingUs() {
    _durationUs = 0;
    uint32_t tof_ticks = 0;
    xTaskNotifyWait(0x00, ULONG_MAX, &tof_ticks, 0);

    gpio_set_level((gpio_num_t)_trigPin, 1);
    esp_rom_delay_us(10);
    gpio_set_level((gpio_num_t)_trigPin, 0);

    if (xTaskNotifyWait(0x00, ULONG_MAX, &tof_ticks, pdMS_TO_TICKS(_timeoutMs)) == pdTRUE) {
        if (tof_ticks > 0) {
            float pulse_width_us = tof_ticks * (1000000.0 / _timerClkHz);
            _durationUs = (unsigned long)pulse_width_us;
            return true;
        }
    }
    _durationUs = 0;
    return false;
}

void HCSR04Sensor::pingMedian() {
    uint8_t targetSamples = (_sampleSize > MAX_SAMPLE_SIZE) ? MAX_SAMPLE_SIZE : _sampleSize;
    std::array<unsigned long, MAX_SAMPLE_SIZE> buffer = { 0 };
    uint8_t validCount = 0; uint8_t attempts = 0;

    while (validCount < _sampleSize && attempts < (_sampleSize * 2)) {
        attempts++;
        if (pingUs() && _durationUs > 0) buffer[validCount++] = _durationUs;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (validCount > _sampleSize / 2) {
        sortBuffer(buffer.data(), validCount);
        _medianUs = buffer[validCount / 2];
    } else { _medianUs = 0; }
}

void HCSR04Sensor::sortBuffer(unsigned long* arr, uint8_t size) {
    for (uint8_t i = 1; i < size; i++) {
        unsigned long key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

float HCSR04Sensor::readDistanceCm(DistanceMode mode, float tempC) {
    uint8_t m = static_cast<uint8_t>(MEDIAN | VALID);
    m &= static_cast<uint8_t>(mode);
    unsigned long us = getDurationUs(static_cast<DistanceMode>(m));
    float speedCmPerUs = (331.3f + (0.606f * tempC)) * 0.0001f;
    float distance = (us * speedCmPerUs) / 2.0f;
    if (m & static_cast<uint8_t>(VALID)) {
        if (!isValidReading(distance)) return -1.0f;
    }
    return distance;
}

unsigned long HCSR04Sensor::getDurationUs(DistanceMode mode) {
    return (static_cast<uint8_t>(mode) & static_cast<uint8_t>(MEDIAN)) ? _medianUs : _durationUs;
}

bool HCSR04Sensor::isValidReading(float d) {
    return d >= _minCm && d <= _maxCm;
}

// multiplex controller
HCSR04Controller::HCSR04Controller(uint32_t burstDelayMs) : 
    HCSR04(burstDelayMs) {}

HCSR04Controller::~HCSR04Controller() {
    while (_sensorCount > 0) {
        removeSensor(_sensors[_sensorCount - 1]);
    }
}

bool HCSR04Controller::addSensor(HCSR04Sensor* sensor) {
    _lastError = OK;
    if (sensor == nullptr) { _lastError = ERR_INVALID_POINTER; return false; }
    if (_sensorCount >= MAX_SENSORS) { _lastError = ERR_CONTROLLER_FULL; return false; }
    sensor->releaseHardware(); 
    if (_isActive) {
        if (!registerTriggerUse(this, sensor->_trigPin)) {
            DEBUG_PRINT("[WARNING] Dynamic add failed: Pin %d conflict!\n", sensor->_trigPin);
            _lastError = ERR_PIN_CONFLICT; return false; 
        }
        gpio_reset_pin((gpio_num_t)sensor->_trigPin);
        gpio_set_direction((gpio_num_t)sensor->_trigPin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)sensor->_trigPin, 0);
        sensor->_timerClkHz = this->_timerClkHz;
    }
    sensor->_isManaged = true; 
    sensor->_isActive = false;
    _sensors[_sensorCount++] = sensor;
    return true;
}

bool HCSR04Controller::removeSensor(HCSR04Sensor* sensor) {
    _lastError = OK;
    if (sensor == nullptr) { _lastError = ERR_INVALID_POINTER; return false; }
    if (_sensorCount == 0) { _lastError = ERR_CONTROLLER_EMPTY; return false; }
    auto it = std::find(_sensors.begin(), _sensors.begin() + _sensorCount, sensor);
    if (it == _sensors.begin() + _sensorCount) { _lastError = ERR_NOT_FOUND; return false; } 
    uint8_t position = std::distance(_sensors.begin(), it);

    uint8_t targetTrigPin = sensor->_trigPin;
    bool pinStillInUse = std::any_of(_sensors.begin(), _sensors.begin() + _sensorCount, 
        [sensor, targetTrigPin](HCSR04Sensor* s) {
            return (s != sensor && s->_trigPin == targetTrigPin);
        }
    );
    if (!pinStillInUse) {
        for (uint8_t i = 0; i < _registryCount; i++) {
            if (_globalTriggerRegistry[i].owner_handle == this) {
                _globalTriggerRegistry[i].triggered_pins_mask &= ~(1ULL << targetTrigPin);
                if (_globalTriggerRegistry[i].triggered_pins_mask == 0) {
                    unregisterTriggerUse(this);
                }
                break;
            }
        }
    }
    sensor->_isManaged = false;
    for (uint8_t i = position; i < _sensorCount - 1; i++) {
        _sensors[i] = _sensors[i + 1];
    }
    _sensors[_sensorCount - 1] = nullptr;
    _sensorCount--;
    return true;
}

bool HCSR04Controller::begin(bool failOnConflict) {
    _lastError = OK;
    if (_sensorCount == 0) { _lastError = ERR_CONTROLLER_EMPTY; return false; }
    if (_isActive) return true;

    uint8_t i = 0;
    while (i < _sensorCount) {
        HCSR04Sensor* sensor = _sensors[i];
        if (registerTriggerUse(this, sensor->_trigPin)) {
            gpio_reset_pin((gpio_num_t)sensor->_trigPin);
            gpio_set_direction((gpio_num_t)sensor->_trigPin, GPIO_MODE_OUTPUT);
            gpio_set_level((gpio_num_t)sensor->_trigPin, 0);
            i++;
        } else {
            DEBUG_PRINT("[WARNING] Controller skipping Sensor (Trig: %d, Echo: %d) due to Trigger Pin Conflict!\n", 
                          sensor->_trigPin, sensor->_echoPin);
            _lastError = ERR_PIN_CONFLICT;
            if (failOnConflict) return false; 

            sensor->_isManaged = false;
            for (uint8_t j = i; j < _sensorCount - 1; j++) { _sensors[j] = _sensors[j + 1]; }
            _sensors[_sensorCount - 1] = nullptr;
            _sensorCount--;
        }
    }
    if (_sensorCount == 0) {
        DEBUG_PRINT("[ERROR] Controller begin failed: All attached sensors had pin conflicts.\n");
        _lastError = ERR_PIN_CONFLICT; return false;
    }

    _cachedSignalIdx = allocateChannel(_sensors[0]->_echoPin);
    if (_cachedSignalIdx == -1) { _lastError = ERR_NO_CHANNELS; return false; }

    for (uint8_t i = 0; i < _sensorCount; i++) {
        _sensors[i]->_timerClkHz = this->_timerClkHz;
    }

    if (xTaskCreatePinnedToCore(
        [](void* p) {
            HCSR04Controller* m = static_cast<HCSR04Controller*>(p);
            while (true) {
                m->processControllerLoop();
                vTaskDelay(pdMS_TO_TICKS(m->_burstDelayMs));
            }
        }, "mngrSequencer", 4096, this, 1, &_taskHandle, 1
    ) != pdPASS) { _lastError = ERR_TASK_CREATION; return false; }

    _isrContext[0] = (void*)_taskHandle; 
    _isrContext[1] = (void*)&_isrStartTicks;
    mcpwm_capture_event_callbacks_t cbs = { .on_cap = hc_sr04_echo_callback };
    mcpwm_capture_channel_register_event_callbacks(_capChannel, &cbs, _isrContext);
    mcpwm_capture_channel_enable(_capChannel);
    _isActive = true;
    return true;
}

void HCSR04Controller::processControllerLoop() {
    std::for_each(_sensors.begin(), _sensors.begin() + _sensorCount, [this](HCSR04Sensor* sensor) {
        if (sensor == nullptr) return;
        updateHardwareSignalRouting(sensor->_echoPin);
        sensor->pingMedian();
        vTaskDelay(pdMS_TO_TICKS(10));
    });
}

void HCSR04Controller::updateHardwareSignalRouting(uint8_t echoPin) {
    gpio_reset_pin((gpio_num_t)echoPin);
    gpio_set_direction((gpio_num_t)echoPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)echoPin, GPIO_PULLDOWN_ONLY);
    esp_rom_gpio_connect_in_signal(echoPin, _cachedSignalIdx, false);

    _isrContext[0] = (void*)_taskHandle;
    _isrContext[1] = (void*)&_isrStartTicks;
    mcpwm_capture_event_callbacks_t cbs = {
        .on_cap = hc_sr04_echo_callback
    };
    mcpwm_capture_channel_register_event_callbacks(_capChannel, &cbs, _isrContext);
}
