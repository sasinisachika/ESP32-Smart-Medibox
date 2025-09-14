// import libraries
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// define OLED parameters
# define SCREEN_WIDTH 128  
# define SCREEN_HEIGHT 64
# define OLED_RESET -1
# define SCREEN_ADDRESS 0x3C

#define BUZZER 2  
#define LED_1 15     
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 26    
#define DHTPIN 12  
#define LDR 35
#define SERVO 14
#define NTP_SERVER     "pool.ntp.org" 
#define UTC_OFFSET_DST 5.5  


// function prototype
void setupWifi();
void setupMqtt();
void connectToBroker();
void adjust_window_angle();
void print_line(String text, int column, int row, int text_size);
void print_time_now(void);
void set_time_zone();
void set_alarm(int alarm);
void view_alarms();
void delete_alarm();
void ring_alarm();
void stop_alarm();
void snooze_alarm();
void sync_time_with_ntp();
void update_time();
void update_time_with_check_alarm(void);
void check_temp();  
void go_to_menu();  
int wait_for_button_press();
void run_code();  
void handleLDRReadings();
void mqttCallback(char* topic, byte* payload, unsigned int length);

// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Servo windowServo;       

// Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
float UTC_OFFSET = 0.0;   
unsigned long timeNow = 0;
unsigned long timeLast = 0;
unsigned long lastNtpSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 1000;
float gamma_factor = 0.75f;          // default Controlling factor for angle calculation
float t_med = 30.0f;          // default Ideal medicine temperature
int theta_offset = 30; // default Minimum angle for servo
bool alarm_enabled = true;   //on and off all alarms
int n_alarms = 2;     
int alarm_hours[]={8,20} ;   
int alarm_minutes[]= {30,30} ;
bool alarm_triggered[] = {false, false,};  
int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] ={C, D, E, F, G, A, B, C_H};
int current_mode = 0;
int max_modes = 6;
String modes[] = {" 1 -  Set Time Zone", " 2 -  Set Alarm 1", " 3 -  Set Alarm 2", " 4 - View Alarm",  "5 - Delete Alarm"};

// LDR related variables
unsigned long lastSampleTime = 0;
unsigned long lastSendTime = 0;
int sampleInterval = 5000;       // Default 5 seconds (in ms)
int sendInterval = 120000;       // Default 2 minutes (in ms)
const int maxSamples = 100;      // Maximum samples to store
float ldrSamples[maxSamples];    // Array to store samples
int sampleCount = 0;             // Current number of samples
float currentAverage = 0;        // Current average light intensity
char ldrValue[10];
char tempAr[6];


void setup() {
  Serial.begin(115200);  

  setupWifi();     // Connect to WiFi
  setupMqtt();     // Set server + try to connect to MQTT
  connectToBroker();

  UTC_OFFSET = 5.5; 
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVER); 

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  windowServo.attach(SERVO);
  windowServo.write(theta_offset); // Initialize to minimum angl

  // Initialize hardware
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  // Initialize OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for (;;); // Stop if display init fails
  }

  display.display(); 
  delay(500);

  configTime(UTC_OFFSET * 3600, 0, NTP_SERVER);
  struct tm timeinfo;
  int retries = 10;
  while (!getLocalTime(&timeinfo) && retries > 0) {
    delay(500);
    retries--;
  }
  display.clearDisplay();
  print_line("Welcome to MediBox!", 0, 20, 2);
  delay(500);
  display.clearDisplay();
}


