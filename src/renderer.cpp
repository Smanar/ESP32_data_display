/* Renderer for esp32-weather-epd.
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

#include "_locale.h"
#include "_strftime.h"
#include "renderer.h"
#include "api_response.h"
#include "config.h"
#include "conversions.h"
#include "display_utils.h"

// fonts
#include FONT_HEADER

// icon header files
#include "icons/icons_16x16.h"
#include "icons/icons_24x24.h"
#include "icons/icons_32x32.h"
#include "icons/icons_48x48.h"
#include "icons/icons_64x64.h"
#include "icons/icons_96x96.h"
#include "icons/icons_128x128.h"
#include "icons/icons_160x160.h"
#include "icons/icons_196x196.h"

#ifdef DISP_BW_V2
  GxEPD2_BW<GxEPD2_750_T7,
            GxEPD2_750_T7::HEIGHT> display(
    GxEPD2_750_T7(PIN_EPD_CS,
                  PIN_EPD_DC,
                  PIN_EPD_RST,
                  PIN_EPD_BUSY));
#endif
#ifdef DISP_3C_B
  GxEPD2_3C<GxEPD2_750c_Z08,
            GxEPD2_750c_Z08::HEIGHT / 2> display(
    GxEPD2_750c_Z08(PIN_EPD_CS,
                    PIN_EPD_DC,
                    PIN_EPD_RST,
                    PIN_EPD_BUSY));
#endif
#ifdef DISP_7C_F
  GxEPD2_7C<GxEPD2_730c_GDEY073D46,
            GxEPD2_730c_GDEY073D46::HEIGHT / 4> display(
    GxEPD2_730c_GDEY073D46(PIN_EPD_CS,
                           PIN_EPD_DC,
                           PIN_EPD_RST,
                           PIN_EPD_BUSY));
#endif
#ifdef DISP_BW_V1
  GxEPD2_BW<GxEPD2_750,
            GxEPD2_750::HEIGHT> display(
    GxEPD2_750(PIN_EPD_CS,
               PIN_EPD_DC,
               PIN_EPD_RST,
               PIN_EPD_BUSY));
#endif

#ifndef ACCENT_COLOR
  #define ACCENT_COLOR GxEPD_BLACK
#endif

/* Returns the string width in pixels
 */
uint16_t getStringWidth(const String &text)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}

/* Returns the string height in pixels
 */
uint16_t getStringHeight(const String &text)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return h;
}

void drawAlphaBar(int16_t x0_t, int16_t y0_t, int16_t x1_t, int16_t y1_t, uint16_t color)
{
    for (int y = y1_t - 1; y > y0_t; y -= 2)
    {
      for (int x = x0_t + (x0_t % 2); x < x1_t; x += 2)
      {
        display.drawPixel(x, y, color);
      }
    }
}

void drawBox(int16_t x, int16_t y, int16_t w, int16_t h)
{
  display.drawFastHLine(x,y,w,GxEPD_BLACK);
  display.drawFastHLine(x,y+h,w,ACCENT_COLOR);
  display.drawFastVLine(x,y,h,GxEPD_BLACK);
  display.drawFastVLine(x+w,y,h,ACCENT_COLOR);

  display.drawFastHLine(x,y+1,w,GxEPD_BLACK);
  display.drawFastHLine(x,y+h-1,w,ACCENT_COLOR);
  display.drawFastVLine(x+1,y,h,GxEPD_BLACK);
  display.drawFastVLine(x+w-1,y,h,ACCENT_COLOR);

}

/* Draws a string with alignment
 */
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment, uint16_t color)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextColor(color);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)
  {
    x = x - w;
  }
  if (alignment == CENTER)
  {
    x = x - w / 2;
  }
  display.setCursor(x, y);
  display.print(text);
  return;
} // end drawString

