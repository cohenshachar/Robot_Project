#include "Motor.h"
// Motor.cpp
#include "DebugConfig.h"
#define DPRINT(x)    DPRINT_MOTOR(x)
#define DPRINTLN(x)  DPRINTLN_MOTOR(x)
#define LOG_FUNC(x)  LOG_MOTOR_MSG(x)

// Global encoder-to-motor pointer map
Motor* encoderMap[40] = { nullptr };

Motor::Motor(String name, int in1, int in2, int enc1, int enc2, int pwmChannel1, int pwmChannel2, int gearR, float tpr, float pp, float pi, float pd, float sp, float si, float sd)
  : name(name), in1(in1), in2(in2), enc1(enc1), enc2(enc2), gearR(gearR), tpr(tpr),
    pwmChannel1(pwmChannel1), pwmChannel2(pwmChannel2),
    positionPid(pp, pi, pd, 0.03), speedPid(sp, si, sd) {

  encoderHistoryPointer = 0;

  positionPid.setOutputLimits(-1.0f, 1.0f);
  speedPid.setOutputLimits(-1.0f, 1.0f);
  speedPid.setIntegralLimits(-1.0f, 1.0f);
  positionPid.setIntegralLimits(-0.15f, 0.15f);  // avoid windup

  for(int i = 0; i < SAMPLES_BUFFER_SIZE; i++){
    encoderHistory[i].timestamp = 0;
    encoderHistory[i].dir = 0;
    encoderHistory[i].pwm = 0;
    mock_state[i].timestamp = 0;
    mock_state[i].dir = 0;
    mock_state[i].pwm = 0;
  }

  encoderMap[enc1] = this;
  encoderMap[enc2] = this;

  pinMode(enc1, INPUT_PULLUP);
  pinMode(enc2, INPUT_PULLUP);

  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);

  // Setup LEDC PWM channels at 15kHz, 8-bit resolution
  ledcSetup(pwmChannel1, 15000, 8);
  ledcSetup(pwmChannel2, 15000, 8);

  ledcAttachPin(in1, pwmChannel1);
  ledcAttachPin(in2, pwmChannel2);

  encoderValue = 0;
  encoderMutex = portMUX_INITIALIZER_UNLOCKED;

  updateNoSpeed(35);
  jumpStart = true;
  positionPid.reset();
  speedPid.reset();
  pwm = 0;
  currentMaxRpm = 44;
  emaRPM = 0;
  emaPWM = 0;

}

void Motor::set_logger(ILogManager* logger){
  db_logger = logger;
}

void Motor::tick(double dt) {
  DPRINT("Tick: ");

  switch (currentDriveMode) {
    case DriveMode::DRIVE:
      DPRINTLN(" Current motor mode: Drive ");
      updateSpeedControl(dt);
      break;

    case DriveMode::IDLE:
      DPRINTLN(" Current motor mode: Idle ");
      stop();
      break;
  }
  printMock();
  portENTER_CRITICAL(&encoderMutex);
  unsigned long prevT = encoderHistory[(encoderHistoryPointer + SAMPLES_BUFFER_SIZE - 1) % SAMPLES_BUFFER_SIZE].timestamp;
  unsigned long currT = micros();

  if((currT - prevT) >= DEFAULT_SPEED_TIMEOUT_US){
    uint8_t failedPwm = emaPWM;
    encoderHistory[encoderHistoryPointer].timestamp = currT;
    encoderHistory[encoderHistoryPointer].dir = 0;
    encoderHistory[encoderHistoryPointer].pwm = pwm;
    mock_state[encoderHistoryPointer].timestamp = currT;
    mock_state[encoderHistoryPointer].dir = 0;
    mock_state[encoderHistoryPointer].pwm = pwm;
    encoderHistoryPointer = (encoderHistoryPointer + 1) % SAMPLES_BUFFER_SIZE;

    if (failedPwm > noSpeed) {
      updateNoSpeed(noSpeed + 7);
    }
    jumpStart = true;
  } else {
    if(emaPWM < noSpeed + 25)
      updateNoSpeed(max((int)30 , (int)noSpeed - 2));
  }
  portEXIT_CRITICAL(&encoderMutex);
}

float Motor::calculateNormalizedPosition(float currPosition){
  float res = currPosition - targetPosition;
  res /= 120.0f;
  if (res > 1.0f) res = 1.0f;
  if (res < -1.0f) res = -1.0f;
  return res;
}

