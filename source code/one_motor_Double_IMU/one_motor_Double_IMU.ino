#include <Wire.h>
#include <MadgwickAHRS.h> 

// ================================================================================
// --- USER CONFIGURATION ---
// ================================================================================
bool invertAngle = true;       // Set to 'true' if flexion results in a negative angle
bool enableGaitPhase = false;   // 'true' = Vibrates on heel impact, 'false' = Vibrates constantly on hyperextension
float warningAngle = 0.0;       // Angle (in degrees) triggering warning feedback
float criticalAngle = -5.0;     // Angle (in degrees) triggering maximum feedback

// ================================================================================
// --- SYSTEM STATE & TIMING ---
// ================================================================================
bool isArmed = false;
unsigned long bootTime = 0;
const unsigned long autoCalibrateDelay = 5000; 

unsigned long previousLoopTime = 0;
const unsigned long loopInterval = 10;         

unsigned long previousSerialTime = 0;
const unsigned long serialInterval = 30;       

// ================================================================================
// --- HARDWARE DEFINITIONS ---
// ================================================================================
#define TCAADDR 0x70
const int MPU = 0x68; 
byte DRV = 0x5A;    
byte ModeReg = 0x01;
#define PWM6        OCR4D
#define PWM13       OCR4A

// ================================================================================
// --- KINEMATICS & SENSOR FUSION ---
// ================================================================================

Madgwick filterThigh;
Madgwick filterCalf;

float thighData[6];
float calfData[6];

float thighGyroOffset[3] = {0, 0, 0};
float calfGyroOffset[3] = {0, 0, 0};

// Calibration offset for the gravity inclination method
float angleOffset = 0.0;         
float kneeAngle = 0.0;       

// Gait Phase Variables
bool stancePhase = false;
unsigned long stanceTimer = 0;
float impactThreshold = 1.5;       
const int stanceDuration = 600;    

// ================================================================================
// --- I2C MULTIPLEXER ---
// ================================================================================
void tcaSelect(uint8_t i) {
  if (i > 7) return; 
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i); 
  Wire.endTransmission();
}

// ================================================================================
// --- INITIALIZATION ---
// ================================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); 

  pwm613configure();    
  delay(2);
  initializeDRV2605();  
  
  tcaSelect(3);
  setupMPU();
  calibrateGyro(3, thighGyroOffset);

  tcaSelect(0);
  setupMPU();
  calibrateGyro(0, calfGyroOffset);

  filterThigh.begin(100);
  filterCalf.begin(100);

  pulse(0.1, 10); 
  bootTime = millis();
}

