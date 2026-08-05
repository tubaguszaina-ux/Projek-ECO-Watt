#ifndef BLE_GATT_CONTROLLER_H
#define BLE_GATT_CONTROLLER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "../models/ConfigModel.h"
#include "../models/TelemetryModel.h"

// ==== UUID Layanan & Karakteristik GATT Kustom ECO Watt ====
// Dipakai sama persis oleh aplikasi mobile (Flutter) untuk kontrol.
// Lihat BLE_PROTOCOL.md di root repo untuk dokumentasi lengkap payload.
#define ECOWATT_SERVICE_UUID   "5f524c4e-0001-4a5b-9c1e-6f2b1a8d3c00"
#define CHAR_STATUS_UUID       "5f524c4e-0002-4a5b-9c1e-6f2b1a8d3c00" // READ + NOTIFY
#define CHAR_CONTROL_UUID      "5f524c4e-0003-4a5b-9c1e-6f2b1a8d3c00" // WRITE (mode override)
#define CHAR_CONFIG_UUID       "5f524c4e-0004-4a5b-9c1e-6f2b1a8d3c00" // WRITE (parameter config)

// BleGattController: penerus BLEController lama, sekarang menjalankan
// DUA peran BLE sekaligus (dual-role), sesuai keputusan migrasi WiFi->BLE:
//
//  1. CENTRAL  - scan MAC Address HP target tiap 2 detik, logika identik
//                dengan BLEController lama (FR-01, FR-02).
//  2. PERIPHERAL - GATT server yang diiklankan (advertise) terus-menerus,
//                supaya aplikasi mobile bisa connect & kontrol relay/config
//                sebagai pengganti Web Dashboard (FR-04, FR-09).
//
// Library diganti dari BLEDevice/BLEScan bawaan Arduino-ESP32 (stack
// Bluedroid) ke NimBLE-Arduino: RAM jauh lebih hemat dan didesain untuk
// menjalankan Central+Peripheral bersamaan tanpa konflik radio.
class BleGattController {
  private:
    // --- Bagian Central (presence scan) ---
    NimBLEScan* pBLEScan;
    unsigned long lastScanTime = 0;
    const unsigned long scanIntervalMs = 2000; // sesuai SDD: setiap 2 detik
    const uint32_t scanDurationMs = 1000;      // NimBLE pakai satuan ms (bukan detik seperti lib lama)

    // --- Bagian Peripheral (GATT server) ---
    NimBLEServer* pServer = nullptr;
    NimBLECharacteristic* pStatusChar = nullptr;
    NimBLECharacteristic* pControlChar = nullptr;
    NimBLECharacteristic* pConfigChar = nullptr;

    ConfigModel* config;
    TelemetryModel* telemetry;
    void (*onConfigSaved)();

    // --- Callback tulis karakteristik (di-forward ke method private di bawah) ---
    class ControlCallbacks : public NimBLECharacteristicCallbacks {
      private:
        BleGattController* owner;
      public:
        ControlCallbacks(BleGattController* o) : owner(o) {}
        void onWrite(NimBLECharacteristic* pChar) override {
          owner->handleControlWrite(pChar->getValue());
        }
    };

    class ConfigCallbacks : public NimBLECharacteristicCallbacks {
      private:
        BleGattController* owner;
      public:
        ConfigCallbacks(BleGattController* o) : owner(o) {}
        void onWrite(NimBLECharacteristic* pChar) override {
          owner->handleConfigWrite(pChar->getValue());
        }
    };

    ControlCallbacks* controlCallbacks = nullptr;
    ConfigCallbacks* configCallbacks = nullptr;

  public:
    void begin(ConfigModel &cfg, TelemetryModel &tel, void (*saveCallback)()) {
      config = &cfg;
      telemetry = &tel;
      onConfigSaved = saveCallback;

      NimBLEDevice::init("ECO-Watt");
      NimBLEDevice::setMTU(185); // cukup besar agar payload JSON status muat sekali kirim

      setupPeripheral();
      setupCentralScan();

      Serial.println("[BLE] GATT server aktif & presence scan berjalan (dual-role, full offline).");
    }

    // Dipanggil tiap iterasi loop scheduler (menggantikan scanTarget() lama).
    // Scan fisik HP target tetap throttle 2 detik; notifikasi status ke app
    // dikirim tiap kali dipanggil (~tiap 100ms sesuai delay() di main loop).
    void update() {
      scanTarget();
      pushStatusNotify();
    }

  private:
    // ==== Setup Peripheral: Service + 3 Characteristic + Advertising ====
    void setupPeripheral() {
      pServer = NimBLEDevice::createServer();
      NimBLEService* pService = pServer->createService(ECOWATT_SERVICE_UUID);

      pStatusChar = pService->createCharacteristic(
        CHAR_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
      );

      controlCallbacks = new ControlCallbacks(this);
      pControlChar = pService->createCharacteristic(
        CHAR_CONTROL_UUID,
        NIMBLE_PROPERTY::WRITE
      );
      pControlChar->setCallbacks(controlCallbacks);

      configCallbacks = new ConfigCallbacks(this);
      pConfigChar = pService->createCharacteristic(
        CHAR_CONFIG_UUID,
        NIMBLE_PROPERTY::WRITE
      );
      pConfigChar->setCallbacks(configCallbacks);

      pService->start();

      NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
      pAdvertising->addServiceUUID(ECOWATT_SERVICE_UUID);
      pAdvertising->setScanResponse(true);
      pAdvertising->start();
    }

