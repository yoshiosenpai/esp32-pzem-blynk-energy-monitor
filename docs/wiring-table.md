# Wiring Table – ESP32 + PZEM-004T v3.0 + Relay + 2 Bulbs

⚠️ **Safety First**: This project involves 230 VAC. Use a fused plug (3 A), insulated terminals/enclosure, and never touch AC wiring while powered.

---

## 🔹 Low-Voltage Wiring (ESP32 → PZEM & Relay)

| From (Device)      | Pin/Terminal | To (Device)  | Pin/Terminal | Notes                    |
| ------------------ | ------------ | ------------ | ------------ | ------------------------ |
| ESP32              | 5V           | PZEM-004T    | VCC          | Powers PZEM logic        |
| ESP32              | GND          | PZEM-004T    | GND          | Common ground            |
| ESP32 (GPIO16 RX2) | RX2          | PZEM-004T    | TX           | Receive from PZEM        |
| ESP32 (GPIO17 TX2) | TX2          | PZEM-004T    | RX           | Send to PZEM             |
| ESP32              | 5V           | Relay module | VCC          | Powers relay board       |
| ESP32              | GND          | Relay module | GND          | Common ground            |
| ESP32 (GPIO23)     | Digital Pin  | Relay module | IN1          | Control Relay 1 (Bulb 1) |
| ESP32 (GPIO22)     | Digital Pin  | Relay module | IN2          | Control Relay 2 (Bulb 2) |

---

## 🔹 High-Voltage Wiring (AC Plug → PZEM → Relay → Bulbs)

### Common Connections

| From (Device) | Pin/Terminal      | To (Device)   | Pin/Terminal | Notes                          |
| ------------- | ----------------- | ------------- | ------------ | ------------------------------ |
| 3-Pin Plug    | Live (L, brown)   | PZEM-004T     | AC-IN L      | Supplies PZEM and feeds relays |
| 3-Pin Plug    | Neutral (N, blue) | PZEM-004T     | AC-IN N      | Supplies PZEM and bulbs        |
| 3-Pin Plug    | Neutral (N, blue) | Bulb 1 Holder | White wire   | Neutral direct to Bulb 1       |
| 3-Pin Plug    | Neutral (N, blue) | Bulb 2 Holder | White wire   | Neutral direct to Bulb 2       |

---

### Bulb 1 (Relay Channel 1)

| From (Device)   | Pin/Terminal | To (Device)   | Pin/Terminal       | Notes                   |
| --------------- | ------------ | ------------- | ------------------ | ----------------------- |
| 3-Pin Plug Live | —            | Relay Ch1     | COM                | Live input              |
| Relay Ch1       | NO           | Bulb 1 Holder | Blue-stripe (Live) | Switched Live to Bulb 1 |

---

### Bulb 2 (Relay Channel 2)

| From (Device)   | Pin/Terminal | To (Device)   | Pin/Terminal       | Notes                   |
| --------------- | ------------ | ------------- | ------------------ | ----------------------- |
| 3-Pin Plug Live | —            | Relay Ch2     | COM                | Live input              |
| Relay Ch2       | NO           | Bulb 2 Holder | Blue-stripe (Live) | Switched Live to Bulb 2 |

---

## 🔹 CT Clamp Wiring

| From (Device) | Pin/Lead   | To (Device) | Pin/Terminal | Notes    |
| ------------- | ---------- | ----------- | ------------ | -------- |
| CT Clamp      | Red lead   | PZEM-004T   | S1           | CT input |
| CT Clamp      | Black lead | PZEM-004T   | S2           | CT input |

* Clamp must be **around one single Live wire only** (e.g. the wire between Relay NO → Bulb Live).
* Do **not** clamp both Live + Neutral, or the reading will always be 0 A.
* If power shows negative, flip clamp orientation.

