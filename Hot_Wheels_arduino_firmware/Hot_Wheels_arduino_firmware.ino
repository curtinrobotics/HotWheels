#include <Bluepad32.h>
#include <Preferences.h>

Preferences prefs;
const char *NVS_NAMESPACE = "hwrc";
const char *NVS_KEY_STEER_TRIM = "trim";

const int servoPin = 9;
const int Forward = 12;
const int Back = 11;

// PWM Configuration for motors
const int PWM_FREQ = 5000;    // 5 KHz PWM frequency for motors
const int PWM_RESOLUTION = 8; // 8-bit resolution (0-255)
const int FORWARD_PWM_CHANNEL = 0;
const int BACK_PWM_CHANNEL = 1;

// PWM Configuration for servo
const int SERVO_PWM_CHANNEL = 2;
const int SERVO_PWM_FREQ = 50;       // 50 Hz for standard servo
const int SERVO_PWM_RESOLUTION = 14; // 14-bit for servo control

ControllerPtr myController = nullptr;
int Drive = 0;
int Reverse = 0;
int angle = 90;

// --- Steering trim (LB/RB) ---
int STEERING_TRIM_DEG = 0;     // applied to steering angle (degrees)
const int TRIM_STEP_DEG = 1;   // change per button press
const int TRIM_LIMIT_DEG = 90; // max +/- trim from center
const bool TRIM_LATCH_START_STATE =
    false; // set both latches true/false at start

// Setup TRIM latch
bool lastTrimLB = TRIM_LATCH_START_STATE;
bool lastTrimRB = TRIM_LATCH_START_STATE;
bool lastDpadUp = TRIM_LATCH_START_STATE;
bool lastTrimFactoryResetCombo = TRIM_LATCH_START_STATE;

const int DEADZONE = 30;
const int SPEED_LIMIT_PERCENT = 100; // Limit speed to 40% of max

// Trigger threshold and mapping
const int TRIGGER_DEADZONE = 10;
const int TRIGGER_MAX = 1023;

// Servo pulse timing (in microseconds)
const int SERVO_MIN_US = 1000;
const int SERVO_MAX_US = 2000;

// Steering limit (degrees from center=90). Example: 30 -> range 60..120
const int STEER_LIMIT_DEG = 90;

static void loadSteeringTrim() {
  STEERING_TRIM_DEG = prefs.getInt(NVS_KEY_STEER_TRIM, 0);
  STEERING_TRIM_DEG =
      constrain(STEERING_TRIM_DEG, -TRIM_LIMIT_DEG, TRIM_LIMIT_DEG);
  Serial.printf("Loaded steering trim: %+d deg\n", STEERING_TRIM_DEG);
}

static void saveSteeringTrim() {
  STEERING_TRIM_DEG =
      constrain(STEERING_TRIM_DEG, -TRIM_LIMIT_DEG, TRIM_LIMIT_DEG);
  prefs.putInt(NVS_KEY_STEER_TRIM, STEERING_TRIM_DEG);
}

