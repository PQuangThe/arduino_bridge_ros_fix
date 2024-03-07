/* Basic Multi Threading Arduino Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
// Please read file README.md in the folder containing this example./*
#define USE_BASE
#define MAX_LINE_LENGTH (64)
#define BAUDRATE     115200
#define MAX_PWM        150

#include "commands.h"

#include "motor_driver.h"

/* Encoder driver function definitions */
#include "encoder_driver.h"

/* PID parameters and functions */
#include "PID_Controller.h"

#define PID_RATE           30     // Hz

/* Convert the rate into an interval */
const int PID_INTERVAL = 1000 / PID_RATE;
/* Stop the robot if it hasn't received a movement command
  in this number of milliseconds */
#define AUTO_STOP_INTERVAL 2000
long lastMotorCommand = AUTO_STOP_INTERVAL;

// Define two tasks for reading and writing from and to the serial port.
void TaskWriteToArg(void *pvParameters);
void TaskReadFromSerial(void *pvParameters);
void TaskPID(void *pvParameters);
void TaskAutoStop(void *pvParameters);
void runCommand(char ch, char a[], char b[]);

// Define Queue handle
QueueHandle_t QueueHandle;
const int QueueElementSize = 10;
typedef struct{
  char line[MAX_LINE_LENGTH];
  uint8_t line_length;
} message_t;

// The setup function runs once when you press reset or power on the board.
void setup() {
  Serial.begin(BAUDRATE);
  while(!Serial){delay(10);}

  ESP32Encoder::useInternalWeakPullResistors = puType::UP;

  // use pin 19 and 18 for the first encoder
  encoder_right.attachFullQuad(RIGHT_ENC_PIN_A, RIGHT_ENC_PIN_B);
  
  // use pin 17 and 16 for the second encoder
  encoder_left.attachFullQuad(LEFT_ENC_PIN_A, LEFT_ENC_PIN_B);
    
  // set starting count value after attaching
  encoder_right.setCount(0);
  encoder_left.setCount(0);

  // clear the encoder's raw count and set the tracked count to zero
  encoder_right.clearCount();
  encoder_left.clearCount();

  pinMode(RIGHT_MOTOR_BACKWARD, PULLDOWN);
  pinMode(LEFT_MOTOR_BACKWARD,  PULLDOWN); 
  pinMode(RIGHT_MOTOR_FORWARD,  PULLDOWN); 
  pinMode(LEFT_MOTOR_FORWARD,   PULLDOWN); 
  pinMode(RIGHT_MOTOR_ENABLE,   PULLDOWN); 
  pinMode(LEFT_MOTOR_ENABLE,    PULLDOWN); 

  initMotorController();
  Init_PID();
  resetPID();

  // Create the queue which will have <QueueElementSize> number of elements, each of size `message_t` and pass the address to <QueueHandle>.
  QueueHandle = xQueueCreate(QueueElementSize, sizeof(message_t));

  // Check if the queue was successfully created
  if(QueueHandle == NULL){
    Serial.println("Queue could not be created. Halt.");
    while(1) delay(1000); // Halt at this point as is not possible to continue
  }

  // Set up two tasks to run independently.
  xTaskCreate(
    TaskWriteToArg
    ,  "Task Write To Arg" // A name just for humans
    ,  8192        // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  NULL        // No parameter is used
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL // Task handle is not used here
    );

  xTaskCreate(
    TaskReadFromSerial
    ,  "Task Read From Serial"
    ,  2048  // Stack size
    ,  NULL  // No parameter is used
    ,  1  // Priority
    ,  NULL // Task handle is not used here
    );

  xTaskCreate(
    TaskPID
    ,  "Task run a PID calculation"
    ,  2048  // Stack size
    ,  NULL  // No parameter is used
    ,  2  // Priority
    ,  NULL // Task handle is not used here
    );

  xTaskCreate(
    TaskAutoStop
    ,  "Task auto-stop interval"
    ,  2048  // Stack size
    ,  NULL  // No parameter is used
    ,  3  // Priority
    ,  NULL // Task handle is not used here
    );
  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
  //Serial.printf("\nAnything you write will return as echo.\nMaximum line length is %d characters (+ terminating '0').\nAnything longer will be sent as a separate line.\n\n", MAX_LINE_LENGTH-1);
}

