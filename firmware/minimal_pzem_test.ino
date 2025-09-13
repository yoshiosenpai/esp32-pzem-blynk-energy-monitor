#include <Arduino.h>
#include <PZEM004Tv30.h>   // by Jakub Mandula

// ESP32 pins (UART2)
#define RX_PIN 16   // ESP32 RX2 <- PZEM TX
#define TX_PIN 17   // ESP32 TX2 -> PZEM RX

HardwareSerial PZEMSerial(2);          // use UART2
PZEM004Tv30 pzem(PZEMSerial, RX_PIN, TX_PIN);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("PZEM-004T v3.0 Test Starting...");
}

void loop() {
  float v = pzem.voltage();
  float i = pzem.current();
  float p = pzem.power();
  float e = pzem.energy();
  float f = pzem.frequency();
  float pf = pzem.pf();

  if (isnan(v)) {
    Serial.println("Error reading from PZEM (check wiring, AC power, TX/RX).");
  } else {
    Serial.printf("V=%.1f V | I=%.3f A | P=%.1f W | E=%.3f kWh | f=%.1f Hz | PF=%.2f\n",
                  v, i, p, e, f, pf);
  }

  delay(2000);
}
