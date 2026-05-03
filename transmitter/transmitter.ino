/*
 * CubeSat Telemetry Simulator — TRANSMITTER (Satellite Side)
 * 
 * Hardware : Arduino Uno/Nano + DHT22 + LoRa RA-02 (SX1278)
 * Author   : Shuvojyoti Palit
 * Version  : 1.0
 *
 * Wiring:
 *   DHT22  → Arduino
 *   VCC    → 5V
 *   DATA   → D4  (+ 10kΩ pull-up to 5V)
 *   GND    → GND
 *
 *   LoRa RA-02  → Arduino
 *   VCC         → 3.3V
 *   GND         → GND
 *   SCK         → D13
 *   MISO        → D12
 *   MOSI        → D11 (via 1kΩ+2kΩ voltage divider)
 *   NSS/CS      → D10
 *   DIO0        → D2
 *   RST         → D9
 *
 * Libraries:
 *   - DHT sensor library by Adafruit
 *   - LoRa by Sandeep Mistry
 */

#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>

// ── Pin definitions ───────────────────────────────────────
#define DHTPIN    4         // DHT22 DATA → D4
#define DHTTYPE   DHT22
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

// ── Constants ─────────────────────────────────────────────
#define LORA_FREQ       433E6   // 433 MHz
#define LORA_SF         7       // Spreading Factor
#define LORA_BW         125E3   // Bandwidth
#define LORA_CR         5       // Coding Rate 4/5
#define LORA_TX_POWER   17      // dBm
#define TX_INTERVAL     2000    // ms between transmissions

// ── Packet layout (17 bytes total) ────────────────────────
// [0]     = 0xAA        START marker
// [1-2]   = seqNum      packet counter (big-endian)
// [3-6]   = temp        float 4 bytes
// [7-10]  = humidity    float 4 bytes
// [11-14] = voltage     float 4 bytes
// [15]    = checksum    XOR of bytes 0-14
// [16]    = 0x55        END marker
#define PKT_SIZE  17
#define PKT_START 0xAA
#define PKT_END   0x55

//  Objects 
DHT dht(DHTPIN, DHTTYPE);
uint16_t seqNum = 0;


void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("============================");
  Serial.println("  CubeSat TX — INITIALIZING");
  Serial.println("============================");

  // Init DHT22
  dht.begin();
  delay(2000);    // DHT22 warm-up time
  Serial.println("[OK] DHT22 initialized");

  // Init LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("[FAIL] LoRa init failed — check wiring");
    while (1);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TX_POWER);
  Serial.println("[OK] LoRa initialized at 433 MHz");

  Serial.println("============================");
  Serial.println("  CubeSat TX — SAT READY");
  Serial.println("============================");
  Serial.println("Transmitting every 2 seconds...");
  Serial.println();
}

uint8_t calcChecksum(uint8_t* buf, int len) {
  uint8_t cs = 0;
  for (int i = 0; i < len; i++) cs ^= buf[i];
  return cs;
}

void loop() {

  // Read sensors 
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  float volt = analogRead(A0) * (5.0 / 1023.0);

  // Guard: skip bad DHT22 readings
  if (isnan(temp) || isnan(hum)) {
    Serial.println("[WARN] DHT22 read failed — check wiring & pull-up resistor");
    delay(TX_INTERVAL);
    return;
  }

  // Build packet
  uint8_t pkt[PKT_SIZE];
  pkt[0] = PKT_START;
  pkt[1] = (seqNum >> 8) & 0xFF;
  pkt[2] = seqNum & 0xFF;
  memcpy(&pkt[3],  &temp, 4);
  memcpy(&pkt[7],  &hum,  4);
  memcpy(&pkt[11], &volt, 4);
  pkt[15] = calcChecksum(pkt, 15);
  pkt[16] = PKT_END;

  // Transmit
  LoRa.beginPacket();
  LoRa.write(pkt, PKT_SIZE);
  LoRa.endPacket();

  //Log to Serial 
  Serial.print("[TX] #");
  Serial.print(seqNum);
  Serial.print("  Temp: ");
  Serial.print(temp, 1);
  Serial.print(" C  Hum: ");
  Serial.print(hum, 1);
  Serial.print(" %  Volt: ");
  Serial.print(volt, 2);
  Serial.print(" V  CHK: 0x");
  Serial.println(pkt[15], HEX);

  seqNum++;
  delay(TX_INTERVAL);
}
