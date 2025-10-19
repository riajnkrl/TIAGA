#include "esp_camera.h"
#include <vehicle.h>
#include <ultrasonic.h>
#include <ESP32Servo.h>
#include <Arduino.h>

// ========== PIN DEFINITIONS ==========
#define Shoot_PIN 32
#define CAMERA_SERVO_PIN 25
#warning "TIAGA_JOANE: Authoritative pin assignments for arm and soil sensor. Do NOT change these values unless you know what you are doing."
// Authoritative pin assignments (TIAGA_JOANE). Keep these values in sync in other sketches.
#define ARM_SERVO_PIN 27
#define SOIL_SENSOR_PIN 34

#define LED_Module1 2
#define LED_Module2 12
#define Left_sensor 35
#define Middle_sensor 36
#define Right_sensor 39
#define Buzzer 33

// ========== COMMAND DEFINITIONS ==========
#define CMD_RUN 1
#define CMD_GET 2
#define CMD_STANDBY 3
#define CMD_TRACK_1 4
#define CMD_TRACK_2 5
#define CMD_AVOID 6
#define CMD_FOLLOW 7
#define CMD_ARM_PUSH 8
#define CMD_ARM_RETRACT 9

// ========== MUSIC NOTES ==========
#define C3 131
#define D3 147
#define E3 165
#define F3 175
#define G3 196
#define A3 221
#define B3 248
#define C4 262
#define D4 294
#define E4 330
#define F4 350
#define G4 393
#define A4 441
#define B4 495
#define C5 525
#define D5 589
#define E5 661
#define F5 700
#define G5 786
#define A5 882
#define B5 990
#define N 0

// ========== HARDWARE OBJECTS ==========
vehicle Acebott;
ultrasonic Ultrasonic;
Servo cameraServo;
Servo armServo;

// ========== LINE TRACKING ==========
int Left_Tra_Value;
int Middle_Tra_Value;
int Right_Tra_Value;
int Black_Line = 2000;
int Off_Road = 4000;

// ========== MOVEMENT ==========
int speeds = 250;
int leftDistance = 0;
int middleDistance = 0;
int rightDistance = 0;

// ========== SOIL MOISTURE ==========
int soilValue = 0;
int soilPercent = 0;
int dryValue = 3000;
int wetValue = 1000;
bool isReadingSoil = false;
unsigned long lastSoilReadTime = 0;
const unsigned long SOIL_READ_INTERVAL = 1000; // read every 1 second

// ========== ROBOT ARM ==========
int armCurrentPosition = 90;
const int ARM_HOME = 90;
const int ARM_DOWN = 180;
const int ARM_MIN = 90;
const int ARM_MAX = 180;
const int ARM_SPEED = 15; // ms delay per degree

// ========== COMMUNICATION ==========
String Version = "Firmware v0.12.22 + Full Integration";
byte dataLen, index_a = 0;
char buffer[52];
unsigned char prevc = 0;
bool isStart = false;
byte val = 0;
int UT_distance = 0;

// ========== MUSIC ==========
int length0, length1, length2, length3;
int tune0[] = {C4, N, C4, G4, N, G4, A4, N, A4, G4, N, F4, N, F4, E4, N, E4, D4, N, D4, C4};
float durt0[] = {0.99, 0.01, 1, 0.99, 0.01, 1, 0.99, 0.01, 1, 1.95, 0.05, 0.99, 0.01, 1, 0.99, 0.01, 1, 0.99, 0.01, 1, 2};
int tune1[] = {E4, N, E4, N, E4, N, E4, N, E4, N, E4, N, E4, G4, C4, D4, E4};
float durt1[] = {0.49, 0.01, 0.49, 0.01, 0.99, 0.01, 0.49, 0.01, 0.49, 0.01, 0.99, 0.01, 0.5, 0.5, 0.75, 0.25, 1, 2};
int tune2[] = {C5, N, C5, N, C5, G4, E5, N, E5, N, E5, C5, N, C5, E5, G5, N, G5, F5, E5, D5, N};
float durt2[] = {0.49, 0.01, 0.49, 0.01, 1, 1, 0.49, 0.01, 0.49, 0.01, 1, 0.99, 0.01, 0.5, 0.5, 0.99, 0.01, 1, 0.5, 0.5, 1, 1};
int tune3[] = {C4, N, C4, N, C4, G3, A3, N, A3, G3, E4, N, E4, D4, N, D4, C4};
float durt3[] = {0.99, 0.01, 0.99, 0.01, 1, 1, 0.99, 0.01, 1, 2, 0.99, 0.01, 1, 0.99, 0.01, 1, 1};

