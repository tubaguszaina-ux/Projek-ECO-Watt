#ifndef TELEMETRY_MODEL_H
#define TELEMETRY_MODEL_H

#include <Arduino.h>

// Model ini menyimpan state operasional sistem secara sementara di RAM.
struct TelemetryModel {
  // Data Pembacaan Sensor PZEM-004T
  float voltage      = 0.0;   // Tegangan (V)
  float current      = 0.0;   // Arus (A)
  float power        = 0.0;   // Daya Aktif (W)
  float energyKwh    = 0.0;   // Akumulasi Energi (kWh)

  // Data Presensi Bluetooth BLE
  bool targetFound   = false; // Status HP terdeteksi
  int currentRssi    = 0;     // Sinyal RSSI saat ini (dBm)

  // Status Output Hardware
  bool relayState    = false; // true = ON (220V Sambung), false = OFF (Cut-off)
  char cutReason[32] = "NONE"; // Reason: "SAFETY_VOLTAGE", "BLE_OUT_OF_RANGE", "FORCE_OFF"
};

#endif