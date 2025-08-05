/* Display helper utilities for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <vector>
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

#include "_locale.h"
#include "_strftime.h"
#include "api_response.h"
#include "config.h"
#include "display_utils.h"

// icon header files
#include "icons/icons.h"

/* Returns battery voltage in millivolts (mv).
 */
uint32_t readBatteryVoltage()
{
  esp_adc_cal_characteristics_t adc_chars;
  // __attribute__((unused)) disables compiler warnings about this variable
  // being unused (Clang, GCC) which is the case when DEBUG_LEVEL == 0.
  esp_adc_cal_value_t val_type __attribute__((unused));
  adc_power_acquire();
  uint16_t adc_val = analogRead(PIN_BAT_ADC);
  adc_power_release();

  // We will use the eFuse ADC calibration bits, to get accurate voltage
  // readings. The DFRobot FireBeetle Esp32-E V1.0's ADC is 12 bit, and uses
  // 11db attenuation, which gives it a measurable input voltage range of 150mV
  // to 2450mV.
  val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db,
                                      ADC_WIDTH_BIT_12, 1100, &adc_chars);

#if DEBUG_LEVEL >= 1
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
  {
    Serial.println("[debug] ADC Cal eFuse Vref");
  }
  else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
  {
    Serial.println("[debug] ADC Cal Two Point");
  }
  else
  {
    Serial.println("[debug] ADC Cal Default");
  }
#endif

  uint32_t batteryVoltage = esp_adc_cal_raw_to_voltage(adc_val, &adc_chars);
  // DFRobot FireBeetle Esp32-E V1.0 voltage divider (1M+1M), so readings are
  // multiplied by 2.
  batteryVoltage *= 2;
  return batteryVoltage;
} // end readBatteryVoltage

/* Returns battery percentage, rounded to the nearest integer.
 * Takes a voltage in millivolts and uses a sigmoidal approximation to find an
 * approximation of the battery life percentage remaining.
 * 
 * This function contains LGPLv3 code from 
 * <https://github.com/rlogiacco/BatterySense>.
 * 
 * Symmetric sigmoidal approximation
 * <https://www.desmos.com/calculator/7m9lu26vpy>
 *
 * c - c / (1 + k*x/v)^3
 */
uint32_t calcBatPercent(uint32_t v, uint32_t minv, uint32_t maxv)
{
  // slow
  //uint32_t p = 110 - (110 / (1 + pow(1.468 * (v - minv)/(maxv - minv), 6)));

  // steep
  //uint32_t p = 102 - (102 / (1 + pow(1.621 * (v - minv)/(maxv - minv), 8.1)));

  // normal
  uint32_t p = 105 - (105 / (1 + pow(1.724 * (v - minv)/(maxv - minv), 5.5)));
  return p >= 100 ? 100 : p;
} // end calcBatPercent

/* Returns 24x24 bitmap incidcating battery status.
 */
const uint8_t *getBatBitmap24(uint32_t batPercent)
{
  if (batPercent >= 93)
  {
    return battery_full_90deg_24x24;
  }
  else if (batPercent >= 79)
  {
    return battery_6_bar_90deg_24x24;
  }
  else if (batPercent >= 65)
  {
    return battery_5_bar_90deg_24x24;
  }
  else if (batPercent >= 50)
  {
    return battery_4_bar_90deg_24x24;
  }
  else if (batPercent >= 36)
  {
    return battery_3_bar_90deg_24x24;
  }
  else if (batPercent >= 22)
  {
    return battery_2_bar_90deg_24x24;
  }
  else if (batPercent >= 8)
  {
    return battery_1_bar_90deg_24x24;
  }
  else
  {  // batPercent < 8
    return battery_0_bar_90deg_24x24;
  }
} // end getBatBitmap24

/* Gets string with the current date.
 */
void getDateStr(String &s, tm *timeInfo)
{
  char buf[48] = {};
  _strftime(buf, sizeof(buf), DATE_FORMAT, timeInfo);
  s = buf;

  // remove double spaces. %e will add an extra space, ie. " 1" instead of "1"
  s.replace("  ", " ");
  return;
} // end getDateStr

/* Gets string with the current date and time of the current refresh attempt.
 */
