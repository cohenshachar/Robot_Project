#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "IPathTracker.h"
#include "Motor.h"           // Motor class for controlling individual motors
#include "positional_stl.h"  // Pose and Point definitions
#include "MotorConfig.h"     // Configuration defines for motors
#include <cmath>
#include "ILogManager.h"

enum class MotionCommandType {
    MOVE_DISTANCE,
    TURN_ANGLE,
    MANUAL_SPEED,
    PATH_TRACK,
    STOP,
};

struct MotionCommand {
    MotionCommand() : type(MotionCommandType::STOP), value(0.0f), path(nullptr) {}
    MotionCommand(MotionCommandType t, float v, const IPath* p) : type(t), value(v), path(p) {};
    MotionCommandType type;
    float value;         // Used for distance, angle, speed
    const IPath* path = nullptr; // Used for pursuit or line updates
};

class MotorControlUnit : public IPoseProvider{
private:

    enum class MovementType {
        IDLE,
        MOVING_DISTANCE,
        TURNING_ANGLE,
        MANUAL_CONTROL,
        PATH_TRACKING
    };
    MotionCommand commandLeash;
    volatile bool hasNewCommand = false;
    volatile bool movementCompeleted = false;
    portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE poseMux = portMUX_INITIALIZER_UNLOCKED;

    Pose robotPos;
    Motor motorLeft, motorRight;
    MovementType currentMovement;
    RelativePIDController syncPid;
    ILogManager* logger;
    IPathTracker* pathTracker;

    TaskHandle_t motionTaskHandle;

    void motionControlLoop(); // Task function
    static void motionTaskEntryPoint(void* pvParameters); // Static wrapper

    void updateOdometry(double dt);
    void sync_Motors(double dt);

    void moveDistance(double distance);
    void turnAngle(double degrees);
    void driveManual(float normalizedSpeed);
    void pathTrack(const IPath* path, float speed);
    void stopMovement();
    
    void HandleCommand();
    void updateMovement(double dt);
    void setMotorsSpeed(float leftTarget, float rightTarget);
public:
    MotorControlUnit(IPathTracker* pathTracker);
    bool begin(ILogManager* db_logger);
    void sendCommand(const MotionCommand& cmd);
    bool isMovementComplete() const;
    void logOdometry();
    Pose getPose() override;
    ~MotorControlUnit() override {if (pathTracker) {delete pathTracker;pathTracker = nullptr;}}
};

//     // ====== Path Planning and Tracking ======
//     // Bezier<2>* path;
//     // Bezier<4>* interPath;
//     double interT;
//     double lookaheadDistance;
//     double planingCycleTime;
//     // Bezier<4>* create_intermidiate_path();
//     // int control_point_search(Point& result, const Point& ref, const Bezier<2>* ref_path, double c);
//     // int planIntermediatePath();