void loop() {
  static int lastSecond = -1;
  sync_time_with_ntp();  

  if(!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();
  
  check_temp();
  handleLDRReadings();
  adjust_window_angle();
  mqttClient.publish("medibox/temperature",tempAr);

  if (millis() - timeLast >= 1000) {  
      timeLast = millis();

      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {  
          if (timeinfo.tm_sec != lastSecond) {
              lastSecond = timeinfo.tm_sec;
              update_time_with_check_alarm();
          }
      } else {
          Serial.println("Failed to obtain time");
      }
  }

  if (digitalRead(PB_OK) == LOW) {
      delay(50); 
      if (digitalRead(PB_OK) == LOW) { 
          go_to_menu();
      }
  }

  if (digitalRead(PB_CANCEL) == LOW) {
      delay(50); 
      if (digitalRead(PB_CANCEL) == LOW) { 
          stop_alarm();
      }
  }
  check_temp();
}


void handleLDRReadings() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentTime;
    
    int rawValue = analogRead(LDR);
    float normalizedValue = 1.0 - (rawValue / 4095.0);
    
    if (sampleCount < maxSamples) {
      ldrSamples[sampleCount] = normalizedValue;
      sampleCount++;
    } else {
      for (int i = 0; i < maxSamples - 1; i++) {
        ldrSamples[i] = ldrSamples[i + 1];
      }
      ldrSamples[maxSamples - 1] = normalizedValue;
    }
    
    Serial.print("LDR Sample: ");
    Serial.println(normalizedValue, 4);
  }
  
  // Calculate and send average at regular intervals
  if (currentTime - lastSendTime >= sendInterval && sampleCount > 0) {
    lastSendTime = currentTime;
    
    // Calculate average
    float sum = 0;
    for (int i = 0; i < sampleCount; i++) {
      sum += ldrSamples[i];
    }
    currentAverage = sum / sampleCount;
    
    // Convert to string and publish
    String(currentAverage, 4).toCharArray(ldrValue, 10);
    mqttClient.publish("medibox/light_intensity", ldrValue);
    
    Serial.print("Average Light Intensity: ");
    Serial.println(currentAverage, 4);
    
    // Reset sample count for next averaging period
    sampleCount = 0;
  }
}

void adjust_window_angle() {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    float temperature = data.temperature;

    Serial.print("theta offset : ");
    Serial.println(theta_offset);
    Serial.print("Average intensity : " );
    Serial.println(currentAverage);;
    Serial.print("gamma : ");
    Serial.println(gamma_factor);
    Serial.print("sampling Interval : ");
    Serial.println((float)sampleInterval);
    Serial.print("sending Interval ; ");
    Serial.println((float)sendInterval);
    Serial.print("Ideal Temp ; ");
    Serial.println(t_med);
    
    // Calculate motor angle using the provided equation
    float theta = theta_offset + (180.0 - theta_offset) * currentAverage * gamma_factor * log((float)sampleInterval/(float)sendInterval) * (temperature / t_med);
    
    Serial.print("Servo motor angle :");
    Serial.print(theta);
    // Move the servo to calculated angle
    windowServo.write((int)theta);

    // Publish the current angle to MQTT
    char angleStr[10];
    dtostrf(theta, 1, 2, angleStr);
    mqttClient.publish("medibox/window_angle", angleStr);
}


