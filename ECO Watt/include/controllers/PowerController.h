#ifndef POWER_CONTROLLER_H
#define POWER_CONTROLLER_H

#include <Arduino.h>
#include <PZEM004Tv30.h>
#include "../models/TelemetryModel.h"

// PowerController: membaca register data PZEM-004T via HardwareSerial UART2 (RX2/TX2)
class PowerController {
  private:
    PZEM004Tv30 pzem;

  public:
    // RX2 = GPIO16, TX2 = GPIO17 (default pin UART2 pada ESP32)
    PowerController(HardwareSerial &serial) : pzem(serial, 16, 17) {}

    void begin() {
      Serial2.begin(9600);
    }

    // Dipanggil setiap iterasi loop scheduler untuk update TelemetryModel
    void readSensor(TelemetryModel &telemetry) {
      float v = pzem.voltage();
      float i = pzem.current();
      float p = pzem.power();
      float e = pzem.energy();

      // PZEM mengembalikan NaN jika sensor belum terkoneksi/terbaca,
      // jadi nilai lama dipertahankan agar tidak merusak state
      if (!isnan(v)) telemetry.voltage   = v;
      if (!isnan(i)) telemetry.current   = i;
      if (!isnan(p)) telemetry.power     = p;
      if (!isnan(e)) telemetry.energyKwh = e;
    }
};

#endif