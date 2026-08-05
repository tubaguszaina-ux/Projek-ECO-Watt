#ifndef CONFIG_MODEL_H
#define CONFIG_MODEL_H

#include <Arduino.h>

// Model ini menyimpan variabel konfigurasi pengguna secara permanen
// di memori Flash NVS agar tidak hilang saat mati listrik.
//
// MIGRASI WiFi -> Bluetooth: field telegramToken/chatID/notifHour dihapus
// karena fitur Telegram (FR-07/08) dihilangkan sesuai keputusan agar sistem
// full-offline. Ditambahkan blePin untuk otorisasi dasar sebelum aplikasi
// mobile boleh menulis ke karakteristik GATT (kontrol relay 220V).
struct ConfigModel {
  char targetMac[18]      = "AA:BB:CC:DD:EE:FF"; // MAC Address HP Target
  int rssiThreshold       = -85;                 // Batas sinyal minimal (dBm)
  int bufferDelaySec      = 10;                  // Delay penundaan cut-off (detik)
  float minVoltage        = 180.0;                // Batas bawah voltase aman (V)
  float maxVoltage        = 240.0;                // Batas atas voltase aman (V)
  char modeOverride[10]   = "AUTO_BLE";           // "AUTO_BLE", "FORCE_ON", "FORCE_OFF"

  // PIN sementara (HARDCODE) untuk otorisasi WRITE via BLE GATT (Kontrol & Config).
  // TODO sebelum produksi: ganti default ini, dan idealnya jadikan proses
  // ganti PIN sendiri butuh re-autentikasi (bukan hanya field config biasa).
  char blePin[7]           = "123456";
};

#endif
