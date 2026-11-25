
#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>

// Components
RTC_DS3231 rtc;
Servo trackerServo;

// Servo pin
const int SERVO_PIN = 9;

// Starting and parking angles
const int START_ANGLE = 20;
const int PARK_ANGLE = 20;

// Schedule
struct ScheduleEntry {
  byte hour;
  byte minute;
  byte second;
  int angle;
};

const ScheduleEntry scheduleTable[] = {
  {7, 0, 0, 20},
  {7, 0, 5, 45},
  {7, 0, 10, 60},
  {7, 0, 15, 90},
  {7, 0, 20, 120},
  {7, 0, 25, 145},
  {7, 0, 30, 155}
};

const int scheduleCount =
  sizeof(scheduleTable) / sizeof(scheduleTable[0]);

// Timing
const unsigned long CLOCK_CHECK_INTERVAL_MS = 200;
const unsigned long DISPLAY_INTERVAL_MS = 1000;

unsigned long lastClockCheck = 0;
unsigned long lastDisplayTime = 0;

// Program state
bool automaticMode = true;
int currentAngle = -1;
int lastAutomaticTarget = -1;

String inputString = "";

// Print RTC time
void printClock() {
  DateTime now = rtc.now();

  Serial.print(F("Live time: "));
  Serial.println(now.timestamp(DateTime::TIMESTAMP_TIME));
}

// Find the scheduled servo angle
int getScheduledAngle(DateTime now) {
  int currentSeconds =
    now.hour() * 3600 +
    now.minute() * 60 +
    now.second();

  // Park at 6 PM or later
  if (now.hour() >= 18) {
    return PARK_ANGLE;
  }

  int targetAngle = START_ANGLE;

  for (int i = 0; i < scheduleCount; i++) {
    int scheduledSeconds =
      scheduleTable[i].hour * 3600 +
      scheduleTable[i].minute * 60 +
      scheduleTable[i].second;

    if (currentSeconds >= scheduledSeconds) {
      targetAngle = scheduleTable[i].angle;
    }
  }

  return targetAngle;
}

// Send servo command
void moveServoTo(int angle) {
  if (angle == currentAngle) {
    Serial.println(F("Servo is already at this angle."));
    return;
  }

  Serial.print(F("Moving servo to "));
  Serial.print(angle);
  Serial.println(F(" degrees"));

  // Non-blocking: lets the clock keep updating
  trackerServo.write(angle);
  currentAngle = angle;

  Serial.println(F("Servo position command sent."));
}

// Set RTC using a serial command
void setRtcFromCommand(String command) {
  int year, month, day, hour, minute, second;

  int valuesRead = sscanf(
    command.c_str(),
    "SET %d-%d-%d %d:%d:%d",
    &year, &month, &day,
    &hour, &minute, &second
  );

  if (valuesRead == 6 &&
      month >= 1 && month <= 12 &&
      day >= 1 && day <= 31 &&
      hour >= 0 && hour <= 23 &&
      minute >= 0 && minute <= 59 &&
      second >= 0 && second <= 59) {

    rtc.adjust(DateTime(
      year, month, day,
      hour, minute, second
    ));

    Serial.println(F("RTC time updated successfully."));

    // Make automatic mode check the schedule again
    lastAutomaticTarget = -1;
  } else {
    Serial.println(F("Invalid date/time format."));
    Serial.println(F("Use: SET 2026-11-25 07:00:00"));
  }
}

// Read commands from Serial Monitor
void readSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputString.length() > 0) {
        inputString.trim();

        if (inputString.startsWith("SET ")) {
          setRtcFromCommand(inputString);
        }

        else if (inputString == "TIME") {
          printClock();
        }

        else if (inputString == "AUTO") {
          automaticMode = true;
          lastAutomaticTarget = -1;

          Serial.println(F("Automatic mode ON."));
        }

        else if (inputString.startsWith("TEST ")) {
          int angle = inputString.substring(5).toInt();

          if (angle >= 0 && angle <= 180) {
            automaticMode = false;

            Serial.println(F("Manual test mode."));
            moveServoTo(angle);
          } else {
            Serial.println(F("Use an angle from 0 to 180."));
          }
        }

        else {
          Serial.println(F("Unknown command."));
          Serial.println(F("Commands: SET, TIME, TEST, AUTO"));
        }

        inputString = "";
      }
    } else {
      inputString += c;
    }
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  trackerServo.attach(SERVO_PIN);

  Serial.println(F("Solar Tracker Starting..."));

  if (!rtc.begin()) {
    Serial.println(F("ERROR: DS3231 not detected."));
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println(F("WARNING: RTC lost power."));
    Serial.println(F("Set the clock using the SET command."));
  }

  trackerServo.write(START_ANGLE);
  currentAngle = START_ANGLE;

  Serial.println(F("Solar Tracker Ready."));
  Serial.println(F("Type TIME to check the clock."));
  Serial.println(F("Type AUTO for automatic tracking."));
  Serial.println(F("Type TEST 90 to test the servo."));
}

void loop() {
  // Always read serial commands
  readSerialCommands();

  if (!automaticMode) {
    return;
  }

  // Check the clock regularly
  if (millis() - lastClockCheck < CLOCK_CHECK_INTERVAL_MS) {
    return;
  }

  lastClockCheck = millis();

  DateTime now = rtc.now();

  // Print live time every second
  if (millis() - lastDisplayTime >= DISPLAY_INTERVAL_MS) {
    lastDisplayTime = millis();
    printClock();
  }

  // Find the correct scheduled angle
  int targetAngle = getScheduledAngle(now);

  // Move only when the scheduled target changes
  if (targetAngle != lastAutomaticTarget) {
    Serial.print(F("TIME: "));
    Serial.print(now.timestamp(DateTime::TIMESTAMP_TIME));

    Serial.print(F(" | NEW SERVO ANGLE: "));
    Serial.print(targetAngle);
    Serial.println(F(" degrees"));

    moveServoTo(targetAngle);

    lastAutomaticTarget = targetAngle;
  }
}
