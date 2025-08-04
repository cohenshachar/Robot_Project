#pragma once
#include "positional_stl.h"

class IPathTracker {
public:
    virtual void updateParams(const IPath* path, float speed) = 0;

    virtual bool computeWheelSpeeds(const Pose& robotPose,
                                     float wheelBase,
                                     float maxRpm1,
                                     float maxRpm2,
                                     float* leftSpeed,
                                     float* rightSpeed) = 0;
    virtual bool isFinished() = 0;
    virtual ~IPathTracker() {}
};

namespace {
    template<typename T>
    T clamp(T val, T minVal, T maxVal) {
        return (val < minVal) ? minVal : (val > maxVal ? maxVal : val);
    }
}