// ========== MODES ==========
enum FUNCTION_MODE {
  STANDBY,
  FOLLOW,
  TRACK_1,
  TRACK_2,
  AVOID,
} function_mode;

int newRunMode = 0;

// ========== FUNCTION DECLARATIONS ==========
unsigned char readBuffer(int index_r);
void writeBuffer(int index_w, unsigned char c);
void initCamera();
void moveArmToPosition(int angle);
void moveArmSmooth(int targetAngle);
void pushSensorSequence();
void retractArmSequence();
void readSoilMoisture();
void model1_func();
void model2_func();
void model3_func();
void model4_func();
void Camera_Servo_Move(int angles);
void Music_a();
void Music_b();
void Music_c();
void Music_d();
void Buzzer_run(int M);
void runModule(int device);
void parseData();
void RXpack_func();

// ========== BUFFER FUNCTIONS ==========
unsigned char readBuffer(int index_r) {
  return buffer[index_r];
}

void writeBuffer(int index_w, unsigned char c) {
  buffer[index_w] = c;
}

// ========== CAMERA INITIALIZATION ==========
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // NOTE: Adjust based on your specific ESP32-CAM model
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
  Serial.println("Camera initialized successfully!");
}

// ========== SETUP ==========
void setup() {
  Serial.setTimeout(10);
  Serial.begin(115200);
  
  // Wait for serial
  delay(1000);
  Serial.println("\n\n=== ACEBOTT ROBOT STARTING ===");

  // Initialize vehicle
  Acebott.Init();
  Serial.println("✓ Vehicle initialized");

  // Initialize ultrasonic
  Ultrasonic.Init(13, 14);
  Serial.println("✓ Ultrasonic sensor initialized");

  // Initialize pins
  pinMode(LED_Module1, OUTPUT);
  pinMode(LED_Module2, OUTPUT);
  pinMode(Shoot_PIN, OUTPUT);
  pinMode(Left_sensor, INPUT);
  pinMode(Middle_sensor, INPUT);
  pinMode(Right_sensor, INPUT);
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(Buzzer, OUTPUT);
  Serial.println("✓ GPIO pins configured");

  // Initialize servos
  ESP32PWM::allocateTimer(1);
  cameraServo.attach(CAMERA_SERVO_PIN);
  cameraServo.write(90);
  Serial.println("✓ Camera servo initialized");

  armServo.attach(ARM_SERVO_PIN);
  moveArmToPosition(ARM_HOME);
  Serial.println("✓ Robot arm initialized at HOME (90°)");

  // Initialize camera (OPTIONAL - comment out if not using ESP32-CAM)
  // initCamera();

  // Stop motors
  Acebott.Move(Stop, 0);

  // Calculate music lengths
  length0 = sizeof(tune0) / sizeof(tune0[0]);
  length1 = sizeof(tune1) / sizeof(tune1[0]);
  length2 = sizeof(tune2) / sizeof(tune2[0]);
  length3 = sizeof(tune3) / sizeof(tune3[0]);

  Serial.println("✓ Music arrays loaded");
  Serial.println("\n=== SYSTEM READY ===");
  Serial.println(Version);
  Serial.println("Commands: p=push arm | r=retract arm\n");

  // Startup beep
  tone(Buzzer, 1000, 100);
  delay(150);
  tone(Buzzer, 1500, 100);
}

// ========== MAIN LOOP ==========
void loop() {
    // Check for simple serial commands first
  if (Serial.available() > 0) {
    char simpleCmd = Serial.read();
    if (simpleCmd == 'p') {
      Serial.println("\n[TEST] Push arm");
      pushSensorSequence();
    }
    else if (simpleCmd == 'r') {
      Serial.println("\n[TEST] Retract arm");
      retractArmSequence();
    }
  }

  // NON-BLOCKING soil moisture reading
  if (isReadingSoil && (millis() - lastSoilReadTime >= SOIL_READ_INTERVAL)) {
    readSoilMoisture();
    lastSoilReadTime = millis();
  }

  // Handle incoming serial commands/packets
  RXpack_func();

  // Execute current mode
  switch (newRunMode) {
    case 4:
      model1_func(); // Line tracking (2 sensors)
      break;
    case 5:
      model4_func(); // Line tracking (3 sensors)
      break;
    case 6:
      function_mode = AVOID;
      model2_func(); // Obstacle avoidance
      break;
    case 7:
      model3_func(); // Object following
      break;
    case 3:
      function_mode = STANDBY;
      Acebott.Move(Stop, 0);
      newRunMode = 0;
      break;
  }
}

