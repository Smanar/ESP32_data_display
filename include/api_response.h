/* API response deserialization declarations for esp32-weather-epd.
 * Copyright (C) 2022-2023  Luke Marzen
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

#ifndef __API_RESPONSE_H__
#define __API_RESPONSE_H__

#include <cstdint>
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define METEO_NUM_HOURLY        24 // 48
#define METEO_NUM_DAILY          8 // 8

/*
 * Current weather data API response
 */
typedef struct meteo_current
{
  int64_t dt;               // Current time, Unix, UTC
  int64_t sunrise;          // Sunrise time, Unix, UTC
  int64_t sunset;           // Sunset time, Unix, UTC
  float   temp_min;
  float   temp_max;
  int     pressure;         // Atmospheric pressure on the sea level, hPa
  int     humidity;         // Humidity, %
  float   dew_point;        // Atmospheric temperature (varying according to pressure and humidity) below which water droplets begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     clouds;           // Cloudiness, %
  float   uvi;              // Current UV index
  float   wind_speed;       // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float   pop;
  int     weather_code;
  int     alert[4];

} meteo_current_t;


/*
 * Hourly forecast weather data API response
 */
typedef struct meteo_hourly
{
  int64_t dt;               // Time of the forecasted data, unix, UTC
  float   temp;             // Temperature. Units - default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     pressure;         // Atmospheric pressure on the sea level, hPa
  int     humidity;         // Humidity, %
  float   dew_point;        // Atmospheric temperature (varying according to pressure and humidity) below which water droplets begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     clouds;           // Cloudiness, %
  float   uvi;              // Current UV index
  int     visibility;       // Average visibility, metres. The maximum value of the visibility is 10km
  float   wind_speed;       // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float   wind_gust;        // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int     wind_deg;         // Wind direction, degrees (meteorological)
  float   pop;              // Probability of precipitation. The values of the parameter vary between 0 and 1, where 0 is equal to 0%, 1 is equal to 100%
  float   rain_1h;          // (where available) Rain volume for last hour, mm
  float   snow_1h;          // (where available) Snow volume for last hour, mm
  int     weather_code;

} meteo_hourly_t;

/*
 * Daily forecast weather data API response
 */
typedef struct meteo_daily
{
  int64_t dt;               // Time of the forecasted data, unix, UTC
  int64_t sunrise;          // Sunrise time, Unix, UTC
  int64_t sunset;           // Sunset time, Unix, UTC
  int64_t moonrise;         // The time of when the moon rises for this day, Unix, UTC
  int64_t moonset;          // The time of when the moon sets for this day, Unix, UTC
  float   moon_phase;       // Moon phase. 0 and 1 are 'new moon', 0.25 is 'first quarter moon', 0.5 is 'full moon' and 0.75 is 'last quarter moon'. The periods in between are called 'waxing crescent', 'waxing gibous', 'waning gibous', and 'waning crescent', respectively.
  float   temp_min;
  int     weather_code;
  float   temp_max;
  int     pressure;         // Atmospheric pressure on the sea level, hPa
  int     humidity;         // Humidity, %
  float   dew_point;        // Atmospheric temperature (varying according to pressure and humidity) below which water droplets begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     clouds;           // Cloudiness, %
  float   uvi;              // Current UV index
  int     visibility;       // Average visibility, metres. The maximum value of the visibility is 10km
  float   wind_speed;       // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float   wind_gust;        // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int     wind_deg;         // Wind direction, degrees (meteorological)
  float   pop;              // Probability of precipitation. The values of the parameter vary between 0 and 1, where 0 is equal to 0%, 1 is equal to 100%
  float   rain;             // (where available) Precipitation volume, mm
  float   snow;             // (where available) Snow volume, mm

} meteo_daily_t;

typedef struct domoticz_data
{
  int icon = 0;
  String description;
  String value;

} domoticz_t;

typedef struct domoticz_data_graph
{
  int value;
  int prev_value;
  char dt[2]; // Only day

} domoticz_graph_t;

/*
 * Response from OpenWeatherMap's OneCall API
 *
 * https://openweathermap.org/api/one-call-api
 */
typedef struct requested_data
{
  meteo_current_t   current; // current meteo data
  meteo_hourly_t    hourly[METEO_NUM_HOURLY]; // not used
  meteo_daily_t     daily[METEO_NUM_DAILY]; // daily meteo data
  domoticz_t        data[7]; // domoticz data
  domoticz_graph_t  graph[35];
  String            memo;

} requested_data_t;


DeserializationError deserialize_Meteo_API(WiFiClient &json, requested_data_t &r);
DeserializationError deserialize_Domoticz_API_IDX(WiFiClient &json, requested_data_t &r);
DeserializationError deserialize_Domoticz_API_GRAPH(WiFiClient &json, requested_data_t &r);

#endif

