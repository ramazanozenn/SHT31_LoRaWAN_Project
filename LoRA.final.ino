#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// SHT31 sensör objesi
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// LoRaWAN Kimlik Bilgileri
// BU ALANLARI KENDİ TTN CİHAZ BİLGİLERİNLE DOLDURMALISIN!
// Örnek değerler SİZİN GÜNCEL DEĞERLERİNİZ DEĞİLDİR!

// DEVEUI: Ters sıra (LSB first) - TTN'deki "End device EUI" değerinin LSB çevrilmiş hali
// Örnek: TTN'de 227627B67B73E73E ise, buraya {0x3E, 0xE7, 0x73, 0x7B, 0xB6, 0x27, 0x76, 0x22} yazın.
static const u1_t PROGMEM DEVEUI[8] = { 0x3E, 0xE7, 0x73, 0x7B, 0xB6, 0x27, 0x76, 0x22 };

// APPEUI (JoinEUI): Ters sıra (LSB first) - TTN'e "0000000000000000" girdiysen, burası {0x00, 0x00, ...} kalır.
// Eğer TTN sana başka bir JoinEUI verdiyse, onun LSB çevrilmiş halini kullanın.
static const u1_t PROGMEM APPEUI[8] = { 0xFF, 0x9C, 0x59, 0x2F, 0xA9, 0xBE, 0x14, 0x13 };

// APPKEY: Düz sıra (MSB first) - TTN'deki "AppKey" değerinin bayt bayt ayrılmış hali
// Örnek: TTN'de F0EE877C22F0A718AC7AE6911ED84732 ise, aşağıdaki gibi yazın.
static const u1_t PROGMEM APPKEY[16] = {
  0xF0, 0xEE, 0x87, 0x7C, 0x22, 0xF0, 0xA7, 0x18, // İlk 8 bayt
  0xAC, 0x7A, 0xE6, 0x91, 0x1E, 0xD8, 0x47, 0x32  // Son 8 bayt
};


// Bu fonksiyonlar LMIC kütüphanesinin kimlik bilgilerine erişmesini sağlar
void os_getArtEui(u1_t* buf)  { memcpy_P(buf, APPEUI, 8); }
void os_getDevEui(u1_t* buf)  { memcpy_P(buf, DEVEUI, 8); }
void os_getDevKey(u1_t* buf)  { memcpy_P(buf, APPKEY, 16); }

// Periyodik görev için osjob objesi
static osjob_t sendjob;

// LoRa Shield v1.4 için LMIC Pin Haritası (Arduino UNO uyumlu, DIO0 D2'ye, DIO1 D3'e)
// Lütfen bu pinlerin shield'inizin Arduino'ya doğru oturduğundan ve kabloların doğru bağlandığından emin olun.
const lmic_pinmap lmic_pins = {
  .nss = 10,                 // LoRa modülünün NSS pini (genellikle SPI Slave Select)
  .rxtx = LMIC_UNUSED_PIN,   // Bazı modüllerde anten kontrolü için kullanılır, Dragino'da genelde kullanılmaz
  .rst = 9,                  // LoRa modülünün RST pini (reset)
  .dio = {2, 3, LMIC_UNUSED_PIN} // DIO0 -> D2, DIO1 -> D3, DIO2 genellikle kullanılmaz
};