/* Draws a string that will flow into the next line when max_width is reached.
 * If a string exceeds max_lines an ellipsis (...) will terminate the last word.
 * Lines will break at spaces(' ') and dashes('-').
 *
 * Note: max_width should be big enough to accommodate the largest word that
 *       will be displayed. If an unbroken string of characters longer than
 *       max_width exist in text, then the string will be printed beyond
 *       max_width.
 */
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color)
{

  uint16_t current_line = 0;
  String textRemaining = text;
  // print until we reach max_lines or no more text remains

  while (current_line < max_lines && !textRemaining.isEmpty())
  {
    int16_t  x1, y1;
    uint16_t w, h;

    display.getTextBounds(textRemaining, 0, 0, &x1, &y1, &w, &h);

    int endIndex = textRemaining.length();
    // check if remaining text is to wide, if it is then print what we can
    String subStr = textRemaining;
    int splitAt = 0;
    int keepLastChar = 0;
    int cr = 0;

    while (w > max_width && splitAt != -1)
    {
      if (keepLastChar)
      {
        // if we kept the last character during the last iteration of this while
        // loop, remove it now so we don't get stuck in an infinite loop.
        subStr.remove(subStr.length() - 1);
      }

      // find the last place in the string that we can break it.
      if (current_line < max_lines - 1)
      {
        splitAt = std::max(subStr.lastIndexOf(" "), subStr.lastIndexOf("-"));
      }
      else
      {
        // this is the last line, only break at spaces so we can add ellipsis
        splitAt = subStr.lastIndexOf(" ");
      }

      // Make cariage return on <br>
      if (subStr.lastIndexOf("<"))
      {
        splitAt = subStr.lastIndexOf("<") - 1;
        cr = 1;
      }

      // if splitAt == -1 then there is an unbroken set of characters that is
      // longer than max_width. Otherwise if splitAt != -1 then we can continue
      // the loop until the string is <= max_width
      if (splitAt != -1)
      {
        endIndex = splitAt;
        subStr = subStr.substring(0, endIndex + 1);

        if (cr == 1)
        {
          endIndex +=3;
        }

        char lastChar = subStr.charAt(endIndex);
        if (lastChar == ' ')
        {
          // remove this char now so it is not counted towards line width
          keepLastChar = 0;
          subStr.remove(endIndex);
          --endIndex;
        }
        else if (lastChar == '-')
        {
          // this char will be printed on this line and removed next iteration
          keepLastChar = 1;
        }

        if (current_line < max_lines - 1)
        {
          // this is not the last line
          display.getTextBounds(subStr, 0, 0, &x1, &y1, &w, &h);
        }
        else
        {
          // this is the last line, we need to make sure there is space for
          // ellipsis
          display.getTextBounds(subStr + "...", 0, 0, &x1, &y1, &w, &h);
          if (w <= max_width)
          {
            // ellipsis fit, add them to subStr
            subStr = subStr + "...";
          }
        }

      } // end if (splitAt != -1)
    } // end inner while

    drawString(x, y + (current_line * line_spacing), subStr, alignment, color);

    // update textRemaining to no longer include what was printed
    // +1 for exclusive bounds, +1 to get passed space/dash
    textRemaining = textRemaining.substring(endIndex + 2 - keepLastChar);

    ++current_line;
  } // end outer while

  return;
} // end drawMultiLnString

/* Initialize e-paper display
 */
void initDisplay()
{
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, HIGH);

#ifdef DRIVER_WAVESHARE
  display.init(115200, true, 2, false);
#endif
#ifdef DRIVER_DESPI_C02
  display.init(115200, true, 10, false);
#endif

  // remap spi
  SPI.end();
  SPI.begin(PIN_EPD_SCK, PIN_EPD_MISO, PIN_EPD_MOSI, PIN_EPD_CS);

  display.setRotation(1);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.setTextWrap(false);
  // display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
  display.firstPage(); // use paged drawing mode, sets fillScreen(GxEPD_WHITE)

  return;
} // end initDisplay

/* Power-off e-paper display
 */
void powerOffDisplay()
{
  display.hibernate(); // turns powerOff() and sets controller to deep sleep for
                       // minimum power use
  digitalWrite(PIN_EPD_PWR, LOW);
  return;
} // end initDisplay

/* This function is responsible for drawing the current conditions and
 * associated icons.
 */