void Motor::setPositionTarget(float targetPosition) {
  clearPositionControl();
  this->targetPosition = get_deg() + targetPosition;
}

void Motor::setSpeedTarget(float normalizedTargetRPM) {
  currentDriveMode = DriveMode::DRIVE;
  speedPid.setSetpoint(normalizedTargetRPM);
}

void Motor::enableJumpStart() {
  jumpStart = true;
}

void Motor::disableJumpStart() {
  jumpStart = false;
}

bool Motor::getJumpStart() const {
  return jumpStart;
}

void Motor::move(bool dir, int speed) {
  currentDriveMode = DriveMode::DRIVE;
  int pwm1 = dir ? pwmChannel1 : pwmChannel2;
  int pwm2 = dir ? pwmChannel2 : pwmChannel1;

  if(jumpStart && speed < lowSpeed){
    speed = lowSpeed + 20;
    jumpStart = false;
  }
  portENTER_CRITICAL(&encoderMutex);
  ledcWrite(pwm1, (uint32_t)speed);
  ledcWrite(pwm2, 0);
  pwm = speed;
  portEXIT_CRITICAL(&encoderMutex);
}

void Motor::stop() {
  ledcWrite(pwmChannel1, 0);
  ledcWrite(pwmChannel2, 0);
  currentDriveMode = DriveMode::IDLE;
}

void Motor::updateNoSpeed(uint8_t newNoSpeed) {
  noSpeed = max((uint8_t)0 , newNoSpeed);
  lowSpeed = min(noSpeed + 50, 255);
}

void Motor::clearPositionControl() {
  positionPid.reset();
  targetPosition = get_deg();
}

void Motor::clearSpeedControl() {
  speedPid.reset();
}

void Motor::updateEncoder() {
  portENTER_CRITICAL(&encoderMutex);
  bool a = digitalRead(enc1);
  bool b = digitalRead(enc2);
  int deltaTicks =  (a^b) ? 1 : -1;
  encoderHistory[encoderHistoryPointer].timestamp = micros();
  encoderHistory[encoderHistoryPointer].dir = deltaTicks;
  encoderHistory[encoderHistoryPointer].pwm = pwm;

  mock_state[encoderHistoryPointer].timestamp = encoderHistory[encoderHistoryPointer].timestamp;
  mock_state[encoderHistoryPointer].dir = deltaTicks;
  mock_state[encoderHistoryPointer].pwm = pwm;

  encoderHistoryPointer = (encoderHistoryPointer + 1) % SAMPLES_BUFFER_SIZE;
  encoderValue += deltaTicks;
  portEXIT_CRITICAL(&encoderMutex);
}

float Motor::getCurrentMaxRpm() const {
  return currentMaxRpm;
}

size_t Motor::copyEncoderTable(EncoderHistory* dst){
  size_t localPointer;
  portENTER_CRITICAL(&encoderMutex);
  for (int i = 0; i < SAMPLES_BUFFER_SIZE; ++i) {
    dst[i].timestamp = encoderHistory[i].timestamp;
    dst[i].dir = encoderHistory[i].dir;
    dst[i].pwm = encoderHistory[i].pwm;
  }
  localPointer = encoderHistoryPointer;
  portEXIT_CRITICAL(&encoderMutex);
  return localPointer;
}

