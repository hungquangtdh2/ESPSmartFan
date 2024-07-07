
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
// Function declarations
void readConfigFromLittleFS();
void saveConfigToLittleFS();
const char *frequencyFilePath = "/frequency.txt";
const char *gearFilePath = "/gear.txt";
// Constants
const char* configFilePath = "/config.json";
void setup() {

  Serial.begin(9600);
  LittleFS.begin(); 
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
 
  readConfigFromLittleFS();
  readFrequencyFromFile();
  // LittleFS.end();

  pinMode(PWM_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, 1);
  pinMode(SWITCH_PIN, INPUT);
  pinMode(IR_RECEIVER_PIN, INPUT);
  
  // Initialize the TM1637 display
  display.setBrightness(2);  // Set brightness (0 to 7)
  display.showNumberDec(9999);

  // Start the IR receiver
  irrecv.enableIRIn();

  // Connect to Wi-Fi
   if (connectToSavedWiFi()) {
    // Successful connection, run your main code
    // Initialize NTP
    timeClient.begin();

    // Setup routes
    server.on("/", HTTP_GET, handleNormalRoot);
    server.on("/config", HTTP_GET, handleConfig);
    server.on("/setFrequency", HTTP_GET, handleSetFrequency);
    server.on("/toggleRelay", HTTP_GET, handleToggleRelay);
     server.on("/restart", HTTP_GET, handleRestart);
    server.on("/mqttconfig", HTTP_GET, handleMQTTConfig);
    server.on("/saveMQTTConfig", HTTP_POST, handleSaveMQTTConfig);
    server.on("/saveConfig", HTTP_POST, handleSaveConfig);
    server.on("/getState", HTTP_GET, handleGetState); 
    loadMQTTConfig();
    if (enableMQTT) {
    connectToMQTT();
      }
    server.begin();
    
  } else {
    
    startAPMode();
  }
  printConfigJson();
 
}
void handleGetState() {
  // You should replace these variables with your actual state variables
  String json = "{\"frequency\": " + String(frequency) + ", \"relayState\": " + (relayState ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void loop() {
  // Check for IR remote signals
  server.handleClient();
  if (enableMQTT){
    reconnect();
  }
  mqttClient.loop();
  
  if (irrecv.decode(&results)) {
    delay(100);
    handleIRCommand(results.value);
    irrecv.resume();  // Receive the next value

    // Update the last time an IR signal was received
    lastIRTime = millis();
    
  }

  // Read ADC value
  adcValue = analogRead(ADC_PIN);

  // Check if ADC value differs by 32
  if (abs(adcValue - previousAdcValue) >= 32) {
    // Update frequency based on ADC

    // Map ADC value to frequency (0 to 400 Hz)
    frequency = map(adcValue, 0, 1023, MinFreequency, MaxFreequency);
    if (frequency <= 50) {
      analogWrite(PWM_PIN, 0); 
      frequency = 0;
        if (enableMQTT){ 
          publishFanSpeed(frequency);
        }
    } else {
      setFanFrequency(frequency);  // 50% duty cycle
    }
    saveFrequencyToLittleFS();
    // Display frequency on TM1637
    display.showNumberDec(frequency);
    display.setBrightness(light_brightness);
    // Print the updated ADC value and frequency
    Serial.print("ADC Value: ");
    Serial.print(adcValue);
    Serial.print(", Frequency: ");
    Serial.println(frequency);

    // Update previous ADC value
    previousAdcValue = adcValue;

    // Update the last time an ADC change occurred
    lastIRTime = millis();
  }

  // Read switch state
  int currentSwitchState = digitalRead(SWITCH_PIN);

  // Check if switch state has changed
  if (currentSwitchState != switchState) {
    // Update relay state
    relayState = currentSwitchState;

    // Print the updated relay state
    Serial.print("Switch State: ");
    Serial.println(relayState);

    // Update switch state
    switchState = currentSwitchState;
    saveGearStatusToLittleFS(relayState);
    digitalWrite(RELAY_PIN, relayState);
     if (enableMQTT) {
       String gearPayload = (relayState == HIGH) ? "ON" : "OFF";
      mqttClient.publish("/Fan/Gear", gearPayload.c_str());
     }
  }
   // Update NTPClient to refresh time
  timeClient.update();

  // Display the time if no IR and no ADC changes in the last 10 seconds
  if (millis() - lastIRTime > 10000 && millis() - lastADCChangeTime > 10000) {
  // Get the current time
  time_t rawTime = timeClient.getEpochTime();
  struct tm *timeInfo = localtime(&rawTime);

  // Adjust for GMT+7
  timeInfo->tm_hour += 7;

  // Format hours, minutes, and seconds
  int hours = timeInfo->tm_hour;
  int minutes = timeInfo->tm_min;
  int seconds = timeInfo->tm_sec;

  // Toggle colon every alternate second
  if (seconds % 2 == 0) {
    colonOn = true;
  } else {
    colonOn = false;
  }

  // Display time on TM1637 with flashing colon
  display.showNumberDecEx((hours * 100) + minutes, colonOn ? 0b01000000 : 0, true); // Use 24-hour format

  // You can use timeClient.getFormattedTime() to display the time where needed

  // Display day and month together every 10 seconds for 5 seconds
  if (millis() - lastDisplayTime > 10000 && millis() - lastDisplayTime < 15000) {
    char dayAndMonth[5];
    sprintf(dayAndMonth, "%02d%02d", timeInfo->tm_mday, timeInfo->tm_mon + 1); // Format as "DDMM"
    uint8_t segments[4];
    for (int i = 0; i < 4; i++) {
      segments[i] = dayAndMonth[i] - '0'; // Convert char to integer
    }
    display.setSegments(segments);
  }
  }
  delay(100);
}
void loadFrequencyFromLittleFS() {

  readFrequencyFromFile();
  // If the frequency is not 0, update PWM based on frequency
  if (frequency != 0) {
    analogWriteFreq(frequency);
    analogWrite(PWM_PIN, 128);
  }
}

void handleIRCommand(unsigned long command) {
  // Handle different IR remote button commands
  switch (command) {
    case POWER_BUTTON2:
    case Senko_On:
    
    case POWER_BUTTON:
      // Toggle frequency between 0 and 250
     
      frequency = (frequency == 0) ? 100 : 0;

      // Update PWM based on frequency
      if (frequency == 0) {
           if (enableMQTT){ 
          publishFanSpeed(frequency);
          setFanGear(0);
        }
        analogWrite(PWM_PIN, 0);  // Set PWM output to 0 when frequency is 0
      } else {

       setFanFrequency(frequency);
       setFanGear(relayState);
      }
      saveFrequencyToLittleFS();
      display.showNumberDec(frequency);
      Serial.print("Power Button Pressed, Frequency: ");
      Serial.println(frequency);
      break;
    case GEAR_BUTTON2:
    case GEAR_BUTTON:
    case Senko_swing:
      // Toggle relay state
      relayState = !relayState;
      setFanGear(relayState);
      saveGearStatusToLittleFS(relayState);
      break;
    case time_Plus_btn2:
    case TIMER_PLUS_BUTTON:
    case Senko_speed_up:
    case QT_speed_up:
      // Increase frequency
      frequency += 25;  // Adjust the increment as needed
      if (frequency < 25) {
        frequency = 25;
      }
      if (frequency > 400) {
        frequency = 400;
      }
      setFanFrequency(frequency);
      saveFrequencyToLittleFS();
      break;
    case time_minus_btn2:
    case TIMER_MINUS_BUTTON:
    case Senko_speed_down:
    case QT_speed_down:
      // Decrease frequency
      frequency -= 25;  // Adjust the decrement as needed
      if (frequency <= 0) {

        frequency = 0;
        if (enableMQTT){ 
          publishFanSpeed(frequency);
        }
       
        analogWrite(PWM_PIN, 0);
      }   // Set PWM output to 0 when frequency is 0 = 0;
      else {
        setFanFrequency(frequency);
      }
      display.showNumberDec(frequency);
      Serial.print("Timer Minus Button Pressed, Frequency: ");
      Serial.println(frequency);
      saveFrequencyToLittleFS();
      break;
    case Senko_Off:
    case Senko_lamp:
      display_led =! display_led;
      // Turn the TM1637 display on or off based on display_led
      if (display_led)
      {
        display.setBrightness(2); // Set brightness when turning ON (adjust as needed)
      }
      else
      {
        display.setBrightness(0x00); // Set brightness to 0 when turning OFF
      
      }
      break;
    case sm_lamp_up:
    case QT_lamp_up:
       light_brightness ++;
       if (light_brightness >7 ){
        light_brightness=7;
       }
      display.setBrightness(light_brightness);
      Serial.println("light brightness ");
      Serial.println(light_brightness);
      break;
    case sm_lamp_down:  
    case QT_lamp_down:
       light_brightness --;
       if (light_brightness <0 ){
        light_brightness=0;
       }
      display.setBrightness(light_brightness);
      Serial.println("light brightness ");
      Serial.println(light_brightness);
      break;
    
    default:
      // Unknown button command
      Serial.println("Unknown IR Command");
      Serial.println(command, DEC);
  }
}
void saveGearStatusToLittleFS(bool gear) {
 saveGearToFile();
}

void loadGearStatusFromLittleFS() {
 readGearFromFile();


  // Update the relay state
  digitalWrite(RELAY_PIN, relayState);
 // LittleFS.end();
}

void startAPMode() {
  Serial.println("Starting AP mode...");

  // Set up an open Wi-Fi network in AP mode
  WiFi.softAP("ESPsmartfan");

  // Start the web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();

  
}




void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESPsmartfan Wi-Fi Configuration</h1>";
  html += "<form action='/save' method='POST'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Password: <input type='password' name='password'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}
void handleSaveConfig() {
  // Handle saving configuration
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  // Save new Wi-Fi credentials to the configuration file
  saveConfigToLittleFS(ssid, password);

  server.send(200, "text/plain", "Configuration Saved. Rebooting...");
  delay(1000);
  ESP.restart();
}
void handleSave() {
 // Handle saving configuration
  ssid = server.arg("ssid");
  password = server.arg("password");

  // Save new Wi-Fi credentials to the configuration file
  saveConfigToLittleFS(ssid, password);
  server.send(200, "text/plain", "Configuration Saved. Rebooting...");
  delay(1000);
  ESP.restart();
}
void handleNormalRoot() {
  // Handle normal root path ("/") - Display frequency control UI
  // You can add HTML here to create a simple UI with a slider for frequency control
  String htmlContent = "<html><head>";
  htmlContent += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css'>";
  htmlContent += "<style>";
  htmlContent += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f4; }";
  htmlContent += "h1 { color: #333; }";
  htmlContent += "form { margin-top: 20px; }";
  htmlContent += ".circle { display: inline-block; width: 20px; height: 20px; border-radius: 50%; margin-right: 5px; }";
  htmlContent += ".on { background-color: red; }";
  htmlContent += ".off { background-color: green; }";
  htmlContent += ".fa-spin { animation: spin 2s infinite linear; }";
  htmlContent += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
  htmlContent += "</style>";
  htmlContent += "<script>";
  htmlContent += "function updateState() {";
  htmlContent += "  var xhttp = new XMLHttpRequest();";
  htmlContent += "  xhttp.onreadystatechange = function() {";
  htmlContent += "    if (this.readyState == 4 && this.status == 200) {";
  htmlContent += "      var state = JSON.parse(this.responseText);";
  htmlContent += "      document.getElementById('frequency').innerHTML = '<i class=\"fas fa-wind\"></i> Current Frequency: ' + state.frequency + ' Hz';";
  htmlContent += "      var relayState = state.relayState ? '<i class=\"fas fa-toggle-on\"></i> ON' : '<i class=\"fas fa-toggle-off\"></i> OFF';";
  htmlContent += "      document.getElementById('relayState').innerHTML = '<i class=\"fas fa-power-off\"></i> Relay State: ' + relayState;";
  htmlContent += "    }";
  htmlContent += "  };";
  htmlContent += "  xhttp.open('GET', '/getState', true);";
  htmlContent += "  xhttp.send();";
  htmlContent += "}";
  htmlContent += "setInterval(updateState, 5000);"; // Update every 5 seconds
  htmlContent += "</script>";
  htmlContent += "<title>ESP8266 Fan Control</title></head><body onload='updateState()'>";
  htmlContent += "<h1>Fan Control</h1>";

  // Display current frequency
  htmlContent += "<p id='frequency'><i class='fas fa-wind'></i> Current Frequency: " + String(frequency) + " Hz</p>";

  // Display relay state using an icon
  htmlContent += "<p id='relayState'><i class='fas fa-power-off'></i> Relay State: ";
  htmlContent += "<i class='fas fa-circle on'></i><span class='on'> ON</span>";
  htmlContent += "<i class='fas fa-circle off'></i><span class='off'> OFF</span>";
  htmlContent += "</p>";

  // Frequency control form
  htmlContent += "<form action='/setFrequency' method='get'>";
  htmlContent += "<label for='frequency'><i class='fas fa-sliders-h'></i> Frequency:</label> ";
  htmlContent += "<input type='range' name='frequency' min='0' max='400' value='" + String(frequency) + "'><br>";
  htmlContent += "<button type='submit' class='btn'><i class='fas fa-play'></i> Set Frequency</button>";
  htmlContent += "</form>";

  // Button to control the relay
  htmlContent += "<form action='/toggleRelay' method='get'>";
  htmlContent += "<button type='submit' class='btn'><i class='fas fa-power-off'></i> Toggle Relay</button>";
  htmlContent += "</form>";
  htmlContent += "<form action='/restart' method='get'>";
  htmlContent += "<button type='submit' class='btn'><i class='fas fa-power-off'></i> Restart</button>";
  htmlContent += "</form>";

  // Link to config page
  htmlContent += "<br><a href='/config'><button class='btn'><i class='fas fa-cogs'></i> Configuration</button></a>";
  htmlContent += "<br><a href='/mqttconfig'><button class='btn'><i class='fas fa-wifi'></i> MQTT Config</button></a>";
  htmlContent += "</body></html>";

  server.send(200, "text/html", htmlContent);
}

void handleToggleRelay() {
  // Toggle the state of the relay
  relayState = !relayState;
  setFanGear(relayState);
  server.sendHeader("Location", "/");
  server.send(303);
}
void handleRestart() {
  // Toggle the state of the relay
   ESP.restart();
}
void handleConfig() {
  String htmlContent = "<html><head><title>ESP8266 Configuration</title>";
  htmlContent += "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css'>";
  htmlContent += "</head><body class='bg-light'>";
  htmlContent += "<div class='container'>";
  htmlContent += "<h1 class='mt-4'>Configuration</h1>";
  htmlContent += "<form action='/saveConfig' method='post'>";
  htmlContent += "<div class='form-group'>";
  htmlContent += "<label for='ssid'>SSID:</label>";
  htmlContent += "<input type='text' class='form-control' name='ssid' required>";
  htmlContent += "</div>";
  htmlContent += "<div class='form-group'>";
  htmlContent += "<label for='password'>Password:</label>";
  htmlContent += "<input type='password' class='form-control' name='password' required>";
  htmlContent += "</div>";

  htmlContent += "<button type='submit' class='btn btn-primary'>Save Config</button>";
  htmlContent += "</form>";
  htmlContent += "</div>";
  htmlContent += "<script src='https://code.jquery.com/jquery-3.2.1.slim.min.js'></script>";
  htmlContent += "<script src='https://cdn.jsdelivr.net/npm/@popperjs/core@2.11.6/dist/umd/popper.min.js'></script>";
  htmlContent += "<script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js'></script>";
  htmlContent += "</body></html>";

  server.send(200, "text/html", htmlContent);
}
void handleMQTTConfig() {
  // Handle MQTT configuration page ("/mqttconfig")
  String htmlContent = "<html><head><title>MQTT Configuration</title></head><body>";
  htmlContent += "<h1>MQTT Configuration</h1>";
  htmlContent += "<form action='/saveMQTTConfig' method='post'>";
  htmlContent += "MQTT Server IP: <input type='text' name='mqttServerIP' value='" + String(mqttServerIP) + "'><br>";
  htmlContent += "MQTT Username: <input type='text' name='mqttUsername' value='" + String(mqttUsername) + "'><br>";
  htmlContent += "MQTT Password: <input type='password' name='mqttPassword' value='" + String(mqttPassword) + "'><br>";
  htmlContent += "Enable MQTT: <input type='checkbox' name='enableMQTT' {{ enableMQTT ? 'checked' : '' }}><br>";
  htmlContent += "<input type='submit' value='Save MQTT Configuration'>";
  htmlContent += "</form>";
  htmlContent += "<br><a href='/'>Back to Main Page</a>";
  htmlContent += "</body></html>";

  server.send(200, "text/html", htmlContent);
}
void handleSaveMQTTConfig() {
  // Handle saving MQTT configuration
  mqttServerIP = server.arg("mqttServerIP");
  mqttUsername = server.arg("mqttUsername");
  
  mqttPassword = server.arg("mqttPassword");
 enableMQTT = server.hasArg("enableMQTT") && (server.arg("enableMQTT") == "on");

  // Save new MQTT configuration to the configuration file
  saveMQTTConfigToLittleFS();
  
  server.send(200, "text/plain", "MQTT Configuration Saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void saveMQTTConfigToLittleFS() {
  DynamicJsonDocument jsonDocument(2048); // Adjust the size based on your config size

  // Open the file in read mode to preserve existing content
  File configFile = LittleFS.open(configFilePath, "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    return;
  }

  // Parse the existing JSON content
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  configFile.close();

  // Parse the existing JSON content
  DeserializationError error = deserializeJson(jsonDocument, buf.get());

  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  // Update the MQTT values in the JSON document
  jsonDocument["mqtt"]["serverIP"] = mqttServerIP;
  jsonDocument["mqtt"]["username"] = mqttUsername;
  jsonDocument["mqtt"]["password"] = mqttPassword;
  jsonDocument["mqtt"]["enabled"] = enableMQTT;

  // Reopen the file in write mode
  configFile = LittleFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  // Serialize JSON to the file
  serializeJson(jsonDocument, configFile);

  // Close the file
  configFile.close();

}

void saveConfigToLittleFS(const String& ssid, const String& password) {
  DynamicJsonDocument jsonDocument(2048); // Adjust the size based on your config size

  // Open the file in write mode
  File configFile = LittleFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  // Set values in the JSON document
  jsonDocument["wifiname"] = ssid;
  jsonDocument["password"] = password;

  // Serialize JSON to the file
  serializeJson(jsonDocument, configFile);

  // Close the file
  configFile.close();
}
void loadMQTTConfig() {
  File configFile = LittleFS.open(configFilePath, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }

  // Parse the JSON content
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  DynamicJsonDocument jsonDocument(2048); // Adjust the size based on your config size
  DeserializationError error = deserializeJson(jsonDocument, buf.get());

  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  // Read MQTT values from JSON
  enableMQTT = jsonDocument["mqtt"]["enabled"];
  mqttServerIP = jsonDocument["mqtt"]["serverIP"].as<String>();
  mqttUsername = jsonDocument["mqtt"]["username"].as<String>();
  mqttPassword = jsonDocument["mqtt"]["password"].as<String>();

  // Update other parts of your code accordingly...

  // Close the file
  configFile.close();
}
void saveMQTTConfig() {
  DynamicJsonDocument jsonDocument(2048); // Adjust the size based on your config size

  // Load existing config from SPIFFS
  File configFile = LittleFS.open(configFilePath, "r");
  if (configFile) {
    size_t size = configFile.size();
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    configFile.close();

    // Deserialize existing config
    DeserializationError error = deserializeJson(jsonDocument, buf.get());
    if (error) {
      Serial.println("Failed to parse existing config file");
      return;
    }
  } else {
    Serial.println("Failed to open existing config file for reading");
  }

  // Set MQTT values in the JSON document
  jsonDocument["mqtt"]["serverIP"] = mqttServerIP;
  jsonDocument["mqtt"]["username"] = mqttUsername;
  jsonDocument["mqtt"]["password"] = mqttPassword;
  jsonDocument["mqtt"]["enabled"] = enableMQTT;

  // Open the file in write mode
  configFile = LittleFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  // Serialize JSON to the file
  serializeJson(jsonDocument, configFile);

  // Close the file
  configFile.close();
}
void handleMQTTConnectionFailure() {
  // Handle MQTT connection failure
  enableMQTT = false;
  // Save updated configuration to 
  saveMQTTConfig();
}
void connectToMQTT() {
Serial.println("enable MQTT or not ");
Serial.println(enableMQTT);
 if (enableMQTT==1) {
    // Connect to the MQTT broker
    mqttClient.setKeepAlive(60);
    mqttClient.setServer(mqttServerIP.c_str(), 1883);
    mqttClient.setCallback(mqttCallback);
    Serial.println(mqttServerIP);
    Serial.println(mqttUsername);
    Serial.println(mqttPassword);
    uint8_t mac[6];
    WiFi.macAddress(mac);

  // Create the clientId by appending the last four digits of the MAC address
    String clientId = "ESP8266Client-";
    for (int i = 2; i < 6; i++) {
      clientId += String(mac[i], HEX);
    }
  
  if (mqttClient.connect(clientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {


      Serial.println("Connected to MQTT broker");
      // Subscribe to MQTT topics if needed
      mqttClient.subscribe("/Fan/State");
      mqttClient.subscribe("/Fan/Speed");
      mqttClient.subscribe("/Fan/Gear");
      mqttClient.subscribe("/Fan/SetFrequency");
      mqttClient.subscribe("/Fan/SetBrightness");
      mqttClient.subscribe("/Fan/SetGear");
    } else {
      Serial.println("Failed to connect to MQTT broker");
      handleMQTTConnectionFailure();
    }
  } else {
    // MQTT is not enabled, do nothing
  }
}
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received MQTT message on topic: ");
  Serial.println(topic);
  lastIRTime = millis();
  Serial.print("Payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Handle /Fan/SetFrequency
  if (strcmp(topic, "/Fan/SetBrightness") == 0) {
    // Ensure payload is a null-terminated string before converting to integer
    char brightStr[length + 1];
    memcpy(brightStr, payload, length);
    brightStr[length] = '\0';

    // Convert the string to an integer
    int newbrightness= atoi(brightStr);
    Serial.print("Converted brightness: ");
    Serial.println(newbrightness);
    // Set the new frequency for the fan
  }

  // Handle /Fan/SetGear
else if (strcmp(topic, "/Fan/SetGear") == 0) {
    // Ensure payload is a null-terminated string
    char gearStr[length + 1];
    memcpy(gearStr, payload, length);
    gearStr[length] = '\0';

    // Convert the string to a boolean
    bool newGear = strcmp(gearStr, "ON") == 0;

    // Set the new gear (relay state) for the fan
    // Assuming setFanGear function takes a boolean parameter
    setFanGear(newGear);
}
else if (strcmp(topic, "/Fan/SetFrequency") == 0) {
    // Ensure payload is a null-terminated string before converting to integer
    char freqStr[length + 1];
    memcpy(freqStr, payload, length);
    freqStr[length] = '\0';

    // Convert the string to an integer
  int newFrequency = atoi(freqStr);
    Serial.print("Converted Frequency: ");
    Serial.println(newFrequency);
    // Set the new frequency for the fan
    setFanFrequency(newFrequency);
  }


  // Add more handlers for other topics if needed
}



void setFanFrequency(int newFrequency) {
 
  if (isPWM == false ){
       if (newFrequency <= 50) {
    newFrequency = 0;
    analogWrite(PWM_PIN, 0);
  } else {
    analogWriteFreq(newFrequency);
    analogWrite(PWM_PIN, 128);  // 50% duty cycle
  }
   Serial.print("Frequency changed to: ");
   display.showNumberDec(newFrequency*100/(MaxFreequency-MinFreequency)); // only show the percent
   if( enableMQTT==1 ){
     publishFanSpeed(newFrequency);
  }
  }
  else{
    int pwmvalue ;
    pwmvalue = newFrequency*100/(MaxFreequency-MinFreequency);  
    analogWrite(PWM_PIN, pwmvalue);
    Serial.print("PWm changed to: ");
    display.showNumberDec(pwmvalue);
  if( enableMQTT==1 ){
     publishFanSpeed(pwmvalue);
  }
  }
 

 
 
  Serial.println(frequency);
  saveFrequencyToLittleFS();
  Serial.println(enableMQTT);
  

   saveFrequencyToFile();
 
}

void publishFanSpeed(int speed) {
  if (enableMQTT) {
    // Construct the MQTT topic
    String topic = "/Fan/Speed";

    // Convert the fan speed to a string
    String payload = String(speed);

    // Publish the payload to the MQTT topic
    mqttClient.publish(topic.c_str(), payload.c_str());
    if (speed == 0 ){
       String topic = "/Fan/State";

    // Convert the fan speed to a string
    String payload = "OFF";
     mqttClient.publish(topic.c_str(), payload.c_str());
    }
    else{
    String topic = "/Fan/State";

    // Convert the fan speed to a string
     String payload = "ON";
     mqttClient.publish(topic.c_str(), payload.c_str());
    }


    Serial.print("Published to MQTT - Topic: ");
    Serial.print(topic);
    Serial.print(", Payload: ");
    Serial.println(payload);
  }
}

void setFanGear(int newGear) {
       relayState = newGear;
      digitalWrite(RELAY_PIN, relayState);
      Serial.print("Gear Button Pressed, Relay State: ");
      Serial.println(relayState);
       lastIRTime = millis();

      digitalWrite(RELAY_PIN, relayState);
     if (enableMQTT) {
       String gearPayload = (relayState == HIGH) ? "ON" : "OFF";
      mqttClient.publish("/Fan/Gear", gearPayload.c_str());
     }
     saveGearStatusToLittleFS(relayState);
}
boolean connectToSavedWiFi() {
 
  Serial.print("Connecting to saved ssid");
  Serial.println(ssid);
  Serial.print("Connecting to pass");
  Serial.println(password);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.println("Connecting to saved Wi-Fi...");
    attempts++;
    
     
  }

  return WiFi.status() == WL_CONNECTED;
}
void saveFrequencyToLittleFS() {
    saveFrequencyToFile();
}

void handleSetFrequency() {

  if (server.hasArg("frequency")) {
    frequency = server.arg("frequency").toInt();
    setFanFrequency(frequency);
 lastIRTime = millis();

  }
  server.sendHeader("Location", "/");
  server.send(303);
}
void reconnect() {
  if (!mqttClient.connected()) {
    if (millis() - reconnectTimer > reconnectInterval) {
      Serial.print("Attempting MQTT connection...");
      if (mqttClient.connect("ESP8266Client", mqttUsername.c_str(), mqttPassword.c_str())) {
        Serial.println("connected");
        // Subscribe to topics here if needed
      } else {
        Serial.print("failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(" try again later");
      }
      // Update the reconnect timer
      reconnectTimer = millis();
    }
  }
}
void readConfigFromLittleFS() {
 
  File configFile = LittleFS.open(configFilePath, "r");
  if (!configFile) {
    Serial.println("Failed to open config file in the setup()");
    return;
  }

  // Parse the JSON content
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  DynamicJsonDocument jsonDocument(2048); // Adjust the size based on your config size
  DeserializationError error = deserializeJson(jsonDocument, buf.get());

  if (error) {
    Serial.println("Failed to parse config file");
    configFile.close();
   // LittleFS.end();
    return;
  }

// Read values from JSON
ssid = jsonDocument["wifiname"].as<String>();
Serial.println("ssid da duoc luu ");
Serial.println(ssid);

password = jsonDocument["password"].as<String>();
Serial.println(password);

mqttServerIP = jsonDocument["mqtt"]["serverIP"].as<String>();
Serial.println(mqttServerIP);

mqttUsername = jsonDocument["mqtt"]["username"].as<String>();
mqttPassword = jsonDocument["mqtt"]["password"].as<String>();

enableMQTT = jsonDocument["mqtt"]["enabled"].as<bool>();

  // Use the values as needed in your code...

  // Close the file
  configFile.close();
  //LittleFS.end();
}

void printConfigJson() {
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Open the config file in read mode
  File configFile = LittleFS.open(configFilePath, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }

  // Parse the JSON content
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  DynamicJsonDocument jsonDocument(2048); // Adjust the size based on your config size
  DeserializationError error = deserializeJson(jsonDocument, buf.get());

  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  // Print the JSON content
  serializeJsonPretty(jsonDocument, Serial);
  Serial.println();

  // Close the file
  configFile.close();
}
void updateFrequencyInConfig(int newFrequency) {
  // Open the config file for reading
  File configFile = LittleFS.open(configFilePath, "r");
  if (!configFile || configFile.isDirectory()) {
    Serial.println("Failed to open config file for reading");
    return;
  }

  // Parse the JSON content
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  DynamicJsonDocument jsonDocument(2048); // Adjust the size based on your config size

  // Parse the JSON content
  DeserializationError error = deserializeJson(jsonDocument, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  // Update the frequency in the JSON document
  jsonDocument["frequency"] = newFrequency;

  // Open the file in write mode
  configFile = LittleFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  // Serialize JSON to the file
  serializeJson(jsonDocument, configFile);

  // Close the file
  configFile.close();
  printConfigJson();
  // Close LittleFS after use
//  LittleFS.end();
}


void saveFrequencyToFile() {
  // Open the file in write mode
  File frequencyFile = LittleFS.open(frequencyFilePath, "w");
  if (!frequencyFile) {
    Serial.println("Failed to open frequency file for writing");
    return;
  }

  // Write the frequency value to the file
  frequencyFile.println(frequency);

  // Close the file
  frequencyFile.close();
}

void readFrequencyFromFile() {
  // Open the file in read mode
  File frequencyFile = LittleFS.open(frequencyFilePath, "r");
  if (!frequencyFile) {
    Serial.println("Failed to open frequency file for reading");
    return; // Keep the current value of frequency
  }

  // Read the frequency value from the file
  frequency = frequencyFile.readStringUntil('\n').toInt();

  // Close the file
  frequencyFile.close();
}
void saveGearToFile() {
  // Open the file in write mode
  File gearFile = LittleFS.open(gearFilePath, "w");
  if (!gearFile) {
    Serial.println("Failed to open gear file for writing");
    return;
  }

  // Write the gear value to the file
  gearFile.println(relayState);

  // Close the file
  gearFile.close();
}

void readGearFromFile() {
  // Open the file in read mode
  File gearFile = LittleFS.open(gearFilePath, "r");
  if (!gearFile) {
    Serial.println("Failed to open gear file for reading");
    return; // Keep the current value of relayState
  }

  // Read the gear value from the file
  relayState = (gearFile.readStringUntil('\n') == "1");

  // Close the file
  gearFile.close();
}