// ========== ROBOT ARM FUNCTIONS ==========

void moveArmToPosition(int angle) {
  angle = constrain(angle, ARM_MIN, ARM_MAX);
  armServo.write(180 - angle); // Inverted for horizontal mount
  armCurrentPosition = angle;
  Serial.printf("Arm position: %d°\n", angle);
}

void moveArmSmooth(int targetAngle) {
  targetAngle = constrain(targetAngle, ARM_MIN, ARM_MAX);
  
  Serial.printf("Moving arm: %d° → %d° (servo will write %d → %d)\n", 
                armCurrentPosition, targetAngle, 
                180 - armCurrentPosition, 180 - targetAngle);
  
  if (targetAngle == armCurrentPosition) {
    Serial.println("Already at target position");
    return;
  }
  
  if (targetAngle > armCurrentPosition) {
    Serial.println("Direction: DOWN (increasing angle)");
    for (int pos = armCurrentPosition; pos <= targetAngle; pos++) {
      armServo.write(180 - pos);
      delay(ARM_SPEED);
      if (pos % 15 == 0) { // Progress indicator every 15 degrees
        Serial.printf("  Position: %d°\n", pos);
      }
    }
  } else {
    Serial.println("Direction: UP (decreasing angle)");
    for (int pos = armCurrentPosition; pos >= targetAngle; pos--) {
      armServo.write(180 - pos);
      delay(ARM_SPEED);
      if (pos % 15 == 0) { // Progress indicator every 15 degrees
        Serial.printf("  Position: %d°\n", pos);
      }
    }
  }
  armCurrentPosition = targetAngle;
  Serial.printf("✓ Arm movement complete. Final position: %d°\n", armCurrentPosition);
}

void pushSensorSequence() {
  Serial.println("\n=== DEPLOYING ARM TO SOIL ===");
  Serial.printf("Current position: %d°\n", armCurrentPosition);

  // Check if already down (within 10 degrees of target)
  if (abs(armCurrentPosition - ARM_DOWN) <= 10) {
    Serial.println("⚠ Arm already deployed");
    if (!isReadingSoil) {
      isReadingSoil = true;
      lastSoilReadTime = millis();
      Serial.println("✓ Restarted soil monitoring");
    }
    return;
  }

  moveArmSmooth(ARM_DOWN);
  isReadingSoil = true;
  lastSoilReadTime = millis();
  
  Serial.println("✓ Arm deployed - monitoring soil moisture");
}

void retractArmSequence() {
  Serial.println("\n=== RETRACTING ARM ===");
  Serial.printf("Current position: %d°\n", armCurrentPosition);

  // Check if already home (within 10 degrees of target)
  if (abs(armCurrentPosition - ARM_HOME) <= 10) {
    Serial.println("⚠ Arm already at home");
    if (isReadingSoil) {
      isReadingSoil = false;
      Serial.println("✓ Stopped soil monitoring");
    }
    return;
  }

  isReadingSoil = false;
  moveArmSmooth(ARM_HOME);
  
  Serial.println("✓ Arm retracted to home position");
}