void loop(){
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskWriteToArg(void *pvParameters){  // This is a task.
  message_t message;
  uint8_t c_arg = 0;
  uint8_t buf = 0;
  char cmd;
  char argv1[16];
  char argv2[16];
  for (;;){
    if(QueueHandle != NULL){ // Sanity check just to make sure the queue actually exists
      int ret = xQueueReceive(QueueHandle, &message, portMAX_DELAY);
      if(ret == pdPASS){
        // The message was successfully received - send it back to Serial port and "Echo: "
        //Serial.printf("Echo line of size %d: \"%s\"\n", message.line_length, message.line);
        // The item is queued by copy, not by reference, so lets free the buffer after use.

        for(int i =0; message.line[i] != NULL; i++)
        {
          if (message.line[i] == 13) 
          {
            if (c_arg == 1) argv1[buf] = NULL;
            else if (c_arg == 2) argv2[buf] = NULL;
            runCommand(cmd, argv1, argv2);
            cmd = NULL;
            memset(argv1, 0, sizeof(argv1));
            memset(argv2, 0, sizeof(argv2));
            c_arg=0;
            buf=0;
          }
          // Use spaces to delimit parts of the command
          else if (message.line[i] == ' ') {
            // Step through the arguments
            if (c_arg == 0) c_arg = 1;
            else if (c_arg == 1)  {
              argv1[buf] = NULL;
              c_arg = 2;
              buf = 0;
            }
            continue;
          }
          else {
            if (c_arg == 0) {
              // The first arg is the single-letter command
              cmd = message.line[i];
            }
            else if (c_arg == 1) {
              // Subsequent arguments can be more than one character
              argv1[buf] = message.line[i];
              buf++;
            }
            else if (c_arg == 2) {
              argv2[buf] = message.line[i];
              buf++;
            }
          }
        }

      }else if(ret == pdFALSE){
        Serial.println("The `TaskWriteToSerial` was unable to receive data from the Queue");
      }
    } // Sanity check
    vTaskDelay(5 / portTICK_PERIOD_MS);
  } // Infinite loop
}

void TaskReadFromSerial(void *pvParameters){  // This is a task.
  message_t message;
  for (;;){
    // Check if any data are waiting in the Serial buffer
    message.line_length = Serial.available();
    if(message.line_length > 0){
      // Check if the queue exists AND if there is any free space in the queue
      if(QueueHandle != NULL && uxQueueSpacesAvailable(QueueHandle) > 0){
        int max_length = message.line_length < MAX_LINE_LENGTH ? message.line_length : MAX_LINE_LENGTH-1;
        for(int i = 0; i < max_length; ++i){
          message.line[i] = Serial.read();
        }
        message.line_length = max_length;
        message.line[message.line_length] = 0; // Add the terminating nul char

        // The line needs to be passed as pointer to void.
        // The last parameter states how many milliseconds should wait (keep trying to send) if is not possible to send right away.
        // When the wait parameter is 0 it will not wait and if the send is not possible the function will return errQUEUE_FULL
        int ret = xQueueSend(QueueHandle, (void*) &message, 0);
        if(ret == pdTRUE){
          // The message was successfully sent.
        }else if(ret == errQUEUE_FULL){
          // Since we are checking uxQueueSpacesAvailable this should not occur, however if more than one task should
          //   write into the same queue it can fill-up between the test and actual send attempt
          //Serial.println("The `TaskReadFromSerial` was unable to send data into the Queue");
        } // Queue send check
      } // Queue sanity check
    }else{
      //delay(100); // Allow other tasks to run when there is nothing to read
    } // Serial buffer check
    vTaskDelay(5 / portTICK_PERIOD_MS);
  } // Infinite loop
}

void TaskPID(void *pvParameters){  // This is a task.
  
  for (;;){
    updatePID();
    vTaskDelay(PID_INTERVAL / portTICK_PERIOD_MS);
  } // Infinite loop
}

void TaskAutoStop(void *pvParameters){  // This is a task.
  
  for (;;){
    if ((millis() - lastMotorCommand) > AUTO_STOP_INTERVAL) 
    {
      setMotorSpeeds(0, 0);
      moving = 0;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  } // Infinite loop
}

/*



*/
void runCommand(char ch, char a[], char b[])
{
  int arg1 = atoi(a);
  int arg2 = atoi(b);
  uint8_t k=0;
  char* p = a;
  char* str;
  int pid_args[4];

  switch(ch) 
  {
    case READ_ENCODERS:
      Serial.print(readEncoder(LEFT));
      Serial.print(" ");
      Serial.println(readEncoder(RIGHT));
      break;
    case RESET_ENCODERS:
      resetEncoders();
      resetPID();
      Serial.println("OK");
      break;
    case MOTOR_SPEEDS:
      /* Reset the auto stop timer */
      lastMotorCommand = millis();
      if (arg1 == 0 && arg2 == 0) {
        setMotorSpeeds(0, 0);
        resetPID();
        moving = 0;
      }
      else moving = 1;
      leftPID.TargetTicksPerFrame = arg1;
      rightPID.TargetTicksPerFrame = arg2;
      Serial.println("OK"); 
      break;
    case MOTOR_RAW_PWM:
      /* Reset the auto stop timer */
      lastMotorCommand = millis();
      resetPID();
      moving = 0; // Sneaky way to temporarily disable the PID
      setMotorSpeeds(arg1, arg2);
      Serial.println("OK"); 
      break;
    case UPDATE_PID_LEFT:
      while ((str = strtok_r(p, ":", &p)) != NULL) {
        pid_args[k] = atoi(str);
        k++;
      }
      SetTuningsMotor(LEFT,pid_args[0], pid_args[1], pid_args[2], pid_args[3]);
      Serial.println("OK");
      break;
    case UPDATE_PID_RIGHT:
      while ((str = strtok_r(p, ":", &p)) != NULL) {
        pid_args[k] = atoi(str);
        k++;
      }
      SetTuningsMotor(RIGHT,pid_args[0], pid_args[1], pid_args[2], pid_args[3]);
      Serial.println("OK");
      break;
    default:
      Serial.println("Invalid Command");
      break;
  }
}