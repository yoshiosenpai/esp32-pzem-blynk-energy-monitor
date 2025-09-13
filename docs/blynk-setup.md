# Blynk Setup (Template, Datastreams, Dashboard)
> You only need to do this once per Template. After that, any device using this firmware can be provisioned in seconds.

---

## 1) Create a Blynk Template

1. Go to **[blynk.cloud](https://blynk.cloud)** → log in.
2. **Developer Zone → Templates → New Template**

   * **Template Name:** `EnergyMonitor`
   * **Hardware:** `ESP32`
   * **Connection Type:** `WiFi`
3. Click **Create**.

### Copy Template IDs

Open **Template → Device Info** and copy:

* `BLYNK_TEMPLATE_ID`
* `BLYNK_TEMPLATE_NAME`
* (We’ll get the device’s **Auth Token** after adding a device.)

Paste these into your firmware’s `config.h` later.

---

## 2) Add Datastreams (Virtual Pins)

Inside **EnergyMonitor → Datastreams → Add Datastream → Virtual Pin**
Create the following:

### Sensor values

| Name    | Virtual Pin | Data Type | Units | Min | Max  | Recommended Widget    |
| ------- | ----------- | --------- | ----- | --- | ---- | --------------------- |
| Voltage | **V0**      | Double    | V     | 0   | 260  | Value Display / Gauge |
| Current | **V1**      | Double    | A     | 0   | 10   | Value Display / Gauge |
| Power   | **V2**      | Double    | W     | 0   | 2500 | Value Display / Gauge |
| Energy  | **V3**      | Double    | kWh   | 0   | 9999 | Value Display         |

> The firmware pushes fresh values every \~2 s.

### Relay controls

| Name    | Virtual Pin | Data Type | Min | Max | Recommended Widget |
| ------- | ----------- | --------- | --- | --- | ------------------ |
| Relay 1 | **V10**     | Integer   | 0   | 1   | Switch / Button    |
| Relay 2 | **V11**     | Integer   | 0   | 1   | Switch / Button    |

> 0 = OFF, 1 = ON. Most 5V relay boards are **active-LOW**; the firmware handles this for you.

*(Optional extras if you add them in code later)*

| Name        | Virtual Pin | Data Type | Units | Min | Max |
| ----------- | ----------- | --------- | ----- | --- | --- |
| Frequency   | V4          | Double    | Hz    | 45  | 65  |
| PowerFactor | V5          | Double    | —     | 0   | 1   |

---

## 3) Create the Web Dashboard

1. In the template, open **Web Dashboard**.
2. Add widgets and map them to the datastreams:

**Recommended layout**

* **Value Display** → Voltage (V0)
* **Value Display** → Current (V1)
* **Value Display** → Power (V2)
* **Switch** → Relay 1 (V10)
* **Switch** → Relay 2 (V11)

**Widget tips**

* For Voltage/Current/Power: set **Decimals** (Voltage: 1; Current: 3; Power: 1)
* For Energy (kWh): set **Decimals: 3** (or 2)
* Title each widget clearly (e.g., “Voltage (V)”)

Click **Save** (top-right).

---

## 4) Create the Mobile Dashboard (optional)

In the **Blynk mobile app** (iOS/Android):

1. Log in with the same account.
2. Add your device (after provisioning below).
3. Add widgets and map the same pins:

   * Value Displays for **V0–V3**
   * Switches for **V10, V11**
4. Save.

---

## 5) Provision a Device (get the Auth Token)

1. Go to **My Devices → New Device → From Template**
2. Choose **EnergyMonitor**
3. Name your device (e.g., “Lab Energy Monitor”)
4. After creation, open the device → **Device Info** → copy the **Auth Token**.

Paste this **Auth Token** into your `config.h`:

```cpp
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "EnergyMonitor"
#define BLYNK_AUTH_TOKEN    "PASTE_DEVICE_TOKEN_HERE"
```

Also set your Wi-Fi:

```cpp
static char WIFI_SSID[] = "YOUR_WIFI";
static char WIFI_PASS[] = "YOUR_PASSWORD";
```

Rebuild & upload the firmware.

---

## 6) Verify Data Flow

* Open **Serial Monitor (115200)** → you should see lines like:

  ```
  V=243.1 V | I=0.042 A | P=9.9 W | E=0.002 kWh
  ```
* Open your **Blynk Web Dashboard** → V0..V3 updating every \~2s.
* Use **Relay 1 / Relay 2** switches → bulbs should toggle.

If values don’t show, see Troubleshooting below.

---

## 7) Expected Ranges

* **Voltage (V0):** 200–260 V (MY mains typically \~230–245 V)
* **Current (V1):** 0.00–0.10 A for 9W LED (≈ 0.04 A typical)
* **Power (V2):** 0–200 W (≈ 7–10 W for one LED bulb)

---

## 8) Troubleshooting (Blynk side)

* **No values on dashboard, but device is online**

  * Check that each widget is mapped to the correct **Virtual Pin** (V0..V3).
  * Ensure datastream **Data Type** matches firmware (Double for sensors).
  * Confirm the device’s **Auth Token** in `config.h` matches the device in Blynk.

* **Device offline**

  * Wrong Wi-Fi SSID/PASS or token. Test with a phone hotspot.
  * Re-upload after fixing `config.h`.

* **Values frozen**

  * Check Serial Monitor for sensor errors. If firmware can’t read PZEM, it won’t push updates.
  * Make sure you **saved** the Web Dashboard after editing.

> For wiring/firmware issues (e.g., V reads OK but I=0), see `docs/troubleshooting.md`.

---

## 9) Security & Best Practices

* Don’t commit `config.h` (contains tokens/passwords). The repo ignores it via `.gitignore`.
* Use a separate **device** in Blynk for each physical ESP32.
* For public demos, regenerate Auth Tokens after.