void onConnectedController(ControllerPtr ctl) {
  if (myController == nullptr) {
    Serial.print("CALLBACK: Controller is connected, index=");
    Serial.println(ctl->index());
    myController = ctl;

    // Initialize trim latch state on connect
    lastTrimLB = TRIM_LATCH_START_STATE;
    lastTrimRB = TRIM_LATCH_START_STATE;
    lastDpadUp = TRIM_LATCH_START_STATE;
    lastTrimFactoryResetCombo = TRIM_LATCH_START_STATE;

    ctl->setColorLED(0, 255, 0);
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (myController == ctl) {
    Serial.print("CALLBACK: Controller is disconnected from index=");
    Serial.println(ctl->index());

    angle = 90;

    // Initialize trim latch state on connect
    lastTrimLB = TRIM_LATCH_START_STATE;
    lastTrimRB = TRIM_LATCH_START_STATE;
    lastDpadUp = TRIM_LATCH_START_STATE;
    lastTrimFactoryResetCombo = TRIM_LATCH_START_STATE;

    // Stop motors when controller disconnects
    ledcWrite(FORWARD_PWM_CHANNEL, 0);
    ledcWrite(BACK_PWM_CHANNEL, 0);

    // Center servo
    setServoAngle(90);

    myController = nullptr;
  }
}

void processController() {
  if (myController && myController->isConnected() && myController->hasData()) {
    int32_t axisX = myController->axisX();
    int32_t throttle = myController->throttle();
    int32_t brake = myController->brake();

    // --- Steering trim via bumpers (edge-triggered) ---
    const bool lb = myController->l1(); // LB/L1
    const bool rb = myController->r1(); // RB/R1

    // D-pad up resets trim (edge-triggered)
    const bool dpadUp = (myController->dpad() & DPAD_UP) != 0;

    // Factory reset combo: LB + RB + D-pad Up clears saved trim
    // (edge-triggered)
    const bool trimFactoryResetCombo = lb && rb && dpadUp;

    bool trimChanged = false;

    if (trimFactoryResetCombo && !lastTrimFactoryResetCombo) {
      STEERING_TRIM_DEG = 0;
      prefs.remove(NVS_KEY_STEER_TRIM);
      Serial.println("Steering trim factory reset: cleared saved value");
    } else {

      if (lb && !lastTrimLB) {
        STEERING_TRIM_DEG += TRIM_STEP_DEG;
        STEERING_TRIM_DEG =
            constrain(STEERING_TRIM_DEG, -TRIM_LIMIT_DEG, TRIM_LIMIT_DEG);
        Serial.printf("Steering trim: %+d deg\n", STEERING_TRIM_DEG);
        trimChanged = true;
      }
      if (rb && !lastTrimRB) {
        STEERING_TRIM_DEG -= TRIM_STEP_DEG;
        STEERING_TRIM_DEG =
            constrain(STEERING_TRIM_DEG, -TRIM_LIMIT_DEG, TRIM_LIMIT_DEG);
        Serial.printf("Steering trim: %+d deg\n", STEERING_TRIM_DEG);
        trimChanged = true;
      }

      if (dpadUp && !lastDpadUp) {
        STEERING_TRIM_DEG = 0;
        Serial.println("Steering trim reset: 0 deg");
        trimChanged = true;
      }

      if (trimChanged) {
        saveSteeringTrim();
      }
    }

    lastTrimLB = lb;
    lastTrimRB = rb;
    lastDpadUp = dpadUp;
    lastTrimFactoryResetCombo = trimFactoryResetCombo;

    if (abs(axisX) < DEADZONE) {
      axisX = 0;
    }

    Drive = throttle;
    Reverse = brake;

    const int leftLimit = 90 + STEER_LIMIT_DEG;
    const int rightLimit = 90 - STEER_LIMIT_DEG;

    angle = map(axisX, -511, 512, leftLimit, rightLimit);

    // Apply trim, then clamp to steering limits
    angle += STEERING_TRIM_DEG;
    angle = constrain(angle, rightLimit, leftLimit);
  }
}

void setServoAngle(int angle) {
  // Map angle (0-180) to microseconds (1000-2000)
  int pulseWidth = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);

  // Calculate duty cycle for 14-bit resolution at 50Hz
  // At 50Hz, period = 20ms = 20000 microseconds
  // For 14-bit: max value = 16383 (2^14 - 1)
  // Duty cycle = (pulseWidth / 20000) * 16383
  uint32_t dutyCycle = ((uint32_t)pulseWidth * 16384) / 20000;

  ledcWrite(SERVO_PWM_CHANNEL, dutyCycle);
}

void updateMotorSpeed() {
  int forwardSpeed = 0;
  int reverseSpeed = 0;

  // Calculate max speed based on limit
  int maxSpeed = (255 * SPEED_LIMIT_PERCENT) / 100;

  // Map trigger values to PWM duty cycle with speed limit
  if (Drive >= TRIGGER_DEADZONE) {
    forwardSpeed = map(Drive, TRIGGER_DEADZONE, TRIGGER_MAX, 0, maxSpeed);
    forwardSpeed = constrain(forwardSpeed, 0, maxSpeed);
  }

  if (Reverse >= TRIGGER_DEADZONE) {
    reverseSpeed = map(Reverse, TRIGGER_DEADZONE, TRIGGER_MAX, 0, maxSpeed);
    reverseSpeed = constrain(reverseSpeed, 0, maxSpeed);
  }

  // Prevent both motors from running simultaneously (prioritize forward)
  if (forwardSpeed > 0 && reverseSpeed > 0) {
    reverseSpeed = 0;
  }

  // Update PWM outputs
  ledcWrite(FORWARD_PWM_CHANNEL, forwardSpeed);
  ledcWrite(BACK_PWM_CHANNEL, reverseSpeed);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting RC Car Controller...");

  prefs.begin(NVS_NAMESPACE, false);
  loadSteeringTrim();

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();

  // Setup PWM for motor control
  ledcSetup(FORWARD_PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(BACK_PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(Forward, FORWARD_PWM_CHANNEL);
  ledcAttachPin(Back, BACK_PWM_CHANNEL);

  // Setup PWM for servo control
  ledcSetup(SERVO_PWM_CHANNEL, SERVO_PWM_FREQ, SERVO_PWM_RESOLUTION);
  ledcAttachPin(servoPin, SERVO_PWM_CHANNEL);

  // Initialize motors to stopped
  ledcWrite(FORWARD_PWM_CHANNEL, 0);
  ledcWrite(BACK_PWM_CHANNEL, 0);

  // Center servo
  angle = 90;
  setServoAngle(angle);

  Serial.println("Setup complete. Waiting for Xbox controller...");
  Serial.println("Use left joystick X-axis for steering");
  Serial.println("Use triggers for variable speed control");
  Serial.printf("Speed limited to %d%% of maximum\n", SPEED_LIMIT_PERCENT);
}

void loop() {
  BP32.update();
  processController();

  // Update motor speeds based on trigger values
  updateMotorSpeed();

  // Update servo position using hardware PWM
  setServoAngle(angle);

  delay(1);
}