    // ==== Setup Central: konfigurasi scanner sama seperti BLEController lama ====
    void setupCentralScan() {
      pBLEScan = NimBLEDevice::getScan();
      pBLEScan->setActiveScan(true);
      pBLEScan->setInterval(100);
      pBLEScan->setWindow(99);
    }

    // Logika identik dengan BLEController lama (FR-01/FR-02), hanya dipindah
    // ke dalam controller gabungan ini. CATATAN: scan tetap blocking selama
    // scanDurationMs (1 detik) tiap 2 detik - perilaku sama seperti sebelumnya,
    // koneksi GATT yang sudah terbentuk tidak terputus selama scan berjalan.
    void scanTarget() {
      unsigned long now = millis();
      if (now - lastScanTime < scanIntervalMs) {
        return; // belum waktunya scan lagi
      }
      lastScanTime = now;

      NimBLEScanResults foundDevices = pBLEScan->getResults(scanDurationMs, false);

      bool deviceSeen = false;
      int rssiResult = -999; // -999 = HP tidak terlihat sama sekali di hasil scan

      for (int i = 0; i < foundDevices.getCount(); i++) {
        NimBLEAdvertisedDevice device = foundDevices.getDevice(i);
        String deviceMac = device.getAddress().toString().c_str();

        if (deviceMac.equalsIgnoreCase(config->targetMac)) {
          deviceSeen = true;
          rssiResult = device.getRSSI();
          break;
        }
      }

      telemetry->currentRssi = rssiResult;

      // FR-02: HP dianggap "dalam jangkauan aman" hanya jika terlihat DAN
      // sinyalnya masih di atas rssiThreshold.
      telemetry->targetFound = deviceSeen && (rssiResult >= config->rssiThreshold);

      pBLEScan->clearResults();
    }

    // Kirim status terkini ke app mobile via NOTIFY (kalau ada yang connect).
    // Format payload didokumentasikan di BLE_PROTOCOL.md.
    void pushStatusNotify() {
      StaticJsonDocument<256> doc;
      doc["mode"]   = config->modeOverride;
      doc["v"]      = telemetry->voltage;
      doc["i"]      = telemetry->current;
      doc["p"]      = telemetry->power;
      doc["e"]      = telemetry->energyKwh;
      doc["found"]  = telemetry->targetFound;
      doc["rssi"]   = telemetry->currentRssi;
      doc["relay"]  = telemetry->relayState;
      doc["reason"] = telemetry->cutReason;

      String payload;
      serializeJson(doc, payload);

      pStatusChar->setValue(payload);
      if (pServer->getConnectedCount() > 0) {
        pStatusChar->notify();
      }
    }

    // Semua WRITE (Control & Config) wajib menyertakan PIN yang benar.
    bool checkPin(JsonObject &obj) {
      if (!obj.containsKey("pin")) return false;
      String pin = obj["pin"].as<String>();
      return pin.equals(config->blePin);
    }

    // Tulis ke CHAR_CONTROL_UUID: {"pin":"123456","mode":"FORCE_ON"}
    void handleControlWrite(std::string value) {
      StaticJsonDocument<128> doc;
      if (deserializeJson(doc, value)) {
        Serial.println("[BLE] Payload Control bukan JSON valid, diabaikan.");
        return;
      }

      JsonObject obj = doc.as<JsonObject>();
      if (!checkPin(obj)) {
        Serial.println("[BLE] Kontrol ditolak: PIN salah/tidak ada.");
        return;
      }

      if (!obj.containsKey("mode")) return;
      String mode = obj["mode"].as<String>();
      if (mode != "AUTO_BLE" && mode != "FORCE_ON" && mode != "FORCE_OFF") {
        Serial.println("[BLE] Mode tidak valid: " + mode);
        return;
      }

      strncpy(config->modeOverride, mode.c_str(), sizeof(config->modeOverride) - 1);
      config->modeOverride[sizeof(config->modeOverride) - 1] = '\0';

      if (onConfigSaved) onConfigSaved(); // simpan ke NVS (FR-09)
      Serial.println("[BLE] Mode override diperbarui via app: " + mode);
    }

    // Tulis ke CHAR_CONFIG_UUID: {"pin":"123456","target_mac":"...", "rssi_threshold":-80, ...}
    void handleConfigWrite(std::string value) {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, value)) {
        Serial.println("[BLE] Payload Config bukan JSON valid, diabaikan.");
        return;
      }

      JsonObject obj = doc.as<JsonObject>();
      if (!checkPin(obj)) {
        Serial.println("[BLE] Update config ditolak: PIN salah/tidak ada.");
        return;
      }

      if (obj.containsKey("target_mac")) {
        strncpy(config->targetMac, obj["target_mac"].as<const char*>(), sizeof(config->targetMac) - 1);
        config->targetMac[sizeof(config->targetMac) - 1] = '\0';
      }
      if (obj.containsKey("rssi_threshold")) {
        config->rssiThreshold = obj["rssi_threshold"].as<int>();
      }
      if (obj.containsKey("buffer_delay_sec")) {
        config->bufferDelaySec = obj["buffer_delay_sec"].as<int>();
      }
      if (obj.containsKey("min_voltage")) {
        config->minVoltage = obj["min_voltage"].as<float>();
      }
      if (obj.containsKey("max_voltage")) {
        config->maxVoltage = obj["max_voltage"].as<float>();
      }

      if (onConfigSaved) onConfigSaved(); // simpan ke NVS (FR-09)
      Serial.println("[BLE] Konfigurasi diperbarui via app.");
    }
};

#endif