// ================================================================================
// --- MAIN CONTROL LOOP ---
// ================================================================================
void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousLoopTime >= loopInterval) {
    previousLoopTime = currentMillis;

    // 1. Process Incoming Serial Commands
    if (Serial.available() > 0) {
      String incomingMsg = Serial.readStringUntil('\n'); 
      
      if (incomingMsg.startsWith("C")) {
        calibrateHingeZero();
      } else if (incomingMsg.startsWith("S:")) {
        incomingMsg.remove(0, 2); 
        
        int commaIndex1 = incomingMsg.indexOf(',');
        int commaIndex2 = incomingMsg.indexOf(',', commaIndex1 + 1);
        int commaIndex3 = incomingMsg.indexOf(',', commaIndex2 + 1);
        
        if (commaIndex1 > 0 && commaIndex2 > 0 && commaIndex3 > 0) {
          warningAngle = incomingMsg.substring(0, commaIndex1).toFloat();
          criticalAngle = incomingMsg.substring(commaIndex1 + 1, commaIndex2).toFloat();
          enableGaitPhase = (incomingMsg.substring(commaIndex2 + 1, commaIndex3).toInt() == 1);
          impactThreshold = incomingMsg.substring(commaIndex3 + 1).toFloat();
          pulse(0.2, 15); 
        }
      }
    }

    // 2. Standalone Auto-Calibration
    if (!isArmed && (currentMillis - bootTime > autoCalibrateDelay)) {
      calibrateHingeZero();
      pulse(0.5, 20); delay(100); pulse(0.5, 20); 
    }

    // 3. Sensor Data Acquisition
    readIMU(3, thighData, thighGyroOffset);
    readIMU(0, calfData, calfGyroOffset);

    // 4. Update 6DOF Quaternions
    filterThigh.updateIMU(thighData[3], thighData[4], thighData[5], thighData[0], thighData[1], thighData[2]);
    filterCalf.updateIMU(calfData[3], calfData[4], calfData[5], calfData[0], calfData[1], calfData[2]);

    // 5. Biomechanical processing: GRAVITY INCLINATION METHOD
    // Extracts the elevation angle of the longitudinal axis (assumed Y-axis) 
    // relative to the horizontal plane, isolating the measurement from yaw drift.
    
    float tw = filterThigh.q0; float tx = filterThigh.q1; float ty = filterThigh.q2; float tz = filterThigh.q3;
    float cw = filterCalf.q0; float cx = filterCalf.q1; float cy = filterCalf.q2; float cz = filterCalf.q3;

    // 5A. Calculate the vertical Z-component of the sensor's local X-axis.
    // Mathematical projection of the local X-axis onto the global Z-axis (gravity vector).
    // Formula derived from the rotation matrix component R31: 2 * (x*z - w*y)
    float thighVerticalComp = constrain(2.0f * (tx * tz - tw * ty), -1.0f, 1.0f);
    float calfVerticalComp = constrain(2.0f * (cx * cz - cw * cy), -1.0f, 1.0f);

    // 5B. Compute the elevation angle (in degrees) relative to the horizontal plane.
    float thighElevation = asin(thighVerticalComp) * 180.0f / PI;
    float calfElevation = asin(calfVerticalComp) * 180.0f / PI;

    // 5C. Determine the true hinge angle based on the relative difference in elevation.
    float trueAngle = calfElevation - thighElevation;

    // 5D. Apply the calibration offset to establish the zero-degree baseline.
    trueAngle -= angleOffset;

    // 5E. Handle physical mounting inversions if required by user configuration.
    if (invertAngle) {
        trueAngle = -trueAngle;
    }
    
    kneeAngle = trueAngle;

    // 6. Gait Phase Detection 
    float accMag2 = sqrt(pow(calfData[0], 2) + pow(calfData[1], 2) + pow(calfData[2], 2));
    if (accMag2 > impactThreshold) {
      stancePhase = true;
      stanceTimer = currentMillis;
    }
    if (currentMillis - stanceTimer > stanceDuration) {
      stancePhase = false;
    }

    // 7. Haptic Actuation Logic
    bool shouldVibrate = enableGaitPhase ? (isArmed && stancePhase) : isArmed;

    if (shouldVibrate) {
      // NOTE: User requested warnings starting at 0.0, going down to -5.0
      if (kneeAngle <= warningAngle && kneeAngle > criticalAngle) {
        float freq = map(kneeAngle * 10, warningAngle * 10, criticalAngle * 10, 40, 120); 
        float intens = map(kneeAngle * 10, warningAngle * 10, criticalAngle * 10, 0.2, 0.6);
        vibrate(freq, intens, 0.05, 50); 
      } else if (kneeAngle <= criticalAngle) {
        impactClick(); 
      }
    }

    // 8. Serial Telemetry Export
    if (currentMillis - previousSerialTime >= serialInterval) {
      previousSerialTime = currentMillis;

      Serial.print("T:"); Serial.print(filterThigh.getPitch()); Serial.print(",");
      Serial.print(filterThigh.getRoll()); Serial.print(",");
      Serial.print(filterThigh.getYaw()); Serial.print("|");

      Serial.print("C:"); Serial.print(filterCalf.getPitch()); Serial.print(",");
      Serial.print(filterCalf.getRoll()); Serial.print(",");
      Serial.print(filterCalf.getYaw()); Serial.print("|");

      Serial.print("A:"); Serial.println(kneeAngle);
    }
  }
}

