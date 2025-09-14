// import libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>


// define OLED parameters
# define SCREEN_WIDTH 128  
# define SCREEN_HEIGHT 64
# define OLED_RESET -1
# define SCREEN_ADDRESS 0x3C

#define BUZZER 2  // buzzer is connected to pin 2
#define LED_1 15     // LED is connected to pin 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35    // push button is connected to pin 35
#define DHTPIN 12  // temp & humidity sensor to pin 12

#define NTP_SERVER     "pool.ntp.org" // responsible for the server address

#define UTC_OFFSET_DST 5.5   



// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
float UTC_OFFSET = 0.0; // Default to UTC   

unsigned long timeNow = 0;
unsigned long timeLast = 0;
unsigned long lastNtpSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 5000;

bool alarm_enabled = true;   //on and off all alarms
int n_alarms = 2;     // No alarms initially
int alarm_hours[]={8,20} ;   // Store up to 5 alarms
int alarm_minutes[]= {30,30} ;
bool alarm_triggered[] = {false, false,};  // keep the track of whether the user responded

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

void setup() {
  UTC_OFFSET = 5.5; // Default Time Zone (UTC+5.5)

  configTime(UTC_OFFSET * 3600, 0, NTP_SERVER);
  
  // Initialize hardware
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  
  Serial.begin(115200);

  // Initialize OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {  
    for (;;);
  }

  display.display(); // Show Adafruit logo
  delay(500);

  // Connect to WiFi
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);
  }
  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);

  // Sync time with NTP
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVER);
  
  // Ensure time sync
  struct tm timeinfo;
  int retries = 10;
  while (!getLocalTime(&timeinfo) && retries > 0) {  
    delay(500);
    retries--;
  }

  // Welcome message
  display.clearDisplay();
  print_line("Welcome to MediBox!", 0, 20, 2);
  delay(500);
  display.clearDisplay();
}

void loop() {
  static int lastSecond = -1;
  sync_time_with_ntp();  

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

  // Detect short button press for entering menu
  if (digitalRead(PB_OK) == LOW) {
      delay(50); // Short debounce delay
      if (digitalRead(PB_OK) == LOW) { // Ensure button is still pressed
          go_to_menu();
      }
  }

  // Detect short button press for stopping alarm
  if (digitalRead(PB_CANCEL) == LOW) {
      delay(50); 
      if (digitalRead(PB_CANCEL) == LOW) { 
          stop_alarm();
      }
  }

  check_temp();
}



void print_line(String text, int column, int row, int text_size){
    
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column,row);  // (column,row)
  display.println(text); //display a custom message

  display.display();
  
}

void print_time_now(void){
  display.fillRect(0, 0, SCREEN_WIDTH, 15, BLACK);  // Only clear time area
  
  print_line(String (hours), 0, 0, 2);
  print_line(":", 22, 0, 2);
  print_line(String (minutes), 35, 0, 2);
  print_line(":", 60, 0, 2);
  print_line(String (seconds), 75, 0, 2);
  
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

          // Ensure time updates immediately
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
          Serial.println("NTP Sync Successful!");
      } else {
          Serial.println("NTP Sync Failed!");
      }
  }
}



void update_time(){   // time from Wifi
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

  // Set hour
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

  // Set minute
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

  // If setting a new alarm, increase the alarm count
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
  display.clearDisplay();  // Clear OLED when alarm is stopped
  display.display();
  delay(1000);

  alarm_enabled = true;  // Ensure alarms can be checked again
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

void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  static float lastTemperature = -1;
  static float lastHumidity = -1;
  static bool lastWarning = false;

  bool warning = false;

  // Only clear the display once before displaying warnings if there's a change in condition
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