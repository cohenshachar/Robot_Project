#include "Robot.h"
#include "DebugConfig.h"
#include "MazeCommands.h"

#define DPRINT(x)    DPRINT_ROBOT(x)
#define DPRINTLN(x)  DPRINTLN_ROBOT(x)
#define LOG_FUNC(x)  LOG_ROBOT_MSG(x)

Robot::Robot(ILogManager* logger, IPathTracker* pathTracker, ICommandParser* commandParser)
    : logger(logger), parser(commandParser), mcu(pathTracker), snu(&mcu), curr_rs(RobotState::COMMAND_EXTRACTION)
    // , actions{MazeAction::LEFT,
    //           MazeAction::LEFT,
    //           MazeAction::FORWARD,
    //     //MazeAction::RIGHT,
    //     MazeAction::END}
{
    DPRINT_INIT("[Robot] Built...");
    if (logger) DPRINT_INIT(" with Logger...");
    if (pathTracker) DPRINT_INIT(" with Path Tracker...");
    if (commandParser) DPRINT_INIT(" with Command Parser...");
    DPRINTLN_INIT("");
}

void Robot::begin() {
    DPRINTLN_INIT("[Robot::begin] Starting robot initialization...");
    DPRINTLN_INIT("[Robot::begin] Initializing SensorDataHandler...");
    bool snu_flag = snu.begin(logger);
    DPRINTLN_INIT("[Robot::begin] SensorDataHandler initialized.");

    DPRINTLN_INIT("[Robot::begin] Initializing MotorControlUnit...");
    bool mcu_flag = mcu.begin(logger);
    DPRINTLN_INIT("[Robot::begin] MotorControlUnit initialized.");
    if(snu_flag && mcu_flag && parser && logger)
      DPRINTLN_INIT("[Robot::begin] Robot initialization COMPLETE.");
    else{
      sendMovementCmd(MotionCommandType::TURN_ANGLE, 3600.0, nullptr);
      curr_rs = RobotState::FAILED_INIT;
    }
}

void Robot::sendMovementCmd(MotionCommandType cmd, float target, const IPath* path) {
    DPRINTLN("[Robot::sendMovementCmd] Sending command to MCU...");
    mcu.sendCommand(MotionCommand(cmd, target, path));
}

void Robot::stop() {
    DPRINTLN("[Robot::stop] Issuing STOP command.");
    mcu.sendCommand(MotionCommand(MotionCommandType::STOP, 0.0f, nullptr));
}

bool Robot::isFinishedMovement() {
    return mcu.isMovementComplete();
}

Line* Robot::getCenterLine() const {
    return snu.getCenterLine();
}
#define PHASE_DELAY_S 5

void Robot::tick(double dt) {
    main_logic(dt);
    // send_line_test(dt);
    // staircase_test(dt);
    // positive_sinus_test(dt);
    // full_sinus_test(dt);
    // distance_test(dt);
    // turn_test(dt);
  }