// ================================================================================
// --- IMU HARDWARE ABSTRACTION ---
// ================================================================================
// ================================================================================
// --- IMU HARDWARE ABSTRACTION ---
// ================================================================================
void calibrateHingeZero() {
  // Retrieve current quaternion states
  float tw = filterThigh.q0; float tx = filterThigh.q1; float ty = filterThigh.q2; float tz = filterThigh.q3;
  float cw = filterCalf.q0; float cx = filterCalf.q1; float cy = filterCalf.q2; float cz = filterCalf.q3;

  // Calculate current vertical projections for the local X-axis
  float thighVerticalComp = constrain(2.0f * (tx * tz - tw * ty), -1.0f, 1.0f);
  float calfVerticalComp = constrain(2.0f * (cx * cz - cw * cy), -1.0f, 1.0f);
  
  // Compute current anatomical elevation angles
  float thighElevation = asin(thighVerticalComp) * 180.0f / PI;
  float calfElevation = asin(calfVerticalComp) * 180.0f / PI;

  // Store the natural resting difference as the zero-degree baseline offset
  angleOffset = calfElevation - thighElevation;
  
  isArmed = true;
}

void setupMPU() {
  Wire.beginTransmission(MPU); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true); 
  Wire.beginTransmission(MPU); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(true); 
  Wire.beginTransmission(MPU); Wire.write(0x1B); Wire.write(0x08); Wire.endTransmission(true); 
}

void calibrateGyro(int channel, float* gyroOffset) {
  tcaSelect(channel);
  int numSamples = 200;
  long gSum[3] = {0, 0, 0};
  
  for (int i = 0; i < numSamples; i++) {
    Wire.beginTransmission(MPU); Wire.write(0x43); Wire.endTransmission(false); Wire.requestFrom(MPU, 6, true);
    gSum[0] += (Wire.read() << 8 | Wire.read());
    gSum[1] += (Wire.read() << 8 | Wire.read());
    gSum[2] += (Wire.read() << 8 | Wire.read());
    delay(3);
  }
  
  gyroOffset[0] = (float)gSum[0] / numSamples / 65.5;
  gyroOffset[1] = (float)gSum[1] / numSamples / 65.5;
  gyroOffset[2] = (float)gSum[2] / numSamples / 65.5;
}

void readIMU(int channel, float* data, float* gyroOffset) {
  tcaSelect(channel); 
  
  Wire.beginTransmission(MPU); Wire.write(0x3B); Wire.endTransmission(false); Wire.requestFrom(MPU, 6, true);
  data[0] = (Wire.read() << 8 | Wire.read()) / 16384.0; 
  data[1] = (Wire.read() << 8 | Wire.read()) / 16384.0; 
  data[2] = (Wire.read() << 8 | Wire.read()) / 16384.0; 
  
  Wire.beginTransmission(MPU); Wire.write(0x43); Wire.endTransmission(false); Wire.requestFrom(MPU, 6, true);
  data[3] = ((Wire.read() << 8 | Wire.read()) / 65.5) - gyroOffset[0];
  data[4] = ((Wire.read() << 8 | Wire.read()) / 65.5) - gyroOffset[1];
  data[5] = ((Wire.read() << 8 | Wire.read()) / 65.5) - gyroOffset[2];
}

// ================================================================================
// =================== TACHAMMER BASIC FUNCTION LIBRARY ===================[TACLIB]
// ================================================================================
void standbyOnB() { Wire.beginTransmission(DRV); Wire.write(ModeReg); Wire.write(0x43); Wire.endTransmission(); }
void standbyOffB() { Wire.beginTransmission(DRV); Wire.write(ModeReg); Wire.write(0x03); Wire.endTransmission(); }

