# ESP8266 Fan Control

## Description

This project involves controlling a fan using an ESP8266 microcontroller. The fan speed can be adjusted, and the state of the fan (ON/OFF) is displayed. Additionally, the project integrates temperature sensing using a DS18B20 sensor, and the temperature is displayed on a TM1637 display.

## Features

- Fan speed control with a frequency slider.
- Toggle the fan ON/OFF.
- Display current fan frequency and relay state.
- Temperature sensing using DS18B20.
- Display temperature on TM1637 display.
- MQTT integration to publish temperature information.
- Add control the Clock/ PWM 
- Control the relay to oscillating
- IR control code
- rheostat to change frequence/ Pwm
- OTA update 
## Components Used

- ESP8266 microcontroller.
- Fan connected to a relay for control.
- DS18B20 temperature sensor.
- TM1637 display for temperature output.
- VS1838 connect 

## Dependencies

- [Arduino IDE](https://www.arduino.cc/en/software)
- [Arduino Libraries](https://www.arduino.cc/en/guide/libraries):
  - [TM1637Display](https://github.com/avishorp/TM1637)
  - [OneWire](https://github.com/PaulStoffregen/OneWire)
  - [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
  - [PubSubClient](https://github.com/knolleary/pubsubclient)

## Wiring
- D1: Clock/ Pwm
- A0: rheostat 
- D4: TM1637 CLK
- D5: TM1637 DIO
- D6: Relay (ON / OFF)
- D7: Switch Relay 
- D8: Ds1820
- D5: VS1838 Data

## Getting Started

1. Clone the repository.
2. Open the project in the Platfromio.
3. Install the required libraries.
4. Configure the Wi-Fi credentials and MQTT broker details in the code.
5. Upload the code to your ESP8266.

## Usage

1. Power on the ESP8266.
2. Access the ESP8266 web interface to control the fan and view temperature information.
3. Monitor MQTT topics for additional information.
the topic of MQTT is 
"/Fan/State" : return the On/off of fan
"/Fan/Speed" : return Frequency value or PWM value 
"/Fan/Gear"   : RElay state
"/Fan/SetFrequency" : listen the frequency / pwm value
"/Fan/SetBrightness" :listen the Tm1637
"/Fan/SetGear" : set the gear on/off
"/Fan/Temperature" : return temp 
Pwm : 0-400 ( corresponding to 0-100%)
Gear : ON,OFF
frequency: 0-400 ( I am using Nidec M460 motor)
bool isPWM = false; // change this if you want to use pwm 
## License



## Acknowledgments

- [TM1637Display library](https://github.com/avishorp/TM1637)
- [OneWire library](https://github.com/PaulStoffregen/OneWire)
- [DallasTemperature library](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [PubSubClient library](https://github.com/knolleary/pubsubclient)

Feel free to contribute or report issues!
