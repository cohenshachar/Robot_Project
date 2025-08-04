#ifndef SENSORCONFIG_H
#define SENSORCONFIG_H

// address we will assign if dual sensor is present
#define LOX1_ADDRESS 0x32
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x30

// set the pins to shutdown
#define SHT_LOX1 25 
#define SHT_LOX2 26
#define SHT_LOX3 27

#define WALL_DETECTION_MAX_DISTANCE 40.0f //cm
#define MIN_SENSOR_READ 0.0f //cm
#define MAX_SENSOR_READ 60.0f //cm


#define CRITICAL_COLLISION_DISTANCE 10.0f
#define SAFE_COLLISION_DISTANCE 20.0f
#define CORRIDOR_SURENESS 25
#endif