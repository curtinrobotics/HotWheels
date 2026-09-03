/*
 * ESP32-S3 RC Car Steering Control
 * Uses Bluepad32 library with Xbox controller
 * Uses manual PWM control for servo (compatible with Bluepad32 board
 * definition)
 */

#include <Bluepad32.h>

// Servo setup
const int SERVO_PIN = 9; // Change to your actual servo pin

// Controller variables
ControllerPtr myController = nullptr;

// Servo control variables
int currentAngle = 90;   // Start at center position
int targetAngle = 90;    // Target angle from joystick
const int DEADZONE = 50; // Joystick deadzone to prevent jitter
bool INVERT_STEERING = false;
// Steering limit (degrees from center=90). Example: 30 -> range 60..120
const int STEER_LIMIT_DEG = 35;

// ===== MOTOR (DRV8833) =====
#define MOTOR_IN1 12
#define MOTOR_IN2 11
#define MOTOR_PWM_CH 1
#define MOTOR_PWM_FREQ 20000
#define MOTOR_PWM_RES 8
#define TRIGGER_NEUTRAL 0
#define TRIGGER_DEADBAND 0

// PWM timing variables
unsigned long lastPWMTime = 0;
const int PWM_PERIOD = 20; // 20ms period for 50Hz

void onConnectedController(ControllerPtr ctl) {
  if (myController == nullptr) {
    Serial.print("CALLBACK: Controller is connected, index=");
    Serial.println(ctl->index());
    myController = ctl;

    // Optional: Set controller LED color to indicate connection
    ctl->setColorLED(0, 255, 0); // Green LED
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (myController == ctl) {
    Serial.print("CALLBACK: Controller is disconnected from index=");
    Serial.println(ctl->index());

    // Return servo to center when controller disconnects
    currentAngle = 90;
    targetAngle = 90;

    myController = nullptr;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting RC Car Controller...");

  // Initialize Bluepad32
  BP32.setup(&onConnectedController, &onDisconnectedController);
  // BP32.forgetBluetoothKeys(); // Remove this line if you don't want to forget
  // paired devices

  // Initialize servo pin
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);

  // Set initial position
  currentAngle = 90;
  targetAngle = 90;

  // Motor pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  // PWM setup for speed control
  ledcSetup(MOTOR_PWM_CH, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttachPin(MOTOR_IN1, MOTOR_PWM_CH);

  // stop motor initially
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_PWM_CH, 0);

  Serial.println("Setup complete. Waiting for Controller...");
  Serial.println("Tip: Use left joystick X-axis for steering");
}

float angleToERS(int angle) {
  // Convert angle (0-180) to pulse width in milliseconds
  // Your servo: 0.5ms = 0°, 1.45ms = 90°, 2.4ms = 180°
  return map(angle, 0, 180, 500, 2400) / 1000.0;
}

void sendServoPulse(int angle) {
  float pulseWidthMs = angleToERS(angle);

  digitalWrite(SERVO_PIN, HIGH);
  delayMicroseconds(pulseWidthMs * 1000); // Convert ms to microseconds
  digitalWrite(SERVO_PIN, LOW);
}

void processController() {
  if (myController && myController->isConnected() && myController->hasData()) {

    // Get left joystick X-axis (-511 to +512)
    int32_t axisX = myController->axisX();

    // Apply deadzone to prevent servo jitter when joystick is centered
    if (abs(axisX) < DEADZONE) {
      axisX = 0;
    }

    // Map joystick input to servo angle with steering limits
    const int leftLimit = 90 + STEER_LIMIT_DEG;
    const int rightLimit = 90 - STEER_LIMIT_DEG;

    if (axisX == 0) {
      targetAngle = 90;
    } else {
      if (INVERT_STEERING)
        targetAngle = map(axisX, -511, 511, rightLimit, leftLimit);
      else
        targetAngle = map(axisX, -511, 511, leftLimit, rightLimit);
    }

    targetAngle = constrain(targetAngle, rightLimit, leftLimit);

    // Optional: Limit servo range for your specific setup
    // targetAngle = map(axisX, -511, 512, 60, 120); // Limited range

    // Smooth servo movement to prevent jerky steering
    if (abs(targetAngle - currentAngle) >
        2) { // Only update if significant change
      currentAngle = (currentAngle * 3 + targetAngle) / 4; // Simple smoothing

      // Debug output
      Serial.print("Joystick X: ");
      Serial.print(axisX);
      Serial.print(" -> Servo Angle: ");
      Serial.println(currentAngle);
    }
  }
}

void processThrottle() {
  if (!(myController && myController->isConnected()))
    return;

  int rt = myController->throttle(); // 0–1023
  int lt = myController->brake();    // 0–1023

  int throttle = rt - lt; // ← THIS IS THE KEY LINE

  // --- NEUTRAL ---
  if (abs(throttle) < TRIGGER_DEADBAND) {
    ledcWrite(MOTOR_PWM_CH, 0);
    digitalWrite(MOTOR_IN2, LOW);
    return;
  }

  // --- FORWARD ---
  if (throttle > 0) {
    int speed = map(throttle, TRIGGER_DEADBAND, 1023, 0, 255);
    speed = constrain(speed, 0, 255);

    digitalWrite(MOTOR_IN2, LOW);
    ledcWrite(MOTOR_PWM_CH, speed);
  }
  // --- REVERSE ---
  else {
    int speed = map(-throttle, TRIGGER_DEADBAND, 1023, 255, 0);
    speed = constrain(speed, 0, 255);

    digitalWrite(MOTOR_IN2, HIGH);
    ledcWrite(MOTOR_PWM_CH, speed);
  }

  Serial.print("RT: ");
  Serial.print(rt);
  Serial.print(" LT: ");
  Serial.print(lt);
  Serial.print(" Throttle: ");
  Serial.println(throttle);
}

void loop() {
  // This call fetches all the controller's data
  BP32.update();

  // Process controller input if connected
  processController();
  processThrottle();

  // Send PWM pulse every 20ms (50Hz)
  unsigned long currentTime = millis();
  if (currentTime - lastPWMTime >= PWM_PERIOD) {
    sendServoPulse(currentAngle);
    lastPWMTime = currentTime;
  }

  // Small delay to prevent overwhelming the system
  delay(1);
}