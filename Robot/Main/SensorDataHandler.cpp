#include "soc/soc.h"
#include "SensorDataHandler.h"
#include "Config.h"
#include <numeric>
#include <vector>

#define DPRINT(x)    DPRINT_SENSOR(x)
#define DPRINTLN(x)  DPRINTLN_SENSOR(x)
#define LOG_FUNC(x)  LOG_SENSOR_MSG(x)

SensorDataHandler::SensorDataHandler(IPoseProvider* poseFactory)
    : leftDistanceSensor(SHT_LOX1),
      frontDistanceSensor(SHT_LOX2),
      rightDistanceSensor(SHT_LOX3),
      poseFactory(poseFactory) {
    DPRINTLN_INIT("[SensorDataHandler] Constructor called");
    estimator = new PCA_CL_Estimator();
}

SensorDataHandler::~SensorDataHandler() {
    delete estimator;
}

bool SensorDataHandler::begin(ILogManager* db_logger) {
    DPRINTLN_INIT("[SensorSetup] Initializing SensorDataHandler...");
    this->logger = db_logger;

    const int maxRetries = 10;
    int attempt = 0;

    while (attempt < maxRetries) {
        DPRINT_INIT("[SensorSetup] Sensor init attempt: ");
        DPRINTLN_INIT(attempt + 1);
        if (setUpDistanceSensors()) {
            DPRINTLN_INIT("[SensorSetup] ✅ All distance sensors initialized successfully.");
            xTaskCreatePinnedToCore(sensorTaskWrapper, "SensorTask", 4096, this, 3, nullptr, 0);
            return true;
        } else {
            DPRINTLN_INIT("[SensorSetup] ❌ One or more sensors failed to begin.");
            // DPRINTLN_INIT("[SensorSetup] ❌ setUpDistanceSensors() failed.");
        }
        attempt++;
        delay(500);
    }

    DPRINTLN_INIT("[SensorSetup] ❌ Failed to initialize sensors after multiple attempts.");
    return false;
}

bool SensorDataHandler::setUpDistanceSensors() {
  DPRINTLN_INIT("[SensorSetup] Starting setup for distance sensors...");
  leftDistanceSensor.turnOff();
  frontDistanceSensor.turnOff();
  rightDistanceSensor.turnOff();
  DPRINTLN_INIT("[SensorSetup] LOW reset pins written.");
  DPRINTLN_INIT("[SensorSetup] Initializing LEFT VL53L0X...");
  if(!leftDistanceSensor.begin(LOX1_ADDRESS)){
    DPRINTLN_INIT("[SensorSetup] Failed to boot LEFT VL53L0X");
    return false;
  }
  DPRINTLN_INIT("[SensorSetup] LEFT VL53L0X initialized.");
  DPRINTLN_INIT("[SensorSetup] Initializing FRONT VL53L0X...");
  if(!frontDistanceSensor.begin(LOX2_ADDRESS)){
    DPRINTLN_INIT("[SensorSetup] Failed to boot FRONT VL53L0X");
    return false;
  }
  DPRINTLN_INIT("[SensorSetup] FRONT VL53L0X initialized.");
  DPRINTLN_INIT("[SensorSetup] Initializing RIGHT VL53L0X...");
  if(!rightDistanceSensor.begin(LOX3_ADDRESS)){
    DPRINTLN_INIT("[SensorSetup] Failed to boot RIGHT VL53L0X");
    return false;
  }
  DPRINTLN_INIT("[SensorSetup] RIGHT VL53L0X initialized.");
  DPRINTLN_INIT("[SensorSetup] Successfully finished setUpDistanceSensors.");
  return true;
}
    

void SensorDataHandler::sensorTaskWrapper(void* pvParams) {
    DPRINTLN("[sensorTaskWrapper] Called");
    SensorDataHandler* self = static_cast<SensorDataHandler*>(pvParams);
    self->sensorTaskLoop();
}

void SensorDataHandler::sensorTaskLoop() {
    int64_t lastTimeUs = esp_timer_get_time();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(80));

        int64_t nowUs = esp_timer_get_time();
        float loopDurationMs = (nowUs - lastTimeUs) / 1000.0f;
        lastTimeUs = nowUs;

        DPRINT_TIMING("[SensorTaskLoop] Tick. Interval (ms): ");
        DPRINTLN_TIMING(loopDurationMs);

        float l, r, f;
        Pose currentPose = poseFactory->getPose();
        DPRINTLN("[SensorTaskLoop] Pose acquired.");

        getRawSensorsReading(l, r, f);
        updateSensorDataHandler(currentPose, l, r, f);
        DPRINTLN("[SensorTaskLoop] Sensor data updated.");

        Line* line = getCenterLine();
        delete line;
        DPRINTLN("[SensorTaskLoop] Center line processed and cleaned.");
    }
}

void SensorDataHandler::getRawSensorsReading(float& l, float& r, float& f) {
    l = leftDistanceSensor.readSensorCM();
    r = rightDistanceSensor.readSensorCM();
    f = frontDistanceSensor.readSensorCM();
    portENTER_CRITICAL(&mux);
    cache_left = l;
    cache_right = r;
    cache_front = f;
    portEXIT_CRITICAL(&mux);
    DPRINTLN(String("Sensor Readings: ") + l +", "+r+", "+f);
    DPRINTLN("[getRawSensorsReading] Sensor values retrieved.");
}

