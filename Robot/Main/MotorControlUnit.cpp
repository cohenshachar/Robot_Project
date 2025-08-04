
#include "MotorControlUnit.h"
#include <cmath>
#include "DebugConfig.h"
#define DPRINT(x)    DPRINT_MCU(x)
#define DPRINTLN(x)  DPRINTLN_MCU(x)
#define LOG_FUNC(x) LOG_MCU_MSG(x)

// Conversion macros
#define MS_TO_RPM(ms) ((ms) * 60.0) / (2.0 * M_PI * (WHEEL_RADIUS))
#define RPM_TO_MS(rpm) ((rpm) * 2.0 * M_PI * (WHEEL_RADIUS) / 60)

#define DEG_TO_DIST_MM(degrees) (((degrees) * (WHEEL_RADIUS) * M_PI) / 180.0)
#define DEG_TO_DIST_CM(degrees) (DEG_TO_DIST_MM(degrees) / 10.0)
#define DEG_TO_DIST_M(degrees)  (DEG_TO_DIST_CM(degrees) / 100.0)

#define MM_DIST_TO_DEG(dist)  (((dist) * 180.0) / (M_PI * WHEEL_RADIUS))
#define CM_DIST_TO_DEG(dist)  MM_DIST_TO_DEG(((dist) * 10.0))
#define M_DIST_TO_DEG(dist)   CM_DIST_TO_DEG(((dist) * 100.0))


MotorControlUnit::MotorControlUnit(IPathTracker* pathTracker) :
    robotPos(),
    motorRight("motorRight", IN_A_1, IN_A_2, ENC_A_1, ENC_A_2, PWM_A_1, PWM_A_2, GEAR_RATIO, TPR, A_PP, A_PI, A_PD, A_SP, A_SI, A_SD),
    motorLeft("motorLeft", IN_B_1, IN_B_2, ENC_B_1, ENC_B_2, PWM_B_1, PWM_B_2, GEAR_RATIO, TPR, B_PP, B_PI, B_PD, B_SP, B_SI, B_SD),
    currentMovement(MovementType::IDLE),
    syncPid(0.00f, 0.00f, 0.00f),
    logger(nullptr),
    pathTracker(pathTracker)
{
    syncPid.setOutputLimits(-0.1f, 0.1f);
}


bool MotorControlUnit::begin(ILogManager* db_logger){
    logger = db_logger;
    attachAllEncoderInterrupts();
    motorLeft.set_logger(db_logger);
    motorRight.set_logger(db_logger);

    // Create the FreeRTOS task
    xTaskCreatePinnedToCore(
        MotorControlUnit::motionTaskEntryPoint,
        "MotionControlTask",
        4096,
        this,
        5,
        &motionTaskHandle,
        APP_CPU_NUM  // or PRO_CPU_NUM if needed
    );

    return true;
}
void MotorControlUnit::motionTaskEntryPoint(void* pvParameters) {
    MotorControlUnit* self = static_cast<MotorControlUnit*>(pvParameters);
    self->motionControlLoop();
}

void MotorControlUnit::motionControlLoop() {
    const TickType_t delayTicks = pdMS_TO_TICKS(15);  // Target loop period (15ms)
    TickType_t lastWakeTime = xTaskGetTickCount();    // Initialize to current time
    int64_t lastTimeUs = esp_timer_get_time();

    while (true) {
        int64_t nowUs = esp_timer_get_time();
        double dt = (nowUs - lastTimeUs) / 1e6f;
        lastTimeUs = nowUs;
        DPRINT_TIMING("[MotionLoop] Loop timing: ");
        DPRINTLN_TIMING(dt);
        motorLeft.setToMove();
        motorRight.setToMove();
        HandleCommand();
        updateMovement(dt);
        // This ensures a fixed 15ms interval from the lastWakeTime
        vTaskDelayUntil(&lastWakeTime, delayTicks);
    }
}



void MotorControlUnit::sendCommand(const MotionCommand& newCmd) {
    taskENTER_CRITICAL(&commandMux);
    commandLeash = newCmd;
    hasNewCommand = true;
    taskEXIT_CRITICAL(&commandMux);
}
void MotorControlUnit::HandleCommand() {
    if (!hasNewCommand) return;
    MotionCommand cmd;
    taskENTER_CRITICAL(&commandMux);
    cmd = commandLeash;
    hasNewCommand = false;
    taskEXIT_CRITICAL(&commandMux);
    switch (cmd.type) {
        case MotionCommandType::MOVE_DISTANCE: moveDistance(cmd.value); break;
        case MotionCommandType::TURN_ANGLE: turnAngle(cmd.value); break;
        case MotionCommandType::MANUAL_SPEED: driveManual(cmd.value); break;
        case MotionCommandType::PATH_TRACK: pathTrack(cmd.path, cmd.value); break;
        case MotionCommandType::STOP: stopMovement(); break;
    }
}

void MotorControlUnit::driveManual(float normalizedSpeed){
    currentMovement = MovementType::MANUAL_CONTROL;
    // setMotorsSpeed(normalizedSpeed, normalizedSpeed);
    pathTracker->updateParams(new Line(robotPos.loc, robotPos.theta), normalizedSpeed);
}

