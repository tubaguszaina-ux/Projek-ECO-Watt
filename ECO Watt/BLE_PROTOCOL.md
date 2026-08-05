# ECO Watt - Protokol BLE GATT (untuk App Mobile)

Dokumen ini menjelaskan cara aplikasi mobile (Flutter) berkomunikasi dengan
ESP32 ECO Watt setelah migrasi dari Web Dashboard (WiFi) ke BLE GATT.
ESP32 mengiklankan diri (advertise) dengan nama **"ECO-Watt"**.

## Service & Characteristic UUID

| Nama            | UUID                                   | Properti      |
|-----------------|-----------------------------------------|----------------|
| Service         | `5f524c4e-0001-4a5b-9c1e-6f2b1a8d3c00`  | -              |
| Status          | `5f524c4e-0002-4a5b-9c1e-6f2b1a8d3c00`  | READ, NOTIFY   |
| Control         | `5f524c4e-0003-4a5b-9c1e-6f2b1a8d3c00`  | WRITE          |
| Config          | `5f524c4e-0004-4a5b-9c1e-6f2b1a8d3c00`  | WRITE          |

Semua nilai dikirim sebagai **string JSON UTF-8** (bukan binary terstruktur),
supaya mudah di-parse dari Flutter (`dart:convert`) maupun untuk debugging
manual pakai app seperti nRF Connect.

## Otorisasi (PIN)

Setiap WRITE ke karakteristik **Control** dan **Config** WAJIB menyertakan
field `"pin"`. Nilai default saat ini: `123456` (lihat `ConfigModel::blePin`,
HARDCODE sementara - harus diganti sebelum unit dipakai produksi nyata,
karena kontrol ini menyalakan/mematikan relay 220V).

## 1. Status (READ / NOTIFY)

ESP32 mem-push notifikasi tiap ~100ms kalau ada device yang subscribe.

```json
{
  "mode": "AUTO_BLE",
  "v": 220.3,
  "i": 1.25,
  "p": 275.4,
  "e": 3.42,
  "found": true,
  "rssi": -62,
  "relay": true,
  "reason": "NONE"
}
```

| Field    | Arti                                                          |
|----------|----------------------------------------------------------------|
| `mode`   | `AUTO_BLE` / `FORCE_ON` / `FORCE_OFF`                          |
| `v`      | Tegangan (Volt)                                                 |
| `i`      | Arus (Ampere)                                                   |
| `p`      | Daya aktif (Watt)                                                |
| `e`      | Akumulasi energi (kWh)                                           |
| `found`  | Apakah HP target terdeteksi dalam jangkauan aman                |
| `rssi`   | Sinyal RSSI HP target (dBm), `-999` jika tidak terlihat sama sekali |
| `relay`  | Status relay saat ini (`true` = ON)                              |
| `reason` | Alasan kondisi relay saat ini: `NONE`, `SAFETY_VOLTAGE`, `BLE_OUT_OF_RANGE`, `FORCE_OFF` |

## 2. Control (WRITE) - ganti mode relay

```json
{ "pin": "123456", "mode": "FORCE_ON" }
```

`mode` valid: `"AUTO_BLE"`, `"FORCE_ON"`, `"FORCE_OFF"`.

## 3. Config (WRITE) - ubah parameter (semua field opsional, kirim yang berubah saja)

```json
{
  "pin": "123456",
  "target_mac": "AA:BB:CC:DD:EE:FF",
  "rssi_threshold": -80,
  "buffer_delay_sec": 10,
  "min_voltage": 180,
  "max_voltage": 240
}
```

## Alternatif: Panel Kontrol Web (Web Bluetooth API)

Selain app Flutter, tersedia juga `web/index.html` — halaman web statis
(tanpa server, tanpa WiFi) yang connect langsung ke ESP32 lewat
**Web Bluetooth API** browser. Cukup buka file itu di Chrome/Edge lalu
klik "Hubungkan". Karena UUID service/characteristic sama persis dengan
yang didokumentasikan di atas, panel ini kompatibel dengan firmware
tanpa perubahan apa pun.

Dukungan browser: Chrome & Edge (desktop dan Android). **Tidak didukung
di Safari/iOS** — itu batasan Web Bluetooth di semua browser iOS, bukan
soal firmware/halaman ini.

## Catatan Implementasi Flutter

- Gunakan paket `flutter_blue_plus` (atau sejenis) untuk scan, connect,
  subscribe notify, dan write characteristic.
- ESP32 menjalankan Central (scan presensi HP) dan Peripheral (GATT server)
  bersamaan - koneksi app tidak akan mengganggu logika presence detection,
  tapi sesekali bisa terasa sedikit "nge-lag" saat scan 1 detik berjalan
  tiap 2 detik.
- MTU di-set ke 185 byte di firmware; pastikan app juga request MTU besar
  (`requestMtu(185)`) supaya payload JSON status tidak terpotong.
