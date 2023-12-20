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
#define humidifier_indication 14
#define ac_disabled_pin A6
 
DHTesp dht;
DFRobot_SHT20 sht20;
LiquidCrystal_I2C lcd(0x27, 16, 4);
const int KEY_UP_TIME = 250;
const int KEY_DOWN_TIME = 50;
const int TEMP_UP_STEPS = 16;
int temp_down_steps = 4;

const float HC_MAX_HIDEX = 27.2;
const float HC_MIN_HIDEX = 26.8;
const float ABOVE_ZERO_LEVEL_HUMD = 33;
const float ZERO_LEVEL_HUMD = 30;
const float MIN_HUMD = 45;
const float MAX_HUMD = 48;
const int MAX_BOUNCE_COUNT = 6;
const long REPEAT_ACTION_INTERVAL = 600000;

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
  TEMPERATURE_SENSOR_FAULT
};

unsigned char ac_state = AC_OFF;

String ac_state_lcd()
{
  String result = "";
  static int repeatConter = 0;
  switch (ac_state)
  {
  case AC_OFF:
    result = "AC_OFF      " + repeat_action_remaining_time();
    repeatConter = 0;
    break;
  case BOUNCING_FOR_AC_ON:
    if (repeatConter % 2)
      result = "BOUNCE_FOR_AC_ON";
    else
      result = "    BOUNCE      ";
    break;
  case AC_ON:
    result = "AC_ON       " + repeat_action_remaining_time();
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
  case TEMPERATURE_SENSOR_FAULT:
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

void ac_ir_command_repeat()
{

  press_rc_button(temp_down_rc_button, false);
  press_rc_button(temp_up_rc_button, true);
}
int ac_ir_cool_target_update()
{
  if (!digitalRead(dip_switch_d))
  {
    return 1;
  }
  else
  {
    return 4;
  }
}

void ac_cooling_enable()
{
  for (int i = 0; i < TEMP_UP_STEPS; i++) // steps = 16
  {
    press_rc_button(temp_up_rc_button, false);
  }

  temp_down_steps = ac_ir_cool_target_update();

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
String repeat_action_remaining_time()
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
  pinMode(ac_disabled_pin, INPUT);
  lcd.init();
  lcd.print("     WELCOME    ");
  lcd.setCursor(0, 1);
  if (analogRead(ac_disabled_pin) > 900)
  {
    lcd.print("ac button = 0");
    ac_state = AC_DISABLED;
  }
  else
  {
    lcd.print("ac butoon = 1");
  }
  ac_cooling_disable();
}

void ac_processing(float min_hidx, float max_hidx, float room_temp, float room_humd, float room_hidx, bool reading_error, bool ac_on_button_on_box)
{

  if (reading_error)
  {
    if (ac_state != AC_DISABLED && ac_state != AC_DISABLING)

    {
      ac_state = TEMPERATURE_SENSOR_FAULT;
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

        static int acIrCoolTarget = ac_ir_cool_target_update();
        if (acIrCoolTarget != ac_ir_cool_target_update())
        {
          ac_cooling_enable();
          acIrCoolTarget = ac_ir_cool_target_update();
        }

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
    if (ac_on_button_on_box == false)
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

  case TEMPERATURE_SENSOR_FAULT:

    ac_on_sensor_fault();
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
  float max_hidx = 0;
  float min_hidx = 0;
  float dip_switch_offset = 0;
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
  dip_switch_offset = Dip_Switch_Offset();
  max_hidx = HC_MAX_HIDEX + dip_switch_offset;
  min_hidx = HC_MIN_HIDEX + dip_switch_offset;
  bool ac_on_button = analogRead(ac_disabled_pin) > 900 ? false : true;
  
  ac_processing(min_hidx, max_hidx, room_temp, room_humd, room_hidx, reading_error, ac_on_button);

  float disier_hidx = max_hidx - 0.2;
  lcd.setCursor(0, 0);
  lcd.print(ac_state_lcd());
  lcd.setCursor(0, 1);
  lcd.print("T" + String(room_temp, 1) + " H" + String(room_humd, 0) + "%" + " #" + String(room_hidx, 1) + "");
  lcd.setCursor(16, 0);
  lcd.print("SET=");
  lcd.setCursor(0, 2);
  lcd.print(String(disier_hidx, 1) + "   " + "IR=" + String(29 - ac_ir_cool_target_update()));
  lcd.setCursor(0, 3);
  lcd.print("ac = " + String(ac_on_button));


  delay(1000);
}
