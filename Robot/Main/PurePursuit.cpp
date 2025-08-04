// PurePursuitController.cpp
#include "PurePursuit.h"
#include <cmath>
#include <Arduino.h>
#include "DebugConfig.h"
#define DPRINT(x)    DPRINT_PURE(x)
#define DPRINTLN(x)  DPRINTLN_PURE(x)
#define LOG_FUNC(x) LOG_PURE_MSG(x)

PurePursuitController::PurePursuitController(float lookaheadRadius, float toleranceCM)
    : endGoalTolerance(toleranceCM), lookaheadRadius(lookaheadRadius),desiredSpeed(1.0f), finished(false), currentPath(nullptr) {
    DPRINT_INIT("[PurePursuit] Initialized with lookahead: ");
    DPRINTLN_INIT(lookaheadRadius);
}

void PurePursuitController::updateParams(const IPath* path, float speed) {
    desiredSpeed = speed;
    if(currentPath) delete currentPath;
    currentPath = path;
    DPRINTLN("[PurePursuit] Parameters updated:");
    DPRINT(" - Desired speed: ");
    DPRINTLN(desiredSpeed);
    DPRINT(" - Lookahead radius: ");
    DPRINTLN(lookaheadRadius);
    DPRINT(" - Path pointer: ");
    DPRINTLN(((uintptr_t)path, HEX));
}

Point PurePursuitController::computeTargetPoint(const Pose& robotPose) {
    if (!currentPath) {
        DPRINTLN("[PurePursuit] No path set. Returning current robot location.");
        return robotPose.loc;
    }

    Point pt = currentPath->getLookaheadPoint(robotPose, lookaheadRadius);
    DPRINTLN("[PurePursuit] Computed target point:");
    DPRINT(" - Target X: "); DPRINTLN(pt.x);
    DPRINT(" - Target Y: "); DPRINTLN(pt.y);
    return pt;
}

bool PurePursuitController::computeWheelSpeeds(const Pose& robotPose,
                                               float wheelBase,
                                               float maxRpmLeft,
                                               float maxRpmRight,
                                               float* leftSpeed,
                                               float* rightSpeed) {
    DPRINTLN("[PurePursuit] ➤ computeWheelSpeeds called");

    if (!currentPath || !leftSpeed || !rightSpeed) {
        DPRINTLN("[PurePursuit] ❌ Invalid input: null path or speed pointers");
        return false;
    }

    Point target = computeTargetPoint(robotPose);
    DPRINT("[PurePursuit] 🎯 Target Point: (");
    DPRINT(target.x); DPRINT(", "); DPRINT(target.y); DPRINTLN(")");
    DPRINT("[PurePursuit] 🎯 Current Location: (");
    DPRINT(robotPose.loc.x); DPRINT(", "); DPRINT(robotPose.loc.y); DPRINTLN(")");

    float dx = target.x - robotPose.loc.x;
    float dy = target.y - robotPose.loc.y;
    float distToTarget = sqrt(dx * dx + dy * dy);

    DPRINTLN("[PurePursuit] Δx / Δy:");
    DPRINT("  • dx: "); DPRINTLN(dx);
    DPRINT("  • dy: "); DPRINTLN(dy);
    DPRINT("  • Distance to target: "); DPRINTLN(distToTarget);

    if (distToTarget < endGoalTolerance) {
        DPRINTLN("[PurePursuit] ✅ Target reached within tolerance.");
        *leftSpeed = 0.0f;
        *rightSpeed = 0.0f;
        finished = true;
        return true;
    }

    // If we're already beyond the target point, consider the path done
    // (no need for correction or reversing)
    Point pathDir = target - robotPose.loc;
    if (pathDir.dot(Point(cos(robotPose.theta), sin(robotPose.theta))) < 0) {
        DPRINTLN("[PurePursuit] 🛑 Robot is past the target — finishing.");
        *leftSpeed = 0.0f;
        *rightSpeed = 0.0f;
        finished = true;
        return true;
    }

    // Transform to robot's local coordinate frame
    float cosTheta = cos(-robotPose.theta);
    float sinTheta = sin(-robotPose.theta);
    float localX = cosTheta * dx - sinTheta * dy;
    float localY = sinTheta * dx + cosTheta * dy;

    DPRINTLN("[PurePursuit] 🧭 Transformed to robot frame:");
    DPRINT("  • localX: "); DPRINTLN(localX);
    DPRINT("  • localY: "); DPRINTLN(localY);

    float effectiveLookahead = fmax(lookaheadRadius, 1e-3f);
    float maxCurvature = 2.0f / wheelBase; // You can tune this
    float curvature = (2 * localY) / (effectiveLookahead * effectiveLookahead);
    curvature = fmax(fmin(curvature, maxCurvature), -maxCurvature);

    DPRINT("  • Curvature: "); DPRINTLN(curvature);

    // Speed scaling based on proximity to target
    float speedScale = 1.0f;
    const float slowRadius = lookaheadRadius * 0.3f;

    if (distToTarget < slowRadius) {
        speedScale = powf(distToTarget / slowRadius, 0.8f);
        DPRINT("  • Speed scaling (slow zone): "); DPRINTLN(speedScale);
    }

    speedScale *= desiredSpeed;
    DPRINT("  • Final speed scale (after desiredSpeed): "); DPRINTLN(speedScale);

    // Differential drive wheel ratio
    float leftRatio = (2.0f - curvature * wheelBase) / 2.0f;
    float rightRatio = (2.0f + curvature * wheelBase) / 2.0f;
    float ratioDiff = fabs(leftRatio - rightRatio);
    if (ratioDiff > 1.5f) {
        float blend = 0.5f + 0.5f * (1.5f / ratioDiff);
        leftRatio = leftRatio * blend + rightRatio * (1.0f - blend);
        rightRatio = rightRatio * blend + leftRatio * (1.0f - blend);
    }
    float maxMag = fmax(fabs(leftRatio), fabs(rightRatio));
    float leftSpeedRaw = (leftRatio / maxMag) * speedScale;
    float rightSpeedRaw = (rightRatio / maxMag) * speedScale;

    DPRINTLN("[PurePursuit] 🌀 Raw wheel speed ratios:");
    DPRINT("  • Left ratio: "); DPRINTLN(leftRatio);
    DPRINT("  • Right ratio: "); DPRINTLN(rightRatio);
    DPRINT("  • Scaled left speed: "); DPRINTLN(leftSpeedRaw);
    DPRINT("  • Scaled right speed: "); DPRINTLN(rightSpeedRaw);

    // Enforce minimum speed
    float minAbsSpeed = fmin(fabs(leftSpeedRaw), fabs(rightSpeedRaw));
    const float minSpeed = 0.4f;

    if (minAbsSpeed < minSpeed && minAbsSpeed > 1e-3f) {
        float scaleUp = fmin(minSpeed / minAbsSpeed, 3.0f);
        leftSpeedRaw *= scaleUp;
        rightSpeedRaw *= scaleUp;
        DPRINT("[PurePursuit] 🔼 Speed up to min limit (");
        DPRINT(minSpeed); DPRINT("), scale factor: "); DPRINTLN(scaleUp);
    }

    // Output final wheel speeds
    *leftSpeed = leftSpeedRaw;
    *rightSpeed = rightSpeedRaw;

    DPRINTLN("[PurePursuit] ✅ Final wheel speeds:");
    DPRINT("  • Left: "); DPRINTLN(*leftSpeed);
    DPRINT("  • Right: "); DPRINTLN(*rightSpeed);

    finished = false;
    return true;
}


bool PurePursuitController::isFinished(){
  return finished;
}