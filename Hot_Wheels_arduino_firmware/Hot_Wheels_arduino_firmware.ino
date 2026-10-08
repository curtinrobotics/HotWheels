#include <Bluepad32.h>
#include <Preferences.h>

Preferences prefs;
const char *NVS_NAMESPACE = "hwrc";
const char *NVS_KEY_STEER_TRIM = "trim";

// Optional: WiFi OTA flashing (Arduino IDE network upload)
#include <ArduinoOTA.h>
#include <WiFi.h>

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME ""
#endif

// If set, Arduino IDE will require this password for OTA uploads.
// Leave blank for no password.
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

// Optional: Stream logs over WiFi using a simple TCP server.
// View logs on your computer with: `nc <board-ip> <port>`
#ifndef LOG_TCP_PORT
#define LOG_TCP_PORT 2323
#endif

static WiFiServer logServer(LOG_TCP_PORT);
static WiFiClient logClient;

static volatile bool otaInProgress = false;
static volatile int lastOtaPercentPrinted = -1;

// Arduino IDE cannot open Serial Monitor on an OTA "Network port". Mirror the
// existing serial output to the TCP client without adding a third-party
// library.

static auto &USBSerial = ::Serial;

class SerialMirrorPrint : public Print {
public:
  SerialMirrorPrint(Print &usbSerial, WiFiClient &networkClient)
      : usbSerial(usbSerial), networkClient(networkClient) {}

  size_t write(uint8_t data) override {
    const size_t written = usbSerial.write(data);
    if (networkClient && networkClient.connected()) {
      networkClient.write(data);
    }
    return written;
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    const size_t written = usbSerial.write(buffer, size);
    if (networkClient && networkClient.connected()) {
      networkClient.write(buffer, size);
    }
    return written;
  }

  void flush() override {
    usbSerial.flush();
    if (networkClient && networkClient.connected()) {
      networkClient.flush();
    }
  }

private:
  Print &usbSerial;
  WiFiClient &networkClient;
};

static SerialMirrorPrint SerialMirror(USBSerial, logClient);

// Redefine Serial in THIS sketch so existing Serial.* calls are mirrored to
// WiFi logs.
#define Serial SerialMirror

static bool isLogClientConnected() {
  return logClient && logClient.connected();
}

void setServoAngle(int angle);

static void handleLogServer() {
  if (WiFi.status() != WL_CONNECTED || LOG_TCP_PORT <= 0) {
    return;
  }

  if (!logServer) {
    // Defensive: in case begin() was never called.
    logServer.begin();
    logServer.setNoDelay(true);
  }

  if (!isLogClientConnected()) {
    WiFiClient newClient = logServer.available();
    if (newClient) {
      logClient = newClient;
      logClient.setNoDelay(true);

      String ip = WiFi.localIP().toString();
      // Print via Serial so it also goes to USB.
      Serial.printf("Connected to HotWheelsRC logs at %s:%d\n", ip.c_str(),
                    LOG_TCP_PORT);
      Serial.printf("WiFi RSSI: %ld dBm\n", WiFi.RSSI());
      Serial.println("Tip: this sketch only prints when events happen (boot, "
                     "controller connect/disconnect, OTA, etc.)");
    }
  } else {
    // Drain any input so the socket stays healthy (we don't implement
    // commands).
    while (logClient.available()) {
      (void)logClient.read();
    }
  }
}

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

static void stopMotionForOTA() {
  Drive = 0;
  Reverse = 0;
  angle = 90;
  ledcWrite(FORWARD_PWM_CHANNEL, 0);
  ledcWrite(BACK_PWM_CHANNEL, 0);
  setServoAngle(angle);
}

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

static void setupWiFiAndOTA() {
  if (strlen(WIFI_SSID) == 0) {
    Serial.println(
        "WiFi OTA disabled (WIFI_SSID not set). See wifi_secrets.example.h");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("Connecting to WiFi SSID '%s'", WIFI_SSID);
  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connect failed; OTA will not be available.");
    return;
  }

  // Improve OTA speed/stability: disable WiFi power saving.
  // (Power save can introduce latency and reduce throughput.)
  WiFi.setSleep(false);

  // Increase TX power (can help OTA throughput on weak signals).
  // Safe no-op if the core doesn't support this enum on your version.
#if defined(WIFI_POWER_19_5dBm)
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
#endif

  // Start log server first so subsequent messages can be viewed over WiFi.
  if (LOG_TCP_PORT > 0) {
    logServer.begin();
    logServer.setNoDelay(true);
  }

  String ip = WiFi.localIP().toString();
  Serial.printf("WiFi connected. IP: %s\n", ip.c_str());
  if (LOG_TCP_PORT > 0) {
    Serial.printf("WiFi logs: nc %s %d\n", ip.c_str(), LOG_TCP_PORT);
  }

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA.onStart([]() {
    stopMotionForOTA();
    otaInProgress = true;
    lastOtaPercentPrinted = -1;
    Serial.println("OTA update start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update end");
    otaInProgress = false;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    const int pct =
        (total > 0) ? static_cast<int>((progress * 100U) / total) : 0;
    // Printing too frequently can slow OTA. Only print every 5%.
    if (pct == 100 || (pct % 5 == 0 && pct != lastOtaPercentPrinted)) {
      lastOtaPercentPrinted = pct;
      // Use USBSerial directly to avoid extra network traffic while uploading.
      USBSerial.printf("OTA progress: %d%%\r", pct);
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error[%u]\n", error);
    otaInProgress = false;
  });

  ArduinoOTA.begin();
  Serial.printf("OTA ready. Hostname: %s\n", OTA_HOSTNAME);
}

void onConnectedController(ControllerPtr ctl) {
  if (myController == nullptr) {
    Serial.printf("CALLBACK: Controller is connected, index=%d\n",
                  ctl->index());
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
    Serial.printf("CALLBACK: Controller is disconnected from index=%d\n",
                  ctl->index());

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
  USBSerial.begin(115200);
  Serial.println("Starting RC Car Controller...");

  prefs.begin(NVS_NAMESPACE, false);
  loadSteeringTrim();

  setupWiFiAndOTA();

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
  handleLogServer();
  ArduinoOTA.handle();

  // During OTA, let the network stack and updater run with minimal extra work.
  if (otaInProgress) {
    delay(0);
    return;
  }

  BP32.update();
  processController();

  // Update motor speeds based on trigger values
  updateMotorSpeed();

  // Update servo position using hardware PWM
  setServoAngle(angle);

  delay(1);
}
