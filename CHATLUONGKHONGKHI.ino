#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <math.h>

#define BLYNK_TEMPLATE_ID   "TMPL6hcEroKe5"
#define BLYNK_TEMPLATE_NAME "lần này là btl"
#define BLYNK_AUTH_TOKEN    "ly_jqTujmhvhl86I_ZHQagzLaz8sG4He"

char ssid[] = "BKFET 2.4G";
char pass[] = "12345678";

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ7_PIN 34
#define LDR_PIN 33
const float VCC = 3.3, RL = 10.0, R0 = 10.0, m = -1.69, b_c = -0.77;

LiquidCrystal_I2C lcd(0x27, 20, 4);
HardwareSerial pmsSerial(2);
int pm25 = -1;

bool blynkEnabled = false;
volatile bool doSensorRead = false;

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  doSensorRead = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
  Serial.begin(115200);
  lcd.init(); lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Start measuring");

  lcd.setCursor(0, 1); lcd.print("WiFi...");
  WiFi.begin(ssid, pass);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    Serial.print("."); lcd.print("."); delay(500);
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK"); lcd.print("WiFi OK");
    blynkEnabled = true;
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    lcd.setCursor(0, 1); lcd.print("Blynk OK");
  } else {
    Serial.println("\nWiFi FAIL"); lcd.print("WiFi FAIL");
    blynkEnabled = false;
  }

  delay(1000);
  lcd.clear();

  dht.begin();
  pmsSerial.begin(9600, SERIAL_8N1, 16, 17);

  // Cấu hình timer mỗi 2 giây
  timer = timerBegin(0, 80, true);  // 80 MHz / 80 = 1 MHz
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 2000000, true);  // 2.000.000 µs = 2s
  timerAlarmEnable(timer);
}

void loop() {
  if (blynkEnabled && WiFi.status() == WL_CONNECTED) Blynk.run();

  if (doSensorRead) {
    portENTER_CRITICAL(&timerMux);
    doSensorRead = false;
    portEXIT_CRITICAL(&timerMux);

    float temperature = dht.readTemperature();
    float humidity    = dht.readHumidity();

    int mq7_adc = analogRead(MQ7_PIN);
    float mq7_v  = (mq7_adc / 4095.0) * VCC;
    float rs     = ((VCC - mq7_v) / mq7_v) * RL;
    float ratio  = rs / R0;
    float co_ppm = pow(10, ((log10(ratio) - b_c) / m));

    int ldr_adc = analogRead(LDR_PIN);
    float resistance = (4095.0 / ldr_adc) - 1.0;
    float light_lux = 500.0 / resistance;

    readPM25();

    bool canhBao = (co_ppm > 50 || pm25 > 75 || temperature > 35 || light_lux < 50);

    float co_val    = roundf(co_ppm * 100) / 100;
    float light_val = roundf(light_lux * 100) / 100;
    float pm25_val  = roundf(pm25 * 100) / 100;

    lcd.setCursor(0, 0); lcd.print("                    ");
    lcd.setCursor(0, 0);
    lcd.print("T:"); lcd.print(temperature, 1); lcd.print("C H:"); lcd.print(humidity, 1); lcd.print("%");

    lcd.setCursor(0, 1); lcd.print("                    ");
    lcd.setCursor(0, 1);
    lcd.print("CO:"); lcd.print(co_val, 2); lcd.print(" ppm ");
    lcd.print("L:");  lcd.print(light_val, 2); lcd.print(" lx");

    lcd.setCursor(0, 2); lcd.print("                    ");
    lcd.setCursor(0, 2);
    lcd.print("PM2.5:"); lcd.print(pm25_val, 2); lcd.print(" ug/m3");

    lcd.setCursor(0, 3); lcd.print("                    ");
    lcd.setCursor(0, 3);
    if (canhBao) {
      lcd.print("Canh bao: ");
      if (co_ppm > 50)      lcd.print("CO ");
      if (pm25   > 75)      lcd.print("PM ");
      if (temperature> 35)  lcd.print("Temp ");
      if (light_lux < 50)   lcd.print("Light");
    } else {
      lcd.print("Khong khi sach");
    }

    if (blynkEnabled && WiFi.status() == WL_CONNECTED) {
      Blynk.virtualWrite(V0, temperature);
      Blynk.virtualWrite(V1, humidity);
      Blynk.virtualWrite(V2, pm25_val);
      Blynk.virtualWrite(V3, light_val);
      Blynk.virtualWrite(V4, canhBao);
      Blynk.virtualWrite(V5, co_val);
    }

    Serial.print("T:"); Serial.print(temperature, 1); Serial.print("C H:"); Serial.print(humidity, 1); Serial.print("% ");
    Serial.print("CO:"); Serial.print(co_val, 2); Serial.print(" ppm L:"); Serial.print(light_val, 2); Serial.print(" lx PM2.5:"); Serial.print(pm25_val, 2); Serial.print(" ug/m3");
    if (canhBao) Serial.print(" Warn!");
    Serial.println();
  }

}

bool readPM25() {
  while (pmsSerial.available() >= 32) {
    if (pmsSerial.read() == 0x42 && pmsSerial.read() == 0x4D) {
      uint8_t buf[30];
      for (int i = 0; i < 30; i++) buf[i] = pmsSerial.read();
      pm25 = (buf[12] << 8) | buf[13];
      return true;
    }
  }
  return false;
}
