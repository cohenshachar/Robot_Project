#ifndef ROBOT_H
#define ROBOT_H

#include "MotorControlUnit.h"
#include "SensorDataHandler.h"
#include "positional_stl.h"

#include "ILogManager.h"
#include "IPathTracker.h"
#include "ICommandParser.h"

#include "Config.h"

class Robot {
public:
      enum class RobotState {
        COMMAND_EXTRACTION, 
        START_POS, 
        IN_CORRIDOR,
        OUT_OF_CORRIDOR,
        TURNING,
        SEEKING_CORRIDOR,
        END_POS,
        FAILED_INIT
    };
private:
    SensorDataHandler snu;
    MotorControlUnit mcu;

    ILogManager* logger;
    ICommandParser* parser;
    RobotState curr_rs;

    void main_logic(double dt);
//tests for pid and commands:
    void send_line_test(double dt);
    void staircase_test(double dt);
    void positive_sinus_test(double dt);
    void full_sinus_test(double dt);
    void distance_test(double dt);
    void turn_test(double dt);
public:
    Robot(ILogManager* logger, IPathTracker* pathTracker, ICommandParser* commandParser);  // Default constructor

    void begin();
    void tick(double dt);
    void sendMovementCmd(MotionCommandType cmd, float target = 1.0f, const IPath* path = nullptr);
    void stop();
    bool isFinishedMovement();
    Line* getCenterLine() const;
    
    RobotState getState() const { return curr_rs; }
};

#endif
