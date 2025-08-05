/* API response deserialization for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
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

#include <vector>
#include <ArduinoJson.h>
#include "api_response.h"
#include "config.h"

//https://github.com/Zindre17/Motivator/blob/master/Motivator.ino

DeserializationError deserialize_Meteo_API(WiFiClient &json, requested_data_t &r)
{
  int i;

/*
  JsonDocument filter;
  filter["current"]  = true;
  filter["hourly"]   = true;
  filter["daily"]    = true;
  filter["alerts"]   = false;

  DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
*/

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);

#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));  // 0 = false
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif

  if (error) {
    return error;
  }

  //Reset alert
  for (int i = 0; i < 3; ++i)
  {
    r.current.alert[i] = 0;
  }
  r.current.alert[0] = 1; // Alien ^^

  JsonObject current = doc["current"];
  r.current.dt         = current["dt"]        .as<int64_t>();
  r.current.sunrise    = current["sunrise"]   .as<int64_t>();
  r.current.sunset     = current["sunset"]    .as<int64_t>();

  r.current.pressure   = current["pressure"]  .as<int>();
  r.current.humidity   = current["humidity"]  .as<int>();
  r.current.dew_point  = current["dew_point"] .as<float>();
  r.current.clouds     = current["clouds"]    .as<int>();
  r.current.uvi        = current["uvi"]       .as<float>();
  r.current.wind_speed = current["wind_speed"].as<float>();

  JsonArray ListTime = doc["daily"]["time"].as<JsonArray>();

  i = 0;
  for(JsonVariant v : ListTime)
  {

    //Today day
    if (i == 0)
    {
      r.current.dt                        = v.as<int64_t>();
      //r.current.sunrise    = current["sunrise"]   .as<int64_t>();
      //r.current.sunset     = current["sunset"]    .as<int64_t>();
      r.current.temp_max                  = doc["daily"]["temperature_2m_max"][i].as<float>();
      r.current.temp_min                  = doc["daily"]["temperature_2m_min"][i].as<float>();
      r.current.wind_speed                = doc["daily"]["wind_speed_10m_max"][i].as<float>();
      r.current.pop                       = doc["daily"]["precipitation_probability_max"][i].as<float>();
      r.current.weather_code              = doc["daily"]["weather_code"][i].as<int>();
      r.current.uvi                       = doc["daily"]["uv_index_max"][i].as<float>();
    }
    else
    {
      r.daily[i-1].dt                  = v.as<int64_t>();
      //r.daily[i].weather.icon        = ListWeather_code[i].as<const char *>();
      //r.daily[i-1].clouds              = 0;//daily["clouds"]    .as<int>();
      r.daily[i-1].temp_min            = doc["daily"]["temperature_2m_min"][i].as<float>();
      r.daily[i-1].temp_max            = doc["daily"]["temperature_2m_max"][i].as<float>();
      r.daily[i-1].weather_code        = doc["daily"]["weather_code"][i].as<int>();

    }

    if (i == METEO_NUM_DAILY - 1)
    {
      break;
    }
    ++i;
  }

  // Use fake value four hourly
  //generate fake data
  for (int i = 1; i < HOURLY_GRAPH_MAX; ++i)
  {
    r.hourly[i].temp = random(10, 30);
    r.hourly[i].pop = random(10, 30);
    r.hourly[i].dt  = r.current.dt + i * 3600;
  }

  return error;
} // end deserialize_Meteo_API


DeserializationError deserialize_Domoticz_API_GRAPH(WiFiClient &json, requested_data_t &r)
{
  int i;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);

#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));  // 0 = false
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif

  if (error) {
    return error;
  }

  JsonArray result = doc["result"].as<JsonArray>();

  i = 0;
  float v1,v2;
  const char * day;

  for(JsonVariant v : result)
  {

    day = v["d"].as<const char *>();
    r.graph[i].dt[0] = day[8];
    r.graph[i].dt[1] = day[9];

    v1 = v["v1"].as<float>();
    v2 = v["v2"].as<float>();
    r.graph[i].value = v1 + v2;

    i+=1;

    //Security
    if (i > 32) { break; }

  }

  JsonArray resultprev = doc["resultprev"].as<JsonArray>();
  i = 0;

  for(JsonVariant v : resultprev)
  {
    v1 = v["v1"].as<float>();
    v2 = v["v2"].as<float>();
    r.graph[i].prev_value = v1 + v2;

    i+=1;

    //Security
    if (i > 32) { break; }
  }

  return error;
}


DeserializationError deserialize_Domoticz_API_IDX(WiFiClient &json, requested_data_t &r)
{
  int i;

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);

#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));  // 0 = false
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif

  if (error) {
    return error;
  }

  JsonArray result = doc["result"].as<JsonArray>();

  int idx = 0;
  i = 0;
  const char* type;

  for(JsonVariant v : result)
  {

    idx = v["idx"].as<int>();
    type = v["Type"].as<const char *>();

    r.data[i].icon = 9;

    r.data[i].description = v["Name"].as<const char *>();
    r.data[i].value = v["Data"].as<const char *>();

    if (idx == 125) // Memo, This one is special don't use same procedure
    {
      r.memo = v["Data"].as<const char *>();
      continue;
    }
    else if (strcmp(type, "Humidity") == 0)
    {
      r.data[i].icon = 3;
      if (r.data[i].value.startsWith("Humidity")) //Humidity sensor
      {
        r.data[i].value.remove(0,9);
      }
    }
    else if (strcmp(type, "Temp") == 0)  // Temperature sensor
    {
      r.data[i].icon = 4;
    }
    else if (idx == 124) // Pancankes
    {
      r.data[i].icon = 2;
    }
    else if (idx == 35)  // Poubelles
    {
      r.data[i].icon = 1;
    }

    //Security
    if (i > 4) { break; }

    ++i;

  }

  //Add memory
  r.data[i].icon = 5;
  r.data[i].description = "Memoire";
  uint32_t freeHeapBytes = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  uint32_t totalHeapBytes = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
  int percentageHeapFree = freeHeapBytes * 100.0f / (float)totalHeapBytes;
  r.data[i].value = String(percentageHeapFree) + " %";

  return error;
}