void getRefreshTimeStr(String &s, bool timeSuccess, tm *timeInfo)
{
  if (timeSuccess == false)
  {
    s = TXT_UNKNOWN;
    return;
  }

  char buf[48] = {};
  _strftime(buf, sizeof(buf), REFRESH_TIME_FORMAT, timeInfo);
  s = buf;

  // remove double spaces.
  s.replace("  ", " ");
  return;
} // end getRefreshTimeStr

/* Takes a String and capitalizes the first letter of every word.
 *
 * Ex:
 *   input   : "severe thunderstorm warning" or "SEVERE THUNDERSTORM WARNING"
 *   becomes : "Severe Thunderstorm Warning"
 */
void toTitleCase(String &text)
{
  text.setCharAt(0, toUpperCase(text.charAt(0)));

  for (int i = 1; i < text.length(); ++i)
  {
    if (text.charAt(i - 1) == ' '
     || text.charAt(i - 1) == '-'
     || text.charAt(i - 1) == '(')
    {
      text.setCharAt(i, toUpperCase(text.charAt(i)));
    }
    else
    {
      text.setCharAt(i, toLowerCase(text.charAt(i)));
    }
  }

  return;
} // end toTitleCase

/* Takes a String and truncates at any of these characters ,.( and trims any
 * trailing whitespace.
 *
 * Ex:
 *   input   : "Severe Thunderstorm Warning, (Starting At 10 Pm)"
 *   becomes : "Severe Thunderstorm Warning"
 */
void truncateExtraAlertInfo(String &text)
{
  if (text.isEmpty())
  {
    return;
  }

  int i = 1;
  int lastChar = i;
  while (i < text.length()
    && text.charAt(i) != ','
    && text.charAt(i) != '.'
    && text.charAt(i) != '(')
  {
    if (text.charAt(i) != ' ')
    {
      lastChar = i + 1;
    }
    ++i;
  }

  text = text.substring(0, lastChar);
  return;
} // end truncateExtraAlertInfo

/* Returns the urgency of an event based by checking if the event String
 * contains any indicator keywords.
 *
 * Urgency keywords are defined in config.h because they are very regional.
 *   ex: United States - (Watch < Advisory < Warning)
 *
 * The index in vector<String> ALERT_URGENCY indicates the urgency level.
 * If an event string matches none of these keywords the urgency is unknown, -1
 * is returned.
 * In the United States example, Watch = 0, Advisory = 1, Warning = 2
 */
int eventUrgency(const String &event)
{
  int urgency_lvl = -1;
  for (int i = 0; i < ALERT_URGENCY.size(); ++i)
  {
    if (event.indexOf(ALERT_URGENCY[i]) >= 0)
    {
      urgency_lvl = i;
    }
  }
  return urgency_lvl;
} // end eventUrgency


/* Returns the descriptor text for the given UV index.
 */
const char *getUVIdesc(unsigned int uvi)
{
  if (uvi <= 2)
  {
    return TXT_UV_LOW;
  }
  else if (uvi <= 5)
  {
    return TXT_UV_MODERATE;
  }
  else if (uvi <= 7)
  {
    return TXT_UV_HIGH;
  }
  else if (uvi <= 10)
  {
    return TXT_UV_VERY_HIGH;
  }
  else // uvi >= 11
  {
    return TXT_UV_EXTREME;
  }
} // end getUVIdesc

/* Returns the wifi signal strength descriptor text for the given RSSI.
 */
const char *getWiFidesc(int rssi)
{
  if (rssi == 0)
  {
    return TXT_WIFI_NO_CONNECTION;
  }
  else if (rssi >= -50)
  {
    return TXT_WIFI_EXCELLENT;
  }
  else if (rssi >= -60)
  {
    return TXT_WIFI_GOOD;
  }
  else if (rssi >= -70)
  {
    return TXT_WIFI_FAIR;
  }
  else
  {  // rssi < -70
    return TXT_WIFI_WEAK;
  }
} // end getWiFidesc

/* Returns 16x16 bitmap incidcating wifi status.
 */
const uint8_t *getWiFiBitmap16(int rssi)
{
  if (rssi == 0)
  {
    return wifi_x_16x16;
  }
  else if (rssi >= -50)
  {
    return wifi_16x16;
  }
  else if (rssi >= -60)
  {
    return wifi_3_bar_16x16;
  }
  else if (rssi >= -70)
  {
    return wifi_2_bar_16x16;
  }
  else
  {  // rssi < -70
    return wifi_1_bar_16x16;
  }
} // end getWiFiBitmap24

