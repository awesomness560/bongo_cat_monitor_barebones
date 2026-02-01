# 🐱 Bongo Cat ESP32 - Ornamental Desk Clock

A minimal, ornamental fork of the [Bongo Cat Monitor](https://github.com/vostoklabs/bongo_cat_monitor) project. This version strips away the typing companion features and desktop apps, turning it into a charming **standalone desk clock** with a touch-interactive Bongo Cat that you can enjoy without a computer connection.

## 🔀 What Changed From the Original

**Removed:**
- Desktop companion application (Windows/Mac)
- Typing detection and WPM tracking
- System monitoring (CPU, RAM)
- Serial communication with PC
- Web flasher references and installer packages

**Added:**
- **Touch controls** – fully interactive via the touchscreen, no PC required
- **Display sleep/wake** – hold to sleep, tap to wake
- **Timezone cycling** – tap the time area to switch between US timezones
- **Cat excitement mode** – tap the cat to trigger happy animations and brightness control

The result is a self-contained ornamental piece: plug it in, set WiFi for time sync, and enjoy your Bongo Cat desk clock.

---

## ✨ Features

- **Clock Display** – Shows current time in 12-hour format (synced via WiFi/NTP)
- **Touch-Interactive Cat** – Tap to excite, wake from sleep, or adjust brightness
- **Timezone Control** – Cycle Central → Eastern → Mountain → Pacific with a tap
- **Display Sleep** – Hold 5 seconds to turn off; tap anywhere to wake
- **Animated States** – Idle, sleepy, and excited animations with blinks and ear twitches
- **Standalone** – No desktop app needed; runs entirely on the ESP32

---

## 👆 Touch Controls

| Touch Location | Action | Result |
|----------------|--------|--------|
| **Anywhere** | Hold 5 seconds | Display sleeps (backlight off) |
| **Anywhere** | Tap when asleep | Display wakes |
| **Top area** (y < 75) | Quick tap | Cycle timezone (CT → ET → MT → PT → CT) |
| **Cat area** (y ≥ 75) | Quick tap (cat sleeping) | Wake cat to idle |
| **Cat area** (y ≥ 75) | Quick tap (idle) | Trigger excited mode (~1 minute) |
| **Cat area** (y ≥ 75) | Quick tap (already excited) | Cycle brightness |

---

## 🛒 Hardware Requirements

You'll need an **ESP32 board with 2.4" TFT display**. The project was designed for:

**[ESP32-2432S028R 2.4" TFT Display Board](https://www.aliexpress.com/item/1005008176009397.html)**

This board includes:
- ESP32-WROOM-32 module
- 2.4" ILI9341 TFT LCD (240×320 resolution)
- Touch screen
- USB-C connector

---

## 🚀 Quick Start

### 1. Configure WiFi and Timezone

Edit `bongo_cat.ino` and add your WiFi credentials:

```cpp
WiFiNetwork wifiNetworks[] = {
    {"Your WiFi Name", "Your Password"},
};
```

The default timezone is Central Time. You can change it in code or cycle it via touch after flashing.

### 2. Install Libraries

1. Open Arduino IDE and install via **Sketch → Include Library → Manage Libraries**:
   - **TFT_eSPI** – display driver
   - **LVGL** – **version 8.x only** (not 9.x); search for "lvgl" and install 8.3.x

2. Find your Arduino libraries folder. Arduino IDE usually installs packages here:
   - **Windows:** `C:\Users\<YourUsername>\Documents\Arduino\libraries`
   - **macOS:** `~/Documents/Arduino/libraries`
   - **Linux:** `~/Arduino/libraries` or `~/Documents/Arduino/libraries`

### 3. Configure LVGL and TFT_eSPI

The project includes `lv_conf.h` and `User_Setup.h` that must be placed inside the installed library folders.

**LVGL (`lv_conf.h`):**
- Go to `libraries/lvgl/` (or `libraries/lvgl8/` depending on install)
- If `lv_conf.h` exists in the library root → **replace** it with the one from this project
- If it does not exist → **copy** the project's `lv_conf.h` into the library root

**TFT_eSPI (`User_Setup.h`):**
- Go to `libraries/TFT_eSPI/`
- If `User_Setup.h` exists in the library root → **replace** it with the one from this project
- If it does not exist → **copy** the project's `User_Setup.h` into the library root

These config files match the ESP32-2432S028R-style 2.4" TFT board.

### 4. Flash the ESP32

1. Open `bongo_cat.ino` in Arduino IDE
2. Select your ESP32 board (**Tools → Board**)
3. Select the correct port (**Tools → Port**)
4. Click **Upload**

### 5. Use It

Power the board via USB. It will connect to WiFi to sync time, then display the clock and Bongo Cat. Use the touchscreen to interact—no computer needed.

---

## 📁 Project Structure

```
├── animations/              # Animation sprite data (C arrays)
│   ├── body/               # Body sprites (standard, ear twitch)
│   ├── faces/              # Face sprites (stock, happy, sleepy, blink)
│   ├── paws/               # Paw sprites (left down, right down, both up)
│   ├── effects/            # Effects (click, sleepy animations)
│   └── table/              # Table/background
├── Sprites/                # Source PNG images for sprites
├── bongo_cat.ino           # Main ESP32 firmware
├── animations_sprites.h    # Sprite layer definitions and animation states
├── lv_conf.h               # LVGL configuration
├── User_Setup.h            # TFT_eSPI display configuration
├── Free_Fonts.h            # Font definitions
└── LICENSE.txt
```

---

## 🎨 Animation System

The cat cycles through several states:

| State | Description |
|-------|-------------|
| **Idle 1** | Paws up, stock face |
| **Idle 2** | Paws hidden (under table) |
| **Idle 3** | Sleepy face |
| **Idle 4** | Sleepy face + floating sleepy effects |
| **Excited** | Happy face, fast paw bounces (triggered by tap) |

Additional behaviors:
- **Blinking** – Random blinks every 3–8 seconds
- **Ear twitch** – Occasional body twitch
- **Auto-cycle** – Switches between idle and sleep every ~10 minutes

---

## ⚙️ Configuration

### WiFi & Timezone

Edit the `wifiNetworks` array and `timezone` string in `bongo_cat.ino`. The project uses POSIX timezone strings for automatic DST handling.

### Required Libraries

- **TFT_eSPI** – Use the project's `User_Setup.h` in the library folder (see Setup above)
- **LVGL 8.x** – Use the project's `lv_conf.h` in the library folder (see Setup above). **Must be 8.x**, not 9.x.
- **WiFi** – Built into ESP32 core

---

## 📝 License

This project is licensed under the MIT License - see [LICENSE.txt](LICENSE.txt) for details.

---

## 🙏 Acknowledgments

- [vostoklabs/bongo_cat_monitor](https://github.com/vostoklabs/bongo_cat_monitor) – Original project
- Original Bongo Cat meme creators
