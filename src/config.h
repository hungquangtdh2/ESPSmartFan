#ifndef CONFIG_H
#define CONFIG_H

// Include necessary libraries and define constants
#include <ESP8266WiFi.h>
#include <TM1637Display.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <OneWire.h> // for ds 18b20
// Create a PubSubClient object
WiFiClient espClient;
PubSubClient mqttClient(espClient);
// Define the pins for the ADC, PWM, TM1637, Relay, Switch, and IR Receiver
#define ADC_PIN A0      // Analog pin for ADC
#define PWM_PIN D1      // Pin for PWM control
#define CLK_PIN D3      // Pin for TM1637 clock
#define DIO_PIN D4      // Pin for TM1637 data
#define RELAY_PIN D6    // Pin for Relay
#define SWITCH_PIN D7   // Pin for Switch
#define IR_RECEIVER_PIN D5  // Pin for IR Receiver

// Define IR remote button codes
#define POWER_BUTTON 16753245
#define POWER_BUTTON2 33456255
#define GEAR_BUTTON 16769565
#define GEAR_BUTTON2 33446055
#define TIMER_PLUS_BUTTON 16769055
#define TIMER_MINUS_BUTTON 16748655
#define time_Plus_btn2 33441975
#define time_minus_btn2 33454215
#define FREQUENCY_EEPROM_ADDR 0
#define GEAR_EEPROM_ADDR 1
#define Senko_On  3458
#define Senko_Off  3457
#define Senko_swing  3472
#define Senko_speed_down  3464
#define Senko_speed_up  3460
#define Senko_lamp  1009394670
#define QT_lamp_up  33448095
#define  QT_speed_up 33439935
#define  QT_speed_down 33472575
#define  QT_lamp_down 33464415 
#define  sm_lamp_down 65263353 
#define  sm_lamp_up 4216646235 
#define  MaxFreequency 400
#define MinFreequency 0
#define pwmFre  18000
// function same lamp1 
// Declare global variables for
// Global variable for frequency
int frequency = 0;
ESP8266WebServer server(80);
// Define variables for ADC, PWM, TM1637, Relay, Switch, and IR Receiver
int adcValue;
int previousAdcValue = 0;
int switchState = HIGH;  // Assume switch is initially in the HIGH state
bool relayState = LOW;    // Assume relay is initially in the LOW state
bool display_led = HIGH;
// Create a TM1637 display object
TM1637Display display(CLK_PIN, DIO_PIN);
unsigned long lastIRTime = 0;
bool displayDay = true;
unsigned long lastADCChangeTime = 0;
unsigned long lastDisplayTime = 0;
// Create an IR receiver object
IRrecv irrecv(IR_RECEIVER_PIN);
decode_results results;
bool colonOn = true;
// Wi-Fi settings
// const char* ssid = "No";
// const char* password = "hunghung1";
#define MQTT_SERVER_IP_SIZE  16      // Size of MQTT Server IP (assuming IPv4)
#define MQTT_USERNAME_SIZE  16      // Size of MQTT Username
#define MQTT_PASSWORD_SIZE  16      // Size of MQTT Password
String mqttServerIP;
String mqttUsername;
String mqttPassword;
bool enableMQTT;
bool isPWM = false; // chuyen sang true neu nhu dung pwm/ false neu dung pwm
// NTP settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// EEPROM address to store SSID and password
int ssidAddress = 128;
int passwordAddress = 32;
#define EEPROM_MQTT_SERVER_IP 3      // Start address for MQTT Server IP
#define EEPROM_MQTT_USERNAME 200      // Start address for MQTT Username
#define EEPROM_MQTT_PASSWORD 96      // Start address for MQTT Password
#define EEPROM_ENABLE_MQTT   64      // Address to store the Enable MQTT flag
unsigned long reconnectTimer = 0;
const unsigned long reconnectInterval = 5000;  // 5 seconds
int light_brightness= 2;
String ssid;
String password;
void setFanFrequency(int newFrequency);
// Define the size of each MQTT configuration element
void setFanGear(int newGear) ;

void handleSetFrequency();
#endif