/* Returns true if icon is a daytime icon, false otherwise.
 */
bool isDay(String icon) 
{
  // OpenWeatherMap indicates sun is up with d otherwise n for night
  return icon.endsWith("d");
}

/* Returns true if the moon is currently in the sky above, false otherwise.
 */
bool isMoonInSky(int64_t current_dt, int64_t moonrise_dt, int64_t moonset_dt,
                 float moon_phase)
{
  // (moon is out if current time is after moonrise but before moonset
  //   OR if moonrises after moonset and the current time is after moonrise)
  // AND (moon phase is not a new moon)
  return ((current_dt >= moonrise_dt && current_dt < moonset_dt)
          || (moonrise_dt > moonset_dt && current_dt >= moonrise_dt))
         && (moon_phase != 0.f && moon_phase != 1.f);
}

/* Takes cloudiness (%) and returns true if it is at least partially cloudy,
 * false otherwise.
 *
 * References:
 *   https://www.weather.gov/ajk/ForecastTerms
 */
bool isCloudy(int clouds) {
  return clouds > 60.25; // partly cloudy / partly sunny
}

/* Takes wind speed and wind gust speed and returns true if it is windy, false
 * otherwise.
 *
 * References:
 *   https://www.weather.gov/ajk/ForecastTerms
 */
bool isWindy(float wind_speed, float wind_gust) {
 return (wind_speed >= 32.2 /*m/s*/
      || wind_gust  >= 40.2 /*m/s*/);
}

/* Takes the current weather and today's daily weather forcast (from
 * OpenWeatherMap API response) and returns a pointer to the icon's 196x196
 * bitmap.
 *
 * Uses multiple factors to return more detailed icons than the simple icon
 * catagories that OpenWeatherMap provides.
 *
 * Last Updated: June 26, 2022
 *
 * References:
 *   https://openweathermap.org/weather-conditions
 */