void setupWifi(){
  Serial.println("Connecting to ");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("broker.hivemq.com", 1883);  // Public broker
  mqttClient.setCallback(mqttCallback);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';
  
  Serial.println(payloadStr);
  
  // Handle configuration messages
  if (strcmp(topic, "medibox/config/sample_interval") == 0) {
    int newInterval = atoi(payloadStr) * 1000; // Convert seconds to milliseconds
    if (newInterval > 0) {
      sampleInterval = newInterval;
      Serial.print("Sample interval set to: ");
      Serial.println(sampleInterval);
    }
  } 
  else if (strcmp(topic, "medibox/config/send_interval") == 0) {
    int newInterval = atoi(payloadStr) * 1000; // Convert seconds to milliseconds
    if (newInterval > 0) {
      sendInterval = newInterval;
      Serial.print("Send interval set to: ");
      Serial.println(sendInterval);
    }
  }
  else if (strcmp(topic, "medibox/config/theta_offset") == 0) {
    int new_theta_offset = atoi(payloadStr);
    theta_offset = new_theta_offset;
    Serial.print("Minimum angle set to: ");
    Serial.println(theta_offset);
  }
  else if (strcmp(topic, "medibox/config/gamma") == 0) {
    float new_gamma_factor = atof(payloadStr);
    gamma_factor = new_gamma_factor;
    Serial.print("Controlling factor set to: ");
    Serial.println(gamma_factor);
  }
  else if (strcmp(topic, "medibox/config/t_med") == 0) {
    float new_t_med = atof(payloadStr);
    t_med = new_t_med;
    Serial.print("Ideal temperature set to: ");
    Serial.println(t_med);
  }
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.println("Attempting MQTT connection...");
    
    if (mqttClient.connect("ESP32-MediBox20021969")) {
      Serial.println("Connected to MQTT broker.");
      mqttClient.subscribe("medibox/config/sample_interval");
      mqttClient.subscribe("medibox/config/send_interval");
      mqttClient.subscribe("medibox/config/gamma");
      mqttClient.subscribe("medibox/config/t_med");
      mqttClient.subscribe("medibox/config/theta_offset");
    } else {
      Serial.println("MQTT connection failed.");
      Serial.print("State: ");
      Serial.println(mqttClient.state());  
      delay(1000);
    }
  }
}

void print_line(String text, int column, int row, int text_size){
    
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column,row);  // (column,row)
  display.println(text); //display a custom message

  display.display();
  
}

void print_time_now(void) {
  static String prev_time = "";

  char buffer[9]; 
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
  String current_time = String(buffer);

  if (current_time != prev_time) {
    display.fillRect(0, 0, SCREEN_WIDTH, 15, BLACK);
    print_line(current_time, 0, 0, 2);
    prev_time = current_time;
  }
}

void set_time_zone() {
  float temp_offset = UTC_OFFSET;  // Use current time zone as the starting value

  while (true) {
      display.clearDisplay();
      print_line("UTC Offset: " + String(temp_offset, 1), 0, 0, 2);

      int pressed = wait_for_button_press();
      if (pressed == PB_UP) temp_offset += 0.5;
      if (pressed == PB_DOWN) temp_offset -= 0.5;
      if (pressed == PB_OK) {
          UTC_OFFSET = temp_offset;  // Apply new time zone (not stored in flash)
          configTime(UTC_OFFSET * 3600, 0, NTP_SERVER); // Apply new timezone
          
          // Wait for time synchronization
          struct tm timeinfo;
          int retries = 10;
          while (!getLocalTime(&timeinfo) && retries > 0) {  
              delay(500);
              retries--;
          }
          update_time(); 
          display.clearDisplay();
          print_line("Time Zone Set", 0, 0, 2);
          delay(1000);
          break;
      }
      if (pressed == PB_CANCEL) break;
  }
}

void sync_time_with_ntp() {
  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi not connected, retrying...");
      return;  // Skip NTP sync if WiFi is not connected
  }

  if (millis() - lastNtpSync > NTP_SYNC_INTERVAL) {  // Sync every 5 seconds
      struct tm timeinfo;
      
      configTime(UTC_OFFSET * 3600, 0, NTP_SERVER);  // Request fresh time
      int retries = 5;
      while (!getLocalTime(&timeinfo) && retries > 0) {  
          delay(500);
          retries--;
      }
      if (retries > 0) {
          lastNtpSync = millis();  // Update last sync time
      } else {
          Serial.println("NTP Sync Failed!");
      }
  }
}

void update_time(){   
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour,3,"%H", &timeinfo); 
  hours = atoi(timeHour);  

  char timeMinute[3];
  strftime(timeMinute,3,"%M", &timeinfo);  
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond,3,"%S", &timeinfo);  
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay,3,"%d", &timeinfo);  
  days = atoi(timeDay);
}

