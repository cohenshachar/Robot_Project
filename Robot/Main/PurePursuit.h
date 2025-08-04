#pragma once
#include "positional_stl.h"
#include "IPathTracker.h"

class PurePursuitController : public IPathTracker {
public:
    PurePursuitController(float lookaheadRadius, float toleranceCM);

    void updateParams(const IPath* path, float speed) override;
    bool computeWheelSpeeds(const Pose& robotPose,
                             float wheelBase,
                             float maxRpmLeft,
                             float maxRpmRight,
                             float* leftSpeed,
                             float* rightSpeed) override;
    bool isFinished() override;

private:
    Point computeTargetPoint(const Pose& robotPose);
    float endGoalTolerance;
    float lookaheadRadius;
    float desiredSpeed;
    bool finished;
    const IPath* currentPath = nullptr;
};

