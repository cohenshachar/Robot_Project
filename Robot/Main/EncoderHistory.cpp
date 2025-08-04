#include "EncoderHistory.h"
#include <Arduino.h>
#include "DebugConfig.h"

#define DPRINT(x)    DPRINT_MOTOR(x)
#define DPRINTLN(x)  DPRINTLN_MOTOR(x)
#define LOG_FUNC(x)  LOG_MOTOR_MSG(x)

void EncoderHistory::addFromISR(unsigned long ts, int8_t dir, uint8_t pwm) {
    size_t index = head_ % ENCODER_BUFFER_SIZE;
    buffer_[index] = {ts, dir, pwm};
    encoderAccumulator_ += dir;
    head_ = (index + 1) % ENCODER_BUFFER_SIZE;
}

void EncoderHistory::addFromMain(unsigned long ts, int8_t dir, uint8_t pwm) {
    addFromISR(ts, dir, pwm);
}

int EncoderHistory::getEncoderAccumulator() {
    return encoderAccumulator_;
}

void EncoderHistory::clear() {
    tail_ = head_;
    encoderAccumulator_ = 0;
}

unsigned long EncoderHistory::getLatestTimestamp() {
    size_t head = head_;
    if (head == tail_) {
        return 0;
    }
    size_t lastIndex = (head + ENCODER_BUFFER_SIZE - 1) % ENCODER_BUFFER_SIZE;
    unsigned long ts = buffer_[lastIndex].timestamp;
    return ts;
}

// Copies current encoder history entries into a user-provided array.
// Assumes the caller handles synchronization.
// Returns the number of entries copied.
size_t EncoderHistory::copyHistory(EncoderEntry* outArray, size_t maxSize) {
    if (head_ == tail_) return 0;  // empty

    size_t count = 0;
    int i = (int(head_) - 1 + ENCODER_BUFFER_SIZE) % ENCODER_BUFFER_SIZE;
    int end = (int(tail_) - 1 + ENCODER_BUFFER_SIZE) % ENCODER_BUFFER_SIZE;

    while (i != end && count < maxSize) {
        outArray[count++] = buffer_[i];
        i = (i - 1 + ENCODER_BUFFER_SIZE) % ENCODER_BUFFER_SIZE;
    }
    return count;
}
