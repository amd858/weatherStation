#include "DHTesp.h"
#include <Wire.h>
#include "DFRobot_SHT20.h"
#include <SPI.h>


#define temp_up_rc_button 17
#define temp_down_rc_button 5
#define ac_on_off_rc_button 16
#define ir_pin 4
#define dip_switch_c 6
#define dip_switch_b 7
#define dip_switch_a 8
#define humidifier_indication 14
#define ac_disabled_pin A6

DHTesp dht;
DFRobot_SHT20 sht20;

const int KEY_UP_TIME = 250;
const int KEY_DOWN_TIME = 50;
const int TEMP_UP_STEPS = 0;
const int TEMP_DOWN_STEPS = 16;
const float HC_MAX_TEMP = 21.5;
const float HC_MIN_TEMP = 21.4;
const float ABOVE_ZERO_LEVEL_HUMD = 33;
const float ZERO_LEVEL_HUMD = 30;
const float MIN_HUMD = 45;
const float MAX_HUMD = 48;
const int MAX_BOUNCE_COUNT = 2;

int bounce_count = 0;
float Dip_Switch_Offset() {

  int a_button = !digitalRead(dip_switch_a); // 0.5
  int b_button = !digitalRead(dip_switch_b); // 1.0
  int minus_button = !digitalRead(dip_switch_c);

  float result = (a_button * 0.5 + b_button * 1);
  if (minus_button)
  {
    result = result * -1;
  }
  return result;
}
enum AcStates {
  HEATER_OFF,             // AC is off
  BOUNCING_FOR_HEATER_ON, // Temperature getting High
  HEATER_ON,
  BOUNCING_FOR_HEATER_OFF, // Temperature Getting Low
  HEATER_DISABLING,
  HEATER_DISABLED,
  TEMPERATURE_SENSOR_FAULT
};
unsigned char ac_state = HEATER_OFF;
void reset_the_Data() {
  radio_data.ch1 = 0;
  radio_data.ch2 = 0;
  radio_data.ch3 = 0;
}
void press_rc_button(int rc_button_pin, bool enable_led) {
  if (enable_led) {
    digitalWrite(ir_pin, HIGH);
    Serial.println("ir on");
    delay(10);
  }
  digitalWrite(rc_button_pin, HIGH);
  delay(KEY_DOWN_TIME);
  digitalWrite(rc_button_pin, LOW);
  delay(KEY_UP_TIME);
  if (enable_led) {
    digitalWrite(ir_pin, LOW);
  }
}
void ac_on_sensor_fault() {

  for (int i = 0; i < 16; i++)
  {
    press_rc_button(temp_down_rc_button, false);
  }

  for (int i = 0; i < 1; i++)
  {
    press_rc_button(temp_up_rc_button, false);
  }
  press_rc_button(temp_up_rc_button, true);
}
void ac_heating_enable() {
  for (int i = 0; i < TEMP_DOWN_STEPS; i++) // steps 16
  {
    press_rc_button(temp_down_rc_button, false);
  }
  for (int i = 0; i < TEMP_UP_STEPS; i++) //steps = 5
  {
    press_rc_button(temp_up_rc_button, false);
  }
  press_rc_button(temp_up_rc_button, true);
}
void ac_heating_disable() {
  for (int i = 0; i < TEMP_DOWN_STEPS; i++) // steps 16
  {
    press_rc_button(temp_down_rc_button, false);
  }
  press_rc_button(temp_down_rc_button, true);
}
void ac_power_toggle() {
  press_rc_button(ac_on_off_rc_button, true);
}
void humidifier_on() {
  digitalWrite(humidifier_indication, HIGH);
}
void humidifier_off() {
  digitalWrite(humidifier_indication, LOW);
}
void setup() {
  Serial.begin(115200);
  printf_begin();
  sht20.initSHT20(); // Init SHT20 Sensor
  delay(300);
  sht20.checkSHT20(); // Check SHT20 Sensor
  pinMode(temp_up_rc_button, OUTPUT);
  pinMode(temp_down_rc_button, OUTPUT);
  pinMode(ac_on_off_rc_button, OUTPUT);
  pinMode(humidifier_indication, OUTPUT);
  pinMode(ir_pin, OUTPUT);
  pinMode(dip_switch_c, INPUT);
  pinMode(dip_switch_b, INPUT);
  pinMode(dip_switch_a, INPUT);
  pinMode(ac_disabled_pin, INPUT);
  tm.init();
  Serial.println("welcome");
  tm.setBrightness(7);
  ac_heating_disable();
}
unsigned char radio_processing(float room_temp, float room_humd, float room_hidx, unsigned char radio_states, int humidifier_state) {

  switch (radio_states)

  {

    case RADIO_ON:
      Serial.println("RADIO_SET_FOR_RX");
      radio.begin();
      radio.setAutoAck(true);
      radio.setDataRate(RF24_250KBPS);
      radio.openReadingPipe(0, MY_RADIO_PIPE_FOR_READING);
      radio.openWritingPipe(MY_RADIO_PIPE_FOR_WRITING);
      radio.startListening(); //We start the radio comunication
      radio.printDetails();
      radio_states = RADIO_READY_FOR_RECEVING;
      break;

    case RADIO_READY_FOR_SENDING:

      Serial.println("RADIO_READY_FOR_SENDING");
      radio.stopListening();
      radio_humd.ch1 = humidifier_state;
      radio.write(&radio_humd, sizeof(Radio_Humd));
      radio_states = RADIO_READY_FOR_RECEVING;

      break;

    case RADIO_READY_FOR_RECEVING:

      radio.startListening();

      Serial.println("RADIO_READY_FOR_RECEVING");

      while (radio.available())

      {
        radio.read(&radio_data, sizeof(Radio_Data));
        Serial.println("RADIO READING");
        lastRecvTime = millis(); //Here we receive the data
      }

      delay(100);

      unsigned long now = millis();
      if (now - lastRecvTime > 20000)
      {

        reset_the_Data();
      }

      outside_temp = radio_data.ch1;    //room_temp
      outside_humd = radio_data.ch2;    //room_humd
      outside_battery = radio_data.ch3; // battery
      radio_states = RADIO_READY_FOR_SENDING;

      break;
  }

  return radio_states;
}
void ac_processing(float min_temp, float max_temp, float room_temp, float room_humd, bool reading_error) {
  if (reading_error)
  {
    if (ac_state != HEATER_DISABLED && ac_state != HEATER_DISABLING)

    {
      ac_state = TEMPERATURE_SENSOR_FAULT;
    }
  }

  switch (ac_state)
  {
    case HEATER_OFF:
      Serial.println("HEATER_OFF");
      fresh_start = 0;
      if (room_temp < min_temp)
      {
        ac_state = BOUNCING_FOR_HEATER_ON;
        bounce_count = 0;
      }

      break;
    case BOUNCING_FOR_HEATER_ON:
      Serial.println("BOUNCING_FOR_HEATER_ON");
      if ((room_temp < min_temp))
      {
        bounce_count++;
        if (bounce_count > MAX_BOUNCE_COUNT)
        {
          ac_state = HEATER_ON;
          // turn on AC, turn on LED
          ac_heating_enable();
        }
      }
      else
      {
        Serial.println("False alarm");
        bounce_count = 0;
        ac_state = HEATER_OFF;
      }
      break;
    case HEATER_ON:
      Serial.println("HEATER_ON");
      if (room_temp > max_temp)
      {
        ac_state = BOUNCING_FOR_HEATER_OFF;
        bounce_count = 0;
      }
      break;
    case BOUNCING_FOR_HEATER_OFF:
      Serial.println("BOUNCING_FOR_HEATER_OFF");
      if (room_temp > max_temp)
      {
        bounce_count++;
        if (bounce_count > MAX_BOUNCE_COUNT)
        {
          ac_state = HEATER_OFF;
          // turn off AC, turn OFF LED
          ac_heating_disable();
        }
      }
      else
      {
        Serial.println("False alarm");
        bounce_count = 0;
        ac_state = HEATER_ON;
      }
      break;
    case HEATER_DISABLING:
      Serial.println("HEATER_DISABLING");
      if (fresh_start)
      {

        fresh_start = 0;
      }
      else
      {
        ac_heating_disable();
        ac_power_toggle();
      }

      ac_state = HEATER_DISABLED;

      break;
    case HEATER_DISABLED:
      Serial.println("HEATER_DISABLED");
      if (analogRead(ac_disabled_pin) < 100)
      {

        ac_power_toggle();
        ac_state = HEATER_OFF;
        Serial.println("HEATER_DISABLED BUTTON RELEASED");
      }
      break;
    case TEMPERATURE_SENSOR_FAULT:

      ac_on_sensor_fault();
      if (!reading_error)
      {
        ac_state = HEATER_OFF;
      }
      break;
  }
}
unsigned char humidifier_processing(float room_humd, unsigned char humidifier_state) {

  switch (humidifier_state)
  {
    case HUMIDIFIER_OFF:
      Serial.println("HUMIDIFIER_OFF");
      if (room_humd < MIN_HUMD)
      {
        humidifier_state = HUMIDIFIER_ON;
        humidifier_on();
      }
      break;
    case HUMIDIFIER_ON:
      Serial.println("HUMIDIFIER_ON");
      if (room_humd > MAX_HUMD)
      {
        humidifier_off();
        humidifier_state = HUMIDIFIER_OFF;
      }
      break;
  }
  return humidifier_state;
}
unsigned char display_processing(unsigned char display_state, String stringTemperature, String stringHumidity, String stringHidx, String stringOutdoorTemperature, String stringOutdoorHumidity, String stringOutdoorHidx, int display_select, bool reading_error) {
  if (reading_error)
  {

    stringTemperature = "t---";
    stringHumidity = "h---";
    stringHidx = "_---";
  }

  switch (display_state)
  {

    case DISPLAY_ON:

      Serial.println("DISPLAY_ON");
      if (display_select == 0)
      {
        display_state = DISPLAY_TEMPERATURE;
      }
      else if (display_select == 1)
      {
        display_state = DISPLAY_HUMD;
      }
      else if (display_select == 2)
      {
        display_state = DISPLAY_HIX;
      }
      else if (display_select == 3)
      {
        display_state = DISPLAY_OUTDOOR_TEMPERATURE;
      }
      else if (display_select == 4)
      {
        display_state = DISPLAY_OUTDOOR_HUMD;
      }
      else if (display_select == 5)
      {
        display_state = DISPLAY_OUTDOOR_HIX;
      }

      break;

    case DISPLAY_TEMPERATURE:
      tm.display(stringTemperature);
      display_state = DISPLAY_ON;
      break;

    case DISPLAY_HUMD:
      tm.display(stringHumidity);
      display_state = DISPLAY_ON;
      break;
    case DISPLAY_HIX:
      tm.display(stringHidx);
      display_state = DISPLAY_ON;
      break;
    case DISPLAY_OUTDOOR_TEMPERATURE:
      tm.display(stringOutdoorTemperature);
      display_state = DISPLAY_ON;
      break;
    case DISPLAY_OUTDOOR_HUMD:
      tm.display(stringOutdoorHumidity);
      display_state = DISPLAY_ON;
      break;
    case DISPLAY_OUTDOOR_HIX:
      tm.display(stringOutdoorHidx);
      display_state = DISPLAY_ON;
      break;
  }
  return display_state;
}
void loop() {
  static float room_humd = 0;
  static float room_temp = 0;
  static float room_hidx = 0;
  float outside_offset = 0;
  float max_temp = 0;
  float min_temp = 0;
  float radio_hidx = 0;
  float dip_switch_offset = 0;
  static unsigned char humidifier_state = HUMIDIFIER_OFF;
  static unsigned char radio_states = RADIO_ON;
  static unsigned char display_state = DISPLAY_ON;
  static unsigned long previousMillis = 0;
  // static unsigned long previousMillisUpdateData = 0;
  const long DISPLAY_INTERVAL = 2500;
  unsigned long currentMillis = millis();
  static int display_selector = 0;
  static bool reading_error = false;
  static bool OutDoorRadioActive = false;
  int displayChangerCount = 0;
  room_temp = sht20.readTemperature(); // Read Temperature
  room_humd = sht20.readHumidity();    // Read Humidity
  if ((room_temp > 10 && room_temp < 60) && (room_humd > 20 && room_humd < 80)) {
    reading_error = false;
  }
  else {
    reading_error = true;
  }
  if (outside_temp > 5)
  {
    radio_hidx = dht.computeHeatIndex(outside_temp, outside_humd, false);
    Serial.println("_________________OUT DOOR________________"); Serial.print("Time:"); Serial.print(millis()); Serial.print(" T:"); Serial.print(outside_temp, 1); Serial.print("C"); Serial.print(" H:"); Serial.print(outside_humd, 1); Serial.print("%"); Serial.print(" I:"); Serial.print(radio_hidx, 1); Serial.print("    battery voltage = "); Serial.println(outside_battery);
    OutDoorRadioActive = true;
  }
  else
  {
    OutDoorRadioActive = false;
  }

  if (radio_hidx > 5)
  {
    Serial.println("out door data processing");
    if ((radio_hidx - room_hidx) > 10) {
      Serial.println("out door +10");
      Serial.println("outside_offset temprature high");
      outside_offset = radio_hidx - room_hidx - 10;
    }
    else if ((radio_hidx - room_hidx) <= -2) {
      Serial.println("out door -2");
      outside_offset = radio_hidx - room_hidx - 1;
      Serial.println("outside_offset tempreture low");
      Serial.print("outside_offset = ");
      Serial.println(outside_offset);
    }
  }
  else {
    Serial.println("out door sensor out of range");
  }

  dip_switch_offset = Dip_Switch_Offset();
  //  max_hidx = HC_MAX_TEMP + dip_switch_offset + outside_offset;
  //  min_hidx = HC_MIN_TEMP + dip_switch_offset + outside_offset;
  max_temp = HC_MAX_TEMP + dip_switch_offset;
  min_temp = HC_MIN_TEMP + dip_switch_offset;
  Serial.println("_________________IN DOOR________________"); Serial.print("Time:"); Serial.print(millis()); Serial.print(" T:"); Serial.print(room_temp, 1); Serial.print("C"); Serial.print(" H:"); Serial.print(room_humd, 1); Serial.print("%"); Serial.print(" I:"); Serial.print(room_hidx, 1); Serial.println("c"); Serial.print("target hidx max = "); Serial.print(max_temp); Serial.print("    target hidx min = "); Serial.println(min_temp);

  if (analogRead(ac_disabled_pin) > 900 && ac_state != HEATER_DISABLED)
  {
    ac_state = HEATER_DISABLING;
  }

  String stringTemperature = 't' + String(int(room_temp * 10));
  String stringHumidity = 'h' + String(int(room_humd * 10));
  String stringHidx = '_' + String(int(((min_temp+max_temp)/2 ) * 10));
  String stringOutdoorTemperature = 'L' + String(int(outside_temp * 10));
  String stringOutdoorHumidity = 'H' + String(int(outside_humd * 10));
  String stringOutdoorHidx = '=' + String(int(radio_hidx * 10));

  ac_processing(min_temp, max_temp, room_temp, room_humd, reading_error);
  humidifier_state = humidifier_processing(room_humd, humidifier_state);
  radio_states = radio_processing(room_temp, room_humd, room_hidx, radio_states, humidifier_state);
  display_state = display_processing(display_state, stringTemperature , stringHumidity, stringHidx, stringOutdoorTemperature, stringOutdoorHumidity, stringOutdoorHidx, display_selector, reading_error);
  if (OutDoorRadioActive)  {
    displayChangerCount = 6;
  }
  else  {
    displayChangerCount = 3;
  }

  if (currentMillis - previousMillis >= DISPLAY_INTERVAL)  {
    previousMillis = currentMillis;
    display_selector++;
    display_selector = display_selector % displayChangerCount;
  }
  delay(1000);
}
