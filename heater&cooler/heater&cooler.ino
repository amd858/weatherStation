#include <DHTesp.h>
#include <Wire.h>
#include "DFRobot_SHT20.h"
#include <LiquidCrystal_I2C.h>
#include <printf.h>
#include <SPI.h>

#define temp_up_rc_button 17
#define temp_down_rc_button 5
#define ac_on_off_rc_button 16
#define ir_pin 4
#define dip_switch_d 3
#define dip_switch_c 6
#define dip_switch_b 7
#define dip_switch_a 8
#define HEATER_MODE 9
#define humidifier_indication 14
#define ac_disabled_pin 11

DHTesp dht;
DFRobot_SHT20 sht20;
LiquidCrystal_I2C lcd(0x27, 16, 4);
const int KEY_UP_TIME = 250;
const int KEY_DOWN_TIME = 50;
const int TEMP_UP_STEPS = 16;
const int temp_down_steps = 4;

const float HC_MAX_HIDEX = 27.2;
const float HC_MIN_HIDEX = 26.8;
const float ABOVE_ZERO_LEVEL_HUMD = 33;
const float ZERO_LEVEL_HUMD = 30;
const float MIN_HUMD = 45;
const float MAX_HUMD = 48;
const int MAX_BOUNCE_COUNT = 6;
const long REPEAT_ACTION_INTERVAL = 600000;
bool HEATER_MODE_SELECTOR;
unsigned long repeatActionOldTime = 0;

int bounce_count = 0;