// LMIC Olay Yönetimi Fonksiyonu
void onEvent(ev_t ev) {
  switch(ev) {
    case EV_JOINING:
      Serial.println(F("TTN ağına katılıyor...")); // Katılım girişimi
      break;
    case EV_JOINED:
      Serial.println(F("TTN ağına katıldı!")); // Başarılı katılım
      // İlk veri gönderimini ağa katıldıktan hemen sonra başlat
      do_send(&sendjob);
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("Veri gönderildi.")); // Veri paketi gönderimi tamamlandı
      // Bir sonraki veri gönderimi için zamanlayıcı ayarla (örneğin 60 saniye sonra)
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(60), do_send);
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("TTN ağına katılım başarısız oldu!")); // Katılım hatası
      // Hata durumunda tekrar denemek için bir süre bekle
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(30), do_send);
      break;
    case EV_TXSTART:
      Serial.println(F("Gönderim başlıyor...")); // Veri gönderimi başlamak üzere
      break;
    case EV_RXSTART:
      Serial.println(F("Alım başlıyor...")); // Cevap bekleniyor
      break;
    // case EV_JOIN_ACCEPT: // Bu satırı ve altındaki kodu yoruma alın veya silin
    //   Serial.println(F("Katılım kabulü alındı."));
    //   break;
    case EV_RFU1:
      // RFU1 genellikle LMiC'in durum bilgisini gösterir, önemli değil
      break;
    default:
      // Diğer olayları RAM için bastırmayalım
      break;
  }
}
// Sensör verilerini okuyup LoRaWAN paketi olarak gönderen fonksiyon
void do_send(osjob_t* j) {
  // LMIC meşgul ise gönderme
  if (LMIC.opmode & OP_TXRXPEND) {
      Serial.println(F("Önceki gönderim bekleniyor, tekrar denenecek..."));
      // Kısa bir gecikme ile tekrar dene
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(5), do_send);
      return;
  }

  // SHT31 sensör verilerini oku
  Serial.println(F("--- SHT31 verileri okunuyor ve paketleniyor ---")); // Debug mesajı
  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();

  Serial.print(F("Okunan Ham Sicaklik: ")); Serial.println(temp); // Debug mesajı
  Serial.print(F("Okunan Ham Nem: ")); Serial.println(hum);     // Debug mesajı

  // Sensör okumalarında hata kontrolü
  if (isnan(temp) || isnan(hum)) {
    Serial.println(F("SHT31 verisi okunamadı! (NaN değerler) Tekrar denenecek..."));
    // Başarısız okumada tekrar denemek için kısa bir gecikme
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(10), do_send);
    return;
  }

  // Payload oluşturma (4 bayt: sıcaklık ve nem * 100)
  uint8_t payload[4];
  int t = (int)(temp * 100); // Sıcaklığı 100 ile çarparak int'e çevir
  int h = (int)(hum * 100);  // Nemi 100 ile çarparak int'e çevir

  payload[0] = t >> 8;      // Sıcaklığın yüksek baytı
  payload[1] = t & 0xFF;    // Sıcaklığın düşük baytı
  payload[2] = h >> 8;      // Nemin yüksek baytı
  payload[3] = h & 0xFF;    // Nemin düşük baytı

  // Seri monitöre sıcaklık ve nem değerlerini yazdır
  Serial.print(F("Sıcaklık: "));
  Serial.print(temp);
  Serial.print(F(" °C, Nem: "));
  Serial.print(hum);
  Serial.println(F(" %"));

  // LoRaWAN paketi gönderme (Port 1, onaysız - unconfirmed)
  // İlk parametre port numarasıdır, 1 genellikle sensör verisi için kullanılır.
  // Son parametre 0: unconfirmed (onaysız), 1: confirmed (onaylı)
  LMIC_setTxData2(1, payload, sizeof(payload), 0);
  Serial.println(F("Veri gönderiliyor..."));
}

void setup() {
  Serial.begin(9600);
  delay(3000); // Seri portun başlaması için yeterli süre
  Serial.println(F("Başlatılıyor...")); // NOKTA 1

  Wire.begin();
  Serial.println(F("I2C başlatıldı.")); // NOKTA 2

  if (!sht31.begin(0x44)) {
    Serial.println(F("SHT31 algılanamadı! Lütfen bağlantıları ve adresi kontrol edin."));
    //while (1); // Sensör bulunamazsa burada sonsuz döngüye girer, program durur.
  } else {
    Serial.println(F("SHT31 başarıyla algılandı.")); // NOKTA 3
  }

  // LMIC ve LoRa modülü başlatma
  Serial.println(F("OS başlatılıyor...")); // NOKTA 4
  os_init();
  Serial.println(F("OS başlatıldı."));     // NOKTA 5

  Serial.println(F("LMIC resetleniyor...")); // NOKTA 6
  LMIC_reset(); // LMIC kütüphanesini sıfırla
  Serial.println(F("LMIC resetlendi."));     // NOKTA 7

  // LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100); // BU SATIR KALDIRILDI!
  
  // Veri hızı (SF7) ve güç (14 dBm) ayarları
  LMIC_setDrTxpow(DR_SF7, 14);

  // LoRaWAN ağa katılım sürecini başlat
  Serial.println(F("LoRaWAN başlatılıyor ve ağa katılım deneniyor...")); // NOKTA 8
  LMIC_startJoining();
}

void loop() {
  os_runloop_once(); // LMIC olay döngüsünü çalıştır
 
}