void drawCurrentConditions(const meteo_current_t &current, const meteo_daily_t &today , float inTemp, float inHumidity, const String &date)
{

  //Just for test to check size
  //display.drawRoundRect(0+36,0+61,480-40,800-115,10,GxEPD_BLACK);

  String dataStr, unitStr;

  // current weather icon
  display.drawInvertedBitmap(X_OFFSET, Y_OFFSET, getCurrentConditionsBitmap196(current, today), 196, 196, GxEPD_BLACK);
  //display.drawRoundRect(X_OFFSET,Y_OFFSET,196,196,10,GxEPD_BLACK);

  // current temp
  dataStr = String(static_cast<int>(std::round(current.temp_min)));
  dataStr += "/";
  dataStr += String(static_cast<int>(std::round(current.temp_max)));
#ifdef UNITS_TEMP_CELSIUS
  unitStr = TXT_UNITS_TEMP_CELSIUS;
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
  unitStr = TXT_UNITS_TEMP_FAHRENHEIT;
#endif
  // FONT_**_temperature fonts only have the character set used for displaying temperature (0123456789.-\260)
  // TODO : Use this kind of font
  //display.setFont(&FONT_48pt8b_temperature);
  display.setFont(&FONT_22pt8b);
#ifndef DISP_BW_V1
    drawString(X_OFFSET + 196 + 164 / 2 - 30, Y_OFFSET + 196 / 2 + 69 / 2 - 20, dataStr, CENTER);
#elif defined(DISP_BW_V1)
    drawString(X_OFFSET + 156 + 164 / 2 - 20, Y_OFFSET + 196 / 2 + 69 / 2, dataStr, CENTER);
#endif
  display.setFont(&FONT_14pt8b);
  drawString(display.getCursorX(), Y_OFFSET + 196 / 2 - 69 / 2 + 20, unitStr, LEFT);

  // Date
  display.setFont(&FONT_12pt8b);
  drawString(USABLE_WIDTH + X_OFFSET - 7, Y_OFFSET + 20, date, RIGHT);

  //Alerts
  display.drawInvertedBitmap(USABLE_WIDTH + X_OFFSET - 5 - 50, Y_OFFSET + 50, alert_icon(current.alert[0]), 48, 48, ACCENT_COLOR);

  // current weather data icons
  display.drawInvertedBitmap(10 + X_OFFSET, Y_OFFSET + 184 + (48 + 8) * 0, wi_raindrops_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(160 + X_OFFSET, Y_OFFSET + 184 + (48 + 8) * 0, wi_day_sunny_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(310 + X_OFFSET, Y_OFFSET + 184 + (48 + 8) * 0, wi_strong_wind_48x48, 48, 48, GxEPD_BLACK);

  // current weather data labels
  display.setFont(&FONT_7pt8b);
  drawString(X_OFFSET + 58, Y_OFFSET +184 + 10 + (48 + 8) * 0, "% Pluie", LEFT);
  drawString(X_OFFSET + 160 + 48, Y_OFFSET +184 + 10 + (48 + 8) * 0, TXT_UV_INDEX, LEFT);
  drawString(X_OFFSET + 310 + 48, Y_OFFSET +184 + 10 + (48 + 8) * 0, TXT_WIND, LEFT);

  // wind
  dataStr = String(static_cast<int>(std::round(current.wind_speed)));
#ifdef UNITS_SPEED_METERSPERSECOND
  unitStr = String(" ") + TXT_UNITS_SPEED_METERSPERSECOND;
#endif
#ifdef UNITS_SPEED_FEETPERSECOND
  unitStr = String(" ") + TXT_UNITS_SPEED_FEETPERSECOND;
#endif
#ifdef UNITS_SPEED_KILOMETERSPERHOUR
  unitStr = String(" ") + TXT_UNITS_SPEED_KILOMETERSPERHOUR;
#endif
#ifdef UNITS_SPEED_MILESPERHOUR
  unitStr = String(" ") + TXT_UNITS_SPEED_MILESPERHOUR;
#endif
#ifdef UNITS_SPEED_KNOTS
  unitStr = String(" ") + TXT_UNITS_SPEED_KNOTS;
#endif
#ifdef UNITS_SPEED_BEAUFORT
  unitStr = String(" ") + TXT_UNITS_SPEED_BEAUFORT;
#endif
  drawString(X_OFFSET + 58 + 310 , Y_OFFSET + 184 + 17 / 2 + (48 + 8) * 0 + 48 / 2, dataStr, LEFT);
  display.setFont(&FONT_8pt8b);
  drawString(display.getCursorX(), Y_OFFSET + 184 + 17 / 2 + (48 + 8) * 0 + 48 / 2, unitStr, LEFT);

  // uv and air quality indices
  // spacing between end of index value and start of descriptor text
  const int sp = 8;

  // uv index
  display.setFont(&FONT_12pt8b);
  unsigned int uvi = static_cast<unsigned int>(std::max(std::round(current.uvi), 0.0f));
  dataStr = String(uvi);
  drawString(X_OFFSET + 150 + 58, Y_OFFSET + 184 + 17 / 2 + (48 + 8) * 0 + 48 / 2, dataStr, LEFT);
  display.setFont(&FONT_7pt8b);
  dataStr = String(getUVIdesc(uvi));
  int max_w = 170 - (display.getCursorX() + sp);
  if (getStringWidth(dataStr) <= max_w)
  { // Fits on a single line, draw along bottom
    drawString(display.getCursorX() + sp, Y_OFFSET + 184 + 17 / 2 + (48 + 8) * 0 + 48 / 2, dataStr, LEFT);
  }
  else
  { // use smaller font
    display.setFont(&FONT_5pt8b);
    if (getStringWidth(dataStr) <= max_w)
    { // Fits on a single line with smaller font, draw along bottom
      drawString(display.getCursorX() + sp, Y_OFFSET + 184 + 17 / 2 + (48 + 8) * 0 + 48 / 2, dataStr, LEFT);
    }
    else
    { // Does not fit on a single line, draw higher to allow room for 2nd line
      drawMultiLnString(display.getCursorX() + sp, Y_OFFSET + 184 + 17 / 2 + (48 + 8) * 0 + 48 / 2 - 10, dataStr, LEFT, max_w, 2, 10);
    }
  }

  //Rain probability
  dataStr = String(static_cast<int>(std::round(current.pop))) + "%";
  display.setFont(&FONT_7pt8b);
  drawString(X_OFFSET + 58 , Y_OFFSET + 184 + 17 / 2 + (48 + 8) * 0 + 48 / 2, dataStr, LEFT);


  return;
} // end drawCurrentConditions


const unsigned char * alert_icon(int v)
{
  switch (v)
  {
    case 1:
      return wi_alien_48x48;
      break;
    case 2:
      return wi_alien_48x48;
      break;
    default:
      return wi_na_48x48;
  }
}

const unsigned char * hackicon(int v)
{
  switch (v)
  {
    case 1:
      return trashcan_48x48;
      break;
    case 2:
      return pancake_48x48;
      break;
    case 3:
      return house_humidity_48x48;
      break;
    case 4:
      return house_thermometer_48x48;
      break;
    case 5:
      return memory_48x48;
      break;
    default:
      return wi_na_48x48;
  }
}

/* This function is responsible for drawing the Domoticz part.
*/
void drawDomoticz(const domoticz_t *data, String memo)
{

  //Make 3 zones
  display.drawRoundRect(X_OFFSET + 1 , Y_OFFSET + 372, USABLE_WIDTH / 2 - 2, 296, 10, GxEPD_BLACK); // icons 
  display.drawRoundRect(X_OFFSET + USABLE_WIDTH / 2 , Y_OFFSET + 372, USABLE_WIDTH / 2 - 2, 150, 10, GxEPD_BLACK); // graph 
  display.drawRoundRect(X_OFFSET + USABLE_WIDTH / 2 , Y_OFFSET + 372 + 150 + 1, USABLE_WIDTH / 2 - 2, 146, 10, GxEPD_BLACK); // To remember

  //Remember list
  display.fillRoundRect(X_OFFSET + USABLE_WIDTH / 2 + 1 , Y_OFFSET + 372 + 150 + 2 , USABLE_WIDTH / 2 - 4, 25, 10, ACCENT_COLOR);
  display.setFont(&FONT_9pt8b);
  drawString(X_OFFSET + 3 * USABLE_WIDTH / 4 , Y_OFFSET + 372 + 150 + 20 , "Ne pas oublier" , CENTER);
  display.setFont(&FONT_8pt8b);
  drawMultiLnString(X_OFFSET + USABLE_WIDTH / 2 + 5  , Y_OFFSET + 372 + 150 + 20 + 22, memo, LEFT, USABLE_WIDTH /2 , 6, 15 );

  int i = 0;

  for (int y = 0; y < 5; ++y)
  {

      if (data[i].icon > 0)
      {
        //background ?
        if ((i+1) % 2 == 0)
        {
          drawAlphaBar(X_OFFSET + 3 , Y_OFFSET + 372 + 3 + y * 48, X_OFFSET + 3 + USABLE_WIDTH / 2 - 7, Y_OFFSET + 372 + 3 + y * 48 + 48, ACCENT_COLOR);
        }

        // Icon
        //display.fillRoundRect(X_OFFSET + 3 , 310 + 140 + y * 48 , 48, 48, 10, ACCENT_COLOR);
        display.drawInvertedBitmap(X_OFFSET + 3 , Y_OFFSET + 372 + 3 + y * 48, hackicon(data[i].icon), 48, 48, GxEPD_BLACK);
        // Title
        display.setFont(&FONT_8pt8b);
        drawString(X_OFFSET + 5 + 48 ,  Y_OFFSET + 372 + 3 + y * 48 + 48/2 + 4, data[i].description , LEFT);
        // Value
        display.setFont(&FONT_10pt8b);
        drawString(X_OFFSET + USABLE_WIDTH / 2 - 15 , Y_OFFSET + 372 + 3 + y * 48 + 48/2 + 4, data[i].value, RIGHT);
      }

      i += 1;

  }


}

/* This function is responsible for drawing the five day forecast.
*/
void drawForecast(const meteo_daily_t *daily, tm timeInfo)
{

  //Don't use the current day
  timeInfo.tm_wday = (timeInfo.tm_wday + 1) % 7; // increment to next day

  display.drawRoundRect(X_OFFSET + 1, Y_OFFSET + 245, USABLE_WIDTH - 2, 126, 10, GxEPD_BLACK);

  drawAlphaBar(X_OFFSET + 1, Y_OFFSET + 245 + 3, X_OFFSET + USABLE_WIDTH - 2, Y_OFFSET + 245 + 3 + 35, GxEPD_BLACK);

  // 5 day, forecast
  String Str;
  String dataStr, unitStr;
  for (int i = 0; i < 5; ++i)
  {

    int x = X_OFFSET + 28 + (i * 82);

    // icons
    display.drawInvertedBitmap(x, Y_OFFSET + 245 + 38, getDailyForecastBitmap64(daily[i]), 64, 64, GxEPD_BLACK);

    // day of week label
    display.setFont(&FONT_11pt8b);
    char dayBuffer[8] = {};
    _strftime(dayBuffer, sizeof(dayBuffer), "%a", &timeInfo); // abbrv'd day
    drawString(x + 31 - 2, Y_OFFSET + 245 + 26, dayBuffer, CENTER);
    timeInfo.tm_wday = (timeInfo.tm_wday + 1) % 7; // increment to next day

    // high | low
    display.setFont(&FONT_8pt8b);
    Str = String(static_cast<int>(std::round(daily[i].temp_min))) + "/";
    Str += String(static_cast<int>(std::round(daily[i].temp_max))) + "\260C";

    drawString(x + 31 - 4, Y_OFFSET + 245 + 38 + 64 + 6, Str, CENTER);

  }

    return;
  } // end drawForecast


/* The % operator in C++ is not a true modulo operator but it instead a
 * remainder operator. The remainder operator and modulo operator are equivalent
 * for positive numbers, but not for negatives. The follow implementation of the
 * modulo operator works for +/-a and +b.
 */
inline int modulo(int a, int b)
{
  const int result = a % b;
  return result >= 0 ? result : result + b;
}

/* Convert temperature in kelvin to the display y coordinate to be plotted.
 */
int kelvin_to_plot_y(float kelvin, int tempBoundMin, float yPxPerUnit, int yBoundMin)
{
#ifdef UNITS_TEMP_CELSIUS
  return static_cast<int>(std::round(yBoundMin - (yPxPerUnit * (kelvin - tempBoundMin)) ));
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
  return static_cast<int>(std::round(yBoundMin - (yPxPerUnit * (kelvin - tempBoundMin)) ));
#endif
}

void drawConsumptionGraph(const domoticz_graph_t *graph , tm timeInfo)
{
  const int xPos0 = X_OFFSET + USABLE_WIDTH / 2 + 27;
  int xPos1 = xPos0 + 185;
  const int yPos0 = Y_OFFSET + 384;
  const int yPos1 = yPos0 + 110;

  // calculate y max/min and intervals
  int yMajorTicks = 5;
  int yTempMajorTicks = 5;

  float valMin = graph[0].value;
  float valMax = valMin;
  float previous_value = graph[0].prev_value;
  float newTemp = 0;

  //Search the first day to use
  int firstday = 0;
  for (int i = 1; i < 34; ++i)
  {
    firstday = i;
    if (graph[i].value == 0) { break;}
  }
  firstday = firstday - DAILY_GRAPH_MAX;

  for (int i = 1; i < DAILY_GRAPH_MAX; ++i)
  {

    newTemp = graph[i + firstday].value;

    valMin = std::min(valMin, newTemp);
    valMax = std::max(valMax, newTemp);

    previous_value = std::max<float>(previous_value, graph[i + firstday].prev_value);

  }

  int valBoundMin = static_cast<int>(valMin - 1) - modulo(static_cast<int>(valMin - 1), yTempMajorTicks);
  int valBoundMax = static_cast<int>(valMax + 1) + (yTempMajorTicks - modulo(static_cast<int>(valMax + 1), yTempMajorTicks));

  // while we have to many major ticks then increase the step
  while ((valBoundMax - valBoundMin) / yTempMajorTicks > yMajorTicks)
  {
    yTempMajorTicks += 5;
    valBoundMin = static_cast<int>(valMin - 1) - modulo(static_cast<int>(valMin - 1), yTempMajorTicks);
    valBoundMax = static_cast<int>(valMax + 1) + (yTempMajorTicks - modulo(static_cast<int>(valMax + 1), yTempMajorTicks));
  }

  // while we have not enough major ticks, add to either bound
  while ((valBoundMax - valBoundMin) / yTempMajorTicks < yMajorTicks)
  {
    // add to whatever bound is closer to the actual min/max
    if (valMin - valBoundMin <= valBoundMax - valMax)
    {
      valBoundMin -= yTempMajorTicks;
    }
    else
    {
      valBoundMax += yTempMajorTicks;
    }
  }

  // draw x axis
  display.drawLine(xPos0, yPos1    , xPos1, yPos1    , GxEPD_BLACK);
  display.drawLine(xPos0, yPos1 - 1, xPos1, yPos1 - 1, GxEPD_BLACK);

  // draw y axis
  float yInterval = (yPos1 - yPos0) / static_cast<float>(yMajorTicks);
  for (int i = 0; i <= yMajorTicks; ++i)
  {
    String dataStr;
    int yTick = static_cast<int>(yPos0 + (i * yInterval));
    display.setFont(&FONT_8pt8b);

    dataStr = String(valBoundMax - (i * yTempMajorTicks));
    //dataStr += "Kw/h";

    drawString(xPos0 - 8, yTick + 4, dataStr, RIGHT, ACCENT_COLOR);

    //Second scale
#if 0
    // PoP
    dataStr = String(100 - (i * 20));
    String precipUnit = "%";

    drawString(xPos1 + 8, yTick + 4, dataStr, LEFT);
    display.setFont(&FONT_5pt8b);
    drawString(display.getCursorX(), yTick + 4, precipUnit, LEFT);
#endif

    // draw dotted line
    if (i < yMajorTicks)
    {
      for (int x = xPos0; x <= xPos1 + 1; x += 3)
      {
        display.drawPixel(x, yTick + (yTick % 2), GxEPD_BLACK);
      }
    }
  }

  int xMaxTicks = 8;
  int hourInterval = static_cast<int>(ceil(DAILY_GRAPH_MAX / static_cast<float>(xMaxTicks)));
  float xInterval = (xPos1 - xPos0 - 1) / static_cast<float>(DAILY_GRAPH_MAX);
  display.setFont(&FONT_8pt8b);
  
  // precalculate all x and y coordinates for values
  float yPxPerUnit = (yPos1 - yPos0) / static_cast<float>(valBoundMax - valBoundMin);
  std::vector<int> x_t;
  std::vector<int> y_t;
  x_t.reserve(DAILY_GRAPH_MAX);
  y_t.reserve(DAILY_GRAPH_MAX);


  for (int i = 0; i < DAILY_GRAPH_MAX; ++i)
  {
    y_t[i] = static_cast<int>(std::round(yPos1 - (yPxPerUnit * (graph[i + firstday].value - valBoundMin))));
    x_t[i] = static_cast<int>(std::round(xPos0 + (i * xInterval) + (0.5 * xInterval) ));
  }

  display.setFont(&FONT_8pt8b);
  for (int i = 0; i < DAILY_GRAPH_MAX; ++i)
  {
    int xTick = static_cast<int>(xPos0 + (i * xInterval));
    int x0_t, x1_t, y0_t, y1_t;

    if (i > 0)
    {
      // value
      x0_t = x_t[i - 1];
      x1_t = x_t[i    ];
      y0_t = y_t[i - 1];
      y1_t = y_t[i    ];
      // graph value
      display.drawLine(x0_t    , y0_t    , x1_t    , y1_t    , ACCENT_COLOR);
      display.drawLine(x0_t    , y0_t + 1, x1_t    , y1_t + 1, ACCENT_COLOR);
      display.drawLine(x0_t - 1, y0_t    , x1_t - 1, y1_t    , ACCENT_COLOR);

    }

    float prevVal = graph[i + firstday].prev_value;

    x0_t = static_cast<int>(std::round( xPos0 + 1 + (i * xInterval)));
    x1_t = static_cast<int>(std::round( xPos0 + 1 + ((i + 1) * xInterval) ));
    
    //yPxPerUnit = (yPos1 - yPos0) / 10;
    yPxPerUnit = (yPos1 - yPos0) / static_cast<float>(valBoundMax - valBoundMin);

    y0_t = static_cast<int>(std::round( yPos1 - (yPxPerUnit * (prevVal)) ));
    y1_t = yPos1;

    // graph Precipitation
    drawAlphaBar(x0_t, y0_t, x1_t, y1_t, GxEPD_BLACK);

    if ((i % hourInterval) == 0)
    {
      // draw x tick marks
      display.drawLine(xTick    , yPos1 + 1, xTick    , yPos1 + 4, GxEPD_BLACK);
      display.drawLine(xTick + 1, yPos1 + 1, xTick + 1, yPos1 + 4, GxEPD_BLACK);

      // draw x axis labels
      drawString(xTick, yPos1 + 1 + 12 + 4 + 3, graph[i + firstday].dt, CENTER);

    }

  }

  return;
}


void drawOutlookGraph(const meteo_hourly_t *hourly, tm timeInfo)
{

  const int xPos0 = 274;
  int xPos1 = xPos0 + 160;
  const int yPos0 = 310 + 145 + 15;
  const int yPos1 = yPos0 + 120;

  // calculate y max/min and intervals
  int yMajorTicks = 5;
  int yTempMajorTicks = 5;

  float tempMin = hourly[0].temp;
  float tempMax = tempMin;
  float precipMax = hourly[0].pop;
  float newTemp = 0;

  for (int i = 1; i < HOURLY_GRAPH_MAX; ++i)
  {

    newTemp = hourly[i].temp;

    tempMin = std::min(tempMin, newTemp);
    tempMax = std::max(tempMax, newTemp);

    precipMax = std::max<float>(precipMax, hourly[i].pop);

  }

  int tempBoundMin = static_cast<int>(tempMin - 1) - modulo(static_cast<int>(tempMin - 1), yTempMajorTicks);
  int tempBoundMax = static_cast<int>(tempMax + 1) + (yTempMajorTicks - modulo(static_cast<int>(tempMax + 1), yTempMajorTicks));

  // while we have to many major ticks then increase the step
  while ((tempBoundMax - tempBoundMin) / yTempMajorTicks > yMajorTicks)
  {
    yTempMajorTicks += 5;
    tempBoundMin = static_cast<int>(tempMin - 1) - modulo(static_cast<int>(tempMin - 1), yTempMajorTicks);
    tempBoundMax = static_cast<int>(tempMax + 1) + (yTempMajorTicks - modulo(static_cast<int>(tempMax + 1), yTempMajorTicks));
  }

  // while we have not enough major ticks, add to either bound
  while ((tempBoundMax - tempBoundMin) / yTempMajorTicks < yMajorTicks)
  {
    // add to whatever bound is closer to the actual min/max
    if (tempMin - tempBoundMin <= tempBoundMax - tempMax)
    {
      tempBoundMin -= yTempMajorTicks;
    }
    else
    {
      tempBoundMax += yTempMajorTicks;
    }
  }

  float precipBoundMax;
  if (precipMax > 0)
  {
    precipBoundMax = 100.0f;
  }
  else
  {
    precipBoundMax = 0.0f;
  }

  // draw x axis
  display.drawLine(xPos0, yPos1    , xPos1, yPos1    , GxEPD_BLACK);
  display.drawLine(xPos0, yPos1 - 1, xPos1, yPos1 - 1, GxEPD_BLACK);

  // draw y axis
  float yInterval = (yPos1 - yPos0) / static_cast<float>(yMajorTicks);
  for (int i = 0; i <= yMajorTicks; ++i)
  {
    String dataStr;
    int yTick = static_cast<int>(yPos0 + (i * yInterval));
    display.setFont(&FONT_8pt8b);
    // Temperature
    dataStr = String(tempBoundMax - (i * yTempMajorTicks));
    dataStr += "\260";

    drawString(xPos0 - 8, yTick + 4, dataStr, RIGHT, ACCENT_COLOR);

    if (precipBoundMax > 0)
    { // don't labels if precip is 0

      // PoP
      dataStr = String(100 - (i * 20));
      String precipUnit = "%";

      drawString(xPos1 + 8, yTick + 4, dataStr, LEFT);
      display.setFont(&FONT_5pt8b);
      drawString(display.getCursorX(), yTick + 4, precipUnit, LEFT);
    } // end draw labels if precip is >0

    // draw dotted line
    if (i < yMajorTicks)
    {
      for (int x = xPos0; x <= xPos1 + 1; x += 3)
      {
        display.drawPixel(x, yTick + (yTick % 2), GxEPD_BLACK);
      }
    }
  }

  int xMaxTicks = 8;
  int hourInterval = static_cast<int>(ceil(HOURLY_GRAPH_MAX / static_cast<float>(xMaxTicks)));
  float xInterval = (xPos1 - xPos0 - 1) / static_cast<float>(HOURLY_GRAPH_MAX);
  display.setFont(&FONT_8pt8b);
  
  // precalculate all x and y coordinates for temperature values
  float yPxPerUnit = (yPos1 - yPos0) / static_cast<float>(tempBoundMax - tempBoundMin);
  std::vector<int> x_t;
  std::vector<int> y_t;
  x_t.reserve(HOURLY_GRAPH_MAX);
  y_t.reserve(HOURLY_GRAPH_MAX);

  for (int i = 0; i < HOURLY_GRAPH_MAX; ++i)
  {
    y_t[i] = kelvin_to_plot_y(hourly[i].temp, tempBoundMin, yPxPerUnit, yPos1);
    x_t[i] = static_cast<int>(std::round(xPos0 + (i * xInterval)
                                          + (0.5 * xInterval) ));
  }

  display.setFont(&FONT_8pt8b);
  for (int i = 0; i < HOURLY_GRAPH_MAX; ++i)
  {
    int xTick = static_cast<int>(xPos0 + (i * xInterval));
    int x0_t, x1_t, y0_t, y1_t;

    if (i > 0)
    {
      // temperature
      x0_t = x_t[i - 1];
      x1_t = x_t[i    ];
      y0_t = y_t[i - 1];
      y1_t = y_t[i    ];
      // graph temperature
      display.drawLine(x0_t    , y0_t    , x1_t    , y1_t    , ACCENT_COLOR);
      display.drawLine(x0_t    , y0_t + 1, x1_t    , y1_t + 1, ACCENT_COLOR);
      display.drawLine(x0_t - 1, y0_t    , x1_t - 1, y1_t    , ACCENT_COLOR);

    }

    float precipVal = hourly[i].pop;

    x0_t = static_cast<int>(std::round( xPos0 + 1 + (i * xInterval)));
    x1_t = static_cast<int>(std::round( xPos0 + 1 + ((i + 1) * xInterval) ));
    yPxPerUnit = (yPos1 - yPos0) / precipBoundMax;
    y0_t = static_cast<int>(std::round( yPos1 - (yPxPerUnit * (precipVal)) ));
    y1_t = yPos1;

    // graph Precipitation
    drawAlphaBar(x0_t, y0_t, x1_t, y1_t, GxEPD_BLACK);

    if ((i % hourInterval) == 0)
    {
      // draw x tick marks
      display.drawLine(xTick    , yPos1 + 1, xTick    , yPos1 + 4, GxEPD_BLACK);
      display.drawLine(xTick + 1, yPos1 + 1, xTick + 1, yPos1 + 4, GxEPD_BLACK);
      // draw x axis labels
      char timeBuffer[12] = {}; // big enough to accommodate "hh:mm:ss am"
      time_t ts = hourly[i].dt;
      tm *timeInfo = localtime(&ts);
      _strftime(timeBuffer, sizeof(timeBuffer), HOUR_FORMAT, timeInfo);
      drawString(xTick, yPos1 + 1 + 12 + 4 + 3, timeBuffer, CENTER);
    }

  }

  // draw the last tick mark
  if ((HOURLY_GRAPH_MAX % hourInterval) == 0)
  {
    int xTick = static_cast<int>(
                std::round(xPos0 + (HOURLY_GRAPH_MAX * xInterval)));
    // draw x tick marks
    display.drawLine(xTick    , yPos1 + 1, xTick    , yPos1 + 4, GxEPD_BLACK);
    display.drawLine(xTick + 1, yPos1 + 1, xTick + 1, yPos1 + 4, GxEPD_BLACK);
    // draw x axis labels
    char timeBuffer[12] = {}; // big enough to accommodate "hh:mm:ss am"
    time_t ts = hourly[HOURLY_GRAPH_MAX - 1].dt + 3600;
    tm *timeInfo = localtime(&ts);
    _strftime(timeBuffer, sizeof(timeBuffer), HOUR_FORMAT, timeInfo);
    drawString(xTick, yPos1 + 1 + 12 + 4 + 3, timeBuffer, CENTER);
  }

  return;
} // end drawOutlookGraph




/* This function is responsible for drawing the status bar along the bottom of
 * the display.
 */
void drawStatusBar(const String &statusStr, const String &refreshTimeStr, int rssi, uint32_t batVoltage)
{
  String dataStr;
  uint16_t dataColor = GxEPD_BLACK;
  display.setFont(&FONT_6pt8b);
  int pos = USABLE_WIDTH - 2;
  const int sp = 2;

#if BATTERY_MONITORING
  // battery - (expecting 3.7v LiPo)
  uint32_t batPercent = calcBatPercent(batVoltage, MIN_BATTERY_VOLTAGE, MAX_BATTERY_VOLTAGE);
#if defined(DISP_3C_B) || defined(DISP_7C_F)
  if (batVoltage < WARN_BATTERY_VOLTAGE)
  {
    dataColor = ACCENT_COLOR;
  }
#endif
  dataStr = String(batPercent) + "%";
#if STATUS_BAR_EXTRAS_BAT_VOLTAGE
  dataStr += " (" + String( std::round(batVoltage / 10.f) / 100.f, 2 ) + "v)";
#endif
  drawString(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
  pos -= getStringWidth(dataStr) + 25;
  display.drawInvertedBitmap(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 17, getBatBitmap24(batPercent), 24, 24, dataColor);
  pos -= sp + 9;
#endif

  // WiFi
  dataStr = String(getWiFidesc(rssi));
  dataColor = rssi >= -70 ? GxEPD_BLACK : ACCENT_COLOR;
#if STATUS_BAR_EXTRAS_WIFI_RSSI
  if (rssi != 0)
  {
    dataStr += " (" + String(rssi) + "dBm)";
  }
#endif
  drawString(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
  pos -= getStringWidth(dataStr) + 19;
  display.drawInvertedBitmap(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 13, getWiFiBitmap16(rssi), 16, 16, dataColor);
  pos -= sp + 8;

  // last refresh
  dataColor = GxEPD_BLACK;
  drawString(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 2, refreshTimeStr, RIGHT, dataColor);
  pos -= getStringWidth(refreshTimeStr) + 25;
  display.drawInvertedBitmap(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 21, wi_refresh_32x32, 32, 32, dataColor);
  pos -= sp;

  // status
  dataColor = ACCENT_COLOR;
  if (!statusStr.isEmpty())
  {
    drawString(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 2, statusStr, RIGHT, dataColor);
    pos -= getStringWidth(statusStr) + 24;
    display.drawInvertedBitmap(X_OFFSET + pos, Y_OFFSET + USABLE_HEIGHT - 1 - 18, error_icon_24x24, 24, 24, dataColor);
  }

  return;
} // end drawStatusBar

/* This function is responsible for drawing prominent error messages to the
 * screen.
 *
 * If error message line 2 (errMsgLn2) is empty, line 1 will be automatically
 * wrapped.
 */
void drawError(const uint8_t *bitmap_196x196, const String &errMsgLn1, const String &errMsgLn2)
{
  display.setFont(&FONT_26pt8b);
  if (!errMsgLn2.isEmpty())
  {
    drawString(USABLE_WIDTH / 2, USABLE_HEIGHT / 2 + 196 / 2 + 21, errMsgLn1, CENTER);
    drawString(USABLE_WIDTH / 2, USABLE_HEIGHT / 2 + 196 / 2 + 21 + 55, errMsgLn2, CENTER);
  }
  else
  {
    drawMultiLnString(USABLE_WIDTH / 2, USABLE_HEIGHT / 2 + 196 / 2 + 21, errMsgLn1, CENTER, USABLE_WIDTH - 200, 2, 55);
  }
  display.drawInvertedBitmap(USABLE_WIDTH / 2 - 196 / 2, USABLE_HEIGHT / 2 - 196 / 2 - 21 - 100, bitmap_196x196, 196, 196, ACCENT_COLOR);

  return;
} // end drawError