void Robot::main_logic(double dt) {
    static bool command_sent = false;
    static int index = 0;
    static int skip_ticks = 0;
    // static int stop_before_turn_ticks = 0;
    static float pre_turn_theta = 0;
    static const float box_turn = 1.571f;
    Pose p;

    // CHANGE 1: Handle null parser safely
    if (!parser) {
        DPRINTLN("[Robot] ❌ No command parser available.");
        return;
    }

    float headSensorReading = snu.headSensor();
    bool detectedCorridor = snu.detectInCorridor();
    bool criticalPotentialCollision = headSensorReading > 0 && headSensorReading < CRITICAL_COLLISION_DISTANCE;
    bool safePotentialCollision = headSensorReading > 0 && headSensorReading < SAFE_COLLISION_DISTANCE;

    DPRINTLN(String("[Robot] HeadSensorRead: ") + headSensorReading);
    DPRINTLN(String("[Robot] Corridor Detected: ") + (detectedCorridor ? "YES" : "NO"));
    DPRINTLN(String("[Robot] Current State: ") + (int)curr_rs);
    if (criticalPotentialCollision) {
        DPRINTLN("[Robot] 🚨 Critical collision while seeking - stopping.");
        stop();
        return;
    }
    switch (curr_rs) {

        // CHANGE 2: New state for command fetching
        case RobotState::COMMAND_EXTRACTION:
            DPRINTLN("[Robot] State: COMMAND_EXTRACTION");

            if (skip_ticks < 50) {
                skip_ticks++;
                DPRINTLN("[Robot] ⏳ Waiting before next command extraction...");
                return;
            }
            skip_ticks = 0;

            if (parser->fetchAndParseCommands() && parser->commandCount() > 0) {
                DPRINTLN("[Robot] ✅ Commands fetched and parsed successfully");
                curr_rs = RobotState::START_POS;
                index = 0;
            } else {
                DPRINTLN("[Robot] ❌ Failed to fetch/parse commands or none available");
            }
            return;

        case RobotState::START_POS:
            DPRINTLN("[Robot] State: START_POS");

            if (!detectedCorridor) {
                if (criticalPotentialCollision) {
                    DPRINTLN("[Robot] 🚨 Critical collision ahead - stopping.");
                    stop();
                } else if (!command_sent) {
                    DPRINTLN("[Robot] Starting initial path tracking.");
                    sendMovementCmd(MotionCommandType::PATH_TRACK, 0.7f, new Line({0, 0}, 0));
                    command_sent = true;
                }
                break;
            }

            DPRINTLN("[Robot] Corridor detected. Switching to IN_CORRIDOR.");
            curr_rs = RobotState::IN_CORRIDOR;
            command_sent = false;
            break;

        case RobotState::IN_CORRIDOR:
            DPRINTLN("[Robot] State: IN_CORRIDOR");

            if (criticalPotentialCollision) {
                DPRINTLN("[Robot] 🚨 Critical collision in corridor - stopping.");
                stop();
                break;
            }

            if (detectedCorridor) {
                DPRINTLN("[Robot] ✅ Continuing path tracking inside corridor.");
                sendMovementCmd(MotionCommandType::PATH_TRACK, 0.7f, getCenterLine());
                break;
            }

            DPRINTLN("[Robot] ❌ Lost corridor. Switching to OUT_OF_CORRIDOR.");
            curr_rs = RobotState::OUT_OF_CORRIDOR;
            command_sent = false;
            break;

        case RobotState::OUT_OF_CORRIDOR: {
            DPRINTLN("[Robot] State: OUT_OF_CORRIDOR");

            if (criticalPotentialCollision) {
                DPRINTLN("[Robot] 🚨 Critical collision outside corridor - stopping.");
                stop();
                break;
            }

            // CHANGE 3: Use parser to retrieve commands
            if (index >= parser->commandCount()) {
                DPRINTLN("[Robot] ✅ No more commands. Moving to END_POS.");
                curr_rs = RobotState::END_POS;
                break;
            }

            MazeCommand cmd = parser->getCommand(index);
            switch (cmd) {
                case MazeCommand::FORWARD:
                    DPRINTLN("[Robot] Action: FORWARD");
                    index++;
                    curr_rs = RobotState::SEEKING_CORRIDOR;
                    break;

                case MazeCommand::LEFT:
                case MazeCommand::RIGHT:
                    DPRINTLN("[Robot] Action: Turn Requested");
                    if (!command_sent) {
                        DPRINTLN("[Robot] Pre-turn: Move forward slightly.");
                        sendMovementCmd(MotionCommandType::MOVE_DISTANCE, 0.15f, nullptr);
                        command_sent = true;
                    } else {
                        DPRINTLN("[Robot] Checking if ready to turn...");
                        if (safePotentialCollision || isFinishedMovement()) {
                            DPRINTLN("[Robot] Pre-turn move complete. Proceeding to turn.");
                            stop();
                            curr_rs = RobotState::TURNING;
                            command_sent = false;
                        }
                    }
                    break;

                case MazeCommand::END:
                    DPRINTLN("[Robot] Action: END - reached final state.");
                    curr_rs = RobotState::END_POS;
                    break;

                default:
                    DPRINTLN("[Robot] ⚠️ Unknown command - skipping.");
                    index++;
                    break;
            }
            break;
        }
        case RobotState::TURNING:
            DPRINTLN("[Robot] State: TURNING");
            pre_turn_theta = mcu.getPose().theta;

            if (!command_sent) {
                MazeCommand cmd = parser->getCommand(index);
                if (cmd == MazeCommand::LEFT) {
                    DPRINTLN("[Robot] Executing LEFT turn.");
                    sendMovementCmd(MotionCommandType::TURN_ANGLE, 90.0f, nullptr);
                    pre_turn_theta += box_turn;
                } else if (cmd == MazeCommand::RIGHT) {
                    DPRINTLN("[Robot] Executing RIGHT turn.");
                    sendMovementCmd(MotionCommandType::TURN_ANGLE, -90.0f, nullptr);
                    pre_turn_theta -= box_turn;
                } else {
                    DPRINTLN("[Robot] ❌ Unexpected command in TURNING state - stopping.");
                    stop();
                    return;
                }
                command_sent = true;
            } else {
                if (!isFinishedMovement()) {
                    DPRINTLN("[Robot] ⏳ Still turning...");
                    return;
                }

                DPRINTLN("[Robot] ✅ Turn complete. Reacquiring corridor...");
                curr_rs = RobotState::SEEKING_CORRIDOR;
                command_sent = false;
                index++;
                p = mcu.getPose();
                sendMovementCmd(MotionCommandType::PATH_TRACK, 0.4f, new Line(p.loc, pre_turn_theta));
                snu.resetCorridorEstimation();
            }
            break;

        case RobotState::SEEKING_CORRIDOR:
            DPRINTLN("[Robot] State: SEEKING_CORRIDOR");
            if (criticalPotentialCollision) {
                DPRINTLN("[Robot] 🚨 Critical collision while seeking - stopping.");
                stop();
                break;
            }

            if (detectedCorridor) {
                MazeCommand cmd = parser->getCommand(index - 1);
                if (!command_sent && (cmd == MazeCommand::LEFT || cmd == MazeCommand::RIGHT)) {
                    snu.resetCorridorEstimation();
                    command_sent = true;
                } else {
                    DPRINTLN("[Robot] ✅ Corridor reacquired. Returning to IN_CORRIDOR.");
                    curr_rs = RobotState::IN_CORRIDOR;
                    command_sent = false;
                }
            }
            break;
        case RobotState::END_POS:
            DPRINTLN("[Robot] State: END_POS - stopping robot.");
            stop();
            break;
        default:
          break;
    }
}