#if 0
template <int BitmapSize>
const uint8_t *getConditionsBitmap(int id, bool day, bool moon, bool cloudy,
                                   bool windy)
{
  switch (id)
  {
  // Group 2xx: Thunderstorm
  case 200: // Thunderstorm  thunderstorm with light rain     11d
  case 201: // Thunderstorm  thunderstorm with rain           11d
  case 202: // Thunderstorm  thunderstorm with heavy rain     11d
  case 210: // Thunderstorm  light thunderstorm               11d
  case 211: // Thunderstorm  thunderstorm                     11d
  case 212: // Thunderstorm  heavy thunderstorm               11d
  case 221: // Thunderstorm  ragged thunderstorm              11d
    if (!cloudy && day)          {return getBitmap(wi_day_thunderstorm, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_alt_thunderstorm, BitmapSize);}
    return getBitmap(wi_thunderstorm, BitmapSize);
  case 230: // Thunderstorm  thunderstorm with light drizzle  11d
  case 231: // Thunderstorm  thunderstorm with drizzle        11d
  case 232: // Thunderstorm  thunderstorm with heavy drizzle  11d
    if (!cloudy && day)          {return getBitmap(wi_day_storm_showers, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_alt_storm_showers, BitmapSize);}
    return getBitmap(wi_storm_showers, BitmapSize);
  // Group 3xx: Drizzle
  case 300: // Drizzle       light intensity drizzle          09d
  case 301: // Drizzle       drizzle                          09d
  case 302: // Drizzle       heavy intensity drizzle          09d
  case 310: // Drizzle       light intensity drizzle rain     09d
  case 311: // Drizzle       drizzle rain                     09d
  case 312: // Drizzle       heavy intensity drizzle rain     09d
  case 313: // Drizzle       shower rain and drizzle          09d
  case 314: // Drizzle       heavy shower rain and drizzle    09d
  case 321: // Drizzle       shower drizzle                   09d
    if (!cloudy && day)          {return getBitmap(wi_day_showers, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_alt_showers, BitmapSize);}
    return getBitmap(wi_showers, BitmapSize);
  // Group 5xx: Rain
  case 500: // Rain          light rain                       10d
  case 501: // Rain          moderate rain                    10d
  case 502: // Rain          heavy intensity rain             10d
  case 503: // Rain          very heavy rain                  10d
  case 504: // Rain          extreme rain                     10d
    if (!cloudy && day && windy)          {return getBitmap(wi_day_rain_wind, BitmapSize);}
    if (!cloudy && day)                   {return getBitmap(wi_day_rain, BitmapSize);}
    if (!cloudy && !day && moon && windy) {return getBitmap(wi_night_alt_rain_wind, BitmapSize);}
    if (!cloudy && !day && moon)          {return getBitmap(wi_night_alt_rain, BitmapSize);}
    if (windy)                            {return getBitmap(wi_rain_wind, BitmapSize);}
    return getBitmap(wi_rain, BitmapSize);
  case 511: // Rain          freezing rain                    13d
    if (!cloudy && day)          {return getBitmap(wi_day_rain_mix, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_alt_rain_mix, BitmapSize);}
    return getBitmap(wi_rain_mix, BitmapSize);
  case 520: // Rain          light intensity shower rain      09d
  case 521: // Rain          shower rain                      09d
  case 522: // Rain          heavy intensity shower rain      09d
  case 531: // Rain          ragged shower rain               09d
    if (!cloudy && day)          {return getBitmap(wi_day_showers, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_alt_showers, BitmapSize);}
    return getBitmap(wi_showers, BitmapSize);
  // Group 6xx: Snow
  case 600: // Snow          light snow                       13d
  case 601: // Snow          Snow                             13d
  case 602: // Snow          Heavy snow                       13d
    if (!cloudy && day && windy)          {return getBitmap(wi_day_snow_wind, BitmapSize);}
    if (!cloudy && day)                   {return getBitmap(wi_day_snow, BitmapSize);}
    if (!cloudy && !day && moon && windy) {return getBitmap(wi_night_alt_snow_wind, BitmapSize);}
    if (!cloudy && !day && moon)          {return getBitmap(wi_night_alt_snow, BitmapSize);}
    if (windy)                            {return getBitmap(wi_snow_wind, BitmapSize);}
    return getBitmap(wi_snow, BitmapSize);
  case 611: // Snow          Sleet                            13d
  case 612: // Snow          Light shower sleet               13d
  case 613: // Snow          Shower sleet                     13d
    if (!cloudy && day)          {return getBitmap(wi_day_sleet, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_alt_sleet, BitmapSize);}
    return getBitmap(wi_sleet, BitmapSize);
  case 615: // Snow          Light rain and snow              13d
  case 616: // Snow          Rain and snow                    13d
  case 620: // Snow          Light shower snow                13d
  case 621: // Snow          Shower snow                      13d
  case 622: // Snow          Heavy shower snow                13d
    if (!cloudy && day)          {return getBitmap(wi_day_rain_mix, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_alt_rain_mix, BitmapSize);}
    return getBitmap(wi_rain_mix, BitmapSize);
  // Group 7xx: Atmosphere
  case 701: // Mist          mist                             50d
    if (!cloudy && day)          {return getBitmap(wi_day_fog, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_fog, BitmapSize);}
    return getBitmap(wi_fog, BitmapSize);
  case 711: // Smoke         Smoke                            50d
    return getBitmap(wi_smoke, BitmapSize);
  case 721: // Haze          Haze                             50d
    if (day && !cloudy) {return getBitmap(wi_day_haze, BitmapSize);}
    return getBitmap(wi_dust, BitmapSize);
  case 731: // Dust          sand/dust whirls                 50d
    return getBitmap(wi_sandstorm, BitmapSize);
  case 741: // Fog           fog                              50d
    if (!cloudy && day)          {return getBitmap(wi_day_fog, BitmapSize);}
    if (!cloudy && !day && moon) {return getBitmap(wi_night_fog, BitmapSize);}
    return getBitmap(wi_fog, BitmapSize);
  case 751: // Sand          sand                             50d
    return getBitmap(wi_sandstorm, BitmapSize);
  case 761: // Dust          dust                             50d
    return getBitmap(wi_dust, BitmapSize);
  case 762: // Ash           volcanic ash                     50d
    return getBitmap(wi_volcano, BitmapSize);
  case 771: // Squall        squalls                          50d
    return getBitmap(wi_cloudy_gusts, BitmapSize);
  case 781: // Tornado       tornado                          50d
    return getBitmap(wi_tornado, BitmapSize);
  // Group 800: Clear
  case 800: // Clear         clear sky                        01d 01n
    if (windy)         {return getBitmap(wi_strong_wind, BitmapSize);}
    if (!day && moon)  {return getBitmap(wi_night_clear, BitmapSize);}
    if (!day && !moon) {return getBitmap(wi_stars, BitmapSize);}
    return getBitmap(wi_day_sunny, BitmapSize);
  // Group 80x: Clouds
  case 801: // Clouds        few clouds: 11-25%               02d 02n
    if (windy)         {return getBitmap(wi_strong_wind, BitmapSize);}
    if (!day && moon)  {return getBitmap(wi_night_alt_partly_cloudy, BitmapSize);}
    if (!day && !moon) {return getBitmap(wi_stars, BitmapSize);}
    return getBitmap(wi_day_sunny_overcast, BitmapSize);
  case 802: // Clouds        scattered clouds: 25-50%         03d 03n
  case 803: // Clouds        broken clouds: 51-84%            04d 04n
    if (windy && day)           {return getBitmap(wi_day_cloudy_gusts, BitmapSize);}
    if (windy && !day && moon)  {return getBitmap(wi_night_alt_cloudy_gusts, BitmapSize);}
    if (windy && !day && !moon) {return getBitmap(wi_cloudy_gusts, BitmapSize);}
    if (!day && moon)           {return getBitmap(wi_night_alt_cloudy, BitmapSize);}
    if (!day && !moon)          {return getBitmap(wi_cloud, BitmapSize);}
    return getBitmap(wi_day_cloudy, BitmapSize);
  case 804: // Clouds        overcast clouds: 85-100%         04d 04n
    if (windy) {return getBitmap(wi_cloudy_gusts, BitmapSize);}
    return getBitmap(wi_cloudy, BitmapSize);
  default:
    // maybe this is a new getBitmap in one of the existing groups
    if (id >= 200 && id < 300) {return getBitmap(wi_thunderstorm, BitmapSize);}
    if (id >= 300 && id < 400) {return getBitmap(wi_showers, BitmapSize);}
    if (id >= 500 && id < 600) {return getBitmap(wi_rain, BitmapSize);}
    if (id >= 600 && id < 700) {return getBitmap(wi_snow, BitmapSize);}
    if (id >= 700 && id < 800) {return getBitmap(wi_fog, BitmapSize);}
    if (id >= 800 && id < 900) {return getBitmap(wi_cloudy, BitmapSize);}
    return getBitmap(wi_na, BitmapSize);
  }
} // end getConditionsBitmap
#endif


