/* 
 * HCSR04_ESP_Hardware - hardware-based (MCPWM) ESP32 Ultrasonic Library by me.
 * This file is free software released under the GNU General Public License v3.0.
 * See the LICENSE file in the project root for full license terms.
*/

#ifndef HCSR04_ESP_H
#define HCSR04_ESP_H

#include <array>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/clk_tree_defs.h" 
#include "soc/soc_caps.h"
#if !defined(SOC_MCPWM_SUPPORTED) || (SOC_MCPWM_SUPPORTED == 0)
  #error "Compilation Failed: The selected ESP32 variant does not feature hardware MCPWM modules. This library cannot be used with this board."
#endif

#include <driver/mcpwm_cap.h>

#define HCSR04_DEBUG 0
#if HCSR04_DEBUG
  #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...) 
#endif

struct ActiveChannelTrigger {
    void* owner_handle;     
    uint64_t triggered_pins_mask;    
};

class HCSR04 {
public:
    enum Error : uint8_t {
        OK = 0,
        ERR_INVALID_POINTER,
        ERR_PIN_CONFLICT,
        ERR_NO_CHANNELS,
        ERR_TASK_CREATION,
        ERR_NOT_FOUND,
        ERR_CONTROLLER_FULL,
        ERR_CONTROLLER_EMPTY
    };
    Error getLastError() const { return _lastError; };
    const char* getLastErrorString() const {
        switch (_lastError) {
            case OK:                    return "HCSR04_OK";
            case ERR_INVALID_POINTER:   return "HCSR04_ERR_INVALID_POINTER";
            case ERR_PIN_CONFLICT:      return "HCSR04_ERR_PIN_CONFLICT";
            case ERR_NO_CHANNELS:       return "HCSR04_ERR_NO_CHANNELS";
            case ERR_TASK_CREATION:     return "HCSR04_ERR_TASK_CREATION";
            case ERR_NOT_FOUND:         return "HCSR04_ERR_NOT_FOUND";
            case ERR_CONTROLLER_FULL:   return "HCSR04_ERR_CONTROLLER_FULL";
            case ERR_CONTROLLER_EMPTY:  return "HCSR04_ERR_CONTROLLER_EMPTY";
            default:                    return "UNKNOWN_ERROR";
        }
    }

    void stop();

protected:
    Error _lastError = OK;
    
    bool                        _isActive = false;
    mcpwm_cap_timer_handle_t    _capTimer = nullptr;
    mcpwm_cap_channel_handle_t  _capChannel = nullptr;
    uint32_t                    _timerClkHz = APB_CLK_FREQ;
    int                         _timerGroup = -1;
    TaskHandle_t                _taskHandle = nullptr; 
    uint32_t                    _isrStartTicks = 0;
    void*                       _isrContext[2];
    uint32_t                    _burstDelayMs;

    HCSR04(uint32_t burstDelayMs) : _burstDelayMs(burstDelayMs) {}
    virtual ~HCSR04() { releaseHardware(); }

    int  allocateChannel(uint8_t echoPin);
    void releaseHardware();

    static constexpr uint8_t MAX_SYSTEM_OBJECTS = SOC_MCPWM_GROUPS * SOC_MCPWM_CAPTURE_CHANNELS_PER_TIMER;
    static ActiveChannelTrigger _globalTriggerRegistry[MAX_SYSTEM_OBJECTS];
    static uint8_t _registryCount;
    static bool registerTriggerUse(void* owner, uint8_t pin);
    static void unregisterTriggerUse(void* owner);

    static mcpwm_cap_timer_handle_t _globalGroupTimers[SOC_MCPWM_GROUPS];
    static int _globalGroupTimerRefs[SOC_MCPWM_GROUPS];
};

class HCSR04Sensor : public HCSR04 {
public:
    enum class DistanceMode : uint8_t {
        MEDIAN = 0x01,          // bit 0
        SINGLE = (uint8_t)~0x01,

        VALID  = 0x02,          // bit 1
        RAW    = (uint8_t)~0x02  
    };
    using enum DistanceMode;
    friend inline DistanceMode operator|(DistanceMode lhs, DistanceMode rhs) {
        uint8_t l = static_cast<uint8_t>(lhs);
        uint8_t r = static_cast<uint8_t>(rhs);
        if ((l & 0xFC) && (r & 0xFC)) return static_cast<DistanceMode>(l & r); 
        return static_cast<DistanceMode>(l | r);
    }
    friend inline DistanceMode operator&(DistanceMode lhs, DistanceMode rhs) {
        return static_cast<DistanceMode>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
    }

    HCSR04Sensor(uint8_t trigPin, uint8_t echoPin, float minCm = 2.0f, float maxCm = 400.0f,
                 uint8_t sampleSize = 5, uint32_t timeoutMs = 35, uint32_t burstDelayMs = 60);
    ~HCSR04Sensor();

    bool begin();
    float readDistanceCm(DistanceMode mode = (MEDIAN | VALID), float tempC = 20.0f);
    float readDistanceCm(float tempC) { return readDistanceCm(MEDIAN | VALID, tempC); }
    unsigned long getDurationUs(DistanceMode mode = MEDIAN); 

private:
    friend class HCSR04Controller;
    bool _isManaged;

    uint8_t _trigPin;
    uint8_t _echoPin;
    float   _minCm; 
    float   _maxCm;

    static const uint8_t MAX_SAMPLE_SIZE = 20;
    uint8_t  _sampleSize;
    uint32_t _timeoutMs;
    
    volatile unsigned long _durationUs; 
    volatile unsigned long _medianUs;  

    bool pingUs();
    void pingMedian(); 
    void sortBuffer(unsigned long* arr, uint8_t size);
    void processSensorLoop();
    bool isValidReading(float distance);
};

class HCSR04Controller : public HCSR04 {
public:
    HCSR04Controller(uint32_t burstDelayMs = 60);
    ~HCSR04Controller();

    bool addSensor(HCSR04Sensor* sensor);
    bool removeSensor(HCSR04Sensor* sensor);
    bool begin(bool failOnConflict = false);    

private:
    static constexpr uint8_t MAX_SENSORS = 30;
    std::array<HCSR04Sensor*, MAX_SENSORS> _sensors{};
    size_t _sensorCount = 0;
    int _cachedSignalIdx;
    void updateHardwareSignalRouting(uint8_t echoPin);
    void processControllerLoop();
};

#endif