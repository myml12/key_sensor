#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// WPA2-Enterprise WiFi設定
#define EAP_IDENTITY "23D5103018L@chuo-u.ac.jp"
#define EAP_USERNAME "23D5103018L@chuo-u.ac.jp"
#define EAP_PASSWORD ""
const char* ssid = "eduroam";

// ピン定義
#define Y_PIN 1
#define LED_PIN 9

// 状態管理
const float THRESHOLD_VOLTAGE = 1.1;
const float ADC_RES = 3.3 / 4095.0;
int lastState = 3;
int preState = -1;
int currentState = -1;

void setup() {
  //Serial.begin(115200);
  delay(500);
  Serial.println("起動中...");

  analogReadResolution(12);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // WiFi 接続
  Serial.println("WiFi 接続開始...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  delay(1000);
  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 接続成功!");

  // Firebase 初期化
  config.api_key = "AIzaSyDc79fV8PdGC8YzKuvICHTzHn8MTYfL0xY";
  auth.user.email = "denken@gmail.com";
  auth.user.password = "denken1202";
  config.database_url = "https://hit-and-blow-analyze-default-rtdb.asia-southeast1.firebasedatabase.app/";
  Firebase.begin(&config, &auth);
  Serial.println("Firebase 初期化完了");
}

void loop() {
  // WiFi再接続処理
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 再接続中...");
    WiFi.disconnect(true);
    delay(1000);
    WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? "\n再接続成功" : "\n再接続失敗");
  }

  // 加速度センサ読み取り
  int raw = analogRead(Y_PIN);
  float voltage = raw * ADC_RES;
  //Serial.printf("ADC: %d -> %.3f V | 状態: %d\n", raw, voltage, lastState);

  if (voltage >= THRESHOLD_VOLTAGE && lastState < 5) lastState++;
  if (voltage < THRESHOLD_VOLTAGE && lastState > 0) lastState--;

  if (lastState == 5 || lastState == 0) {
    
    currentState = lastState;

    if (currentState != preState) {
      // 🔔 状態変化検出 → LED 1回点滅
      blinkLED(1);

      String msg = (currentState == 5)
        ? "🔓【新1号館1047号室】解錠されました! OPEN✨"
        : "🔒【新1号館1047号室】施錠されました! CLOSE🔐";
      bool locked = (currentState != 5);

      Serial.println("状態変化検出！");
      Serial.println(msg);

      // Firebase送信 + 成功時に2回点滅
     if (setLockStateWithTimestamp(locked)) {
        blinkLED(2);
      }

      preState = currentState;
    }
  }

  delay(400);
}

bool setLockStateWithTimestamp(int state) {
  FirebaseJson json;
  json.set("state", state);
  json.set("timestamp/.sv", "timestamp");

  const int maxRetries = 5;
  bool success = false;

  for(int i = 0; i < maxRetries; i++) {
    
    if (i == maxRetries - 1) ESP.restart();

    // WiFi再接続をチェック
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFiが切断されています。再接続中...");
      WiFi.disconnect(true);
      delay(1000);
      WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);
      unsigned long startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
      }
      Serial.println(WiFi.status() == WL_CONNECTED ? "\n再接続成功" : "\n再接続失敗");
    }

    if(Firebase.ready()){
      success = Firebase.RTDB.updateNode(&fbdo, "/room/1047", &json);
      if (success){
        return true;
      }
      else{
        delay(1000);
      }
    } else {
      Firebase.begin(&config, &auth);
      delay(1000);
    }
  }
  return false;
}

void blinkLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
}