void update_time_with_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarm_enabled) {
      for (int i = 0; i < n_alarms; i++) {
          if (alarm_hours[i] == hours && alarm_minutes[i] == minutes && !alarm_triggered[i]) {
              digitalWrite(LED_1, HIGH);  // Ensure LED turns on when alarm rings
              ring_alarm();
              alarm_triggered[i] = true;  // Prevent multiple rings in the same minute
          } else if (alarm_hours[i] != hours || alarm_minutes[i] != minutes) {
              alarm_triggered[i] = false;   
          }
      }
  }
}

void set_alarm(int alarm){  
  if (alarm >= n_alarms) {  
    alarm_hours[alarm] = 0;
    alarm_minutes[alarm] = 0;
  }
  int temp_hour = alarm_hours[alarm];   

  while (true){
    display.clearDisplay();
    print_line("Enter hour: "+ String(temp_hour), 0, 0, 2);  
    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      temp_hour = (temp_hour + 1) % 24;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_hour = (temp_hour - 1 + 24) % 24;
    }
    else if (pressed == PB_OK){
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break; 
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      return;  // Cancel setting the alarm
    }
  }
  int temp_minute = alarm_minutes[alarm];

  while (true){
    display.clearDisplay();
    print_line("Enter minute: "+ String(temp_minute), 0, 0, 2);
  
    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      temp_minute = (temp_minute + 1) % 60;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_minute = (temp_minute - 1 + 60) % 60;
    }
    else if (pressed == PB_OK){
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break; 
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      return;
    }
  }
  if (alarm == n_alarms) {
    n_alarms++;
  }

  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);
}

void view_alarms() {
  display.clearDisplay();

  if (n_alarms == 0) {  // If no alarms are set
      print_line("No active alarms!", 10, 20, 2);
      delay(1000);
      return;
  }
  
  for (int i = 0; i < n_alarms; i++) {
      String alarmText = String(alarm_hours[i]) + ":" + 
                         (alarm_minutes[i] < 10 ? "0" : "") + 
                         String(alarm_minutes[i]) +
                         (alarm_enabled ? " ON" : " OFF");

      print_line(alarmText, 20, i * 25, 2); 
  }
  display.display();

  // Wait for PB_CANCEL to exit
  while (true) {
      int pressed = wait_for_button_press();
      if (pressed == PB_CANCEL) {
          break; // Exit the view
      }
  }
}

void ring_alarm() {
  bool alarm_active = true;
  digitalWrite(LED_1, HIGH);

  while (alarm_active) {
      display.clearDisplay();
      print_line("MEDICINE TIME!", 0, 0, 2);
      display.display();

      for (int i = 0; i < n_notes; i++) {
          tone(BUZZER, notes[i]);
          delay(500);
          noTone(BUZZER);
          delay(2);

          //  for stopping the alarm
          if (digitalRead(PB_CANCEL) == LOW) {
              delay(50);
              if (digitalRead(PB_CANCEL) == LOW) {
                  stop_alarm();
                  return;
              }
          }
          //  for snoozing
          if (digitalRead(PB_OK) == LOW) {
              delay(50);
              if (digitalRead(PB_OK) == LOW) {
                  stop_alarm();
                  snooze_alarm();
                  return;
              }
          }
      }
  }
}

void stop_alarm() {
  digitalWrite(LED_1, LOW);  // Ensure LED is turned off
  noTone(BUZZER);
  display.clearDisplay();  
  display.display();
  delay(1000);

  alarm_enabled = true;  
}

void snooze_alarm() {
  digitalWrite(LED_1, LOW);  // Turn off LED during snooze

  for (int j = 0; j < n_alarms; j++) {
      if (alarm_hours[j] == hours && alarm_minutes[j] == minutes) {
          alarm_minutes[j] += 5;
          if (alarm_minutes[j] >= 60) {
              alarm_minutes[j] -= 60;
              alarm_hours[j] = (alarm_hours[j] + 1) % 24;
          }
          alarm_triggered[j] = false;  // Allow alarm to ring again after snooze
          break;
      }
  }

  unsigned long snooze_start = millis();
  while (millis() - snooze_start < 300000) { // 5-minute snooze
      update_time_with_check_alarm();
      sync_time_with_ntp();
      check_temp();

      if (digitalRead(PB_CANCEL) == LOW) { 
          stop_alarm();
          go_to_menu();
          return;
      }
      if (digitalRead(PB_OK) == LOW) { 
          go_to_menu();
          return;
      }
      delay(1000);
  }
  // After snooze time ends, ring the alarm again
  ring_alarm();
}

