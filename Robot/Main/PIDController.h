#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H
#include <cmath>


class PIDController {
public:
    virtual ~PIDController() = default;

    virtual void reset() = 0;
    virtual void setSetpoint(float setpoint) = 0;
    virtual float update(float measured, double dt) = 0;
    virtual bool finished() const = 0;

    virtual float getSetPoint() const = 0;
    virtual float getPrevError() const = 0;
    virtual float getIntegError() const = 0;

    virtual void setOutputLimits(float minOutput, float maxOutput) = 0;
    virtual void setIntegralLimits(float minIntegral, float maxIntegral) = 0;
};


class AbsolutePIDController : public PIDController {
public:
    AbsolutePIDController(float kp, float ki, float kd, float minimalErr = 1.0f);

    void reset() override;
    void setSetpoint(float setpoint) override;
    float update(float measured, double dt) override;
    bool finished() const override;

    float getSetPoint() const override;
    float getPrevError() const override;
    float getIntegError() const override;

    void setOutputLimits(float minOutput, float maxOutput) override;
    void setIntegralLimits(float minIntegral, float maxIntegral) override;

private:
    float kp, ki, kd;
    float setpoint;
    float prevError;
    float integral;
    float minimalErr;
    float outputMin, outputMax;
    float integralMin, integralMax;
    bool reached;
};

class RelativePIDController : public PIDController {
public:
    RelativePIDController(float kp, float ki, float kd, float minimalErr = 1.0f);

    void reset() override;
    void setSetpoint(float setpoint) override;
    float update(float measured, double dt) override;
    bool finished() const override;

    float getSetPoint() const override;
    float getPrevError() const override;
    float getIntegError() const override;

    void setOutputLimits(float minOutput, float maxOutput) override;
    void setIntegralLimits(float minIntegral, float maxIntegral) override;

private:
    float kp, ki, kd, minimalErr;
    float setpoint;
    float prevError;
    float prevPrevError;
    float integral;
    float prevOutput;
    float outputMin, outputMax;
    float integralMin, integralMax;
    bool reached = false;
};


#endif // PIDCONTROLLER_H