void readSoilMoisture() {
  soilValue = analogRead(SOIL_SENSOR_PIN);
  soilPercent = map(soilValue, dryValue, wetValue, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  Serial.printf("🌱 Soil: %d%% (raw: %d)\n", soilPercent, soilValue);

  if (soilPercent < 30) {
    Serial.println("⚠ CRITICAL: Soil very dry!");
    tone(Buzzer, 800, 200);
  } else if (soilPercent < 50) {
    Serial.println("⚠ WARNING: Soil moisture low");
    tone(Buzzer, 600, 100);
  } else {
    Serial.println("✓ Soil moisture OK");
    noTone(Buzzer);
  }
}

// ========== MOVEMENT MODES ==========

// AVOID MODE: Obstacle avoidance with ultrasonic
void model2_func() {
  cameraServo.write(90);
  UT_distance = Ultrasonic.Ranging();
  delay(50); // Prevent ultrasonic interference
  
  middleDistance = UT_distance;

  if (middleDistance <= 25 && middleDistance > 0) {
    Acebott.Move(Stop, 0);
    delay(100);
    
    // Random evasive maneuver
    int randNumber = random(1, 5);
    switch (randNumber) {
      case 1:
        Acebott.Move(Backward, 180);
        delay(400);
        Acebott.Move(Move_Left, 180);
        delay(400);
        break;
      case 2:
        Acebott.Move(Clockwise, 180);
        delay(800);
        break;
      case 3:
        Acebott.Move(Contrarotate, 180);
        delay(800);
        break;
      case 4:
        Acebott.Move(Backward, 180);
        delay(400);
        Acebott.Move(Move_Right, 180);
        delay(400);
        break;
    }
    Acebott.Move(Stop, 0);
    delay(100);
  } else {
    Acebott.Move(Forward, 180);
  }
}

// FOLLOW MODE: Follow object at safe distance
void model3_func() {
  cameraServo.write(90);
  UT_distance = Ultrasonic.Ranging();
  delay(50); // Prevent interference
  
  if (UT_distance < 15 && UT_distance > 0) {
    Acebott.Move(Backward, 150);
  } else if (UT_distance >= 15 && UT_distance <= 20) {
    Acebott.Move(Stop, 0);
  } else if (UT_distance > 20 && UT_distance <= 25) {
    Acebott.Move(Forward, 180);
  } else if (UT_distance > 25 && UT_distance <= 50) {
    Acebott.Move(Forward, 220);
  } else {
    Acebott.Move(Stop, 0);
  }
}

// TRACK_2 MODE: 3-sensor line following
void model4_func() {
  cameraServo.write(90);
  Left_Tra_Value = analogRead(Left_sensor);
  Middle_Tra_Value = analogRead(Middle_sensor);
  Right_Tra_Value = analogRead(Right_sensor);

  // All sensors off line - stop
  if (Left_Tra_Value >= Off_Road && Middle_Tra_Value >= Off_Road && Right_Tra_Value >= Off_Road) {
    Acebott.Move(Stop, 0);
  }
  // Left sensor on black, turn left
  else if (Left_Tra_Value >= Black_Line && Middle_Tra_Value < Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Contrarotate, 200);
  }
  // Right sensor on black, turn right
  else if (Left_Tra_Value < Black_Line && Middle_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line) {
    Acebott.Move(Clockwise, 200);
  }
  // Middle sensor on black, go forward
  else if (Middle_Tra_Value >= Black_Line) {
    Acebott.Move(Forward, 180);
  }
  // Default forward
  else {
    Acebott.Move(Forward, 150);
  }
}

// TRACK_1 MODE: 2-sensor line following
void model1_func() {
  Left_Tra_Value = analogRead(Left_sensor);
  Right_Tra_Value = analogRead(Right_sensor);

  // Both sensors off road - stop
  if (Left_Tra_Value >= Off_Road && Right_Tra_Value >= Off_Road) {
    Acebott.Move(Stop, 0);
  }
  // Both on line - go forward
  else if (Left_Tra_Value < Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Forward, 130);
  }
  // Left on black - turn left
  else if (Left_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Contrarotate, 150);
  }
  // Right on black - turn right
  else if (Left_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line) {
    Acebott.Move(Clockwise, 150);
  }
  // Both on black (between lines) - stop
  else if (Left_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line && 
           Left_Tra_Value < Off_Road && Right_Tra_Value < Off_Road) {
    Acebott.Move(Stop, 0);
  }
}

// ========== CAMERA SERVO ==========

void Camera_Servo_Move(int angles) {
  angles = constrain(angles, 1, 180);
  cameraServo.write(map(angles, 1, 180, 130, 70));
  delay(10);
}

// ========== MUSIC FUNCTIONS ==========

void Music_a() {
  for (int x = 0; x < length0; x++) {
    tone(Buzzer, tune0[x]);
    delay(500 * durt0[x]);
    noTone(Buzzer);
  }
}

void Music_b() {
  for (int x = 0; x < length1; x++) {
    tone(Buzzer, tune1[x]);
    delay(500 * durt1[x]);
    noTone(Buzzer);
  }
}

