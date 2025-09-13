# Quickstart Guide

*ESP32 + PZEM-004T v3.0 + Blynk + Relay Energy Monitor*

---

## 1. Hardware

* ESP32 Dev Board (e.g. ESP32-DEVKIT V1)
* PZEM-004T v3.0 (AC energy monitor with CT clamp)
* 4-Channel relay module (5V, active-LOW)
* 2 × AC bulb + bulb holders (blue-stripe = Live, white = Neutral)
* CT clamp (red/black leads → S1/S2 on PZEM)
* 3-pin plug top with 3A fuse
* Breadboard / jumper wires / insulated enclosure

⚠️ **Warning:** This project uses 230 VAC mains. Always insulate, use a fused plug, and test with low-wattage LED bulbs first.

---

## 2. Arduino IDE Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the **ESP32 board core**:

   * In IDE: `File → Preferences → Additional Board URLs`
   * Add:

     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   * Then `Tools → Board → Boards Manager` → search **ESP32 by Espressif** → Install.
3. Install required libraries (`Tools → Manage Libraries`):

   * **Blynk** (by Volodymyr Shymanskyy)
   * **PZEM004Tv30** (by Jakub Mandula)

---

## 3. Clone the Repository

```bash
git clone https://github.com/yoshiosenpai/esp32-pzem-blynk-energy-monitor.git
cd esp32-pzem-blynk-energy-monitor/firmware/main.ino
```

---

## 4. Configure Firmware

1. Copy the example config:

   ```bash
   cp config.example.h config.h
   ```
2. Open `config.h` and edit:

   ```cpp
   #define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
   #define BLYNK_TEMPLATE_NAME "YOUR_TEMPLATE_NAME"
   #define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_AUTH_TOKEN"

   static char WIFI_SSID[] = "YOUR_WIFI";
   static char WIFI_PASS[] = "YOUR_PASSWORD";
   ```
3. In Arduino IDE: open `main.ino`.

---

## 5. Flash the ESP32

1. Select board: `Tools → Board → ESP32 Dev Module`.
2. Select correct COM port.
3. Click **Upload**.
4. Open Serial Monitor (115200 baud) → you should see live readings like:

   ```
   V=243.1 V | I=0.042 A | P=9.9 W |
   ```

---

## 6. Wiring Overview

* **Low voltage (ESP32 side):**

  * ESP32 5V → Relay VCC, PZEM VCC
  * ESP32 GND → Relay GND, PZEM GND
  * ESP32 GPIO16 (RX2) ← PZEM TX
  * ESP32 GPIO17 (TX2) → PZEM RX
  * ESP32 GPIO23 → Relay IN1
  * ESP32 GPIO22 → Relay IN2

* **High voltage (AC side):**

  * Plug **Live (L)** → PZEM L + Relay COM (both)
  * Relay NO Ch1 → Bulb1 blue-stripe (Live)
  * Relay NO Ch2 → Bulb2 blue-stripe (Live)
  * Plug **Neutral (N)** → PZEM N + Bulb1 & Bulb2 white (Neutral)

* **CT Clamp:**

  * Red → PZEM S1, Black → PZEM S2
  * Clamp around **one single Live wire** feeding the bulbs (NOT both L+N).

👉 Full wiring tables are in [`docs/wiring-table.md`](wiring-table.md).

---

## 7. Blynk Setup

1. In [Blynk Cloud](https://blynk.cloud): create a **Template → EnergyMonitor**.
2. Add Datastreams:

   * **V0** = Voltage (V)
   * **V1** = Current (A)
   * **V2** = Power (W)
   * **V10** = Relay 1 (Button)
   * **V11** = Relay 2 (Button)
3. Add Widgets:

   * Gauges / Value Displays for V0–V3
   * Switches for V10 & V11
4. Copy the **Template ID, Name, Auth Token** into `config.h`.

---

## 8. First Test

* Power on the ESP32 via USB.
* Power on the plug (with fuse) to supply the bulbs + PZEM.
* Open Serial Monitor → check Voltage ≈ 230–250 V.
* In Blynk dashboard → see V/I/P/E update every 2 s.
* Press Relay1 / Relay2 buttons → bulbs switch ON/OFF.

---

## 9. Troubleshooting

* **All values -1.0** → swap TX/RX, check 5V/GND.
* **Voltage OK, Current 0.000** → CT clamp on wrong wire / not tight / bulb OFF.
* **MCB trips immediately** → Neutral was put through relay; fix so only Live goes COM→NO.
* **ESP32 boot error (invalid header)** → unplug peripherals, erase & flash Blink, then reconnect.

---