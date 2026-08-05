#include <Arduino.h>
#include <Preferences.h>

#include "models/ConfigModel.h"
#include "models/TelemetryModel.h"
#include "controllers/PowerController.h"
#include "controllers/BleGattController.h"
#include "controllers/SafetyController.h"
#include "controllers/RelayController.h"

// ==== INSTANCE MODEL (state global, dipakai bersama semua controller) ====
ConfigModel config;
TelemetryModel telemetry;

// ==== INSTANCE CONTROLLER ====
// CATATAN MIGRASI WiFi -> Bluetooth:
//  - WiFi, NTP, LittleFS, WebServerView, dan NotifController (Telegram)
//    dihapus total - sistem sekarang full offline, tidak butuh internet.
//  - BLEController lama diganti BleGattController: tetap menjalankan scan
//    presensi HP (FR-01/02) SEKALIGUS jadi GATT server untuk app mobile
//    (pengganti Web Dashboard, FR-04/09), dual-role via NimBLE-Arduino.
PowerController powerController(Serial2);
BleGattController bleGattController;
SafetyController safetyController;
RelayController relayController;

// ==== NVS Preferences (FR-09) - tetap dipakai, tidak terkait WiFi ====
Preferences preferences;
const char* NVS_NAMESPACE = "ecowatt";
const char* NVS_KEY       = "config";

// ==== FUNGSI PENYIMPANAN NVS ====
void loadConfigFromNVS() {
  preferences.begin(NVS_NAMESPACE, true); // mode read-only
  size_t storedLen = preferences.getBytesLength(NVS_KEY);

  if (storedLen == sizeof(ConfigModel)) {
    preferences.getBytes(NVS_KEY, &config, sizeof(ConfigModel));
    Serial.println("[NVS] Konfigurasi berhasil dimuat.");
  } else {
    Serial.println("[NVS] Data tidak ditemukan, menggunakan Default Fallback Value.");
    // config tetap pada nilai default dari struct ConfigModel (NFR-06)
  }
  preferences.end();
}

// Callback ini dipasok ke BleGattController, dipanggil tiap kali karakteristik
// Control atau Config berhasil ditulis oleh app mobile, supaya perubahan
// langsung permanen di Flash.
void saveConfigToNVS() {
  preferences.begin(NVS_NAMESPACE, false); // mode read-write
  preferences.putBytes(NVS_KEY, &config, sizeof(ConfigModel));
  preferences.end();
  Serial.println("[NVS] Konfigurasi disimpan.");
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);

  // 1. Muat konfigurasi tersimpan dari NVS Flash (FR-09)
  loadConfigFromNVS();

  // 2. Inisialisasi hardware & controller (tanpa WiFi/NTP)
  powerController.begin();
  relayController.begin(telemetry); // NFR-03 Fail-Safe State: default relay OFF
  bleGattController.begin(config, telemetry, saveConfigToNVS); // scan presensi + GATT server

  Serial.println("[SYSTEM] ECO Watt siap beroperasi (full offline via Bluetooth).");
}

// ==== LOOP SCHEDULER ====
// Urutan mengikuti diagram alur SDD bagian 6.1 (versi offline):
// Read Power -> Safety Guard -> BLE (scan presensi + GATT server) -> Evaluasi Relay
void loop() {
  // 1. Baca data daya dari sensor PZEM-004T (FR-05)
  powerController.readSensor(telemetry);

  // 2. Evaluasi Safety Guard - overvoltage/undervoltage (FR-06)
  bool voltageDangerous = safetyController.isVoltageDangerous(config, telemetry);

  // 3. BLE dual-role: scan presensi HP (FR-01, throttle 2 detik di dalamnya)
  //    + kirim status terkini ke app mobile via NOTIFY
  bleGattController.update();

  // 4. Evaluasi & eksekusi Relay - prioritas: Safety > Override manual (dari
  //    app via BLE) > Auto BLE presence (FR-02/03/04)
  relayController.evaluate(config, telemetry, voltageDangerous);

  delay(100); // jeda kecil, memberi ruang untuk stack BLE ESP32 bekerja
}
