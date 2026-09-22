#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int SENSOR_PIN = A0;
const int SENSOR_PWR = 8;   // VCC sensor dicolok ke D8
const int RELAY_PIN  = 7;

const int BATAS = 400;  // di bawah 400 = basah, selain itu = kering

const int RELAY_ON  = LOW;   // tukar kalau relay kebalik
const int RELAY_OFF = HIGH;

const unsigned long DURASI_STATUS   = 2000;
const unsigned long DURASI_NILAI    = 5000;
const unsigned long INTERVAL_BACA   = 50;
const unsigned long INTERVAL_SERIAL = 500;
const unsigned long REFRESH_LCD     = 1000;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // sesuaikan alamat kalau beda

bool menyiram = false;
bool hb = false;
int nilai = 0;
int layar = 0;  // 0 = status, 1 = nilai

unsigned long tBaca = 0, tSerial = 0, tRefresh = 0, tGanti = 0;

int bacaMoisture() {
  digitalWrite(SENSOR_PWR, HIGH);   // nyalakan sensor
  delay(20);                        // tunggu stabil
  analogRead(SENSOR_PIN);           // buang bacaan pertama

  int s[5];
  for (int i = 0; i < 5; i++) {
    s[i] = analogRead(SENSOR_PIN);
    delay(2);
  }
  digitalWrite(SENSOR_PWR, LOW);    // matikan sensor

  // urutkan, ambil nilai tengah (median)
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4 - i; j++) {
      if (s[j] > s[j + 1]) {
        int t = s[j]; s[j] = s[j + 1]; s[j + 1] = t;
      }
    }
  }
  return s[2];
}

void resetLCD() {
  lcd.init();
  lcd.backlight();
}

void tulis(int baris, const char* teks) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", teks);
  lcd.setCursor(0, baris);
  lcd.print(buf);
}

void tampil() {
  if (menyiram) {
    tulis(0, "SEDANG MENYIRAM");
    tulis(1, "Mohon tunggu...");
  }
  else if (layar == 0) {
    tulis(0, "Kondisi Tanah:");
    tulis(1, "TIDAK KERING");
  }
  else {
    char b[17];
    snprintf(b, sizeof(b), "%d", nilai);
    tulis(0, "Nilai Sensor:");
    tulis(1, b);
  }
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);  // pompa OFF saat start
  pinMode(SENSOR_PWR, OUTPUT);
  digitalWrite(SENSOR_PWR, LOW);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  Wire.begin();
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);
#endif

  resetLCD();
  tulis(0, "Penyiram Otomat");
  tulis(1, "Siap...");
  delay(1500);

  nilai = bacaMoisture();
  menyiram = (nilai >= BATAS);
  digitalWrite(RELAY_PIN, menyiram ? RELAY_ON : RELAY_OFF);

  unsigned long now = millis();
  tBaca = tSerial = tRefresh = tGanti = now;
  tampil();
}

void loop() {
  unsigned long now = millis();

  // 1. Baca sensor & kontrol relay
  if (now - tBaca >= INTERVAL_BACA) {
    nilai = bacaMoisture();
    now = millis();
    tBaca = now;

    bool kering = (nilai >= BATAS);
    if (kering != menyiram) {
      menyiram = kering;
      digitalWrite(RELAY_PIN, menyiram ? RELAY_ON : RELAY_OFF);
      delay(50);
      resetLCD();

      now = millis();
      layar = 0;
      tGanti = now;
      tRefresh = now;
      tampil();
    }
  }

  // 2. Ganti tampilan (hanya saat tidak menyiram)
  if (!menyiram) {
    unsigned long durasi = (layar == 0) ? DURASI_STATUS : DURASI_NILAI;
    if (now - tGanti >= durasi) {
      layar = !layar;
      tGanti = now;
      tRefresh = now;
      tampil();
    }
  }

  // 3. Gambar ulang LCD berkala
  if (now - tRefresh >= REFRESH_LCD) {
    tRefresh = now;
    tampil();
  }

  // 4. Serial + LED indikator hidup
  if (now - tSerial >= INTERVAL_SERIAL) {
    tSerial = now;

    hb = !hb;
    digitalWrite(LED_BUILTIN, hb);

    Serial.print("t=");
    Serial.print(now / 1000);
    Serial.print("s | Moisture: ");
    Serial.print(nilai);
    Serial.println(menyiram ? " | KERING -> NYIRAM" : " | BASAH -> STOP");
  }
}
