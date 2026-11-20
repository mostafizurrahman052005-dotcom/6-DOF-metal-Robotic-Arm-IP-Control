#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define I2C_SDA 8
#define I2C_SCL 9
#define RGB_PIN 48

const char* ssid = "Robotic_Arm_6DOF";
const char* password = "12345678";

WebServer server(80);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

#define SERVOMIN  110
#define SERVOMAX  510
#define SERVO_FREQ 50

#define MAX_FRAMES 300         
#define REC_SAMPLE_RATE 120    
#define PLAY_FRAME_DELAY 220   
#define SMOOTH_INTERVAL 15     
#define SMOOTH_STEP 0.7f       

float currentAngles[6] = {90, 90, 90, 90, 90, 90};
int targetAngles[6]    = {90, 90, 90, 90, 90, 90};

uint8_t recordedFrames[MAX_FRAMES][6];
int totalFrames = 0;
bool isRecording = false;
bool isPlaying = false;
int playIndex = 0;

unsigned long lastLedTime = 0;
unsigned long lastRecordTime = 0;
unsigned long lastSmoothTime = 0;
unsigned long lastFrameSwitchTime = 0;
bool redState = false;
int hue = 0;

void setRainbowLED(int h) {
  int r, g, b;
  int region = h / 43;
  int remainder = (h - (region * 43)) * 6;
  int q = (255 * (255 - remainder)) >> 8;
  int t = (255 * remainder) >> 8;

  switch (region) {
    case 0:  r = 255; g = t;   b = 0;   break;
    case 1:  r = q;   g = 255; b = 0;   break;
    case 2:  r = 0;   g = 255; b = t;   break;
    case 3:  r = 0;   g = q;   b = 255; break;
    case 4:  r = t;   g = 0;   b = 255; break;
    default: r = 255; g = 0;   b = q;   break;
  }
  neopixelWrite(RGB_PIN, r / 3, g / 3, b / 3);
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>6-DOF Smooth Robotic Arm</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #121212;
      color: #ffffff;
      text-align: center;
      margin: 0;
      padding: 15px;
    }
    h2 { color: #00e676; margin-bottom: 15px; }
    .container {
      max-width: 500px;
      margin: auto;
      background: #1e1e1e;
      padding: 18px;
      border-radius: 12px;
      box-shadow: 0 4px 15px rgba(0,0,0,0.5);
    }
    .servo-control {
      margin-bottom: 18px;
      text-align: left;
    }
    .label-row {
      display: flex;
      justify-content: space-between;
      margin-bottom: 6px;
      font-size: 15px;
      font-weight: bold;
    }
    .val-badge {
      background: #00e676;
      color: #000;
      padding: 2px 8px;
      border-radius: 6px;
      font-size: 13px;
      min-width: 36px;
      text-align: center;
    }
    input[type=range] {
      width: 100%;
      -webkit-appearance: none;
      background: #333;
      height: 10px;
      border-radius: 5px;
      outline: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 24px;
      height: 24px;
      border-radius: 50%;
      background: #00e676;
      cursor: pointer;
    }
    .btn-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-top: 18px;
    }
    button {
      border: none;
      padding: 14px;
      font-size: 16px;
      border-radius: 8px;
      cursor: pointer;
      font-weight: bold;
      color: white;
      transition: 0.2s;
    }
    .btn-rec { background-color: #ff9800; }
    .btn-rec.active { background-color: #e65100; box-shadow: