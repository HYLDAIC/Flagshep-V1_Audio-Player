#include <Arduino.h>
#include <SD_MMC.h>
#include <FS.h>
#include <Wire.h>
#include <WiFi.h>
#include <Audio.h>        // ESP32-audioI2S by schreibfaul1
#include <Adafruit_GFX.h>     // Adafruit GFX Library
#include <Adafruit_SSD1306.h> // Adafruit SSD1306 Library
#include "esp_pm.h"
#include "esp_bt.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_pm.h"

#define ENABLE_DEBUG_SERIAL 0

#if ENABLE_DEBUG_SERIAL
  #define DBG_BEGIN(baud)   Serial.begin(baud)
  #define DBG_PRINT(x)      Serial.print(x)
  #define DBG_PRINTLN(x)    Serial.println(x)
#else
  #define DBG_BEGIN(baud)
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
#endif

const uint8_t COL1 = 3;
const uint8_t COL2 = 13;
const uint8_t COL3 = 14;
const uint8_t ROW1 = 4;
const uint8_t ROW2 = 5;

const uint8_t ROWS = 2;
const uint8_t COLS = 3;
const uint8_t rowPins[ROWS] = {ROW1, ROW2};
const uint8_t colPins[COLS] = {COL1, COL2, COL3};

void enterDeepSleep();

enum ButtonSlot {
  SLOT_R1C1 = 0,
  SLOT_R1C2,
  SLOT_R1C3,
  SLOT_R2C1,
  SLOT_R2C2,
  SLOT_R2C3      // Switch Menus
};

const ButtonSlot slotMap[ROWS][COLS] = {
  { SLOT_R1C1, SLOT_R1C2, SLOT_R1C3 },
  { SLOT_R2C1, SLOT_R2C2, SLOT_R2C3 }
};

enum MenuScreen {
  MENU_MAIN,       // Normal
  MENU_SECONDARY   // Extras
};

MenuScreen currentMenu = MENU_MAIN;

bool lastRawState[ROWS][COLS];
bool stableState[ROWS][COLS];
unsigned long lastChangeTime[ROWS][COLS];

const uint8_t PCM_LRCK = 6;
const uint8_t PCM_DIN  = 7;
const uint8_t PCM_BCK  = 15;
const uint8_t PCM_SCK  = 16;

const uint8_t BAT_SDA = 21;
const uint8_t BAT_SCL = 47;

TwoWire batteryWire = TwoWire(1);
const uint8_t MAX17048_ADDR = 0x36;
const uint8_t MAX17048_REG_SOC = 0x04; //State-of-charge register

//Intervals 
const unsigned long DEBOUNCE_MS = 30;
const unsigned long BUTTON_SCAN_INTERVAL_MS = 8;
unsigned long lastButtonScanTime = 0;

unsigned long lastBatteryCheckTime = 0;
const unsigned long BATTERY_CHECK_INTERVAL_MS = 10000; //Every 10 sec

unsigned long lastActivityTime = 0;
const unsigned long INACTIVITY_SLEEP_MS = 20000; // 20s idle -> sleep

const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 300;
unsigned long lastDisplayUpdateTime = 0;

float lastBatteryPercent = -1.0f; //-1.0f = no valid reading

const uint8_t TFT_SCL = 8;
const uint8_t TFT_SDA = 18;

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define SCREEN_I2C_ADDR 0x3C

TwoWire displayWire = TwoWire(0);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &displayWire, -1);
bool displayReady = false;

void updateDisplay(bool force = false);

const uint8_t SD_DAT0 = 9;
const uint8_t SD_CLK  = 10;
const uint8_t SD_CMD  = 11;
const uint8_t SD_CD   = 12;

Audio audio;