template <int BitmapSize>
const uint8_t *getConditionsBitmap(int id, bool day, bool moon, bool cloudy,
                                   bool windy)
{
  switch (id)
  {
  case 0: //0 Clear sky
    return getBitmap(wi_day_sunny, BitmapSize);

  case 1: //1,2,3 Mainly clear, partly cloudy, and overcast
    return getBitmap(wi_day_sunny_overcast, BitmapSize);
  case 2:
    return getBitmap(wi_day_cloudy, BitmapSize);
  case 3:
    return getBitmap(wi_cloudy, BitmapSize);

  case 45: // 45,48 Fog and depositing rime fog
    return getBitmap(wi_fog, BitmapSize);
  case 48:
    return getBitmap(wi_fog, BitmapSize);

  case 51: //51,53,55 Drizzle: Light, moderate, and dense intensity
    return getBitmap(wi_showers, BitmapSize);
  case 53:
    return getBitmap(wi_showers, BitmapSize);
  case 55:
    return getBitmap(wi_showers, BitmapSize);

  case 56: // 56,57 Freezing Drizzle: Light and dense intensity
    return getBitmap(wi_showers, BitmapSize);
  case 57:
    return getBitmap(wi_showers, BitmapSize);

  case 61: //61,63,65 Rain: Slight, moderate and heavy intensity
    return getBitmap(wi_day_rain, BitmapSize);
  case 63:
    return getBitmap(wi_day_rain_wind, BitmapSize);
  case 65:
    return getBitmap(wi_rain, BitmapSize);

  case 66: //66,67 Freezing Rain: Light and heavy intensity
    return getBitmap(wi_rain_mix, BitmapSize);
  case 67:
    return getBitmap(wi_rain_mix, BitmapSize);

  case 71: //71,73,75 Snow fall: Slight, moderate, and heavy intensity
    return getBitmap(wi_day_snow, BitmapSize);
  case 73:
    return getBitmap(wi_snow, BitmapSize);
  case 75:
    return getBitmap(wi_snow, BitmapSize);

  case 77: //Snow grains
  return getBitmap(wi_snow, BitmapSize);

  case 80: //80.81.82 Rain showers: Slight, moderate, and violent
    return getBitmap(wi_rain, BitmapSize);
  case 81:
    return getBitmap(wi_rain, BitmapSize);
  case 82:
    return getBitmap(wi_rain, BitmapSize);

  case 85: //85,86 Snow showers slight and heavy
    return getBitmap(wi_snow, BitmapSize);
  case 86:
    return getBitmap(wi_snow, BitmapSize);

  case 95: //95 Thunderstorm: Slight or moderate
    return getBitmap(wi_day_thunderstorm, BitmapSize);

  case 96: //96,99 Thunderstorm with slight and heavy hail
    return getBitmap(wi_thunderstorm, BitmapSize);
  case 99:
    return getBitmap(wi_thunderstorm, BitmapSize);

  default:
    return getBitmap(wi_na, BitmapSize);
  }
} // end getConditionsBitmap