void initializeDRV2605() {
  Wire.beginTransmission(DRV); Wire.write(ModeReg); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(DRV); Wire.write(0x1D); Wire.write(0xA8); Wire.endTransmission();
  Wire.beginTransmission(DRV); Wire.write(0x03); Wire.write(0x02); Wire.endTransmission();
  Wire.beginTransmission(DRV); Wire.write(0x17); Wire.write(0xff); Wire.endTransmission();
  Wire.beginTransmission(DRV); Wire.write(ModeReg); Wire.write(0x03); Wire.endTransmission();
}

#define PWM12k  5
void pwm613configure() {
  TCCR4A = 0; TCCR4B = PWM12k; TCCR4C = 0; TCCR4D = 0;
  PLLFRQ = (PLLFRQ & 0xCF) | 0x30; OCR4C = 255; pwmSet13();
}
void pwmSet13() { OCR4A = 0; DDRC |= _BV(7); TCCR4A = 0x82; }

void usdelay(double time) { double us = time - ((int)time); for (int i = 0; i <= time; i++) { delay(1); } delayMicroseconds(us * 1000); }
void pause(double milliseconds) { double us = milliseconds - ((int)milliseconds); standbyOnB(); for (int i = 0; i <= milliseconds; i++) { delay(1); } delayMicroseconds(us * 1000); }

void pulse(double intensity, double milliseconds) {
  int minimumint = 140; int maximumint = 255; int pwmintensity = (intensity * (maximumint - minimumint)) + minimumint;
  standbyOffB(); PWM13 = pwmintensity; usdelay(milliseconds); standbyOnB();
}

void singlePulse(double intensity, double milliseconds) { pulse(intensity, milliseconds); pause(3); pulse(intensity * 3 / 100, milliseconds * 2); }

void hit(double intensity, double milliseconds) {
  int minimumint = 0; int maximumint = 110; int pwmintensity = maximumint - (intensity * (maximumint - minimumint));
  standbyOffB(); PWM13 = pwmintensity; usdelay(milliseconds); standbyOnB();
}

void vibrate(double frequency, double intensity, double duration, int dutycycle) {
  int hitduration = 10 * dutycycle / frequency; boolean hold = false;
  double delayy = (1 / frequency * 1000) - hitduration; double timedown = duration * 1000;
  if (duration == 0) { hold = true; }
  while (hold) { pulse(intensity, hitduration); pause(delayy); }
  while (timedown >= 0 && frequency < 60) { pulse(intensity, hitduration); pause(3); pulse(0.002, delayy-3); timedown -= (delayy + hitduration); }
  while (timedown >= 0 && frequency >= 60) { pulse(intensity, hitduration); pause(delayy); timedown -= (delayy + hitduration); }
}

// ================================================================================
// =============================== HAPTIC LIBRARY =========================[HAPLIB]
// ================================================================================
void impactClick() { pulse(1, 6); hit(1, 21); }
void ermRumble() { vibrate(30,0.7,0.33,30); }
void lraClick() { vibrate(250,1,0.01,70); }
void heartbeat() {
  int hrate = 0;
  for (int i=0;i<16;i++){
    pulse(0.3, 45); pause(12); pulse(0.3, 3); pause(250 - hrate);
    pulse(0.1, 45); pause(12); pulse(0.3, 3); pause(510 - hrate);
    if (hrate < 200) { hrate += 15; }
  }   
}
void rifle(bool audible) {
  if (audible) { pause(20); pulse(1, 6); hit(1, 21); }
  else { hit(1,3); pulse(1,21); pause(2); pulse(0.002,21); }
}
void shotgun(bool audible, bool reload) {
  if (!reload){
    if (audible) { pulse(1, 6); hit(1, 21); pause(250); }
    else { hit(1, 3); singlePulse(1, 21); pause(200); }
  } else {
    if (audible) { pulse(.5, 30); hit(.35, 10); pause(180); hit(.37, 17); }
    else { pulse(.3, 30); pulse(.55, 30); pause(2.8); pulse(0.03, 60); pause(115); pulse(.55, 30); pause(3); pulse(0.03, 60); }
  }
}