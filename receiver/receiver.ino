/*
 * CubeSat Telemetry Simulator — RECEIVER (Ground Station Side)
 *
 * Hardware : Arduino Uno/Nano + LoRa RA-02 (SX1278)
 * Author   : Shuvojyoti Palit
 * Version  : 1.0
 *
 * Wiring:
 *   LoRa RA-02  → Arduino
 *   VCC         → 3.3V
 *   GND         → GND
 *   SCK         → D13 (via 1kΩ+2kΩ voltage divider)
 *   MISO        → D12 (direct)
 *   MOSI        → D11 (via 1kΩ+2kΩ voltage divider)
 *   NSS/CS      → D10
 *   DIO0        → D2
 *   RST         → D9
 *
 * Output format (CSV over Serial):
 *   seq,temp,humidity,voltage,rssi,snr,lost,errors
 *
 * Libraries:
 *   - LoRa by Sandeep Mistry
 */

#include <SPI.h>
#include <LoRa.h>

// LoRa pin definitions 
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

//  Must match transmitter exactly 
#define LORA_FREQ  433E6
#define LORA_SF    7
#define LORA_BW    125E3
#define LORA_CR    5

// Packet constants
#define PKT_SIZE  17
#define PKT_START 0xAA
#define PKT_END   0x55

// Stats 
uint16_t lastSeq   = 0;
uint32_t totalPkts = 0;
uint32_t errorPkts = 0;
uint32_t lostPkts  = 0;
bool     firstPkt  = true;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("============================");
  Serial.println("  CubeSat RX — INITIALIZING");
  Serial.println("============================");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("[FAIL] LoRa init failed — check wiring");
    while (1);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  Serial.println("[OK] LoRa initialized at 433 MHz");

  Serial.println("============================");
  Serial.println("  CubeSat RX — GND READY");
  Serial.println("============================");
  Serial.println("Waiting for packets...");
  Serial.println();

  // CSV header — Python uses this to parse columns
  Serial.println("seq,temp,humidity,voltage,rssi,snr,lost,errors");
}

uint8_t calcChecksum(uint8_t* buf, int len) {
  uint8_t cs = 0;
  for (int i = 0; i < len; i++) cs ^= buf[i];
  return cs;
}

void loop() {

  int pktSize = LoRa.parsePacket();
  if (pktSize == 0) return;

  //  Wrong size 
  if (pktSize != PKT_SIZE) {
    Serial.print("[WARN] Wrong packet size: ");
    Serial.println(pktSize);
    while (LoRa.available()) LoRa.read();
    return;
  }

  // Read bytes 
  uint8_t buf[PKT_SIZE];
  for (int i = 0; i < PKT_SIZE; i++) buf[i] = LoRa.read();

  //  Check frame markers 
  if (buf[0] != PKT_START || buf[16] != PKT_END) {
    Serial.println("[WARN] Bad frame markers — discarded");
    errorPkts++;
    return;
  }

  //Verify checksum 
  uint8_t expectedCS = calcChecksum(buf, 15);
  if (expectedCS != buf[15]) {
    Serial.print("[WARN] Checksum error. Got: 0x");
    Serial.print(buf[15], HEX);
    Serial.print("  Expected: 0x");
    Serial.println(expectedCS, HEX);
    errorPkts++;
    return;
  }

  //  Unpack sequence number 
  uint16_t seq = ((uint16_t)buf[1] << 8) | buf[2];

  // Detect lost packets 
  if (!firstPkt && seq > lastSeq + 1) {
    uint16_t lost = seq - lastSeq - 1;
    lostPkts += lost;
    Serial.print("[WARN] Lost ");
    Serial.print(lost);
    Serial.println(" packet(s)");
  }
  firstPkt = false;
  lastSeq  = seq;
  totalPkts++;

  //  Unpack sensor floats 
  float temp, hum, volt;
  memcpy(&temp, &buf[3],  4);
  memcpy(&hum,  &buf[7],  4);
  memcpy(&volt, &buf[11], 4);

  // Signal quality 
  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();

  // Output CSV for Python 
  Serial.print(seq);        Serial.print(",");
  Serial.print(temp,  2);   Serial.print(",");
  Serial.print(hum,   2);   Serial.print(",");
  Serial.print(volt,  3);   Serial.print(",");
  Serial.print(rssi);       Serial.print(",");
  Serial.print(snr,   1);   Serial.print(",");
  Serial.print(lostPkts);   Serial.print(",");
  Serial.println(errorPkts);
}
