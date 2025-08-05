/* Client side utility declarations for esp32-weather-epd.
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

#ifndef __CLIENT_UTILS_H__
#define __CLIENT_UTILS_H__

#include <Arduino.h>
#include "api_response.h"
#include "config.h"
#ifdef USE_HTTP
  #include <WiFiClient.h>
#else
  #include <WiFiClientSecure.h>
#endif

wl_status_t startWiFi(int &wifiRSSI);
void killWiFi();
bool waitForSNTPSync(tm *timeInfo);
bool printLocalTime(tm *timeInfo);
#ifdef USE_HTTP
  int getMeteocall(WiFiClient &client, requested_data_t &r);
#else
  int getMeteocall(WiFiClientSecure &client, requested_data_t &r);
#endif
#ifdef USE_HTTP
  int getDomoticzcall_IDX(WiFiClient &client, requested_data_t &r);
#else
  int getDomoticzcall_IDX(WiFiClientSecure &client, requested_data_t &r);
#endif
#ifdef USE_HTTP
  int getDomoticzcall_GRAPH(WiFiClient &client, requested_data_t &r);
#else
  int getDomoticzcall_GRAPH(WiFiClientSecure &client, requested_data_t &r);
#endif

#endif