//Playlist
//Builds and links the playlist back to a text file with all the tracks kept in it, keeps ram usage low
#define MAX_TRACKS 3000
const char *PLAYLIST_FILE_PATH = "/playlist.txt";
uint32_t trackOffsets[MAX_TRACKS]; //Byte offset of the start of each track's line
int trackCount = 0;
int currentTrack = 0;
bool isPlaying = false;
int currentVolume = 15;

File playlistWriteFile; //For building or rebuilding the playlist

//Shuffle bag algorithm 
bool shuffleMode = false;
int shuffleBag[MAX_TRACKS];
int shuffleBagSize = 0;
int shuffleBagPos = 0;
int lastShuffledTrack = -1;

//DEEP SLEEP
RTC_DATA_ATTR int  rtcCurrentTrack  = 0;
RTC_DATA_ATTR int  rtcCurrentVolume = 15;
RTC_DATA_ATTR bool rtcShuffleMode   = false;

//SHUFFLE & DIRECTORY---------------------------------------------------------------------------
void seedRandomOnce() {
  static bool seeded = false;
  if (!seeded) {
    randomSeed(esp_random());
    seeded = true;
  }
}

void refillShuffleBag() {
  seedRandomOnce();
  shuffleBagSize = trackCount;
  for (int i = 0; i < shuffleBagSize; i++) {
    shuffleBag[i] = i;
  }

  //Fisher-Yates shuffle
  for (int i = shuffleBagSize - 1; i > 0; i--) {
    int j = random(0, i + 1);
    int tmp = shuffleBag[i];
    shuffleBag[i] = shuffleBag[j];
    shuffleBag[j] = tmp;
  }

  //Avoid repeating the track that just finished as the very next pick
  if (shuffleBagSize > 1 && shuffleBag[0] == lastShuffledTrack) {
    int swapWith = 1 + (shuffleBagSize > 2 ? random(0, shuffleBagSize - 1) : 0);
    int tmp = shuffleBag[0];
    shuffleBag[0] = shuffleBag[swapWith];
    shuffleBag[swapWith] = tmp;
  }

  shuffleBagPos = 0;
  DBG_PRINTLN("Shuffle bag refilled");
}

int getNextShuffledTrack() {
  if (trackCount == 0) return -1;
  if (shuffleBagSize == 0 || shuffleBagPos >= shuffleBagSize) {
    refillShuffleBag();
  }
  int track = shuffleBag[shuffleBagPos++];
  lastShuffledTrack = track;
  return track;
}

void toggleShuffle() {
  shuffleMode = !shuffleMode;
  if (shuffleMode) {
    //Force a fresh bag whenever shuffle is turned on
    shuffleBagSize = 0;
    DBG_PRINTLN("Shuffle: ON");
  } else {
    DBG_PRINTLN("Shuffle: OFF");
  }
  updateDisplay(true);
}

bool AudioFileType(const String &name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".mp3")  ||
         lower.endsWith(".wav")  ||
         lower.endsWith(".flac") ||
         lower.endsWith(".aac")  ||
         lower.endsWith(".m4a");
}

