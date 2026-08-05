#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "../models/ConfigModel.h"
#include "../models/TelemetryModel.h"

// RelayController: memindahkan logika output pada GPIO23 untuk
// menyalakan/mematikan relay berdasarkan evaluasi dari controller lain
// (Safety, Mode Override, dan BLE). Urutan prioritas mengikuti diagram
// alur scheduler pada SDD bagian 6.1.
class RelayController {
  private:
    static const uint8_t RELAY_PIN = 23;
    unsigned long outOfRangeSince = 0; // waktu pertama kali HP terdeteksi hilang
    bool wasOutOfRange = false;

  public:
    void begin(TelemetryModel &telemetry) {
      pinMode(RELAY_PIN, OUTPUT);
      // NFR-03 Fail-Safe State: default relay OFF saat baru menyala/restart
      setRelay(telemetry, false, "NONE");
    }

    // Dipanggil setiap iterasi loop scheduler.
    // Prioritas evaluasi: Safety Guard > Mode Override manual > Logika Auto BLE.
    void evaluate(ConfigModel &config, TelemetryModel &telemetry, bool voltageDangerous) {

      // Prioritas 1: Safety Guard - Cut-Off Darurat (<1 detik), mengabaikan mode apapun
      if (voltageDangerous) {
        setRelay(telemetry, false, "SAFETY_VOLTAGE");
        return;
      }

      // Prioritas 2: Mode Override manual dari Web Dashboard (FR-04)
      String mode = String(config.modeOverride);

      if (mode == "FORCE_ON") {
        resetBleTimer();
        setRelay(telemetry, true, "NONE");
        return;
      }

      if (mode == "FORCE_OFF") {
        resetBleTimer();
        setRelay(telemetry, false, "FORCE_OFF");
        return;
      }

      // Prioritas 3: Mode AUTO_BLE (FR-01, FR-02, FR-03)
      evaluateAutoBle(config, telemetry);
    }

  private:
    void evaluateAutoBle(ConfigModel &config, TelemetryModel &telemetry) {
      if (telemetry.targetFound) {
        // HP terdeteksi dalam jangkauan -> reset timer & sambungkan kembali (FR-03)
        resetBleTimer();
        setRelay(telemetry, true, "NONE");
        return;
      }

      // HP tidak terdeteksi -> mulai/lanjutkan hitung buffer delay
      unsigned long now = millis();
      if (!wasOutOfRange) {
        wasOutOfRange = true;
        outOfRangeSince = now;
      }

      unsigned long elapsedSec = (now - outOfRangeSince) / 1000;

      if (elapsedSec >= (unsigned long)config.bufferDelaySec) {
        // Buffer delay habis -> Auto Cut-Off (FR-02)
        setRelay(telemetry, false, "BLE_OUT_OF_RANGE");
      }
      // Selama buffer belum habis, relay TETAP pada kondisi terakhir -
      // mencegah mati-nyala mendadak akibat fluktuasi sinyal BLE (NFR-04)
    }

    void resetBleTimer() {
      wasOutOfRange = false;
      outOfRangeSince = 0;
    }

    void setRelay(TelemetryModel &telemetry, bool on, const char* reason) {
      digitalWrite(RELAY_PIN, on ? HIGH : LOW);
      telemetry.relayState = on;
      strncpy(telemetry.cutReason, reason, sizeof(telemetry.cutReason) - 1);
      telemetry.cutReason[sizeof(telemetry.cutReason) - 1] = '\0';
    }
};

#endif