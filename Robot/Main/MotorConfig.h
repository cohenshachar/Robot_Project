#ifndef MOTORCONFIG_H
#define MOTORCONFIG_H


// Motor A Pin Configuration (Hardware-Specific) RIGHT
#define IN_A_1 12
#define IN_A_2 14
#define ENC_A_1 34
#define ENC_A_2 35
#define PWM_A_1 0
#define PWM_A_2 1

#define A_PP 0.8
#define A_PI 0.1
#define A_PD 0.03
#define A_SP 0.07
#define A_SI 0.88
#define A_SD 0.0007

// Motor B Pin Configuration (Hardware-Specific) LEFT
#define IN_B_1 19
#define IN_B_2 18
#define ENC_B_1 32
#define ENC_B_2 33
#define PWM_B_1 2
#define PWM_B_2 3

#define B_PP 1
#define B_PI 0.1
#define B_PD 0.03
#define B_SP 0.08
#define B_SI 0.451
#define B_SD 0.0003

// Miscellaneous
#define GEAR_RATIO 100
#define TPR 10.56 // TPR + magic number


#define KICKSTART_DELAY_MS 3
// #define FORWARD 1
// #define REVERSE 0


#define WHEEL_RADIUS 22.5 //in mm
#define WHEEL_BASE 85

#define DEFAULT_SPEED_TIMEOUT_US 30000
#define DEFAULT_SPEED_FRAME_US 20000 
#define ENCODER_BUFFER_SIZE 16
#define SAMPLES_FOR_SMOOTHING 5
#define SAMPLES_BUFFER_SIZE 128
// Forward declare the global encoder map

#define TICK_TO_CONTROL 5
#define LOOK_AHEAD_DIST_CM 25.0f
//Motor A has correct degree for divisor of 60 degrees at calculation.
// jump starts for 250 initial 5 delay @ 110. 
 

#endif // MOTORCONFIG_H