//Recursively walks the SD card and appends every audio file 
void scanDirectory(File dir, const String &path) {
  File entry = dir.openNextFile();
  while (entry) {
    String entryName = entry.name();
    String fullPath;

    if (entryName.startsWith("/")) {
      fullPath = entryName;
    } else {
      fullPath = path + "/" + entryName;
    }

    if (entry.isDirectory()) {
      scanDirectory(entry, fullPath);
    } else {
      if (AudioFileType(fullPath) && trackCount < MAX_TRACKS) {
        trackOffsets[trackCount] = playlistWriteFile.position();
        playlistWriteFile.println(fullPath);
        trackCount++;
        DBG_PRINT("Found: ");
        DBG_PRINTLN(fullPath);
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
}

//Builds the on-disk playlist file from scratch by scanning the SD
void buildPlaylist() {
  trackCount = 0;

  // FILE_WRITE truncates any existing file, so every rebuild starts from a
  // clean slate - important so removed songs don't linger as stale entries.
  playlistWriteFile = SD_MMC.open(PLAYLIST_FILE_PATH, FILE_WRITE);
  if (!playlistWriteFile) {
    DBG_PRINTLN("Failed to open playlist file for writing");
    return;
  }

  File root = SD_MMC.open("/");
  if (!root) {
    DBG_PRINTLN("Failed to open root dir");
    playlistWriteFile.close();
    return;
  }
  scanDirectory(root, "");
  root.close();
  playlistWriteFile.close();

  DBG_PRINT("Total tracks found: ");
  DBG_PRINTLN(trackCount);

  // Reset shuffle state whenever the playlist changes
  shuffleBagSize = 0;
  shuffleBagPos = 0;
  lastShuffledTrack = -1;
}

//Looks up a single track's full path on demand by seeking straight to its recorded offset in the playlist file and reading that one line
String getTrackPath(int index) {
  if (index < 0 || index >= trackCount) return String();

  File f = SD_MMC.open(PLAYLIST_FILE_PATH, FILE_READ);
  if (!f) {
    DBG_PRINTLN("Failed to open playlist file for reading");
    return String();
  }

  f.seek(trackOffsets[index]);
  String line = f.readStringUntil('\n');
  f.close();

  // println() may have written a trailing '\r' before the '\n' we already
  // stopped at; strip it so it doesn't end up as part of the path.
  while (line.length() > 0 && line.endsWith("\r")) {
    line.remove(line.length() - 1);
  }
  return line;
}

void playTrack(int index);

//Rescans the SD card and rewrites the playlist file, e.g. after songs were added or removed
void rebuildPlaylistRequested() {
  DBG_PRINTLN("Rebuilding playlist...");

  // Immediate on-screen feedback, since a full SD rescan isn't instant.
  if (displayReady) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 28);
    display.print("Rebuilding playlist...");
    display.display();
  }

  bool wasPlaying = isPlaying;
  String currentPath = (trackCount > 0) ? getTrackPath(currentTrack) : String();

  buildPlaylist();

  int newIndex = 0;
  if (trackCount > 0 && currentPath.length() > 0) {
    for (int i = 0; i < trackCount; i++) {
      if (getTrackPath(i) == currentPath) {
        newIndex = i;
        break;
      }
    }
  }

  if (trackCount == 0) {
    audio.stopSong();
    isPlaying = false;
    currentTrack = 0;
    DBG_PRINTLN("Playlist rebuilt: no tracks found");
  } else {
    currentTrack = newIndex;
    DBG_PRINT("Playlist rebuilt, tracks: ");
    DBG_PRINTLN(trackCount);
    if (wasPlaying) {
      playTrack(currentTrack); // re-attach the file handle after the rescan
    }
  }

  updateDisplay(true);
}

//Re-indexes an existing playlist file by reading straight through it once and recording each line's offset
void loadPlaylistIndexFromFile() {
  trackCount = 0;

  File f = SD_MMC.open(PLAYLIST_FILE_PATH, FILE_READ);
  if (!f) {
    DBG_PRINTLN("No existing playlist file found on wake - a full rebuild is needed");
    return;
  }

  while (f.available() && trackCount < MAX_TRACKS) {
    uint32_t offset = f.position();
    String line = f.readStringUntil('\n');
    if (line.length() == 0) continue; // ignore stray blank/trailing lines
    trackOffsets[trackCount++] = offset;
  }
  f.close();

  DBG_PRINT("Loaded playlist index from file, tracks: ");
  DBG_PRINTLN(trackCount);

  // Reset shuffle state since this is effectively a fresh load
  shuffleBagSize = 0;
  shuffleBagPos = 0;
  lastShuffledTrack = -1;
}

//ACTIONS & BUTTONS------------------------------------------------------------------------
void playTrack(int index) {
  if (trackCount == 0) {
    DBG_PRINTLN("No tracks in playlist");
    return;
  }
  if (index < 0) index = trackCount - 1;
  if (index >= trackCount) index = 0;

  currentTrack = index;
  String path = getTrackPath(currentTrack);
  DBG_PRINT("Playing: ");
  DBG_PRINTLN(path);

  audio.connecttoFS(SD_MMC, path.c_str());
  isPlaying = true;
  updateDisplay(true);
}

void playNext() {
  if (shuffleMode) {
    int idx = getNextShuffledTrack();
    if (idx >= 0) playTrack(idx);
  } else {
    playTrack(currentTrack + 1);
  }
}

void playPrevious() {
  playTrack(currentTrack - 1);
}

void togglePlayPause() {
  if (isPlaying) {
    audio.pauseResume();
    isPlaying = false;
    DBG_PRINTLN("Paused");
  } else {
    audio.pauseResume();
    isPlaying = true;
    DBG_PRINTLN("Resumed");
  }
  updateDisplay(true);
}

void volumeUp() {
  currentVolume = min(21, currentVolume + 1);
  audio.setVolume(currentVolume);
  DBG_PRINT("Volume: ");
  DBG_PRINTLN(currentVolume);
  updateDisplay(true);
}

void volumeDown() {
  currentVolume = max(0, currentVolume - 1);
  audio.setVolume(currentVolume);
  DBG_PRINT("Volume: ");
  DBG_PRINTLN(currentVolume);
  updateDisplay(true);
}

//Menu switching
void toggleMenu() {
  currentMenu = (currentMenu == MENU_MAIN) ? MENU_SECONDARY : MENU_MAIN;
  DBG_PRINT("Menu switched to: ");
  DBG_PRINTLN(currentMenu == MENU_MAIN ? "MAIN" : "SECONDARY");
  updateDisplay(true);
}

//Dispatches a button press according to the slot's physical position and the currently active menu
void handleButtonPress(ButtonSlot slot) {
  if (slot == SLOT_R2C3) {
    toggleMenu();
    return;
  }

  if (currentMenu == MENU_MAIN) {
    switch (slot) {
      case SLOT_R1C1: playPrevious();    break;
      case SLOT_R1C2: togglePlayPause(); break;
      case SLOT_R1C3: playNext();        break;
      case SLOT_R2C1: volumeDown();      break;
      case SLOT_R2C2: volumeUp();        break;
      default: break;
    }
  } else { //MENU_SECONDARY
    switch (slot) {
      case SLOT_R1C1: enterDeepSleep();
      case SLOT_R1C2: break;
      case SLOT_R1C3: break;
      case SLOT_R2C1: toggleShuffle();  break;
      case SLOT_R2C2: rebuildPlaylistRequested();    break;
      default: break;
    }
  }
}

//Non-blocking button matrix with debounce
void setupButtonMatrix() {
  for (uint8_t r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], LOW); // idle LOW
  }
  for (uint8_t c = 0; c < COLS; c++) {
    pinMode(colPins[c], INPUT_PULLDOWN); // ESP32 has real internal pulldowns
  }
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      lastRawState[r][c] = LOW;
      stableState[r][c] = LOW;
      lastChangeTime[r][c] = 0;
    }
  }
}

//Standard button matrix scanning algorithm
void scanButtons() {
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t rr = 0; rr < ROWS; rr++) {
      digitalWrite(rowPins[rr], rr == r ? HIGH : LOW);
    }
    delayMicroseconds(50); // let signal settle

    for (uint8_t c = 0; c < COLS; c++) {
      bool raw = digitalRead(colPins[c]);

      if (raw != lastRawState[r][c]) {
        lastChangeTime[r][c] = millis();
        lastRawState[r][c] = raw;
      }

      if ((millis() - lastChangeTime[r][c]) > DEBOUNCE_MS) {
        if (raw != stableState[r][c]) {
          stableState[r][c] = raw;

          if (stableState[r][c] == HIGH) {
            lastActivityTime = millis();
            handleButtonPress(slotMap[r][c]);
          }
        }
      }
    }
  }

  // Reset rows back to idle LOW after scan
  for (uint8_t r = 0; r < ROWS; r++) {
    digitalWrite(rowPins[r], LOW);
  }
}

//BATTERY GAUGE (MAX17048G+T10)------------------------------------------------------------
//Reads the SOC register (0x04): high byte = whole percent, low byte = 1/256ths of a percent. Returns -1.0f if the read failed
void batteryQuickStart() {
  batteryWire.beginTransmission(MAX17048_ADDR);
  batteryWire.write(0x06);   // MODE register
  batteryWire.write(0x40);   // QuickStart command, MSB
  batteryWire.write(0x00);   // QuickStart command, LSB
  if (batteryWire.endTransmission() != 0) {
    DBG_PRINTLN("MAX17048 QuickStart write failed (check wiring/address)");
    return;
  }
  delay(10);
}