/* Takes the daily weather forecast (from OpenWeatherMap API response) and
 * returns a pointer to the icon's 32x32 bitmap.
 *
 * The daily weather forcast of today is needed for moonrise and moonset times.
 */
const uint8_t *getHourlyForecastBitmap32(const meteo_hourly_t &hourly, const meteo_daily_t  &today)
{
  const int id = hourly.weather_code;
  const bool day = true;
  const bool moon = isMoonInSky(hourly.dt, today.moonrise, today.moonset, today.moon_phase);
  const bool cloudy = isCloudy(hourly.clouds);
  const bool windy = isWindy(hourly.wind_speed, hourly.wind_gust);

  return getConditionsBitmap<32>(id, day, moon, cloudy, windy);
}

/* Takes the daily weather forecast (from OpenWeatherMap API response) and
 * returns a pointer to the icon's 64x64 bitmap.
 */
const uint8_t *getDailyForecastBitmap64(const meteo_daily_t &daily)
{
  const int id = daily.weather_code;
  // always show daytime icon for daily forecast
  const bool day = true;
  const bool moon = false;
  const bool cloudy = isCloudy(daily.clouds);
  const bool windy = isWindy(daily.wind_speed, daily.wind_gust);
  return getConditionsBitmap<64>(id, day, moon, cloudy, windy);
} // end getForecastBitmap64

/* Takes the current weather and today's daily weather forcast (from
 * OpenWeatherMap API response) and returns a pointer to the icon's 196x196
 * bitmap.
 * 
 * The daily weather forcast of today is needed for moonrise and moonset times.
 */
const uint8_t *getCurrentConditionsBitmap196(const meteo_current_t &current,
                                             const meteo_daily_t   &today)
{
  const int id = current.weather_code;
  const bool day = true;
  const bool moon = isMoonInSky(current.dt, today.moonrise, today.moonset, today.moon_phase);
  const bool cloudy = isCloudy(current.clouds);
  const bool windy = true;
  return getConditionsBitmap<196>(id, day, moon, cloudy, windy);
} // end getCurrentConditionsBitmap196


/* Returns true of a String, s, contains any of the strings in the terminology
 * vector.
 *
 * Note: This function is case sensitive.
 */
bool containsTerminology(const String s, const std::vector<String> &terminology)
{
  for (const String &term : terminology)
  {
    if (s.indexOf(term) >= 0)
    {
      return true;
    }
  }
  return false;
} // end containsTerminology


/* This function returns a pointer to a string representing the meaning for a
 * HTTP response status code or an arduino client error code.
 * ArduinoJson DeserializationError codes are also included here and are given a
 * negative 100 offset to distinguish them from other client error codes.
 *
 * HTTP response status codes [100, 599]
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
 *
 * HTTP client errors [0, -255]
 * https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/src/HTTPClient.h
 *
 * ArduinoJson DeserializationError codes [-256, -511]
 * https://arduinojson.org/v6/api/misc/deserializationerror/
 * 
 * WiFi Status codes [-512, -767]
 * https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFiType.h
 */
