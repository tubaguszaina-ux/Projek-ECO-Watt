#ifndef SAFETY_CONTROLLER_H
#define SAFETY_CONTROLLER_H

#include <Arduino.h>
#include "../models/ConfigModel.h"
#include "../models/TelemetryModel.h"

// SafetyController: mengevaluasi tegangan (V). Jika di luar rentang aman
// (default 180V-240V), memicu Cut-Off Darurat dalam <1 detik (FR-06).
class SafetyController {
  public:
    // Mengembalikan true jika kondisi tegangan BERBAHAYA (di luar rentang aman)
    bool isVoltageDangerous(ConfigModel &config, TelemetryModel &telemetry) {
      float v = telemetry.voltage;

      // Abaikan pembacaan 0V (sensor belum siap/belum terkoneksi ke PLN)
      // supaya tidak dianggap under-voltage palsu saat sistem baru menyala
      if (v <= 0.0) {
        return false;
      }

      return (v < config.minVoltage || v > config.maxVoltage);
    }
};

#endif