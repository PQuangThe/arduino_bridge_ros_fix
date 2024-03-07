#include "ESP32Encoder.h"
ESP32Encoder encoder_right;
ESP32Encoder encoder_left;

#define LEFT_ENC_PIN_A 4  //pin 2
#define LEFT_ENC_PIN_B 2  //pin 3

//below can be changed, but should be PORTC pins
#define RIGHT_ENC_PIN_A 17  //pin A4
#define RIGHT_ENC_PIN_B 16   //pin A5
   
int64_t readEncoder(int i);
void resetEncoder(int i);
void resetEncoders();