void delete_alarm() {
  if (n_alarms == 0) {
    display.clearDisplay();
    print_line("No alarms set!", 0, 0, 2);
    delay(1000);
    return;
  }
  int selected_alarm = 0;  // Index of the selected alarm
  
  while (true) {
    display.clearDisplay();
    print_line("Select   Alarm:", 10, 0, 2);
    
    print_line(String(alarm_hours[selected_alarm]) + ":" + 
               String(alarm_minutes[selected_alarm]), 10, 40, 2);
    
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      selected_alarm = (selected_alarm + 1) % n_alarms;
    } 
    else if (pressed == PB_DOWN) {
      selected_alarm = (selected_alarm - 1 + n_alarms) % n_alarms;
    }
    else if (pressed == PB_OK) {
      // Shift alarms up to remove the selected one
      for (int i = selected_alarm; i < n_alarms - 1; i++) {
        alarm_hours[i] = alarm_hours[i + 1];
        alarm_minutes[i] = alarm_minutes[i + 1];
        alarm_triggered[i] = alarm_triggered[i + 1];
      }
      n_alarms--; // Reduce the number of alarms
      
      display.clearDisplay();
      print_line("  Alarm   Deleted!", 0, 0, 2);
      delay(1000);
      break;
    }
    else if (pressed == PB_CANCEL) {
      break;
    }
  }
}

int wait_for_button_press(){
  while (true){
    if (digitalRead(PB_UP) == LOW){
      delay (200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW){
      delay (200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW){
      delay (200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW){
      delay (200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void run_mode(int mode){
  if (mode == 0){
    set_time_zone();
  }
  if (mode == 1 || mode == 2){
    set_alarm(mode-1);
  } 
  else if (mode == 3) { // New option for viewing alarms
    view_alarms();
  }
  else if (mode == 4) {
    delete_alarm();
    }
}

void go_to_menu() {
  bool inMenu = true;
  while (inMenu) {
      display.clearDisplay();
      print_line(modes[current_mode], 0, 0, 2);
      display.display();

      int pressed = wait_for_button_press();
      if (pressed == PB_UP) {
          current_mode = (current_mode + 1) % max_modes;
      } else if (pressed == PB_DOWN) {
          current_mode = (current_mode - 1 + max_modes) % max_modes;
      } else if (pressed == PB_OK) {
          run_mode(current_mode);
      } else if (pressed == PB_CANCEL) {
          inMenu = false;  // Exit the menu
      }
  }
  display.clearDisplay();
  display.display();
  delay(500);
}

void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
  static float lastTemperature = -1;
  static float lastHumidity = -1;
  static bool lastWarning = false;
  bool warning = false;

  if (data.temperature != lastTemperature || data.humidity != lastHumidity) {
      display.clearDisplay(); 
  }
  if (data.temperature > 32) {
      warning = true;
      print_line("temp high", 0, 25, 2);
  }
  else if (data.temperature < 24) {
      warning = true;
      print_line("temp low", 0, 25, 2);
  }
  if (data.humidity > 80) {
      warning = true;
      print_line("humid high", 0, 42, 2);
  }
  else if (data.humidity < 65) {
      warning = true;
      print_line("humid low", 0, 42, 2);
  }
  if (warning) {
      display.display(); // Update the display only if a warning condition is met
  }
  // Store the current temperature and humidity for comparison next time
  lastTemperature = data.temperature;
  lastHumidity = data.humidity;
}