float map(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
void Motor::calculateSmoothedRPM() {
    EncoderHistory encoderHistoryBuffer[SAMPLES_BUFFER_SIZE];
    size_t localPointer = copyEncoderTable(encoderHistoryBuffer);

    const unsigned long now = micros();
    static const unsigned long windowUs = DEFAULT_SPEED_FRAME_US;  // e.g. 20000 µs (20ms)
    static const float tickToRevFactor = 1.0f / (2 * tpr * gearR);
    static const float maxExpectedRPS = 1.0f;
    static const float minDt = 200.0f;  // µs
    static const float maxDt = DEFAULT_SPEED_TIMEOUT_US;  // e.g. 30000 µs

    // Decay logic
    static unsigned long lastUpdateUs = 0;
    static const float decayRatePerSecond = 0.03333f;
    static const float maxDropPerFrame = 0.1f;
    static const float deadzone = 0.01f;

    float prevRPM = emaRPM;
    float prevPWM = emaPWM;

    int samplesUsed = 0;

    for (int s = SAMPLES_BUFFER_SIZE - 1; s > 0; --s) {
        size_t i = (localPointer + SAMPLES_BUFFER_SIZE - s) % SAMPLES_BUFFER_SIZE;
        size_t j = (localPointer + SAMPLES_BUFFER_SIZE - s - 1) % SAMPLES_BUFFER_SIZE;

        unsigned long ti = encoderHistoryBuffer[i].timestamp;
        unsigned long tj = encoderHistoryBuffer[j].timestamp;

        if ((now - ti) > windowUs || ti <= tj) continue;

        unsigned long dt = ti - tj;
        if (dt < minDt || dt > maxDt) continue;

        float dtSec = dt / 1e6f;
        float rps = encoderHistoryBuffer[i].dir * tickToRevFactor / dtSec;
        if (fabs(rps) > maxExpectedRPS) continue;

        // Dynamic tau and alpha
        float dynamicTau = map(fabs(emaRPM), 0.0f, maxExpectedRPS * 60.0f, 0.03f, 0.25f);
        dynamicTau = constrain(dynamicTau, 0.02f, 0.27f);
        float alpha = dtSec / (dynamicTau + dtSec);

        // Smart initialization
        if (samplesUsed == 0 && emaRPM == 0) {
            emaRPM = rps;
            emaPWM = encoderHistoryBuffer[i].pwm;
        } else {
            emaRPM = alpha * rps + (1.0f - alpha) * emaRPM;
            emaPWM = alpha * encoderHistoryBuffer[i].pwm + (1.0f - alpha) * emaPWM;
        }

        samplesUsed++;
    }

    if (samplesUsed > 0) {
        // Optional secondary smoothing (low-pass filter on EMA itself)
        emaRPM = 0.6f * emaRPM + 0.4f * prevRPM;
        emaPWM = 0.6f * emaPWM + 0.4f * prevPWM;
        lastUpdateUs = now;
        // // Clamp downward drop
        // if (emaRPM < prevRPM) {
        //     float minAllowed = prevRPM - maxDropPerFrame;
        //     emaRPM = fmaxf(emaRPM, minAllowed);
        // }
    } 
    else {
        // No valid samples — apply decay if beyond timeout
        unsigned long sinceLastUpdate = now - lastUpdateUs;
        if (sinceLastUpdate > windowUs) {
            float seconds = sinceLastUpdate / 1e6f;
            float decayFactor = expf(-decayRatePerSecond * seconds);
            emaRPM *= decayFactor;
            emaPWM *= decayFactor;
        }
    }

    // Apply deadzone to silence small RPMs
    // if (fabs(emaRPM) < deadzone) emaRPM = 0.0f;
    // if (fabs(emaPWM) < deadzone) emaPWM = 0.0f;
    movementDir = emaRPM >= 0;

    // Optional debug:
    // Serial.println(String("[Motor] RPM: ") + emaRPM +
    //                ", PWM: " + emaPWM +
    //                ", Samples: " + samplesUsed);
}

float Motor::getSmoothedRPM(){
  return 60.0*emaRPM;
}


double Motor::get_deg() {
  portENTER_CRITICAL(&encoderMutex);
  double degrees_motor = (encoderValue / (float)tpr) * 180; //was 180 because we had two encoders
  portEXIT_CRITICAL(&encoderMutex);
  return degrees_motor / gearR;
}

void Motor::clearHistory() {
  portENTER_CRITICAL(&encoderMutex);
  encoderValue = 0;
  for(int i = 0; i < SAMPLES_BUFFER_SIZE; i++){
    encoderHistory[i].timestamp = 0;
    encoderHistory[i].dir = 0;
    encoderHistory[i].pwm = 0;
    mock_state[i].pwm = 0;
    mock_state[i].timestamp = 0;
    mock_state[i].dir = 0;
  }
  portEXIT_CRITICAL(&encoderMutex);
}

bool Motor::updatePositionControl(double dt) {
  double currentPosition = get_deg();
  float normalizedPos = calculateNormalizedPosition(currentPosition);
  float velocitySetpoint = positionPid.update(normalizedPos, dt);

  DPRINT("----- Position Control ----- Real Target: ");
  DPRINT(targetPosition);
  DPRINT(" || Real Position: ");
  DPRINT(currentPosition);
  DPRINT(" || Normalized Position: ");
  DPRINT(normalizedPos);
  DPRINT(" || PID output: ");
  DPRINTLN(velocitySetpoint);

  if(positionPid.finished()){
    clearPositionControl();
    stop();
    return true;
  }

  speedPid.setSetpoint(velocitySetpoint);
  return false;
}

void Motor::updateSpeedControl(double dt) {
    double rpm = getSmoothedRPM();
    DPRINT("[motor " + name + "] got RPM ");
    float safeMaxRpm = fmaxf(currentMaxRpm, 1.0f);
    DPRINT("[motor " + name + "] safe maxed: " + safeMaxRpm);
    float normalizedRPM = rpm / safeMaxRpm;
    DPRINT("[motor " + name + "] normalizedRPM: " + normalizedRPM);
    DPRINT("[motor " + name + "] entering pid with dt: " + dt);
    float output = speedPid.update(normalizedRPM, dt);
    DPRINT("[motor " + name + "] loging");
    logPID(rpm, output,speedPid.getSetPoint()*safeMaxRpm, false);

    int outputPWM = static_cast<int>(output * 255.0f);
    bool direction = (outputPWM >= 0);
    outputPWM = min(abs(outputPWM), 255);

    DPRINT("[motor " + name + "] RPM: "); DPRINT(rpm);
    DPRINT(" | normalizedRPM: "); DPRINT(normalizedRPM);
    DPRINT(" | safeMaxRpm: "); DPRINT(safeMaxRpm);
    DPRINT(" | setPoint: "); DPRINT(speedPid.getSetPoint());
    DPRINT(" | OutputPWM: "); DPRINT(outputPWM);
    DPRINT(" | Direction: "); DPRINTLN(direction);

    move(direction, outputPWM);
}

void Motor::printMock() {
  getSmoothedRPM();
  DPRINT("Motor Sampling Time Delta: [");
  double totalTime = 0;
  for(int i = 0; i < SAMPLES_BUFFER_SIZE; i++){
    DPRINT("(");
    DPRINT(mock_state[i].timestamp);
    DPRINT(",");
    DPRINT(mock_state[i].dir);
    DPRINT(",");
    DPRINT(mock_state[i].pwm);
    DPRINT("), ");
    totalTime += mock_state[i].timestamp;
  }
  DPRINTLN("]");

  double averageTime = totalTime / SAMPLES_BUFFER_SIZE;
  DPRINT("Motor Sampling average Hz: ");
  DPRINTLN(1e6 / averageTime);
}

float Motor::getTargetSpeed() const {
  return speedPid.getSetPoint();
}

float Motor::getTargetPosition() const {
  return targetPosition;
}

DriveMode Motor::get_DriveMode() const {
  return currentDriveMode;
}

void Motor::setToMove() {
  currentDriveMode = DriveMode::DRIVE;
  calculateSmoothedRPM();
}

void Motor::logPID(float input, float output, float target, bool positional) {
  LOG_FUNC(
    float pwm_w_dir = movementDir ? (float)pwm : -(float)pwm;
    // const char* logType = positional ? "position" : "velocity";
    DPRINTLN(String("[Motor] ") + name + " starting loging...");

    if (db_logger && db_logger->isReady()) {
      ILogRecord* rec = db_logger->createRecord(name.c_str(), false); 
      if (!rec) {
        DPRINTLN("WARNING: PID log skipped: record creation failed");
        return;
      }
      rec->set("input", input)
         .set("output", output)
         .set("target", target)
         .set("max_speed", currentMaxRpm)
         .set("pwm", pwm_w_dir)
         .set("error", positional ? positionPid.getPrevError() : speedPid.getPrevError());

      if (!rec->commit(false)) {
        DPRINTLN("WARNING: PID log skipped: commit failed");
        delete rec;  // safe cleanup if commit failed
      }
    }
  );
}


// Interrupt Helpers and Attachments

#define EXPAND(x) x

#define ENCODER_PINS \
  X(ENC_A_1)         \
  X(ENC_B_1)

#define X(PIN) \
  void IRAM_ATTR encoderISR##PIN() { \
    if (encoderMap[EXPAND(PIN)]) { \
      encoderMap[EXPAND(PIN)]->updateEncoder(); \
    } \
  }
ENCODER_PINS
#undef X

void attachAllEncoderInterrupts() {
  #define X(PIN) attachInterrupt(digitalPinToInterrupt(PIN), encoderISR##PIN, CHANGE);
  ENCODER_PINS
  #undef X
}


