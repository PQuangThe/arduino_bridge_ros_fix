
#define RIGHT_MOTOR_BACKWARD 25
#define LEFT_MOTOR_BACKWARD  26
#define RIGHT_MOTOR_FORWARD  32
#define LEFT_MOTOR_FORWARD   33
#define RIGHT_MOTOR_ENABLE 18
#define LEFT_MOTOR_ENABLE 19

void initMotorController();
void setMotorSpeed(int i, int spd);
void setMotorSpeeds(int leftSpeed, int rightSpeed);