void MotorControlUnit::moveDistance(double distance){
    currentMovement = MovementType::MOVING_DISTANCE;
    distance*=100;
    float endPointX = distance * cos(robotPos.theta);
    float endPointY = distance * sin(robotPos.theta);
    DPRINTLN(String("Distance Start Point: (") +robotPos.loc.x+", "+robotPos.loc.y+")" ", End Point: (" +endPointX+", "+endPointY+")");
    pathTracker->updateParams(new Line(robotPos.loc, robotPos.loc + Point(endPointX,endPointY)), 0.7f);
}

void MotorControlUnit::turnAngle(double degrees){
    currentMovement = MovementType::TURNING_ANGLE;
    double wheelDegrees = (WHEEL_BASE / (2.0 * WHEEL_RADIUS)) * degrees;
    motorLeft.setPositionTarget(-wheelDegrees);
    motorRight.setPositionTarget(wheelDegrees);
    setMotorsSpeed(motorLeft.getTargetSpeed(),motorRight.getTargetSpeed());
}

void MotorControlUnit::pathTrack(const IPath* path, float speed){
    if(pathTracker){
      currentMovement = MovementType::PATH_TRACKING;
      pathTracker->updateParams(path, speed);
    }else{
      driveManual(0.5);
    }
}

void MotorControlUnit::stopMovement() {
    currentMovement = MovementType::IDLE;
    motorLeft.stop();
    motorRight.stop();
}

void MotorControlUnit::setMotorsSpeed(float leftTarget, float rightTarget) {
    motorLeft.setSpeedTarget( leftTarget );
    motorRight.setSpeedTarget( rightTarget);
}

void MotorControlUnit::updateOdometry(double dt) {
    if (dt <= 0.0) dt = 1.0 / 50.0;

    static double lastLeftDeg = 0.0;
    static double lastRightDeg = 0.0;

    double leftDeg = motorLeft.get_deg();
    double rightDeg = motorRight.get_deg();

    double dLeft = DEG_TO_DIST_CM(leftDeg - lastLeftDeg);
    double dRight = DEG_TO_DIST_CM(rightDeg - lastRightDeg);

    lastLeftDeg = leftDeg;
    lastRightDeg = rightDeg;

    double dCenter = (dLeft + dRight) / 2.0;
    double dTheta = (dRight - dLeft) / (WHEEL_BASE/10.0);
    double dx, dy;
    if (fabs(dTheta) < 1e-6) {
        // Straight-line approximation
        dx = dCenter * cos(robotPos.theta);
        dy = dCenter * sin(robotPos.theta);
    } else if (fabs(dCenter) < 1e-2) {
    // Pure rotation in place: no translation
        dx = 0;
        dy = 0;
    } else {
        // Arc motion
        double R = dCenter / dTheta;
        dx = R * (sin(robotPos.theta  + dTheta) - sin(robotPos.theta));
        dy = -R * (cos(robotPos.theta  + dTheta) - cos(robotPos.theta));
    }
    taskENTER_CRITICAL(&poseMux);  // use a mutex or spinlock protecting robotPos
    robotPos.loc.x += dx;
    robotPos.loc.y += dy;
    robotPos.theta += dTheta;
    robotPos.theta = fmod(robotPos.theta + 2 * M_PI, 2 * M_PI);  // Normalize

    robotPos.vLinear = dCenter / dt;
    robotPos.vAngular = dTheta / dt;
    taskEXIT_CRITICAL(&poseMux);

    DPRINTLN("Updated Odometry:");
    DPRINT("Position: (");
    DPRINT(robotPos.loc.x);
    DPRINT(", ");
    DPRINT(robotPos.loc.y);
    DPRINT("), Theta: ");
    DPRINTLN(robotPos.theta);
}

void MotorControlUnit::updateMovement(double dt){
    updateOdometry(dt);
    logOdometry();
    DPRINT("ACTION: ");
    float leftSpeedTarget = 0.5;
    float rightSpeedTarget = 0.5;
    switch(currentMovement){
        case MovementType::TURNING_ANGLE:
            DPRINTLN("TURNING_ANGLE");
            motorLeft.updatePositionControl(dt);
            motorRight.updatePositionControl(dt);
            movementCompeleted = motorLeft.get_DriveMode() == DriveMode::IDLE && motorRight.get_DriveMode() == DriveMode::IDLE;
            break;

        case MovementType::IDLE:
            DPRINTLN("IDLE");
            stopMovement();
            break;
        case MovementType::MOVING_DISTANCE:
        case MovementType::MANUAL_CONTROL:
        case MovementType::PATH_TRACKING:
            if (pathTracker->computeWheelSpeeds(robotPos, WHEEL_BASE, motorLeft.getCurrentMaxRpm(), motorRight.getCurrentMaxRpm(), &leftSpeedTarget, &rightSpeedTarget)){
              setMotorsSpeed(leftSpeedTarget, rightSpeedTarget);
              movementCompeleted = pathTracker->isFinished();
            }
            break;
    }
    DPRINTLN("motorLeft tick");
    motorLeft.tick(dt);
    DPRINTLN("motorRight tick");
    motorRight.tick(dt);
}