const char *getHttpResponsePhrase(int code)
{
  switch (code)
  {
  // 1xx - Informational Responses
  case 100: return TXT_HTTP_RESPONSE_100;
  case 101: return TXT_HTTP_RESPONSE_101;
  case 102: return TXT_HTTP_RESPONSE_102;
  case 103: return TXT_HTTP_RESPONSE_103;

  // 2xx - Successful Responses
  case 200: return TXT_HTTP_RESPONSE_200;
  case 201: return TXT_HTTP_RESPONSE_201;
  case 202: return TXT_HTTP_RESPONSE_202;
  case 203: return TXT_HTTP_RESPONSE_203;
  case 204: return TXT_HTTP_RESPONSE_204;
  case 205: return TXT_HTTP_RESPONSE_205;
  case 206: return TXT_HTTP_RESPONSE_206;
  case 207: return TXT_HTTP_RESPONSE_207;
  case 208: return TXT_HTTP_RESPONSE_208;
  case 226: return TXT_HTTP_RESPONSE_226;

  // 3xx - Redirection Responses
  case 300: return TXT_HTTP_RESPONSE_300;
  case 301: return TXT_HTTP_RESPONSE_301;
  case 302: return TXT_HTTP_RESPONSE_302;
  case 303: return TXT_HTTP_RESPONSE_303;
  case 304: return TXT_HTTP_RESPONSE_304;
  case 305: return TXT_HTTP_RESPONSE_305;
  case 307: return TXT_HTTP_RESPONSE_307;
  case 308: return TXT_HTTP_RESPONSE_308;

  // 4xx - Client Error Responses
  case 400: return TXT_HTTP_RESPONSE_400;
  case 401: return TXT_HTTP_RESPONSE_401;
  case 402: return TXT_HTTP_RESPONSE_402;
  case 403: return TXT_HTTP_RESPONSE_403;
  case 404: return TXT_HTTP_RESPONSE_404;
  case 405: return TXT_HTTP_RESPONSE_405;
  case 406: return TXT_HTTP_RESPONSE_406;
  case 407: return TXT_HTTP_RESPONSE_407;
  case 408: return TXT_HTTP_RESPONSE_408;
  case 409: return TXT_HTTP_RESPONSE_409;
  case 410: return TXT_HTTP_RESPONSE_410;
  case 411: return TXT_HTTP_RESPONSE_411;
  case 412: return TXT_HTTP_RESPONSE_412;
  case 413: return TXT_HTTP_RESPONSE_413;
  case 414: return TXT_HTTP_RESPONSE_414;
  case 415: return TXT_HTTP_RESPONSE_415;
  case 416: return TXT_HTTP_RESPONSE_416;
  case 417: return TXT_HTTP_RESPONSE_417;
  case 418: return TXT_HTTP_RESPONSE_418;
  case 421: return TXT_HTTP_RESPONSE_421;
  case 422: return TXT_HTTP_RESPONSE_422;
  case 423: return TXT_HTTP_RESPONSE_423;
  case 424: return TXT_HTTP_RESPONSE_424;
  case 425: return TXT_HTTP_RESPONSE_425;
  case 426: return TXT_HTTP_RESPONSE_426;
  case 428: return TXT_HTTP_RESPONSE_428;
  case 429: return TXT_HTTP_RESPONSE_429;
  case 431: return TXT_HTTP_RESPONSE_431;
  case 451: return TXT_HTTP_RESPONSE_451;

  // 5xx - Server Error Responses
  case 500: return TXT_HTTP_RESPONSE_500;
  case 501: return TXT_HTTP_RESPONSE_501;
  case 502: return TXT_HTTP_RESPONSE_502;
  case 503: return TXT_HTTP_RESPONSE_503;
  case 504: return TXT_HTTP_RESPONSE_504;
  case 505: return TXT_HTTP_RESPONSE_505;
  case 506: return TXT_HTTP_RESPONSE_506;
  case 507: return TXT_HTTP_RESPONSE_507;
  case 508: return TXT_HTTP_RESPONSE_508;
  case 510: return TXT_HTTP_RESPONSE_510;
  case 511: return TXT_HTTP_RESPONSE_511;

  // HTTP client errors [0, -255]
  case HTTPC_ERROR_CONNECTION_REFUSED:  return TXT_HTTPC_ERROR_CONNECTION_REFUSED;
  case HTTPC_ERROR_SEND_HEADER_FAILED:  return TXT_HTTPC_ERROR_SEND_HEADER_FAILED;
  case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return TXT_HTTPC_ERROR_SEND_PAYLOAD_FAILED;
  case HTTPC_ERROR_NOT_CONNECTED:       return TXT_HTTPC_ERROR_NOT_CONNECTED;
  case HTTPC_ERROR_CONNECTION_LOST:     return TXT_HTTPC_ERROR_CONNECTION_LOST;
  case HTTPC_ERROR_NO_STREAM:           return TXT_HTTPC_ERROR_NO_STREAM;
  case HTTPC_ERROR_NO_HTTP_SERVER:      return TXT_HTTPC_ERROR_NO_HTTP_SERVER;
  case HTTPC_ERROR_TOO_LESS_RAM:        return TXT_HTTPC_ERROR_TOO_LESS_RAM;
  case HTTPC_ERROR_ENCODING:            return TXT_HTTPC_ERROR_ENCODING;
  case HTTPC_ERROR_STREAM_WRITE:        return TXT_HTTPC_ERROR_STREAM_WRITE;
  case HTTPC_ERROR_READ_TIMEOUT:        return TXT_HTTPC_ERROR_READ_TIMEOUT;

  // ArduinoJson DeserializationError codes  [-256, -511]
  case -256 - (DeserializationError::Code::Ok):              return TXT_DESERIALIZATION_ERROR_OK;
  case -256 - (DeserializationError::Code::EmptyInput):      return TXT_DESERIALIZATION_ERROR_EMPTY_INPUT;
  case -256 - (DeserializationError::Code::IncompleteInput): return TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT;
  case -256 - (DeserializationError::Code::InvalidInput):    return TXT_DESERIALIZATION_ERROR_INVALID_INPUT;
  case -256 - (DeserializationError::Code::NoMemory):        return TXT_DESERIALIZATION_ERROR_NO_MEMORY;
  case -256 - (DeserializationError::Code::TooDeep):         return TXT_DESERIALIZATION_ERROR_TOO_DEEP;

  // WiFi Status codes [-512, -767]
  case -512 - WL_NO_SHIELD:       return TXT_WL_NO_SHIELD;
  // case -512 - WL_STOPPED:       return TXT_WL_STOPPED; // future
  case -512 - WL_IDLE_STATUS:     return TXT_WL_IDLE_STATUS;
  case -512 - WL_NO_SSID_AVAIL:   return TXT_WL_NO_SSID_AVAIL;
  case -512 - WL_SCAN_COMPLETED:  return TXT_WL_SCAN_COMPLETED;
  case -512 - WL_CONNECTED:       return TXT_WL_CONNECTED;
  case -512 - WL_CONNECT_FAILED:  return TXT_WL_CONNECT_FAILED;
  case -512 - WL_CONNECTION_LOST: return TXT_WL_CONNECTION_LOST;
  case -512 - WL_DISCONNECTED:    return TXT_WL_DISCONNECTED;

  default:  return "";
  }
} // end getHttpResponsePhrase


