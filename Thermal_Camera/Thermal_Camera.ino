/*
 * Project Name: MLX90640 Thermal Camera
 * Project Brief: Firmware for Thermal camera built around ESP32 and MLX90640
 * Author: Jobit Joseph
 * Copyright © Jobit Joseph
 * Copyright © Semicon Media Pvt Ltd
 * Copyright © Circuitdigest.com
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <Preferences.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "FS.h"
#include <Adafruit_MLX90640.h>
#include <TFT_eSPI.h>
#include <AnimatedGIF.h>
#include <Fonts/GFXFF/gfxfont.h>  //Include a library of Fonts
#include "Open_Sans_ExtraBold_10.h"
#include "BootAnimation.h"
#include "success.h"
#include "Error.h"
SPIClass spiSD(HSPI);
#define SD_CS 15
//#define USE_DMA
#define NORMAL_SPEED
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
#define BUFFER_SIZE 320
uint16_t usTemp[BUFFER_SIZE];
#define VBAT_PIN 33
#define BATTV_MAX 4.2  // maximum voltage of battery
#define BATTV_MIN 3.2  // what we regard as an empty battery
#define GIF_IMAGE BootAnimationIMG
#define SGIF_IMAGE success_GIF
#define EGIF_IMAGE Error_GIF
bool dmaBuf = 0;
Adafruit_MLX90640 mlx;
Preferences preferences;
AnimatedGIF gif;
TFT_eSPI tft = TFT_eSPI();
File bmpFile;
float frame[32 * 24];
float batv;
const int upButton = 35;
const int middleButton = 36;
const int downButton = 39;
const int backlightPin = 4;
int interpolationMode = 0;
int AutoScale = 0;
volatile bool MenuChange = false;
volatile bool _upShort = false;
volatile bool _downShort = false;
volatile bool middleShort = false;
volatile bool middlePressed = false;
unsigned long middlePressStartTime = 0;
int Menu = 0, Menuitem = 0;
int BLPWM = 0;
int RefreshRate = 0;
float MinT, MaxT;
String menuItems[7] = { "Auto Scale    : ", "Min Temp      : ", "Max Temp      : ", "Interpolation : ", "Palette       : ", "Refresh Rate  : ", "BackLight     : " };
String ASindex[2] = { "Off", "On" };
String IPindex[6] = { "Nearest Neighbor", "Average", "Bilinear", "Bilinear Fast", "Triangle" };
String RRindex[4] = { "4 Hz", "8 Hz", "16 Hz", "32 Hz" };
float xRatios[320];
float yRatios[240];
float xOppositeRatios[320];
float yOppositeRatios[240];
#define PALETTE_COUNT 10
uint16_t colorPalettes[PALETTE_COUNT][6] = {
  { TFT_BLUE, TFT_CYAN, TFT_GREEN, TFT_YELLOW, TFT_RED, TFT_MAGENTA },
  { TFT_BLACK, TFT_DARKGREY, TFT_LIGHTGREY, TFT_WHITE, TFT_ORANGE, TFT_PINK },
  { TFT_NAVY, TFT_OLIVE, TFT_DARKGREEN, TFT_DARKCYAN, TFT_MAROON, TFT_PURPLE },
  { TFT_BLUE, TFT_GREEN, TFT_DARKGREEN, TFT_ORANGE, TFT_MAROON, TFT_RED },
  { TFT_NAVY, TFT_DARKGREEN, TFT_GREEN, TFT_YELLOW, TFT_ORANGE, TFT_RED },
  { TFT_CYAN, TFT_BLUE, TFT_MAGENTA, TFT_YELLOW, TFT_GREEN, TFT_RED },
  { TFT_WHITE, TFT_ORANGE, TFT_RED, TFT_BLUE, TFT_GREEN, TFT_BLACK },
  { TFT_PURPLE, TFT_MAGENTA, TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN },
  { TFT_YELLOW, TFT_PINK, TFT_WHITE, TFT_BLUE, TFT_DARKCYAN, TFT_DARKGREEN },
  { TFT_RED, TFT_YELLOW, TFT_GREEN, TFT_CYAN, TFT_BLUE, TFT_MAGENTA }
};
int paletteIndex = 0;

float tempMin = 20.0;  // Minimum temperature
float tempMax = 32.0;  // Maximum temperature

TFT_eSprite sprite = TFT_eSprite(&tft);


void IRAM_ATTR upButton_ISR() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200) {  // simple debounce
    _upShort = true;
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR downButton_ISR() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200) {  // simple debounce
    _downShort = true;
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR middleButton_ISR() {
  if (digitalRead(middleButton) == LOW) {
    // Button press event
    if (!middlePressed) {  // If button was not already being pressed
      middlePressed = true;
      middlePressStartTime = millis();  // Save the start time of button press
    }
  } else {
    // Button release event
    if (middlePressed) {                             // If the button was being pressed
      if (millis() - middlePressStartTime < 1000) {  // If the button was pressed for less than 1 second
        middleShort = true;
      }
      middlePressed = false;
      middlePressStartTime = 0;  // Reset the start time of button press
    }
  }
}



void MLXInit() {

  Wire.begin();
  Wire.setClock(1000000);
  if (!mlx.begin()) {
    Serial.println("Failed to initialize MLX90640!");
  }
  mlx.setResolution(MLX90640_ADC_18BIT);
  ConfigRefreshrate();
}
void ConfigRefreshrate() {
  switch (RefreshRate) {
    case 1:
      mlx.setRefreshRate(MLX90640_4_HZ);
      break;
    case 2:
      mlx.setRefreshRate(MLX90640_8_HZ);
      break;
    case 3:
      mlx.setRefreshRate(MLX90640_16_HZ);
      break;
    case 4:
      mlx.setRefreshRate(MLX90640_32_HZ);
      break;
    default:
      break;
  }
}
void ReadConfig() {
  preferences.begin("Config", false);
  String temp;
  temp = preferences.getString("AutoScale", "");
  AutoScale = temp.toInt();
  temp = preferences.getString("MinTe", "");
  MinT = temp.toFloat();
  temp = preferences.getString("MaxTe", "");
  MaxT = temp.toFloat();
  temp = preferences.getString("Imode", "");
  interpolationMode = temp.toInt();
  temp = preferences.getString("PIndex", "");
  paletteIndex = temp.toInt();
  temp = preferences.getString("RefreshRate", "");
  RefreshRate = temp.toInt();
  temp = preferences.getString("BLPWM", "");
  BLPWM = temp.toInt();
  preferences.end();
}
void WriteConfig() {
  preferences.begin("Config", false);
  preferences.putString("AutoScale", String(AutoScale));
  preferences.putString("MinTe", String(MinT));
  preferences.putString("MaxTe", String(MaxT));
  preferences.putString("Imode", String(interpolationMode));
  preferences.putString("PIndex", String(paletteIndex));
  preferences.putString("RefreshRate", String(RefreshRate));
  preferences.putString("BLPWM", String(BLPWM));
  preferences.end();
}

void initSDcard() {
  //spiSD.begin(14, 12, 13, SD_CS);  //CLK,MISO,MOIS,SS
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("Card Mount Failed");
    return;
  } else {
    Serial.println("Card Mount Successful");
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
}
String generateFilename(fs::FS &fs) {
  for (int i = 0; i <= 9999; i++) {
    char filename[23];                               // Allocate the char array
    sprintf(filename, "/ThermalCamera%04d.bmp", i);  // Print formatted string into char array

    if (!fs.exists(filename)) {
      return String(filename);  // Return a String object
    }
  }

  return "";  // return empty string if all filenames are taken
}
int writeBMP(fs::FS &fs, const char *path, TFT_eSprite *sprite) {
  const int width = sprite->width();
  const int height = sprite->height();

  // BMP file header (14 bytes)
  uint8_t bmpFileHeader[14] = { 'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0 };

  // The size of the BMP file in bytes
  uint32_t fileSize = 54 + width * height * 2;
  bmpFileHeader[2] = (uint8_t)(fileSize);
  bmpFileHeader[3] = (uint8_t)(fileSize >> 8);
  bmpFileHeader[4] = (uint8_t)(fileSize >> 16);
  bmpFileHeader[5] = (uint8_t)(fileSize >> 24);

  // BMP info header (40 bytes)
  uint8_t bmpInfoHeader[40] = { 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 16, 0 };

  bmpInfoHeader[4] = (uint8_t)(width);
  bmpInfoHeader[5] = (uint8_t)(width >> 8);
  bmpInfoHeader[6] = (uint8_t)(width >> 16);
  bmpInfoHeader[7] = (uint8_t)(width >> 24);

  bmpInfoHeader[8] = (uint8_t)(height);
  bmpInfoHeader[9] = (uint8_t)(height >> 8);
  bmpInfoHeader[10] = (uint8_t)(height >> 16);
  bmpInfoHeader[11] = (uint8_t)(height >> 24);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return 0;
  } else {

    //showSavingImageMessage();
    // Write headers
    file.write(bmpFileHeader, 14);
    file.write(bmpInfoHeader, 40);

    // Write pixel data
    for (int y = height - 1; y >= 0; y--) {  // BMP is stored bottom-top
      for (int x = 0; x < width; x++) {
        uint16_t pixel = sprite->readPixel(x, y);
        // Swap red and green channels
        uint16_t r = (pixel >> 11) & 0x1F;
        uint16_t g = (pixel >> 5) & 0x3F;
        uint16_t b = pixel & 0x1F;
        pixel = (b << 11) | (r << 5) | g;
        file.write(pixel >> 8);  // high byte
        file.write(pixel);       // low byte
      }
    }
    file.close();
    return 1;
  }
}


void navigationUpdate() {
  if (_upShort == true) {
    _upShort = false;
    if (Menu == 0) {
      interpolationMode++;
      if (interpolationMode > 4) interpolationMode = 0;
    } else {
      if (Menuitem == 0) {
        AutoScale++;
        if (AutoScale > 1) AutoScale = 1;
      } else if (Menuitem == 1) {
        MinT++;
        if (MinT > 300) MinT = 300;
      } else if (Menuitem == 2) {
        MaxT++;
        if (MaxT > 300) MaxT = 300;
      } else if (Menuitem == 3) {
        interpolationMode++;
        if (interpolationMode > 4) interpolationMode = 4;
      } else if (Menuitem == 4) {
        paletteIndex++;
        if (paletteIndex > 9) paletteIndex = 9;
      } else if (Menuitem == 5) {
        RefreshRate++;
        if (RefreshRate > 3) RefreshRate = 3;
        ConfigRefreshrate();
      } else if (Menuitem == 6) {
        BLPWM = BLPWM + 10;
        if (BLPWM > 100) BLPWM = 100;
        analogWrite(backlightPin, map(BLPWM, 0, 100, 0, 255));  // Turn on backlight
      }
      MenuChange = true;
    }
    WriteConfig();
    Serial.println("Up Short Press");
  }
  if (_downShort == true) {
    _downShort = false;
    if (Menu == 0) {
      paletteIndex = (paletteIndex + 1) % PALETTE_COUNT;
    } else {
      if (Menuitem == 0) {
        AutoScale--;
        if (AutoScale < 0) AutoScale = 0;
      } else if (Menuitem == 1) {
        MinT--;
        if (MinT < 0) MinT = 0;
      } else if (Menuitem == 2) {
        MaxT--;
        if (MaxT < 5) MaxT = 5;
      } else if (Menuitem == 3) {
        interpolationMode--;
        if (interpolationMode < 0) interpolationMode = 0;
      } else if (Menuitem == 4) {
        paletteIndex--;
        if (paletteIndex < 0) paletteIndex = 0;
      } else if (Menuitem == 5) {
        RefreshRate--;
        if (RefreshRate < 0) RefreshRate = 0;
        ConfigRefreshrate();
      } else if (Menuitem == 6) {
        BLPWM = BLPWM - 10;
        if (BLPWM < 10) BLPWM = 10;
        analogWrite(backlightPin, map(BLPWM, 0, 100, 0, 255));  // Turn on backlight
      }
      MenuChange = true;
    }
    WriteConfig();
    Serial.println("down Short Press");
  }
  if (middleShort == true) {
    middleShort = false;
    if (Menu == 0) {
      if (!SD.begin(SD_CS, spiSD)) {
        Serial.println("Card Mount Failed");
        if (gif.open((uint8_t *)EGIF_IMAGE, sizeof(EGIF_IMAGE), GIFDraw1)) {
          Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
          while (gif.playFrame(true, NULL)) {
            sprite.pushSprite(0, 0);  // Push the sprite to screen after every frame
            yield();
          }
          gif.close();
        }
        return;
      } else {
        Serial.println("Card Mount Successful");
      }
      uint8_t cardType = SD.cardType();

      if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");

        if (gif.open((uint8_t *)EGIF_IMAGE, sizeof(EGIF_IMAGE), GIFDraw1)) {
          Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
          while (gif.playFrame(true, NULL)) {
            sprite.pushSprite(0, 0);  // Push the sprite to screen after every frame
            yield();
          }
          gif.close();
        }
        return;
      } else {
        String filename = generateFilename(SD);
        if (filename != "") {
          if (writeBMP(SD, filename.c_str(), &sprite)) {

            if (gif.open((uint8_t *)SGIF_IMAGE, sizeof(SGIF_IMAGE), GIFDraw1)) {
              Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
              while (gif.playFrame(true, NULL)) {
                sprite.pushSprite(0, 0);  // Push the sprite to screen after every frame
                yield();
              }
              gif.close();
            }
          } else {

            if (gif.open((uint8_t *)EGIF_IMAGE, sizeof(EGIF_IMAGE), GIFDraw1)) {
              Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
              while (gif.playFrame(true, NULL)) {
                sprite.pushSprite(0, 0);  // Push the sprite to screen after every frame
                yield();
              }
              gif.close();
            }
          }
        } else {
          Serial.println("Failed to create filename.");

          if (gif.open((uint8_t *)EGIF_IMAGE, sizeof(EGIF_IMAGE), GIFDraw1)) {
            Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
            while (gif.playFrame(true, NULL)) {
              sprite.pushSprite(0, 0);  // Push the sprite to screen after every frame
              yield();
            }
            gif.close();
          }
        }
      }
    } else {
      Menuitem++;
      if (Menuitem > 6) {
        Menuitem = 0;
      }
      MenuChange = true;
    }
    Serial.println("Middle Short Press");
    Serial.print(Menu);
    Serial.print("");
    Serial.println(Menuitem);
  }
  if (middlePressed && !middleShort && millis() - middlePressStartTime > 1000 && middlePressed && !middleShort && millis() - middlePressStartTime < 1500) {
    // If the button is being pressed, no short press has been registered, and it has been over 1 second
    Serial.println("Middle Long Press");
    middlePressed = false;     // Reset the pressed flag
    middlePressStartTime = 0;  // Reset the press start time
    if (Menu == 0) {
      Menu = 1;
      MenuChange = true;
      Menuitem = 0;
    } else {
      Menu = 0;
    }
  }
}

void displayUpdate() {

  if (!mlx.getFrame(frame)) {
    // Failed to get frame, so reinitialize
    MLXInit();
  }

  float tempMinRead = 1000;   // some high value
  float tempMaxRead = -1000;  // some low value
  float tempCenter = 0.0;     // Temperature at center

  // Update the minimum and maximum temperatures read from the sensor
  for (int i = 0; i < 32 * 24; i++) {
    if (frame[i] < tempMinRead) {
      tempMinRead = frame[i];
    }
    if (frame[i] > tempMaxRead) {
      tempMaxRead = frame[i];
    }
    // Get the temperature at center
    if (i == 32 * 12 + 16) {
      tempCenter = frame[i];
    }
  }


  sprite.fillSprite(TFT_BLACK);

  if (interpolationMode == 0) {
    // Nearest neighbor interpolation
    for (int y = 0; y < 240; y++) {
      int yIndex = (y / 10) * 32;
      for (int x = 0; x < 320; x++) {
        float val = frame[yIndex + (x / 10)];
        drawPixel(319 - x, y, val);
      }
    }
  } else if (interpolationMode == 1) {
    // Average Interpolation
    for (int y = 0; y < 240; y++) {
      int yIndex = (y / 10) * 32;
      int yNextIndex = ((y / 10) + 1) * 32;  // Next row in original data
      for (int x = 0; x < 320; x++) {
        int xIndex = x / 10;

        // Take average of current and next points in x and y
        float val = (frame[yIndex + xIndex] + frame[yIndex + xIndex + 1] + frame[yNextIndex + xIndex] + frame[yNextIndex + xIndex + 1]) / 4.0;

        drawPixel(319 - x, y, val);
      }
    }
  } else if (interpolationMode == 2) {
    // Bilinear interpolation
    for (int y = 0; y < 240; y++) {
      int yIndex = (y / 10) * 32;
      float y_ratio = (y % 10) / 10.0;
      float y_opposite_ratio = 1 - y_ratio;
      for (int x = 0; x < 320; x++) {
        float x_ratio = (x % 10) / 10.0;
        float x_opposite_ratio = 1 - x_ratio;
        int x_over_10 = x / 10;
        float val = y_opposite_ratio * (x_opposite_ratio * frame[yIndex + x_over_10] + x_ratio * frame[yIndex + x_over_10 + 1]) + y_ratio * (x_opposite_ratio * frame[(yIndex + 32) + x_over_10] + x_ratio * frame[(yIndex + 32) + x_over_10 + 1]);
        drawPixel(319 - x, y, val);
      }
    }
  } else if (interpolationMode == 3) {
    // Bilinear interpolation
    int yIndex;
    int xIndex;
    int yNextIndex;

    for (int y = 0; y < 240; y++) {
      yIndex = (y / 10) * 32;
      yNextIndex = yIndex + 32;
      for (int x = 0; x < 320; x++) {
        xIndex = x / 10;
        float val = yOppositeRatios[y] * (xOppositeRatios[x] * frame[yIndex + xIndex] + xRatios[x] * frame[yIndex + xIndex + 1]) + yRatios[y] * (xOppositeRatios[x] * frame[yNextIndex + xIndex] + xRatios[x] * frame[yNextIndex + xIndex + 1]);
        drawPixel(319 - x, y, val);
      }
    }
  }

  else if (interpolationMode == 4) {
    for (int y = 0; y < 240; y++) {
      int yIndex = (y / 10) * 32;
      int yNextIndex = ((y / 10) + 1) * 32;  // Next row in original data
      for (int x = 0; x < 320; x++) {
        int xIndex = x / 10;

        // Determine which triangle the point is in and interpolate accordingly
        float t = (x % 10) / 10.0f;  // Horizontal distance from left pixel center
        float u = (y % 10) / 10.0f;  // Vertical distance from top pixel center
        float val;
        if (t > u) {  // Point is in lower-left triangle
          // Interpolate between bottom-left, top-right, and bottom-right
          val = (1 - t) * frame[yNextIndex + xIndex] + (1 - u) * frame[yIndex + xIndex + 1] + (t + u - 1) * frame[yNextIndex + xIndex + 1];
        } else {  // Point is in upper-right triangle
          // Interpolate between top-left, bottom-right, and top-right
          val = (1 - t) * frame[yIndex + xIndex] + (1 - u) * frame[yNextIndex + xIndex + 1] + (t + u - 1) * frame[yIndex + xIndex + 1];
        }

        drawPixel(319 - x, y, val);
      }
    }
  }



  // Display the maximum, minimum and center temperatures as overlay
  sprite.setTextColor(TFT_BLACK);
  sprite.setTextSize(1);

  sprite.setFreeFont(&Open_Sans_ExtraBold_10);

  sprite.drawString("T Min: " + String(tempMinRead) + " C", 15, 220);
  sprite.drawString("T Max: " + String(tempMaxRead) + " C", 220, 220);
  sprite.drawString("Tc: " + String(tempCenter, 1) + "C", 135, 220);
  // Display a small crosshair at the center
  sprite.drawLine(155, 120, 165, 120, TFT_WHITE);
  sprite.drawLine(160, 115, 160, 125, TFT_WHITE);

  detachInterrupt(digitalPinToInterrupt(downButton));
  batv = ((float)analogRead(VBAT_PIN) / 4095) * 2 * 1.07 * 3.3;
  attachInterrupt(digitalPinToInterrupt(downButton), downButton_ISR, CHANGE);
  int batpc = (uint8_t)(((batv - BATTV_MIN) / (BATTV_MAX - BATTV_MIN)) * 100);
  drawBattery(batpc);
  if (Menu == 1) {
    MenuChange = false;
    sprite.fillRect(60, 40, 200, 120, TFT_BLACK);
    sprite.fillRect(60, 40, 200, 10, TFT_WHITE);
    sprite.setTextFont(0);
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_BLACK);
    sprite.drawString("Settings", 140, 41);
    sprite.setTextColor(TFT_WHITE);
    for (int i = 0; i < 7; i++) {
      if (i == Menuitem) sprite.setTextColor(TFT_GREEN);
      int y = 55 + (15 * i);
      sprite.drawString(menuItems[i], 65, y);
      if (i == 0) {
        sprite.drawString(ASindex[AutoScale], 160, y);
      } else if (i == 1) {
        sprite.drawString(String(MinT) + " C", 160, y);
      } else if (i == 2) {
        sprite.drawString(String(MaxT) + " C", 160, y);
      } else if (i == 3) {
        sprite.drawString(IPindex[interpolationMode], 160, y);
      } else if (i == 4) {
        sprite.drawString("Palette " + String(paletteIndex), 160, y);
      } else if (i == 5) {
        sprite.drawString(RRindex[RefreshRate], 160, y);
      } else if (i == 6) {
        sprite.drawString(String(BLPWM) + " %", 160, y);
      }
      sprite.setTextColor(TFT_WHITE);
    }
  }
  sprite.pushSprite(0, 0);
}

void drawPixel(int x, int y, float val) {
  int colorIndex;
  if (AutoScale == 1) {
    if (val <= tempMin) {
      colorIndex = 0;
    } else if (val >= tempMax) {
      colorIndex = 5;
    } else {
      // Map the value to a color index between 0 and 5
      colorIndex = int(map(val, tempMin, tempMax, 0, 5));
    }
  } else {
    if (val <= MinT) {
      colorIndex = 0;
    } else if (val >= MaxT) {
      colorIndex = 5;
    } else {
      // Map the value to a color index between 0 and 5
      colorIndex = int(map(val, MinT, MaxT, 0, 5));
    }
  }
  int color = colorPalettes[paletteIndex][colorIndex];
  sprite.drawPixel(x, y, color);
}
void drawBattery(int batpc) {
  int x = 290;
  int y = 10;
  int w = 20;
  int h = 10;
  int color = TFT_RED;  // Default color for the lowest level


  // Determine fill level and color based on batpc
  int fillLevel = 0;
  if (batpc > 75) {
    fillLevel = w;  // 100%
    color = TFT_GREEN;
  } else if (batpc > 50) {
    fillLevel = w * 3 / 4;  // 75%
    color = TFT_GREEN;
  } else if (batpc > 25) {
    fillLevel = w / 2;  // 50%
    color = TFT_BLUE;
  } else if (batpc > 0) {
    fillLevel = w / 4;  // 25%
    color = TFT_BLUE;
  }


  // Draw battery outline
  sprite.drawRect(x, y, w, h, TFT_WHITE);
  sprite.drawRect(x + w, y + h / 4, 2, h / 2, TFT_WHITE);
  // Draw fill level
  if (fillLevel > 0) {
    sprite.fillRect(x + 1, y + 1, fillLevel - 2, h - 2, color);
  }
}


void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > TFT_WIDTH)
    iWidth = TFT_WIDTH - pDraw->iX;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y;

  if (y >= TFT_HEIGHT || pDraw->iX >= TFT_WIDTH || iWidth < 1)
    return;

  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  s = pDraw->pPixels;
  if (pDraw->ucHasTransparency) {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    pEnd = s + iWidth;
    x = 0;
    while (x < iWidth) {
      c = ucTransparent - 1;
      d = &usTemp[0];
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          s--;
        } else {
          *d++ = usPalette[c];
        }
      }
      if (d > &usTemp[0]) {
        sprite.pushImage(pDraw->iX + x, y, d - &usTemp[0], 1, usTemp);  // Push the image to the sprite
        x += d - &usTemp[0];
      }
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  } else {
    s = pDraw->pPixels;
    for (x = 0; x < iWidth; x++) {
      usTemp[x] = usPalette[*s++];
    }
    sprite.pushImage(pDraw->iX, y, iWidth, 1, usTemp);  // Push the image to the sprite
  }
}


void GIFDraw1(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > TFT_WIDTH)
    iWidth = TFT_WIDTH - pDraw->iX;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y;

  if (y >= TFT_HEIGHT || pDraw->iX >= TFT_WIDTH || iWidth < 1)
    return;

  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  s = pDraw->pPixels;
  if (pDraw->ucHasTransparency) {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    pEnd = s + iWidth;
    x = 0;
    while (x < iWidth) {
      c = ucTransparent - 1;
      d = &usTemp[0];
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          s--;
        } else {
          *d++ = usPalette[c];
        }
      }
      if (d > &usTemp[0]) {
        sprite.pushImage(pDraw->iX + x +80, y + 40, d - &usTemp[0], 1, usTemp);  // Push the image to the sprite
        x += d - &usTemp[0];
      }
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  } else {
    s = pDraw->pPixels;
    for (x = 0; x < iWidth; x++) {
      usTemp[x] = usPalette[*s++];
    }
    sprite.pushImage(pDraw->iX + 80 , y + 40 , iWidth, 1, usTemp);  // Push the image to the sprite
  }
}



void setup() {
  Serial.begin(115200);
  tft.init();
#ifdef USE_DMA
  tft.initDMA();
#endif
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  gif.begin(BIG_ENDIAN_PIXELS);
  sprite.createSprite(320, 240);

  pinMode(upButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(upButton), upButton_ISR, CHANGE);

  pinMode(downButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(downButton), downButton_ISR, CHANGE);

  pinMode(middleButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(middleButton), middleButton_ISR, CHANGE);
  ReadConfig();
  MLXInit();
  if (MinT == 0 && MaxT == 0) {
    MinT = 20.0;
    MaxT = 32.0;
    WriteConfig();
  }
  if (BLPWM == 0) {
    BLPWM = 100;
    WriteConfig();
  }

  // mlx.setRefreshRate(MLX90640_32_HZ);
  pinMode(backlightPin, OUTPUT);
  analogWrite(backlightPin, map(BLPWM, 0, 100, 0, 255));  // Turn on backlight
  if (gif.open((uint8_t *)GIF_IMAGE, sizeof(GIF_IMAGE), GIFDraw)) {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    while (gif.playFrame(true, NULL)) {
      sprite.pushSprite(0, 0);  // Push the sprite to screen after every frame
      yield();
    }
    gif.close();
  }
  delay(1000);
  initSDcard();
  // Precompute ratios
  for (int x = 0; x < 320; x++) {
    xRatios[x] = (x % 10) / 10.0f;
    xOppositeRatios[x] = 1 - xRatios[x];
  }
  for (int y = 0; y < 240; y++) {
    yRatios[y] = (y % 10) / 10.0f;
    yOppositeRatios[y] = 1 - yRatios[y];
  }
}

void loop() {
  navigationUpdate();
  displayUpdate();
}