float Dip_Switch_Offset()
{
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

enum AcStates
{
  AC_OFF,             // AC is off
  BOUNCING_FOR_AC_ON, // Temperature getting High
  AC_ON,
  BOUNCING_FOR_AC_OFF, // Temperature Getting Low
  AC_DISABLING,
  BOUNCING_FOR_AC_DISABLE,
  AC_DISABLED,
  BOUNCING_FOR_AC_ENABLE,
  COOL_TEMPERATURE_SENSOR_FAULT,
  HEAT_MODE
};

enum HeaterStates
{
  HEATING_DISABLE,             // AC is off
  BOUNCING_FOR_HEATING_ENABLE, // Temperature getting High
  HEATING_ENABLE,
  BOUNCING_FOR_HEATING_DISABLE, // Temperature Getting Low
  BOUNCING_FOR_HEATER_OFF,
  HEATER_OFF,
  BOUNCING_FOR_HEATER_ON,
  HEATER_TEMPERATURE_SENSOR_FAULT,
  COOL_MODE
};

unsigned char heater_state = HEATING_DISABLE;
unsigned char ac_state = AC_OFF;

String ac_state_lcd()
{
  String result = "";
  static int repeatConter = 0;
  switch (ac_state)
  {
  case AC_OFF:
    result = "AC_OFF      " + repeat_action_remaining_time_string_get();
    repeatConter = 0;
    break;
  case BOUNCING_FOR_AC_ON:
    if (repeatConter % 2)
      result = "BOUNCE_FOR_AC_ON";
    else
      result = "    BOUNCE      ";
    break;
  case AC_ON:
    result = "AC_ON       " + repeat_action_remaining_time_string_get();
    repeatConter = 0;
    break;
  case BOUNCING_FOR_AC_OFF:
    if (repeatConter % 2)
      result = "BOUNC_FOR_AC_OFF";
    else
      result = "    BOUNCE      ";
    break;
  case AC_DISABLING:
    result = "  AC_DISABLING  ";
    break;
  case BOUNCING_FOR_AC_DISABLE:
    result = "  BOUNC_FOR_DISABL  ";
    break;
  case AC_DISABLED:
    result = "  AC_DISABLED   ";
    break;
  case BOUNCING_FOR_AC_ENABLE:
    result = "BOUN_FO_AC_ENABL";
    break;
  case COOL_TEMPERATURE_SENSOR_FAULT:
    result = " !SENSOR_FAULT! ";
    break;
  }
  repeatConter++;
  return result;
}

void press_rc_button(int rc_button_pin, bool enable_led) // remoteKey
{
  if (enable_led)
  {
    digitalWrite(ir_pin, HIGH);
    Serial.println("ir on");
    delay(10);
  }
  digitalWrite(rc_button_pin, HIGH);
  delay(KEY_DOWN_TIME);
  digitalWrite(rc_button_pin, LOW);
  delay(KEY_UP_TIME);
  if (enable_led)
  {
    digitalWrite(ir_pin, LOW);
  }
}

void ac_on_sensor_fault()
{

  for (int i = 0; i < 30; i++)
  {
    press_rc_button(temp_up_rc_button, false);
  }

  for (int i = 0; i < 2; i++)
  {
    press_rc_button(temp_down_rc_button, false);
  }
  press_rc_button(temp_down_rc_button, true);
}
void heater_on_sensor_fault()
{

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

void heating_enable()
{
  for (int i = 0; i < 16; i++)
  {
    press_rc_button(temp_down_rc_button, false);
  }
  for (int i = 0; i < 3; i++)
  {
    press_rc_button(temp_up_rc_button, false);
  }
  press_rc_button(temp_up_rc_button, true);
}
void heating_disable()
{
  for (int i = 0; i < 16; i++) // steps 16
  {
    press_rc_button(temp_down_rc_button, false);
  }
  press_rc_button(temp_down_rc_button, true);
}

void ac_ir_command_repeat()
{

  press_rc_button(temp_up_rc_button, false);
  press_rc_button(temp_down_rc_button, true);
}

float Heater_Dip_Switch_Offset()
{
  int a_button = !digitalRead(dip_switch_a); // 0.5
  int b_button = !digitalRead(dip_switch_b); // 1.0
  int c_button = !digitalRead(dip_switch_c); // 2.0
  int minus_button = !digitalRead(dip_switch_d);

  float result = (a_button * 0.5 + b_button * 1 + c_button * 2);
  if (minus_button)
  {
    result = result * -1;
  }
  return result;
}

void ac_cooling_enable()
{
  for (int i = 0; i < TEMP_UP_STEPS; i++) // steps = 16
  {
    press_rc_button(temp_up_rc_button, false);
  }

  for (int i = 0; i < temp_down_steps; i++) // steps 5
  {
    press_rc_button(temp_down_rc_button, false);
  }
  press_rc_button(temp_down_rc_button, true);
}

void ac_cooling_disable()
{
  for (int i = 0; i < TEMP_UP_STEPS; i++) // step =16
  {
    press_rc_button(temp_up_rc_button, false);
  }
  press_rc_button(temp_up_rc_button, true);
}

void ac_power_toggle()
{
  press_rc_button(ac_on_off_rc_button, true);
}

void humidifier_on()
{
  digitalWrite(humidifier_indication, HIGH);
}

void humidifier_off()
{
  digitalWrite(humidifier_indication, LOW);
}
String repeat_action_remaining_time_string_get()
{
  int repeatActionRemainingTimeInSeconds = (REPEAT_ACTION_INTERVAL - (millis() - repeatActionOldTime)) / 1000;
  int minutes = repeatActionRemainingTimeInSeconds / 60;
  int seconds = repeatActionRemainingTimeInSeconds % 60;

  return String(minutes) + ":" + String(seconds < 10 ? "0" : "") + String(seconds);
}
void setup()
{
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
  pinMode(dip_switch_d, INPUT);
  pinMode(dip_switch_c, INPUT);
  pinMode(dip_switch_b, INPUT);
  pinMode(dip_switch_a, INPUT);
  pinMode(HEATER_MODE, INPUT);
  pinMode(ac_disabled_pin, INPUT);
  lcd.init();
  lcd.print("     WELCOME    ");
  Serial.println("     WELCOME    ");
  Serial.println(!digitalRead(ac_disabled_pin));
  HEATER_MODE_SELECTOR = digitalRead(HEATER_MODE);
  lcd.setCursor(0, 1);
  if (!digitalRead(ac_disabled_pin))
  {
    lcd.print("bisable");
    Serial.println("     bisable    ");

    ac_state = AC_DISABLED;
    heater_state = HEATER_OFF;
  }
  else
  {
    lcd.print("enable");
    Serial.println("     enable    ");
    lcd.setCursor(0, 2);
    HEATER_MODE_SELECTOR = digitalRead(HEATER_MODE);
    Serial.print("     HEATER_MODE_SELECTOR=");
    Serial.println(HEATER_MODE_SELECTOR);
    if (HEATER_MODE_SELECTOR)
    {
      heating_disable();
      lcd.print("Heater");
      Serial.println("     Heater    ");
    }
    else
    {
      ac_cooling_disable();
      Serial.println("     Cooler    ");

      lcd.print("Cooler");
    }
    lcd.setCursor(0, 1);
    lcd.print("active");
  }

  lcd.clear();
  delay(1000);
}

void heater_processing(float min_temp, float max_temp, float room_temp, bool reading_error, bool ac_on_button_on_box)

{
  static String lcd_message = "";
  static int lcd_location = 0;

  lcd_location = 0;
  lcd.setCursor(lcd_location, 0);

  if (reading_error)
  {
    if (heater_state != HEATER_OFF)
    {
      ac_state = HEATER_TEMPERATURE_SENSOR_FAULT;
    }
  }
  switch (heater_state)
  {
  case HEATING_DISABLE:
    Serial.println("HEATING_DISABLE");
    if (!ac_on_button_on_box)
    {
      lcd_location = 0;

      lcd_message = "ON           OFF";
      heater_state = BOUNCING_FOR_HEATER_OFF;
    }
    else
    {
      if (room_temp < min_temp)
      {
        lcd_location = 0;

        lcd_message = "Dis           En";
        heater_state = BOUNCING_FOR_HEATING_ENABLE;
        bounce_count = 0;
      }
      else
      {
        lcd_message = ("Heating OFF " + repeat_action_remaining_time_string_get());
        if (millis() - repeatActionOldTime >= REPEAT_ACTION_INTERVAL)
        {
          ac_ir_command_repeat();
          repeatActionOldTime = millis();
        }
      }
    }
    break;
  case BOUNCING_FOR_HEATING_ENABLE:
    Serial.println("BOUNCING_FOR_HEATING_ENABLE");

    if (!ac_on_button_on_box)
    {
      lcd_location = 0;

      lcd_message = "ON           OFF";
      heater_state = BOUNCING_FOR_HEATER_OFF;
    }
    else
    {
      if (room_temp < min_temp)
      {
        bounce_count++;

        lcd_location = 3 + bounce_count;
        lcd_message = ">";

        if (bounce_count > MAX_BOUNCE_COUNT)
        {
          lcd_location = 0;

          lcd_message = "                ";
          heater_state = HEATING_ENABLE;
          heating_enable();
          Serial.println("heating_enable");
          repeatActionOldTime = millis();
        }
      }
      else
      {
        Serial.println("False alarm");
        bounce_count = 0;
        heater_state = HEATING_DISABLE;
      }
    }
    break;
  case HEATING_ENABLE:
    Serial.println("HEATING_ENABLE");
    if (!ac_on_button_on_box)
    {
      lcd_location = 0;

      lcd_message = "ON           OFF";
      heater_state = BOUNCING_FOR_HEATER_OFF;
    }
    else
    {

      if (room_temp > max_temp)
      {
        lcd_location = 0;

        lcd_message = "En           Dis";
        heater_state = BOUNCING_FOR_HEATING_DISABLE;
        bounce_count = 0;
      }
      else
      {

        lcd_message = ("Heating  ON " + repeat_action_remaining_time_string_get());
        if (millis() - repeatActionOldTime >= REPEAT_ACTION_INTERVAL)
        {
          ac_ir_command_repeat();
          repeatActionOldTime = millis();
        }
      }
    }
    break;
  case BOUNCING_FOR_HEATING_DISABLE:
    Serial.println("BOUNCING_FOR_HEATING_DISABLE");

    if (!ac_on_button_on_box)
    {
      lcd_location = 0;

      lcd_message = "ON           OFF";
      heater_state = BOUNCING_FOR_HEATER_OFF;
    }
    else
    {

      if (room_temp > max_temp)
      {
        bounce_count++;
        lcd_location = 3 + bounce_count;
        lcd_message = ">";
        if (bounce_count > MAX_BOUNCE_COUNT)
        {
          lcd_location = 0;

          lcd_message = "                ";
          heater_state = HEATING_DISABLE;

          repeatActionOldTime = millis();
          // turn off AC, turn OFF LED
          heating_disable();
          Serial.println("heating_disable");
        }
      }
      else
      {
        Serial.println("False alarm");
        bounce_count = 0;
        heater_state = HEATING_ENABLE;
      }
    }
    break;

    ///////////////////////////////

  case BOUNCING_FOR_HEATER_OFF:
    Serial.println("BOUNCING_FOR_HEATER_OFF");
    bounce_count++;

    if (!ac_on_button_on_box)
    {
      lcd_location = 3 + bounce_count;
      lcd_message = ">";

      if (bounce_count > MAX_BOUNCE_COUNT)
      {
        lcd_location = 0;
        lcd_message = "       OFF      ";
        heating_disable();
        ac_power_toggle();
        bounce_count = 0;
        heater_state = HEATER_OFF;
        Serial.println("HEATER_OFF");
      }
    }
    else
    {
      Serial.println("False alarm");
      bounce_count = 0;
      heater_state = HEATING_DISABLE;
    }
    break;

  case HEATER_OFF:
    lcd_location = 0;

    lcd_message = "       OFF      ";

    Serial.println("HEATER_OFF");

    if (ac_on_button_on_box)
    {
      lcd_location = 0;
      lcd_message = "OFF           ON";
      heater_state = BOUNCING_FOR_HEATER_ON;
      bounce_count = 0;
    }
    break;
    //////////////////////////

  case BOUNCING_FOR_HEATER_ON:
    Serial.println("BOUNCING_FOR_HEATER_ON");
    bounce_count++;
    if (ac_on_button_on_box)
    {
      lcd_location = 3 + bounce_count;
      lcd_message = ">";
      if (bounce_count > MAX_BOUNCE_COUNT)
      {
        lcd_location = 0;
        lcd_message = "       ON       ";
        ac_power_toggle();
        Serial.println("AC_DISABLED BUTTON RELEASED");
        heater_state = HEATING_DISABLE;
      }
    }
    else
    {
      Serial.println("False alarm");
      bounce_count = 0;
      heater_state = HEATER_OFF;
    }
    break;

  case HEATER_TEMPERATURE_SENSOR_FAULT:
    Serial.println("HEATER_TEMPERATURE_SENSOR_FAULT");
    ac_on_sensor_fault();
    if (!reading_error)
    {
      lcd_message = "  SENSOR_FAULT  ";
      heater_state = HEATING_DISABLE;
    }
    break;
  case COOL_MODE:

    break;
  }
  if (lcd_message != "")
  {
    lcd.setCursor(lcd_location, 0);
    lcd.print(lcd_message);
  }
}

void ac_processing(float min_hidx, float max_hidx, float room_temp, float room_humd, float room_hidx, bool reading_error, bool ac_on_button_on_box)
{

  if (reading_error)
  {
    if (ac_state != AC_DISABLED && ac_state != AC_DISABLING)
    {
      ac_state = COOL_TEMPERATURE_SENSOR_FAULT;
    }
  }

  switch (ac_state)
  {
  case AC_OFF:
    Serial.println("AC_OFF");

    if (!ac_on_button_on_box)
    {
      ac_state = AC_DISABLING;
    }
    else
    {
      if (room_hidx > max_hidx && room_humd > ABOVE_ZERO_LEVEL_HUMD)
      {
        ac_state = BOUNCING_FOR_AC_ON;
        bounce_count = 0;
      }
      else
      {
        ac_state = AC_OFF;
        Serial.println((REPEAT_ACTION_INTERVAL - (millis() - repeatActionOldTime)) / 1000);
        if (millis() - repeatActionOldTime >= REPEAT_ACTION_INTERVAL)
        {
          ac_ir_command_repeat();
          repeatActionOldTime = millis();
        }
      }
    }
    break;
  case BOUNCING_FOR_AC_ON:
    Serial.println("BOUNCING_FOR_AC_ON");

    if (ac_on_button_on_box == false)
    {
      ac_state = AC_DISABLING;
    }
    else
    {

      if (room_hidx > max_hidx && room_humd > ABOVE_ZERO_LEVEL_HUMD)
      {
        bounce_count++;
        if (bounce_count > MAX_BOUNCE_COUNT)
        {
          ac_state = AC_ON;
          // turn on AC, turn on LED
          ac_cooling_enable();
          repeatActionOldTime = millis();
        }
      }
      else
      {
        Serial.println("False alarm");
        bounce_count = 0;
        ac_state = AC_OFF;
      }
    }
    break;
  case AC_ON:
    Serial.println("AC_ON");
    if (!ac_on_button_on_box)
    {
      ac_state = AC_DISABLING;
    }
    else
    {

      if (room_hidx < min_hidx || room_humd < ZERO_LEVEL_HUMD)
      {
        ac_state = BOUNCING_FOR_AC_OFF;
        bounce_count = 0;
      }
      else
      {
        ac_state = AC_ON;

        // Serial.println((millis() - repeatActionOldTime) / 1000);
        if (millis() - repeatActionOldTime >= REPEAT_ACTION_INTERVAL)
        {
          ac_ir_command_repeat();
          repeatActionOldTime = millis();
        }
      }
    }
    break;
  case BOUNCING_FOR_AC_OFF:
    Serial.println("BOUNCING_FOR_AC_OFF");

    if (!ac_on_button_on_box)
    {
      ac_state = AC_DISABLING;
    }
    else
    {

      if (room_hidx < min_hidx || room_humd < ZERO_LEVEL_HUMD)
      {
        bounce_count++;
        if (bounce_count > MAX_BOUNCE_COUNT)
        {
          ac_state = AC_OFF;

          repeatActionOldTime = millis();
          // turn off AC, turn OFF LED
          ac_cooling_disable();
        }
      }
      else
      {
        Serial.println("False alarm");
        bounce_count = 0;
        ac_state = AC_ON;
      }
    }
    break;

    ///////////////////////////////

  case AC_DISABLING:
    Serial.println("AC_DISABLING");
    bounce_count = 0;
    ac_state = BOUNCING_FOR_AC_DISABLE;
    break;

  case BOUNCING_FOR_AC_DISABLE:
    Serial.println("BOUNCING_FOR_AC_DISABLE");
    bounce_count++;
    if (!ac_on_button_on_box)
    {
      if (bounce_count > MAX_BOUNCE_COUNT)
      {
        ac_cooling_disable();
        ac_power_toggle();
        ac_state = AC_DISABLED;
      }
    }
    else
    {
      Serial.println("False alarm");
      bounce_count = 0;
      ac_state = AC_OFF;
    }
    break;

  case AC_DISABLED:
    if (ac_on_button_on_box)
    {
      ac_state = BOUNCING_FOR_AC_ENABLE;
      bounce_count = 0;
    }
    break;
    //////////////////////////

  case BOUNCING_FOR_AC_ENABLE:
    Serial.println("BOUNCING_FOR_AC_ENABLE");
    bounce_count++;
    if (ac_on_button_on_box)
    {
      if (bounce_count > MAX_BOUNCE_COUNT)
      {
        ac_power_toggle();
        Serial.println("AC_DISABLED BUTTON RELEASED");
        ac_state = AC_OFF;
      }
    }
    else
    {
      Serial.println("False alarm");
      bounce_count = 0;
      ac_state = AC_DISABLED;
    }
    break;

  case COOL_TEMPERATURE_SENSOR_FAULT:
    heater_on_sensor_fault();
    if (!reading_error)
    {
      ac_state = AC_OFF;
    }
    break;
  }
}

void loop()
{

  static float room_humd = 0;
  static float room_temp = 0;
  static float room_hidx = 0;
  static float max_hidx = 0;
  static float min_hidx = 0;
  static float dip_switch_offset = 0;
  static bool reading_error = false;
  room_temp = sht20.readTemperature(); // Read Temperature
  room_humd = sht20.readHumidity();    // Read Humidity
  if ((room_temp > 10 && room_temp < 60) && (room_humd > 20 && room_humd < 80))
  {
    room_hidx = dht.computeHeatIndex(room_temp, room_humd, false);
    if (room_hidx >= 10 && room_hidx <= 70)
    {
      reading_error = false;
    }
    else
    {
      reading_error = true;
    }
  }
  else
  {
    reading_error = true;
  }

  bool ac_on_button = !digitalRead(ac_disabled_pin);

  if (HEATER_MODE_SELECTOR)
  {
    Serial.print("HEATER_MODE_SELECTOR=");
    Serial.println(HEATER_MODE_SELECTOR);

    dip_switch_offset = Heater_Dip_Switch_Offset();
    float min_temp = 19.8 + dip_switch_offset;
    float max_temp = 20.2 + dip_switch_offset;
    heater_processing(min_temp, max_temp, room_temp, reading_error, ac_on_button);
    lcd.setCursor(0, 1);
    lcd.print("T" + String(room_temp, 1) + " H" + String(room_humd, 0) + "%");
    lcd.setCursor(16, 0);
    lcd.print("S" + String(max_temp - 0.2, 1));
    lcd.setCursor(9, 2);
    lcd.print(String(dip_switch_offset, 1));
    lcd.setCursor(0, 3);
    if (ac_on_button)
    {
      lcd.print("UNIT On    ");
    }
    else
    {
      lcd.print("UNIT Off   ");
    }
  }

  else
  {
    Serial.print("HEATER_MODE_SELECTOR=");
    Serial.println(HEATER_MODE_SELECTOR);
    dip_switch_offset = Dip_Switch_Offset();
    max_hidx = HC_MAX_HIDEX + dip_switch_offset;
    min_hidx = HC_MIN_HIDEX + dip_switch_offset;
    ac_processing(min_hidx, max_hidx, room_temp, room_humd, room_hidx, reading_error, ac_on_button);
    float disier_hidx = max_hidx - 0.2;
    lcd.setCursor(0, 0);
    lcd.print(ac_state_lcd());
    lcd.setCursor(0, 1);
    lcd.print("T" + String(room_temp, 1) + " H" + String(room_humd, 0) + "%" + " #" + String(room_hidx, 1) + "");
    lcd.setCursor(14, 0);
    lcd.print("=");
    lcd.setCursor(0, 2);
    lcd.print(String(disier_hidx, 1));
    lcd.setCursor(0, 3);
    if (ac_on_button)
    {
      lcd.print("UNIT On    ");
    }
    else
    {
      lcd.print("UNIT Off   ");
    }
  }
  delay(1000);
}