bool MotorControlUnit::isMovementComplete() const{
    return !hasNewCommand && movementCompeleted;
}

void MotorControlUnit::logOdometry(){
  LOG_FUNC(
    if (!(logger && logger->isReady()))
        return;

    ILogRecord* rec = logger->createRecord("Odometry", false);
    if(!rec) {
        DPRINTLN("WARNING: PID log skipped due to allocation failure");
        return;
    }
    if(!rec->set("x", robotPos.loc.x)
       .set("y", robotPos.loc.y)
       .set("theta", robotPos.theta)
       .set("vA", robotPos.vAngular)
       .set("vL", robotPos.vLinear)
       .set("v_m1", motorLeft.getSmoothedRPM())
       .set("v_m2", motorRight.getSmoothedRPM())
       .commit(false))
       delete rec;
  );
}

Pose MotorControlUnit::getPose(){ 
    return robotPos;
}


/////////////////////////////////////////////// EXPIRAMENTAL //////////////////////////////////////////////////////////////////////////

// void MotorControlUnit::drivePath(Bezier<2>* path){  
//   if(!path)
//     return;
//   this->path = path;
//   currentMovement = MovementType::FOLLOWING_PATH;
// }


// int MotorControlUnit::control_point_search(Point& res, const Point& ref, const Bezier<2>* ref_path , double c){
//   // int flag = 3;
//   // double t = ref_path->findClosestPoint(ref,100);
//   // double ds = (ref_path->at(t) - ref).length();
//   // if(ds  > lookaheadDistance){
//   //   res = (lookaheadDistance / ds) * (ref_path->at(t) - ref);
//   //   if(ds > 2*lookaheadDistance)
//   //     return 2;
//   //   else
//   //     return 1;
//   // }
//   // double dp = 0;
//   // double dt = (1-t)/100;
//   // double t_i = t + dt;
//   // for(; t_i < 1; t_i += dt){
//   //   dp += (ref_path->at(t_i) - ref_path->at(t_i - dt)).length();
//   //   double d = sqrt(dp*dp + ds*ds);
//   //   double k = ref_path->curvature_at(t_i);
//   //   double l_crit = min(lookaheadDistance, 1/(c*k));
//   //   if(d >= l_crit){
//   //     flag = 0;
//   //     break;
//   //   }
//   // }
//   // res = ref_path->at(t_i);
//   // return flag;
//   return 0;
// }

// Bezier<4>* MotorControlUnit::create_intermidiate_path(){
//   std::array<Point, 5> controlPoints;
//   // double tt_time; // total time of each planning cycle;
//   // double max_v = RPM_TO_MS(min(motor1.getCurrentMaxRpm(), motor2.getCurrentMaxRpm()));
//   // planingCycleTime = (4*lookaheadDistance) / max_v;
//   // tt_time = planingCycleTime*planingCycleTime;
//   // controlPoints[0] = robotPos.loc;
//   // Point localP1 = Point(robotPos.vLinear*planingCycleTime/4, 0);
//   // double p2y = robotPos.vAngular*robotPos.vLinear*tt_time / 12;
//   // Point localP2 = Point(localP1.x  + sqrt(max(pow((max_v*planingCycleTime / 4),2) - (p2y*p2y),0.0)), p2y);
//   // controlPoints[1] = toWorldFrame(robotPos, localP1);
//   // controlPoints[2] = toWorldFrame(robotPos, localP2);
//   // int flag = control_point_search(controlPoints[3] , controlPoints[2], path, 1);
//   // if(flag == 0 ){
//   //   control_point_search(controlPoints[4] , controlPoints[3], path, 1);
//   // }
//   // else if(flag == 1){
//   //   controlPoints[3] += controlPoints[2];
//   //   control_point_search(controlPoints[4] , controlPoints[3], path, 1);
//   // }else if(flag == 2){
//   //   controlPoints[4] = 2 * controlPoints[3];
//   //   controlPoints[3] += controlPoints[2];
//   //   controlPoints[4] += controlPoints[2];
//   // }else{
//   //   finishedPath = true;
//   // }
//   return new Bezier<4>(controlPoints);
// }

// int MotorControlUnit::planIntermediatePath() {
//     // if(interPath) delete interPath;
//     // if(!finishedPath){
//     //   interPath = create_intermidiate_path();
//     //   interPathStartTime = millis();
//     //   return 1;
//     // }
//      return 0;
// }

// int MotorControlUnit::pathPursuitController() {
//   // if(!path)
//   //   return 1; // no path;
//   // if(!interPath){
//   //   planIntermediatePath();
//   // }
//   // double elapsedTime = (millis() - interPathStartTime) / 1000.0; // seconds
//   // double t = elapsedTime / planingCycleTime;
//   // if (t >= 1) {
//   //   if(planIntermediatePath())
//   //     t = 0;
//   //   else
//   //     return 1; 
//   // }
//   // sendMotorsCommands(t);
//    return 0;  // Return success
// }
