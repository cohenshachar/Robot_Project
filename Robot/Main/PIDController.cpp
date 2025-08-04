#include "PIDController.h"


// Clamp helper for environments without std::clamp
template<typename T>
static inline T clamp(T val, T minVal, T maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}


// =====================================
// AbsolutePIDController Implementation
// =====================================

AbsolutePIDController::AbsolutePIDController(float kp, float ki, float kd, float minimalErr)
    : kp(kp), ki(ki), kd(kd), minimalErr(minimalErr), setpoint(0), prevError(0), integral(0),
      outputMin(-1.0f), outputMax(1.0f), integralMin(-1.0f), integralMax(1.0f), reached(false) {}

void AbsolutePIDController::reset() {
    prevError = 0;
    integral = 0;
    reached = false;
}

void AbsolutePIDController::setSetpoint(float sp) {
    setpoint = sp;
}

float AbsolutePIDController::update(float measured, double dt) {
    float error = setpoint - measured;
    integral += error * dt;
    integral = clamp(integral, integralMin, integralMax);

    float derivative = (dt > 0.0f) ? (error - prevError) / dt : 0.0f;
    float output = kp * error + ki * integral + kd * derivative;
    output = clamp(output, outputMin, outputMax);

    prevError = error;
    reached = std::fabs(error) < minimalErr;

    return output;
}

bool AbsolutePIDController::finished() const {
    return reached;
}

float AbsolutePIDController::getSetPoint() const { return setpoint; }
float AbsolutePIDController::getPrevError() const { return prevError; }
float AbsolutePIDController::getIntegError() const { return integral; }

void AbsolutePIDController::setOutputLimits(float minOutput, float maxOutput) {
    outputMin = minOutput;
    outputMax = maxOutput;
}

void AbsolutePIDController::setIntegralLimits(float minIntegral, float maxIntegral) {
    integralMin = minIntegral;
    integralMax = maxIntegral;
}


// =====================================
// RelativePIDController Implementation
// =====================================


RelativePIDController::RelativePIDController(float kp, float ki, float kd, float minimalErr)
    : kp(kp), ki(ki), kd(kd), minimalErr(minimalErr),
      setpoint(0), prevError(0), prevPrevError(0),
      integral(0), prevOutput(0),
      outputMin(-1.0f), outputMax(1.0f),
      integralMin(-1.0f), integralMax(1.0f),
      reached(false)
{}

void RelativePIDController::reset() {
    prevError = 0;
    prevPrevError = 0;
    integral = 0;
    prevOutput = 0;
    reached = false;
}

void RelativePIDController::setSetpoint(float sp) {
    setpoint = sp;
}

float RelativePIDController::update(float measured, double dt) {
    float error = setpoint - measured;

    float deltaOutput = kp * (error - prevError)
                      + ki * error * dt
                      + kd * ((error - 2 * prevError + prevPrevError) / dt);

    float output = prevOutput + deltaOutput;
    output = clamp(output, outputMin, outputMax);

    prevPrevError = prevError;
    prevError = error;
    prevOutput = output;

    reached = std::fabs(error) < minimalErr;

    return output;
}


bool RelativePIDController::finished() const {
    return reached;
}

float RelativePIDController::getSetPoint() const {
    return setpoint;
}

float RelativePIDController::getPrevError() const {
    return prevError;
}

float RelativePIDController::getIntegError() const {
    return integral;
}

void RelativePIDController::setOutputLimits(float minOutput, float maxOutput) {
    outputMin = minOutput;
    outputMax = maxOutput;
}

void RelativePIDController::setIntegralLimits(float minIntegral, float maxIntegral) {
    integralMin = minIntegral;
    integralMax = maxIntegral;
}