void Robot::send_line_test(double dt) {
  sendMovementCmd(MotionCommandType::PATH_TRACK, 0.8, snu.getCenterLine());
}

void Robot::positive_sinus_test(double dt) {
    static float time = 0.0f;

    // Output range: [0.5, 1.0]
    const float amplitude = 0.25f;       // (1.0 - 0.5) / 2
    const float offset = 0.75f;          // (1.0 + 0.5) / 2
    const float frequencyHz = 0.1f;      // One full cycle every 10 seconds

    float omega = 2.0f * M_PI * frequencyHz;
    float command = amplitude * sinf(omega * time) + offset;

    sendMovementCmd(MotionCommandType::MANUAL_SPEED, command, nullptr);

    time += dt;
}

void Robot::full_sinus_test(double dt){
    static float time = 0.0f;

    // Sinusoidal wave parameters
    const float amplitude = 1.0f;       // Peak command: ±1.0
    const float frequencyHz = 0.1f;     // 0.1 Hz = 10 seconds per full wave
    const float offset = 0.0f;          // Centered around 0

    // Generate command using sine wave
    float omega = 2.0f * M_PI * frequencyHz;
    float command = amplitude * sinf(omega * time) + offset;

    sendMovementCmd(MotionCommandType::MANUAL_SPEED, command, nullptr);

    time += dt;
  }


