#pragma once
#include <cstdint>     // for uint8_t, int8_t
#include <cstddef>     // for size_t
#include <limits>      // for UINT32_MAX
#include "MotorConfig.h"      // for UINT32_MAX

struct EncoderEntry {
    unsigned long timestamp;
    int8_t dir;
    uint8_t pwm;
};

class EncoderHistory {

public:
    void addFromISR(unsigned long ts, int8_t dir, uint8_t pwm);   // From ISR
    void addFromMain(unsigned long ts, int8_t dir, uint8_t pwm);  // From main
    int getEncoderAccumulator() ;
    unsigned long getLatestTimestamp() ;
    size_t copyHistory(EncoderEntry* outArray, size_t maxSize);
    void clear();

private:
    EncoderEntry buffer_[ENCODER_BUFFER_SIZE];

    size_t head_ = 0;       // next write position; guarded by mux_
    size_t tail_ = 0;       // next read position; main only modifies
    int encoderAccumulator_ = 0; // guarded by mux_
};