/* This function returns a pointer to a string representing the meaning for a
 * WiFi status (wl_status_t).
 *
 * wl_status_t type definition
 * https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFiType.h
 */
const char *getWifiStatusPhrase(wl_status_t status)
{
  switch (status)
  {
  case WL_NO_SHIELD:       return TXT_WL_NO_SHIELD;
  // case WL_STOPPED:       return TXT_WL_STOPPED; // future
  case WL_IDLE_STATUS:     return TXT_WL_IDLE_STATUS;
  case WL_NO_SSID_AVAIL:   return TXT_WL_NO_SSID_AVAIL;
  case WL_SCAN_COMPLETED:  return TXT_WL_SCAN_COMPLETED;
  case WL_CONNECTED:       return TXT_WL_CONNECTED;
  case WL_CONNECT_FAILED:  return TXT_WL_CONNECT_FAILED;
  case WL_CONNECTION_LOST: return TXT_WL_CONNECTION_LOST;
  case WL_DISCONNECTED:    return TXT_WL_DISCONNECTED;

  default:  return "";
  }
} // end getWifiStatusPhrase

/* This function sets the builtin LED to LOW and disables it even during deep
 * sleep.
 */
void disableBuiltinLED()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  gpio_hold_en(static_cast<gpio_num_t>(LED_BUILTIN));
  gpio_deep_sleep_hold_en();
  return;
} // end disableBuiltinLED