float readBatteryPercent() {
  batteryWire.beginTransmission(MAX17048_ADDR);
  batteryWire.write(MAX17048_REG_SOC);
  if (batteryWire.endTransmission(false) != 0) { //Repeated start
    return -1.0f;
  }

  uint8_t bytesReceived = batteryWire.requestFrom((int)MAX17048_ADDR, 2);
  if (bytesReceived != 2) {
    return -1.0f;
  }

  uint8_t msb = batteryWire.read();
  uint8_t lsb = batteryWire.read();
  uint16_t raw = ((uint16_t)msb << 8) | lsb;

  return raw / 256.0f;
}

void reportBatteryPercent() {
  float pct = readBatteryPercent();
  if (pct < 0) {
    DBG_PRINTLN("Battery: read error (check MAX17048 wiring/address)");
    return;
  }
  DBG_PRINT("Battery: ");
  DBG_PRINT(pct);
  DBG_PRINTLN("%");

  lastBatteryPercent = pct;
  updateDisplay(true); //Refresh the on-screen battery readout right away
}

void checkBatteryPeriodically() {
  unsigned long now = millis();
  if (now - lastBatteryCheckTime >= BATTERY_CHECK_INTERVAL_MS) {
    lastBatteryCheckTime = now;
    reportBatteryPercent();
  }
}

// DISPLAY (SSD1306 128x64 OLED over I2C)--------------------------------------------------

//Initializes the display
void setupDisplay() {
  displayWire.begin(TFT_SDA, TFT_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_I2C_ADDR)) {
    DBG_PRINTLN("SSD1306 allocation failed (check wiring/address)");
    displayReady = false;
    return;
  }

  displayReady = true;
  display.clearDisplay();
  display.setRotation(2);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.display();
}

//Small battery glyph
void drawBatteryIcon(int x, int y, float pct) {
  const int w = 18, h = 9;
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  display.fillRect(x + w, y + 2, 2, h - 4, SSD1306_WHITE); // terminal tip

  if (pct >= 0) {
    int fillW = map(constrain((int)pct, 0, 100), 0, 100, 0, w - 4);
    if (fillW > 0) {
      display.fillRect(x + 2, y + 2, fillW, h - 4, SSD1306_WHITE);
    }
  } else {
    //No reading yet : draw an "X" through the icon instead
    display.drawLine(x, y, x + w, y + h, SSD1306_WHITE);
    display.drawLine(x, y + h, x + w, y, SSD1306_WHITE);
  }
}

String formatTime(uint32_t totalSeconds) {
  uint32_t m = totalSeconds / 60;
  uint32_t s = totalSeconds % 60;
  String out = String(m) + ":";
  if (s < 10) out += "0";
  out += String(s);
  return out;
}

//Strips the directory path off a full SD path leaving just the filename
String trackDisplayName(const String &path) {
  int slash = path.lastIndexOf('/');
  return (slash >= 0) ? path.substring(slash + 1) : path;
}