void Robot::staircase_test(double dt){
  static float now = 0;
  static int phase = 0;
  // Check if we're ready to move to the next phase
    if ((phase >= 0 && phase <= 4) && (now >= PHASE_DELAY_S)) {
        DPRINT("[robot] Advancing phase from ");
        DPRINT(phase);
        phase++;
        DPRINT(" to ");
        DPRINTLN(phase);
        now = 0;
    }

    // Execute phase behavior
    switch (phase) {
        case 0:
            DPRINTLN("[robot] Phase 0: Sending initial movement (1.0f)");
            sendMovementCmd(MotionCommandType::MANUAL_SPEED, 1.0f, nullptr);
            break;

        case 1:
            DPRINTLN("[robot] Phase 1: Sending movement command (1.0f)");
            sendMovementCmd(MotionCommandType::MANUAL_SPEED, 1.0f, nullptr);
            break;

        case 2:
            DPRINTLN("[robot] Phase 2: Sending movement command (0.2f)");
            sendMovementCmd(MotionCommandType::MANUAL_SPEED, 0.2f, nullptr);
            break;

        case 3:
            DPRINTLN("[robot] Phase 3: Sending movement command (0.8f)");
            sendMovementCmd(MotionCommandType::MANUAL_SPEED, 0.8f, nullptr);
            break;

        case 4:
            DPRINTLN("[robot] Phase 4: Sending movement command (0.4f)");
            sendMovementCmd(MotionCommandType::MANUAL_SPEED, 0.4f, nullptr);
            break;
        case 5:
            DPRINTLN("[robot] Phase 5: back to phase 0");
            phase = 0;
            break;

        default:
            DPRINTLN("[robot] All phases completed or invalid phase.");
            break;
    }
    now+=dt;
  }


void Robot::distance_test(double dt) {
    static int phase = 0;
    static bool commandIssued = false;

    if (!commandIssued) {
        switch (phase) {
            case 0:
                DPRINTLN("[robot] Phase 0: Move forward 1.0 meters");
                sendMovementCmd(MotionCommandType::MOVE_DISTANCE, 1.0f, nullptr);
                break;

            case 1:
                DPRINTLN("[robot] Phase 1: Move forward 0.1 meters (simulated turn)");
                sendMovementCmd(MotionCommandType::MOVE_DISTANCE, 0.1f, nullptr);
                break;

            case 2:
                DPRINTLN("[robot] Phase 2: Move backward 0.5 meters");
                sendMovementCmd(MotionCommandType::MOVE_DISTANCE, 0.5f, nullptr);
                break;

            case 3:
                DPRINTLN("[robot] Phase 3: Move backward 0.1 meters (simulated reverse turn)");
                sendMovementCmd(MotionCommandType::MOVE_DISTANCE, 0.7f, nullptr);
                break;

            case 4:
                DPRINTLN("[robot] Phase 4: Done. Resetting test.");
                phase = 0;
                return;

            default:
                DPRINTLN("[robot] Invalid phase");
                return;
        }

        commandIssued = true;
    }

    // Move to next phase when movement finishes
    if (isFinishedMovement()) {
        phase++;
        commandIssued = false;
    }
}


void Robot::turn_test(double dt) {
    static int phase = 0;
    static bool commandIssued = false;
    const int stopingCycle = 30;
    static int stoping = 0;
    static bool toStop = false;
    if(toStop && stoping < stopingCycle){
      stoping++;
      return;
    }else{
      stoping = 0;
      toStop= false;
    }
    if (!commandIssued) {
        switch (phase) {
            case 0:
                DPRINTLN("[robot] Phase 0: Turn 90 degrees");
                sendMovementCmd(MotionCommandType::TURN_ANGLE, 90.0f, nullptr);
                break;

            case 1:
                DPRINTLN("[robot] Phase 1: Turn 60 degrees");
                sendMovementCmd(MotionCommandType::TURN_ANGLE, 60.0f, nullptr);
                break;

            case 2:
                DPRINTLN("[robot] Phase 2: Move backward -60 meters");
                sendMovementCmd(MotionCommandType::TURN_ANGLE, -60.0f, nullptr);
                break;

            case 3:
                DPRINTLN("[robot] Phase 3: Turn 1080 degrees");
                sendMovementCmd(MotionCommandType::TURN_ANGLE, 1080.0f, nullptr);
                break;

            case 4:
                phase = 0;
                break;

            case 5:
                DPRINTLN("[robot] Phase 5: Reset to phase 0");
                phase = 0;
                return;

            default:
                DPRINTLN("[robot] Invalid phase");
                return;
        }

        commandIssued = true;
    }

    // Wait for movement to complete
    if (mcu.isMovementComplete()) {
        phase++;
        stop();
        toStop = true;
        commandIssued = false;
    }
}

