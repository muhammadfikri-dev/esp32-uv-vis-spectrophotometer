# ⚡ ESP32 UV-Vis Spectrophotometer Optical Absorbance Analyzer

[![Lisensi: MIT](https://img.shields.io/badge/Lisensi-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32%20|%20FreeRTOS-blue.svg)](#)
[![Framework: Arduino IDE](https://img.shields.io/badge/Framework-Arduino%20IDE%202.0%2B-teal.svg)](https://www.arduino.cc/)
[![Status: Firmware Produksi](https://img.shields.io/badge/Status-Firmware%20Produksi-brightgreen.svg)](#)
[![Developer: Muhammad Fikri](https://img.shields.io/badge/Developer-Muhammad%20Fikri-blue.svg)](#)

Optical absorption spectrometer controlling stepper monochromator diffraction grating, Hamamatsu photodiode array, and computing Beer-Lambert absorbance curves.

---

## 📊 Diagram Blok Arsitektur & Skema Alur Rangkaian

Visualisasi interaktif alur daya, akuisisi sinyal sensor, pemrosesan algoritma inti, dan aktuasi proteksi perangkat:

```mermaid
graph TD
    subgraph Instrument_FrontEnd ["🔬 Front-End Instrumen Presisi"]
        PROBE["Probe Pengujian / Transduser Laboratorium"] --> INA["Penguat Instrumentasi INA128"]
        REF["Tegangan Referensi LM399 (Ultra-Low Noise)"] --> ADC["ADC 24-Bit ADS1256"]
        INA --> ADC
        ADC -->|"SPI / I2C"| MCU["🧠 ESP32 (Dual-Core Xtensa LX6)"]
    end

    subgraph Math_Engine ["🧠 Pemrosesan Sinyal Digital (DSP)"]
        MCU -->|"Oversampling 64x"| DSP["Filtering & Penghilang Noise Termal"]
        DSP -->|"Polynomial Calibration"| CALIB["Koreksi Non-Linearitas & Zero Tare"]
        CALIB -->|"Kalkulasi Metrik"| RESULT["Hasil Ukur 6.5 Digit / Spektrum"]
    end

    subgraph Scientific_UI ["📊 Output & Antarmuka Data"]
        RESULT -->|"I2C"| GRAPH["Layar Grafis OLED / LCD Display"]
        MCU -->|"USB / Serial"| PC["Software Analitik PC (LabVIEW / Python)"]
        MCU -->|"Trigger"| RELAY_SAFE["Relay Proteksi Over-Range"]
    end

    style MCU fill:#1565c0,stroke:#0d47a1,stroke-width:2px,color:#fff
    style CALIB fill:#2e7d32,stroke:#1b5e20,stroke-width:2px,color:#fff
    style GRAPH fill:#00838f,stroke:#006064,stroke-width:2px,color:#fff
```

---

## 📦 Daftar Komponen & Bahan Lengkap (Bill of Materials - BOM)

Berikut rincian spesifikasi komponen fisik dan modul yang dibutuhkan untuk membangun proyek ini:

| No | Nama Komponen / Modul | Estimasi Jumlah | Fungsi & Spesifikasi Teknis |
|:---|:---|:---|:---|
| 1 | **ESP32 Dev Module (30/38-Pin)** | 1 Unit | Mikrokontroler Dual-Core dengan FreeRTOS, Wi-Fi 2.4GHz & BLE |
| 2 | **Regulator Step-Down LM2596 / PSU 5V 2A** | 1 Unit | Sumber daya listrik 5V DC teregulasi |
| 3 | **Modul ADC 24-Bit Presisi Tinggi ADS1256 / ADS1115** | 1 Unit | Konversi analog-ke-digital resolusi tinggi untuk instrumen laboratorium |
| 4 | **Referensi Tegangan Ultra-Stabil LM399 / REF5025** | 1 Unit | Standar referensi tegangan presisi rendah noise |
| 5 | **Penguat Instrumentasi INA128 / INA219** | 1 Unit | Penguat sinyal diferensial microvolt |
| 6 | **Layar Grafis OLED SSD1306 / LCD 20x4 I2C** | 1 Unit | Visualisasi kurva spektrum dan pembacaan 6 digit |
| 7 | **Rotary Encoder & Tombol Kalibrasi Zero/Tare** | 1 Set | Pengaturan kalibrasi instrumen digital |

---

## 🧠 Arsitektur Sistem & Fitur Utama

- **FreeRTOS Multi-Core Priority Scheduling:** Memisahkan pemrosesan sinyal presisi tinggi dari task telemetri untuk mencegah *latency jitter*.
- **Digital Signal Processing (DSP) & Filtering:** Dilengkapi algoritma digital filtering terdedikasi untuk eliminasi derau sinyal analog.
- **Non-Volatile Storage (NVS Preferences Flash):** Parameter kalibrasi, *setpoint*, dan konfigurasi tersimpan secara persisten terhadap siklus pemadaman daya.
- **Hardware Failsafe & Emergency Interlock:** Perlindungan otomatis jika terjadi anomali tegangan, kelebihan beban arus, atau pemicuan tombol *Emergency Stop*.
- **Industrial Telemetry & Diagnostics:** Pelaporan status operasional secara real-time via Serial/JSON stream.

---

## 🔌 Skema Pinout & Koneksi Hardware

| Komponen / Sinyal | Pin (ESP32) | Deskripsi Fungsi |
|:---|:---|:---|
| **Sensor Analog Input** | `GPIO 36 (ADC1)` | Jalur pembacaan sensor utama berpresisi tinggi |
| **Emergency Stop (E-Stop)** | `GPIO 34` | Pemicu pengaman darurat hardware interrupt |
| **Actuator / Relay Utama** | `GPIO 26` | Pengendali beban daya tinggi / relay aktuator |
| **Acoustic Alarm Buzzer** | `GPIO 25` | Indikator peringatan audible saat terjadi anomali |
| **Status / Heartbeat LED** | `GPIO 2` | Indikator status aktivitas sistem real-time |

---

## 🛠️ Panduan Perakitan Hardware (Langkah Demi Langkah)

1. **Persiapan Catu Daya:** Hubungkan catu daya utama ke jalur daya mikrokontroler. Pasang kapasitor *decoupling* 100nF di dekat pin VCC untuk meredam ripple switching.
2. **Pemasangan Sensor & Modul:** Sambungkan jalur sinyal sensor ke pin mikrokontroler yang telah ditentukan. Gunakan resistor pull-up 4.7kΩ pada jalur SDA/SCL jika menggunakan modul I2C.
3. **Pemasangan Aktuator:** Hubungkan modul relay / gate driver MOSFET ke pin kontrol output. Pasang dioda *flyback* (1N4007) pada beban induktif untuk mengeliminasi lonjakan tegangan balik (*back-EMF*).
4. **Pemasangan Tombol Emergency Stop:** Sambungkan tombol darurat ke pin interupsi eksternal dengan konfigurasi *Active-LOW* menggunakan resistor *pull-up*.
5. **Verifikasi Koneksi:** Lakukan pengecekan jalur ground bersama (*Common Ground*) pada seluruh modul sebelum menyalakan daya.

---

## 🚀 Panduan Kompilasi & Upload (Arduino IDE)

1. Buka **Arduino IDE 2.0+**.
2. Masuk ke menu **Tools > Board**:
   * Pilih **`ESP32 Dev Module`**.
3. Pastikan dependensi pustaka terpasang via Library Manager:
   * `ArduinoJson`
   * `Wire` & `SPI`
   * `Preferences`
4. Buka berkas [`esp32-uv-vis-spectrophotometer.ino`](./esp32-uv-vis-spectrophotometer.ino).
5. Klik tombol **Verify** (✓) kemudian **Upload** (➔).
6. Buka **Serial Monitor** pada baudrate **`115200`** untuk melihat streaming telemetri dan status operasional.

---

## 📄 Lisensi
Didistribusikan di bawah lisensi open-source **MIT License**. Dikembangkan oleh **Muhammad Fikri**.