//Menu 1: normal playback screen
void drawMainMenuScreen() {
  //Battery percentage + menu indicator
  display.setCursor(0, 0);
  if (lastBatteryPercent >= 0) {
    display.print(lastBatteryPercent);
    display.print('%');
  } else {
    display.print("--%");
  }
  display.setCursor(92, 0);
  display.print("M1");
  drawBatteryIcon(SCREEN_WIDTH - 20, 0, lastBatteryPercent);
  display.drawFastHLine(0, 11, SCREEN_WIDTH, SSD1306_WHITE);

  //Track name (+scroll)
  display.setCursor(0, 18);
  if (trackCount == 0) {
    display.print("No tracks found");
  } else {
    String name = trackDisplayName(getTrackPath(currentTrack));
    const int maxChars = SCREEN_WIDTH / 6;

    if ((int)name.length() > maxChars) {
      static unsigned long lastScrollTime = 0;
      static int scrollOffset = 0;
      static int lastScrollTrack = -1;

      unsigned long now = millis();
      if (lastScrollTrack != currentTrack) {
        //New track -> restart the scroll.
        lastScrollTrack = currentTrack;
        scrollOffset = 0;
        lastScrollTime = now;
      } else if (now - lastScrollTime > 300) {
        lastScrollTime = now;
        scrollOffset++;
        if (scrollOffset > (int)name.length() + 3) scrollOffset = 0;
      }

      String looped = name + "   " + name; // wrap-around buffer
      display.print(looped.substring(scrollOffset, scrollOffset + maxChars));
    } else {
      display.print(name);
    }
  }

  // ---- Track counter ----
  display.setCursor(0, 30);
  display.print("Track ");
  display.print(trackCount > 0 ? currentTrack + 1 : 0);
  display.print('/');
  display.print(trackCount);

  // ---- Playback state + shuffle indicator ----
  display.setCursor(0, 42);
  display.print(isPlaying ? "> Playing" : "|| Paused");
  if (shuffleMode) {
    display.setCursor(SCREEN_WIDTH - 30, 42);
    display.print("SHUF");
  }

  // ---- Volume bar ----
  display.setCursor(0, 54);
  display.print("Vol");
  const int barX = 24, barY = 55, barW = SCREEN_WIDTH - barX, barH = 7;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  int filled = map(currentVolume, 0, 21, 0, barW - 2);
  if (filled > 0) {
    display.fillRect(barX + 1, barY + 1, filled, barH - 2, SSD1306_WHITE);
  }
}

//Menu 2: shuffle / rebuild screen / Sleep
void drawSecondaryMenuScreen() {
  display.setCursor(0, 0);
  if (lastBatteryPercent >= 0) {
    display.print(lastBatteryPercent);
    display.print('%');
  } else {
    display.print("--%");
  }

  display.setCursor(92, 0);
  display.print("M2");
  drawBatteryIcon(SCREEN_WIDTH - 20, 0, lastBatteryPercent);
  display.drawFastHLine(0, 11, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 18);
  if (trackCount == 0) {
    display.print("No tracks found");
  } else {
    String name = trackDisplayName(getTrackPath(currentTrack));
    const int maxChars = SCREEN_WIDTH / 6;

    if ((int)name.length() > maxChars) {
      static unsigned long lastScrollTime = 0;
      static int scrollOffset = 0;
      static int lastScrollTrack = -1;

      unsigned long now = millis();
      if (lastScrollTrack != currentTrack) {
        //New track -> restart the scroll
        lastScrollTrack = currentTrack;
        scrollOffset = 0;
        lastScrollTime = now;
      } else if (now - lastScrollTime > 300) {
        lastScrollTime = now;
        scrollOffset++;
        if (scrollOffset > (int)name.length() + 3) scrollOffset = 0;
      }

      String looped = name + "   " + name; // wrap-around buffer
      display.print(looped.substring(scrollOffset, scrollOffset + maxChars));
    } else {
      display.print(name);
    }
  }

  // Elapsed / total time, useful while seeking
  display.setCursor(0, 30);
  if (trackCount > 0) {
    uint32_t elapsed = audio.getAudioCurrentTime();
    uint32_t duration = audio.getAudioFileDuration();
    display.print(formatTime(elapsed));
    display.print(" / ");
    display.print(formatTime(duration));
  } else {
    display.print("--:-- / --:--");
  }

  display.setCursor(0, 42);
  display.print("Enter Deep Sleep");
  display.setCursor(0, 54);
  display.print(shuffleMode ? "SHUF:ON" : "SHUF:OFF");
  display.setCursor(74, 54);
  display.print("REBUILD");
}