void SensorDataHandler::updateSensorDataHandler(const Pose& robotPose,float l, float r, float f) {
    DPRINTLN("[SensorDataHandler] entered updateSensorDataHandler.");
    float dLeft = l ;
    float dRight = r ;
    DPRINT("[SensorDataHandler] Raw sensor readings - Left: ");
    DPRINT(dLeft);
    DPRINT(", Right: ");
    DPRINTLN(dRight);

    bool leftValid = dLeft > 0 && dLeft < MAX_SENSOR_READ;
    bool rightValid = dRight > 0 && dRight < MAX_SENSOR_READ;

    if (!leftValid || !rightValid) {
        DPRINT("[SensorDataHandler] Invalid readings - Left: ");
        DPRINT(leftValid);
        DPRINT(", Right: ");
        DPRINTLN(rightValid);
        badFrameCount++;
        if (badFrameCount >= badFrameThreshold) {
            DPRINTLN("[SensorDataHandler] resetting estimator.");
            estimator->reset();
            badFrameCount = 0;
        }
        return;
    } else
        badFrameCount = 0;

    const double& theta = robotPose.theta;
    const Point& pos = robotPose.loc;

    DPRINTLN("[SensorDataHandler] Theta: " + String(theta));
    DPRINTLN("[SensorDataHandler] Robot pos: x=" + String(pos.x) + ", y=" + String(pos.y));

    float dx_perp = -sin(theta);
    float dy_perp = cos(theta);

    Point leftWall = pos + Point(dx_perp * dLeft, dy_perp * dLeft);
    Point rightWall = pos - Point(dx_perp * dRight, dy_perp * dRight);

    DPRINTLN("[SensorDataHandler] Calculated left wall: x=" + String(leftWall.x) + ", y=" + String(leftWall.y));
    DPRINTLN("[SensorDataHandler] Calculated right wall: x=" + String(rightWall.x) + ", y=" + String(rightWall.y));

    LOG_FUNC(
      if (logger && logger->isReady()) {
          ILogRecord* rec = logger->createRecord("Walls", true); 
          if (rec) {
              ILogRecord& rec1 = rec->set("Walls Exist", "true")
                  .set("lWall_x", leftWall.x)
                  .set("lWall_y", leftWall.y)
                  .set("rWall_x", rightWall.x)
                  .set("rWall_y", rightWall.y);
              if (!rec->commit(true)) {
                  DPRINTLN("WARNING: SensorData log skipped: commit failed");
                  delete rec;
              }
          } else {
              DPRINTLN("WARNING: Log record creation failed");
          }
      })
    estimator->addMeasurement(leftWall, rightWall);
    DPRINTLN("[SensorDataHandler] Measurement added to estimator.");
}

Line* SensorDataHandler::getCenterLine() const {
    DPRINTLN("[SensorDataHandler] Called");
    Line* res = estimator->getSmoothedCenterLine();
    // Point dst[MAX_HISTORY];
    // estimator->getHistory(dst);
    // Line* res = new Line({dst[MAX_HISTORY - 1].x , -10}, 0.0);
    if (!res) {
        DPRINTLN("[getCenterLine] No result - estimator not ready.");
        return nullptr;
    }
    Point line_p = res ? res->getLineDefPoint() : Point(0,0);
    float line_theta = res ? res->getLineTheta() : 0;
    DPRINT("[SensorDataHandler] Got line: x=");
    DPRINT(line_p.x);
    DPRINT(", y=");
    DPRINT(line_p.y);
    DPRINT(", theta=");
    DPRINTLN(line_theta);
    LOG_FUNC(
      bool a= false;
      if (logger && logger->isReady()){
          ILogRecord* rec = logger->createRecord("CenterLine", true); 
          if (rec) {
              ILogRecord& rec1 = rec->set("Line_x", line_p.x)
                  .set("Line_y",line_p.y)
                  .set("Line_theta",line_theta)
                  .set("Exist", res != nullptr);
              if (!rec->commit(true)) {
                  DPRINTLN("WARNING: SensorData log skipped: commit failed");
                  delete rec;
              }
          } else {
              DPRINTLN("WARNING: Log record creation failed");
          }
    })
    return res;
}
void SensorDataHandler::resetCorridorEstimation(){
  estimator->reset();
  detectInCorridor();
}

bool SensorDataHandler::detectInCorridor() {
  static int corridorEstimator = 0;
  DPRINTLN("[SensorDataHandler] Checking corridor detection...");
  corridorEstimator = estimator->isReady() ? corridorEstimator+1 : 0; 
  DPRINTLN(String("[SensorDataHandler] Line exists: ") + res);
  return corridorEstimator >= CORRIDOR_SURENESS;
}

float SensorDataHandler::headSensor() {
    portENTER_CRITICAL(&mux);
    float res = cache_front;
    portEXIT_CRITICAL(&mux);
    return res;
}