void Music_c() {
  for (int x = 0; x < length2; x++) {
    tone(Buzzer, tune2[x]);
    delay(500 * durt2[x]);
    noTone(Buzzer);
  }
}

void Music_d() {
  for (int x = 0; x < length3; x++) {
    tone(Buzzer, tune3[x]);
    delay(300 * durt3[x]);
    noTone(Buzzer);
  }
}

void Buzzer_run(int M) {
  switch (M) {
    case 0x01:
      Music_a();
      break;
    case 0x02:
      Music_b();
      break;
    case 0x03:
      Music_c();
      break;
    case 0x04:
      Music_d();
      break;
    default:
      break;
  }
}

// ========== MODULE CONTROL ==========

void runModule(int device) {
  val = readBuffer(12);
  switch (device) {
    case 0x0C: // Movement control
      {
        switch (val) {
          case 0x01: Acebott.Move(Forward, speeds); break;
          case 0x02: Acebott.Move(Backward, speeds); break;
          case 0x03: Acebott.Move(Move_Left, speeds); break;
          case 0x04: Acebott.Move(Move_Right, speeds); break;
          case 0x05: Acebott.Move(Top_Left, speeds); break;
          case 0x06: Acebott.Move(Bottom_Left, speeds); break;
          case 0x07: Acebott.Move(Top_Right, speeds); break;
          case 0x08: Acebott.Move(Bottom_Right, speeds); break;
          case 0x0A: Acebott.Move(Clockwise, speeds); break;
          case 0x09: Acebott.Move(Contrarotate, speeds); break;
          case 0x00: Acebott.Move(Stop, 0); break;
        }
      }
      break;
    case 0x02: // Camera servo
      Camera_Servo_Move(val);
      break;
    case 0x03: // Buzzer/Music
      Buzzer_run(val);
      break;
    case 0x05: // LEDs
      digitalWrite(LED_Module1, val);
      digitalWrite(LED_Module2, val);
      break;
    case 0x08: // Shoot pin
      digitalWrite(Shoot_PIN, HIGH);
      delay(200);
      digitalWrite(Shoot_PIN, LOW);
      break;
    case 0x0D: // Speed control
      speeds = val;
      Serial.printf("Speed set to: %d\n", speeds);
      break;
    case 0x0E: // ARM PUSH
      pushSensorSequence();
      break;
    case 0x0F: // ARM RETRACT
      retractArmSequence();
      break;
    case 0x10: // ARM DIRECT CONTROL
      moveArmSmooth(val);
      break;
  }
}

// ========== PACKET PARSING ==========

void parseData() {
  isStart = false;
  int action = readBuffer(9);
  int device = readBuffer(10);
  int index2 = readBuffer(2);

  if (index2 == 7) {
    newRunMode = action;
    Serial.printf("Mode changed to: %d\n", newRunMode);
  }

  switch (action) {
    case CMD_RUN:
      function_mode = STANDBY;
      runModule(device);
      break;
    case CMD_STANDBY:
      function_mode = STANDBY;
      Acebott.Move(Stop, 0);
      cameraServo.write(90);
      Serial.println("STANDBY mode");
      break;
    case CMD_ARM_PUSH:
      pushSensorSequence();
      break;
    case CMD_ARM_RETRACT:
      retractArmSequence();
      break;
    default:
      break;
  }
}

// ========== SERIAL PACKET HANDLER ==========

void RXpack_func() {
  while (Serial.available() > 0) {
    unsigned char c = Serial.read() & 0xff;
    
    // Check for test commands (single character)
    if (!isStart && index_a == 0) {
      if (c == 'p') {
        Serial.println("\n[TEST] Push arm");
        pushSensorSequence();
        return;
      }
      else if (c == 'r') {
        Serial.println("\n[TEST] Retract arm");
        retractArmSequence();
        return;
      }
    }
    
    // Packet protocol
    if (c == 0x55 && isStart == false) {
      if (prevc == 0xff) {
        index_a = 1;
        isStart = true;
      }
    } else {
      prevc = c;
      if (isStart) {
        if (index_a == 2) {
          dataLen = c;
        } else if (index_a > 2) {
          dataLen--;
        }
        writeBuffer(index_a, c);
      }
    }
    
    index_a++;
    if (index_a > 120) {
      index_a = 0;
      isStart = false;
    }
    
    if (isStart && dataLen == 0 && index_a > 3) {
      isStart = false;
      parseData();
      index_a = 0;
    }
  }
}