// Redraws the whole screen
void updateDisplay(bool force) {
  if (!displayReady) return;

  unsigned long now = millis();
  if (!force && (now - lastDisplayUpdateTime) < DISPLAY_UPDATE_INTERVAL_MS) {
    return;
  }
  lastDisplayUpdateTime = now;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (currentMenu == MENU_MAIN) {
    drawMainMenuScreen();
  } else {
    drawSecondaryMenuScreen();
  }

  display.display();
}

//POWER MANAGEMENT------------------------------------------------------------------------
//Would enable DFS (dynamic frequency scaling) + automatic light sleep
void setupPowerManagement() {
#if CONFIG_PM_ENABLE
  esp_pm_config_t pm_config = {};
  pm_config.max_freq_mhz = 240;   // burst up for audio decode / SD reads
  pm_config.min_freq_mhz = 80;    // idle down otherwise
  pm_config.light_sleep_enable = true;

  esp_err_t err = esp_pm_configure(&pm_config);
  if (err != ESP_OK) {
    DBG_PRINT("esp_pm_configure failed, err=");
    DBG_PRINTLN((int)err);
  }
#else
  DBG_PRINTLN("CONFIG_PM_ENABLE not compiled in - using manual CPU scaling fallback");
  setCpuFrequencyMhz(240);
#endif
}

//Manual fallback used only when CONFIG_PM_ENABLE isn't available
void updateManualCpuScaling() {
#if !CONFIG_PM_ENABLE
  static bool wasPlaying = true; // force one scaling call on first run
  if (isPlaying != wasPlaying) {
    wasPlaying = isPlaying;
    setCpuFrequencyMhz(isPlaying ? 240 : 80);
  }
#endif
}

// Explicitly disable WiFi/BT
void disableRadios() {
  WiFi.mode(WIFI_OFF);
  btStop();
  esp_bt_controller_disable();
}

