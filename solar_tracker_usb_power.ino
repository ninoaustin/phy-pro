/*
  TIME-BASED SOLAR TRACKER - Arduino Uno + DS3231 + one SG90 servo
  Libraries: RTClib by Adafruit. Servo and Wire are built into Arduino IDE.

  DS3231: VCC->5V, GND->GND, SDA->A4, SCL->A5
  Servo: signal orange/yellow->D9, red->regulated 5V servo supply,
         brown/black->GND. Servo supply GND must join Arduino GND.

  POWER: Laptop USB can power the Uno and RTC. An SG90 may draw more current
  than the Uno/USB 5V rail can reliably supply, especially under load.
  For reliable operation, use a separate regulated 5V servo supply with
  sufficient current and common GND. Do not connect two 5V sources together.
  Test without the panel attached.

  RTC SETUP: If the DS3231 has lost power, this sketch sets it to the date/time
  when the sketch was compiled. After the first upload, comment out the
  rtc.adjust(...) line below and upload again so the RTC keeps its time.
*/

#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>

const byte SERVO_PIN = 9;
const int SERVO_MIN_ANGLE = 10;
const int SERVO_MAX_ANGLE = 170;
const int PARKING_ANGLE = 90;
const byte PARK_HOUR = 18;
const byte PARK_MINUTE = 0;

struct ScheduleEntry {
  byte hour;
  byte minute;
  int angle;
};

// 7 AM and 8 AM are your requested angles.
// Later values are examples; calibrate them for your panel and mount.
const ScheduleEntry scheduleTable[] = {
  { 7,  0,  20 },
  { 8,  0,  34 },
  { 9,  0,  45 },
  {10,  0,  60 },
  {11,  0,  75 },
  {12,  0,  90 },
  {13,  0, 105 },
  {14,  0, 120 },
  {15,  0, 135 },
  {16,  0, 145 },
  {17,  0, 155 }
};

const byte SCHEDULE_COUNT = sizeof(scheduleTable) / sizeof(scheduleTable[0]);
const unsigned long UPDATE_INTERVAL_MS = 1000;
const unsigned int STEP_DELAY_MS = 20;

RTC_DS3231 rtc;
Servo trackerServo;

int currentAngle = PARKING_ANGLE;
int lastTargetAngle = -1;
unsigned long lastUpdate = 0;

int limitAngle(int angle) {
  return constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
}

int findTargetAngle(const DateTime &now) {
  int nowMinutes = now.hour() * 60 + now.minute();
  int parkMinutes = PARK_HOUR * 60 + PARK_MINUTE;

  if (nowMinutes >= parkMinutes) {
    return limitAngle(PARKING_ANGLE);
  }

  int target = PARKING_ANGLE;
  int latestEntryMinutes = -1;

  for (byte i = 0; i < SCHEDULE_COUNT; i++) {
    int entryMinutes = scheduleTable[i].hour * 60 + scheduleTable[i].minute;
    if (entryMinutes <= nowMinutes && entryMinutes > latestEntryMinutes) {
      latestEntryMinutes = entryMinutes;
      target = scheduleTable[i].angle;
    }
  }
  return limitAngle(target);
}

void moveServoTo(int requestedAngle) {
  int target = limitAngle(requestedAngle);
  if (target == lastTargetAngle) return;

  Serial.print(F("Moving servo: "));
  Serial.print(currentAngle);
  Serial.print(F(" -> "));
  Serial.print(target);
  Serial.println(F(" degrees"));

  int direction = (target > currentAngle) ? 1 : -1;
  while (currentAngle != target) {
    currentAngle += direction;
    trackerServo.write(currentAngle);
    delay(STEP_DELAY_MS);
  }
  lastTargetAngle = target;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  trackerServo.attach(SERVO_PIN);
  trackerServo.write(PARKING_ANGLE);
  delay(500);

  if (!rtc.begin()) {
    Serial.println(F("ERROR: DS3231 not found. Check VCC, GND, SDA=A4, SCL=A5."));
    while (true) delay(1000);
  }

  if (rtc.lostPower()) {
    Serial.println(F("RTC lost power; setting it to sketch compile time."));
    // Upload once to set RTC, then comment out this line and upload again.
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
  Serial.println(F("Time-based solar tracker started."));
  Serial.print(F("RTC date/time: "));
  Serial.print(now.timestamp(DateTime::TIMESTAMP_DATE));
  Serial.print(' ');
  Serial.println(now.timestamp(DateTime::TIMESTAMP_TIME));
}

void loop() {
  if (millis() - lastUpdate < UPDATE_INTERVAL_MS) return;
  lastUpdate = millis();

  DateTime now = rtc.now();
  int targetAngle = findTargetAngle(now);

  Serial.print(now.timestamp(DateTime::TIMESTAMP_TIME));
  Serial.print(F(" | Target: "));
  Serial.print(targetAngle);
  Serial.println(F(" degrees"));

  moveServoTo(targetAngle);
}
