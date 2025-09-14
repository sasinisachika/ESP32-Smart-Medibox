# 💊 ESP32 Smart Medibox

## 📖 Overview
The Medibox project is a smart medicine management system using an ESP32. It allows users to set alarms, monitor temperature, humidity, and light intensity, and automatically adjusts a servo-controlled shaded window to maintain optimal storage conditions. A Node-RED dashboard provides real-time data visualization and configurable system settings for enhanced usability.

---

## 🌟 Key Features

### Time Management & Alarms
- Fetches current time from an NTP server via Wi-Fi (Time Synchronization).  
- Allows time zone configuration.  
- OLED display shows the current time.  
- Set, manage, and delete up to 2 alarms (Alarm Management).  
- View, delete, stop, or snooze alarms (5 minutes).  
- Alarm notifications via buzzer, LED, or OLED messages.  
- Alarm dismissal via push buttons.  
- Persistent storage of alarms and user settings in non-volatile memory.  

### Environmental Monitoring
- Monitors temperature and humidity using DHT11.  
- Issues warnings when values exceed healthy limits:  
  - Temperature: 24°C–32°C  
  - Humidity: 65%–80%  
- Continuous monitoring of environmental conditions.  
- Change detection to minimize power consumption.  

### Light Intensity Monitoring & Control
- Measures light using an LDR sensor at configurable intervals.  
- Averages readings over a period and sends data to Node-RED dashboard.  
- Displays numerical values and historical charts on Node-RED.  
- Servo motor adjusts shaded sliding window based on light intensity.  
- Motor angle calculated considering configurable parameters: minimum angle (θoffset), controlling factor (γ), ideal storage temperature (Tmed).  

### User Interface
- Menu-driven OLED display for easy navigation and configuration.  
- Allows alarm setup, time zone adjustments, and system status checks.  
- Displays warnings when environmental thresholds are exceeded.  

### Communication Management
- Connects securely to an MQTT broker for remote data transmission and control.  
- Publishes sensor data to MQTT topics for Node-RED dashboard visualization.  
- Subscribes to topics for remote control of the servo motor.  

---

## 🛠 Software & Tools
- Arduino IDE / PlatformIO – ESP32 programming  
- Wokwi Simulator – Microcontroller simulation - https://wokwi.com/projects/422617064133968897
- Node-RED – Dashboard for visualization and control - http://127.0.0.1:1880/#flow/7f4a8f1e.6d1b5  

*Libraries:*  
- WiFi.h – Wi-Fi connectivity  
- NTPClient.h – Fetching NTP time  
- Adafruit_SSD1306.h – OLED display control  
- DHT.h – Temperature & humidity sensing  

---

## 🧰 Hardware Components
- ESP32 Development Board  
- OLED Display  
- Buzzer  
- Push Buttons  
- DHT11 Sensor  
- LDR Sensors  
- Servo Motor  

---

## 🔧 How It Works

### Time & Alarm Management
- ESP32 connects to Wi-Fi and fetches current time from an NTP server.  
- User sets timezone and alarms via OLED menu.  
- Alarm notifications trigger buzzer, LED, and OLED messages.  
- Alarms can be stopped or snoozed via push buttons.  

### Environment Monitoring
- Temperature and humidity are continuously monitored.  
- Alerts are triggered when values exceed healthy limits.  

### Light Control & Automation
- LDR sensors measure ambient light and send averaged data to Node-RED.  
- Servo motor adjusts shaded sliding window based on light and temperature.  
- Node-RED dashboard allows real-time monitoring and parameter adjustments.  

---


### Wokwi Simulation: 
  <img width="786" height="457" alt="Wokwi_simulation" src="https://github.com/user-attachments/assets/22c56942-468d-4e39-bbea-f28ede15818a" />


### Node-RED Dashboard:  
  <img width="786" height="457" alt="node_red_dashboard" src="https://github.com/user-attachments/assets/22a22a5b-d80f-4fce-bf47-bb794033f8e1" />



---
