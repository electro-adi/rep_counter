
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "OTA.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_VL53L0X.h"

#define I2C_SDA 2 
#define I2C_SCL 0
#define Button 3

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     1 

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

typedef struct struct_message
{
  bool shock = false;
  bool reset = false;
  bool ota_mode = false;

} struct_message;
struct_message Data;

uint8_t Shocker_Address[] = {0x8C, 0xAA, 0xB5, 0xC6, 0x57, 0x43}; //ESP-01 MAC

int reps;
int min_value = 10000;
bool calibrated = false;
int bar = 0;
bool rep_finished = true;
bool error = false;
bool esp_done = false;
bool ota = false;

int esp_now_state = 0; //0 idle, 1 sent, 2 sent error; 
unsigned long on_display = 0;

unsigned long timer_start = 0;
bool shocker_online = false;

unsigned long sleep_timer = 0;
bool sleep = false;

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == 0)
  {
    esp_now_state = 1; 
  }
  else 
  {
    esp_now_state = 2; 
  }
  on_display = millis();
}

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {

  shocker_online = true;
}

void setup() {

  pinMode(Button, INPUT);

  Wire.begin(I2C_SDA, I2C_SCL);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.setRotation(2);
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.clearDisplay();
  display.setCursor(7, 7);
  display.println("ADI'S SUPER AWESOME");
  display.setCursor(16, 20);
  display.print("REP COUNTER 3000");
  display.drawRect(0, 0, 128, 32, 1);
  display.display();

  delay(3000);

  if(digitalRead(Button) == HIGH)//start ota functions if the button is high during boot
  {
    ota = true;
    setupOTA("RepCounter", "WIFI_SSID_HERE", "WIFI_PASSWORD_HERE");
    display.clearDisplay();
    display.setCursor(10, 7);
    display.println(" OTA STARTED!");
    display.display();
    delay(2000);
  } 

  if (!lox.begin()) {//sensor init
    display.clearDisplay();
    display.drawRect(0, 0, 128, 32, 1);
    display.setCursor(10, 20);
    display.print("Failed to boot -_-");
    display.display();
    error = true;
    delay(3000);
  }

  lox.startRangeContinuous();
}

void loop() {

  if(ota)//if ota mode is active
  {
    ArduinoOTA.handle();
  }
  else
  {
    if(!esp_done)//start esp now if not done already
    {
      WiFi.mode(WIFI_STA);
      esp_now_init();
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      esp_now_add_peer(Shocker_Address, ESP_NOW_ROLE_COMBO, 0, NULL, 0);;
      esp_done = true;
    }

    if(shocker_online)//if shock bracelet is turned on, then display this. 
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.drawRect(0, 0, 128, 32, 1);
      display.drawRect(2, 2, 124, 28, 1);
      display.setCursor(23, 7);
      display.print("Shock Bracelet");
      display.setCursor(35, 18);
      display.print("Connected");
      display.display();
      shocker_online = false;
      delay(2000);
    }

    if(millis() - timer_start > 5000 && reps != 0)//if the machine is not moving in the middle of the set, then shock the user
    {
      Data.shock = true;
      esp_now_send(Shocker_Address, (uint8_t *) &Data, sizeof(Data));
      timer_start = millis();//reset timer and shock again after 4 seconds if the user is still idle
    }

    if(millis() - sleep_timer > 20000)//if the machine is not moving at all, then enter sleep mode
    {
      sleep = true;
      display.clearDisplay();
      display.display();
    }
  }

  if(digitalRead(Button) == HIGH)
  {
    calibrated = true;//press button to end calibration mode
    reps = 0;//press button to reset the rep count to 0
    sleep = false;//press to wake up device
  }

  if (lox.isRangeComplete() && !error && !sleep) {

    int value = lox.readRange() / 10;//convert recorded mm into cm

    if(!calibrated)//calibrate (bring leg press machine to the lowest distance)
    {
      if(value < min_value) min_value = value;//find the lowest value

      display.clearDisplay();
      display.drawRect(0, 0, 128, 32, 1);
      display.setCursor(5, 5);
      display.println("Calibrating...");
      display.setCursor(5, 18);
      display.print("Minimum Value ");
      display.print(min_value);
      display.print(" cm");
      display.display();
    }
    else// CALIBRATION DONE 
    {
      if(value <= min_value && rep_finished)//leg press at lowest point
      {
        reps++;
        rep_finished = false;
        timer_start = millis();//reset the shock timer
        sleep_timer = millis();//reset the sleep timer
      }

      if(value > min_value && !rep_finished)//leg press at the highest point
      {
        rep_finished = true;
        timer_start = millis();//reset the shock timer
        sleep_timer = millis();//reset the sleep timer
      }

      //display the animation for the 2 bars
      bar = 128 - (value - min_value) * (128 / (float)value);
      if (value <= min_value) bar = 128;

      display.clearDisplay();
      display.setTextSize(2);

      display.fillRect(0, 0, bar, 4, 1);
      display.fillRect(0, 28, bar, 4, 1);

      //display number of reps

      display.setCursor(2, 9);
      display.print("Reps ");
      display.print(reps);

      //display the measured distance

      display.setTextSize(1);
      display.setCursor(95, 13);
      display.print(value);
      display.print("cm");
      

      if(esp_now_state == 1)//if transmission to the shock braclet is successful then show 2 lines
      {
        display.fillRect(95, 8, 25, 2, 1);
        display.fillRect(95, 23, 25, 2, 1);
      }
      else if(esp_now_state == 2)//if transmission to the shock braclet failed then show dotted lines
      {
        display.fillRect(95, 8, 5, 2, 1);
        display.fillRect(95, 23, 5, 2, 1);
        display.fillRect(105, 8, 5, 2, 1);
        display.fillRect(105, 23, 5, 2, 1);
        display.fillRect(115, 8, 5, 2, 1);
        display.fillRect(115, 23, 5, 2, 1);
      }

      if(millis() - on_display > 1000) esp_now_state = 0;//remove the dotted lines or line after 1 second 

      display.display();
    }
  }
}