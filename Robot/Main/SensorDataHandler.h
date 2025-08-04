 #ifndef SENSOR_DATA_HANDLER_H
 #define SENSOR_DATA_HANDLER_H

// #include "ClusteringAlgorithms.h"
#include "DistanceSensor.h"  // Include the DistanceSensor class
#include "CenterLineEstimator.h"
#include <vector>
#include "DebugConfig.h"
#include "ILogManager.h"
#include "positional_stl.h"
#include <mutex>

 class SensorDataHandler {
 private:
  
  DistanceSensor leftDistanceSensor;  
  DistanceSensor frontDistanceSensor; 
  DistanceSensor rightDistanceSensor; 
  volatile float cache_left = -1,cache_right= -1, cache_front= -1; 
  CL_Estimator* estimator;
  ILogManager* logger;
  IPoseProvider* poseFactory;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;


  int badFrameCount = 0;
  const int badFrameThreshold = 3;
  
  bool setUpDistanceSensors();
  void updateSensorDataHandler(const Pose& roboPose, float l, float r, float f);

  static void sensorTaskWrapper(void* pvParams);
    void getRawSensorsReading(float& l, float& r, float& f);
  void sensorTaskLoop();
 public:
 //SensorDataHandler(std::shared_ptr<ClusteringAlgorithm> clustering_method);
  SensorDataHandler(IPoseProvider* poseFactory);
  ~SensorDataHandler();
  
  bool begin(ILogManager* db_logger);  // Initialize the sensor

  void resetCorridorEstimation();

  bool detectInCorridor();

  float headSensor();

  Line* getCenterLine() const;

 };

 #endif // SENSOR_DATA_HANDLER_H
