#include <Wire.h>

// ================================================================================
// --- HAPTIC & MULTIPLEXER DEFINITIONS ---
// ================================================================================
#define TCAADDR 0x70
#define PWM6        OCR4D
#define PWM13       OCR4A
byte DRV = 0x5A;    
byte ModeReg = 0x01;

// ================================================================================
// --- IMU & SYSTEM VARIABLES ---
// ================================================================================
const int MPU = 0x68; 

// IMU 1 (Thigh - I2C Channel 2)
float roll1 = 0.0, AccErrorX1 = 0.0, GyroErrorX1 = 0.0;

// IMU 2 (Calf - I2C Channel 3)
float roll2 = 0.0, AccErrorX2 = 0.0, GyroErrorX2 = 0.0;
float AccX2, AccY2, AccZ2; // Required for dynamic impact detection

float GyroX, accAngleX;
float elapsedTime, currentTime, previousTime;

// Gait Phase Detection Variables
bool stancePhase = false;
unsigned long stanceTimer = 0;
const float impactThreshold = 1.5; // Acceleration magnitude threshold for heel strike (g)
const int stanceDuration = 600;    // Estimated stance phase duration (ms)
bool isArmed = false;

// ================================================================================
// --- I2C MULTIPLEXER ROUTING ---
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

  // 1. Initialize Haptic Driver (Channel 0)
  tcaSelect(0);
  pwm613configure();    
  delay(2);
  initializeDRV2605();  
  
  // 2. Initialize & Calibrate Upper IMU (Channel 2)
  tcaSelect(2);
  setupMPU();
  calibrateIMU(AccErrorX1, GyroErrorX1);

  // 3. Initialize & Calibrate Lower IMU (Channel 3)
  tcaSelect(3);
  setupMPU();
  calibrateIMU(AccErrorX2, GyroErrorX2);

  // System verification pulse
  tcaSelect(0);
  pulse(0.1, 10); 
  Serial.println("System Initialized: Dual-IMU Genu Recurvatum Monitor Active");
}

// ================================================================================
// --- MAIN CONTROL LOOP ---
// ================================================================================
void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'C') {
      isArmed = true; 
    }
  }
  // Time delta calculation for continuous integration
  previousTime = currentTime;
  currentTime = millis();
  elapsedTime = (currentTime - previousTime) / 1000.0;

  // 1. Data Acquisition
  updateIMU(2, roll1, AccErrorX1, GyroErrorX1);
  updateIMU(3, roll2, AccErrorX2, GyroErrorX2);

  // 2. Biomechanical Processing
  float kneeAngle = roll1 - roll2; 

  // 3. Gait Phase Detection (Heel Strike)
  float accMag2 = sqrt(pow(AccX2, 2) + pow(AccY2, 2) + pow(AccZ2, 2));
  
  if (accMag2 > impactThreshold) {
    stancePhase = true;
    stanceTimer = millis();
  }
  
  if (millis() - stanceTimer > stanceDuration) {
    stancePhase = false;
  }

  // 4. Proportional Haptic Feedback (Hyperextension Prevention)
  if (isArmed && stancePhase) {
    if (kneeAngle <= 5.0 && kneeAngle > 0.0) {
      // Pre-warning zone: Increasing frequency as joint approaches 0 degrees
      float freq = map(kneeAngle * 10, 50, 0, 40, 120); 
      float intens = map(kneeAngle * 10, 50, 0, 0.2, 0.6);
      
      tcaSelect(0);
      vibrate(freq, intens, 0.05, 50); 
      
    } else if (kneeAngle <= 0.0) {
      // Critical zone: Hyperextension detected, trigger mechanical impact cue
      tcaSelect(0);
      impactClick();
    }
  }

  // 5. Serial Output for Digital Twin Visualization (Unity)
  Serial.print("D:"); Serial.print(roll1); Serial.print(",");
  Serial.print("K:"); Serial.print(roll2); Serial.print(",");
  Serial.print("A:"); Serial.println(kneeAngle);

  delay(5); // Loop stabilization
}

// ================================================================================
// --- SENSOR FUSION & IMU ABSTRACTION ---
// ================================================================================
void setupMPU() {
  Wire.beginTransmission(MPU); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true);
  Wire.beginTransmission(MPU); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(true); // Set accelerometer to ±2g
  Wire.beginTransmission(MPU); Wire.write(0x1B); Wire.write(0x00); Wire.endTransmission(true); // Set gyroscope to ±250 deg/s
}

void calibrateIMU(float &errAcc, float &errGyro) {
  int c = 0; float AccX, AccY, AccZ, GyroX_raw;
  
  // Accelerometer calibration
  while (c < 200) {
    Wire.beginTransmission(MPU); Wire.write(0x3B); Wire.endTransmission(false); Wire.requestFrom(MPU, 6, true);
    AccX = (Wire.read() << 8 | Wire.read()) / 16384.0; AccY = (Wire.read() << 8 | Wire.read()) / 16384.0; AccZ = (Wire.read() << 8 | Wire.read()) / 16384.0;
    errAcc += ((atan(AccY / sqrt(pow(AccX, 2) + pow(AccZ, 2))) * 180 / PI));
    c++; delay(3);
  }
  errAcc /= 200; 
  c = 0;
  
  // Gyroscope calibration
  while (c < 200) {
    Wire.beginTransmission(MPU); Wire.write(0x43); Wire.endTransmission(false); Wire.requestFrom(MPU, 2, true); 
    GyroX_raw = (Wire.read() << 8 | Wire.read());
    errGyro += (GyroX_raw / 131.0);
    c++; delay(3);
  }
  errGyro /= 200;
}

void updateIMU(int channel, float &currentRoll, float errAcc, float errGyro) {
  tcaSelect(channel); 
  float AccX, AccY, AccZ;
  
  // Read accelerometer data
  Wire.beginTransmission(MPU); Wire.write(0x3B); Wire.endTransmission(false); Wire.requestFrom(MPU, 6, true);
  AccX = (Wire.read() << 8 | Wire.read()) / 16384.0; AccY = (Wire.read() << 8 | Wire.read()) / 16384.0; AccZ = (Wire.read() << 8 | Wire.read()) / 16384.0;
  
  // Store lower limb acceleration for impact detection
  if (channel == 3) { AccX2 = AccX; AccY2 = AccY; AccZ2 = AccZ; }
  
  accAngleX = (atan(AccY / sqrt(pow(AccX, 2) + pow(AccZ, 2))) * 180 / PI) - errAcc;
  
  // Read gyroscope data
  Wire.beginTransmission(MPU); Wire.write(0x43); Wire.endTransmission(false); Wire.requestFrom(MPU, 2, true);
  GyroX = (Wire.read() << 8 | Wire.read()) / 131.0 - errGyro;
  
  // Closed-loop complementary filter
  currentRoll = 0.96 * (currentRoll + GyroX * elapsedTime) + 0.04 * accAngleX;
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
  delay(100);
}

#define PWM12k  5
void pwm613configure() {
  TCCR4A = 0; TCCR4B = PWM12k; TCCR4C = 0; TCCR4D = 0;
  PLLFRQ = (PLLFRQ & 0xCF) | 0x30; OCR4C = 255; pwmSet13();
}
void pwmSet6() { OCR4D = 0; DDRD |= _BV(7); TCCR4C |= 0x09; }
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