//Puts the S3 into deep sleep
void enterDeepSleep() {
  rtcCurrentTrack  = currentTrack;
  rtcCurrentVolume = currentVolume;
  rtcShuffleMode   = shuffleMode;

  DBG_PRINTLN("Entering deep sleep due to inactivity...");
  DBG_PRINT("(wake by pressing any ROW2 button: VOL-, VOL+, or MENU)");

  if (displayReady) {
    display.clearDisplay();
    display.display();
  }

  delay(20);

  //ROW2 to exit deep sleep
  rtc_gpio_init((gpio_num_t)ROW2);
  rtc_gpio_set_direction((gpio_num_t)ROW2, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level((gpio_num_t)ROW2, 1);
  rtc_gpio_hold_en((gpio_num_t)ROW2);

  const gpio_num_t wakeCols[] = {
    (gpio_num_t)COL1, (gpio_num_t)COL2, (gpio_num_t)COL3
  };

  uint64_t wakeMask = 0;
  for (gpio_num_t col : wakeCols) {
    rtc_gpio_init(col);
    rtc_gpio_set_direction(col, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en(col);
    rtc_gpio_pullup_dis(col);
    wakeMask |= (1ULL << col);
  }

  //Wake on any of ROW2's columns
  esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_HIGH);

  esp_deep_sleep_start(); // does not return
}

//Tracks idle time and triggers deep sleep once the device has been stopped and untouched for long enough.
void checkIdleSleep() {
  if (isPlaying) {
    lastActivityTime = millis();
    return;
  }

  if (millis() - lastActivityTime >= INACTIVITY_SLEEP_MS) {
    enterDeepSleep();
  }
}

//Audio library callbacks & Main loop-----------------------------------------------------
void audio_eof_mp3(const char *info) {
  DBG_PRINT("End of track: ");
  DBG_PRINTLN(info);
  playNext();
}

void audio_info(const char *info) {
  DBG_PRINT("audio_info: ");
  DBG_PRINTLN(info);
}

void audio_id3data(const char *info) {
  DBG_PRINT("ID3: ");
  DBG_PRINTLN(info);
}

void setup() {
  DBG_BEGIN(9600);

  //Release ROW2's hold and hand all the wake-related pins back to normal digital GPIO 
  rtc_gpio_hold_dis((gpio_num_t)ROW2);
  rtc_gpio_deinit((gpio_num_t)ROW2);
  rtc_gpio_deinit((gpio_num_t)COL1);
  rtc_gpio_deinit((gpio_num_t)COL2);
  rtc_gpio_deinit((gpio_num_t)COL3);

  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
  DBG_PRINT("Wakeup cause: ");
  DBG_PRINTLN((int)wakeReason);

  setupPowerManagement();
  disableRadios();

  pinMode(SD_CMD, INPUT_PULLUP);
  pinMode(SD_DAT0, INPUT_PULLUP);
  pinMode(SD_CD, INPUT_PULLUP);

  SD_MMC.setPins(SD_CLK, SD_CMD, SD_DAT0);

  if (!SD_MMC.begin("/sdcard", true)) {
    DBG_PRINTLN("SD initialization failed!");
    return;
  }

  uint8_t type = SD_MMC.cardType();
  if (type == CARD_NONE) {
    DBG_PRINTLN("No SD card");
    return;
  }

  DBG_PRINT("Card Size: ");
  DBG_PRINT(SD_MMC.cardSize() / (1024 * 1024));
  DBG_PRINTLN(" MB");

  setupDisplay();

  audio.setPinout(PCM_BCK, PCM_LRCK, PCM_DIN);

  // Restore playback state carried over deep sleep
  currentVolume = rtcCurrentVolume;
  shuffleMode   = rtcShuffleMode;
  audio.setVolume(currentVolume);

  setupButtonMatrix();

  // Init the battery gauge on its own I2C bus (separate pins from the screen)
  batteryWire.begin(BAT_SDA, BAT_SCL);

  if (wakeReason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    batteryQuickStart();
  }
  reportBatteryPercent(); // print an initial reading right away
  lastBatteryCheckTime = millis();

  //A cold boot gets a full rescan (in case songs were added/removed while the device was off)
  //Waking from deep sleep just reloads the index from the playlist file already on disk 
  if (wakeReason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    buildPlaylist();
  } else {
    loadPlaylistIndexFromFile();
    if (trackCount == 0) {
      DBG_PRINTLN("Playlist index empty after wake - falling back to full rebuild");
      buildPlaylist();
    }
  }

  if (trackCount > 0) {
    int startTrack = (rtcCurrentTrack >= 0 && rtcCurrentTrack < trackCount)
                        ? rtcCurrentTrack
                        : 0;
    playTrack(startTrack);
  } else {
    DBG_PRINTLN("No audio files found on SD card.");
    updateDisplay(true);
  }

  lastActivityTime = millis();
}

void loop() {
  audio.loop();

  unsigned long now = millis();
  if (now - lastButtonScanTime >= BUTTON_SCAN_INTERVAL_MS) {
    lastButtonScanTime = now;
    scanButtons();
  }

  checkBatteryPeriodically();
  checkIdleSleep();
  updateManualCpuScaling();
  updateDisplay();
}
