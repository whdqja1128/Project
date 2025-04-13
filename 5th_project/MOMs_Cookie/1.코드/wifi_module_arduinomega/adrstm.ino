#include <WiFi.h>

#define WIFI_SSID "embA"
#define WIFI_PASS "embA1234"
#define SERVER_IP "10.10.141.73"
#define SERVER_PORT 5000

#define SERIAL_BAUD 38400  // Mega 쪽 Serial1과 

// Mega와 연결될 UART 핀 (Serial2)
#define RXD2 16  // Mega TX1과 연결
#define TXD2 17  // Mega RX1과 연결

WiFiClient client;

void setup() {
  Serial.begin(115200);        // 디버깅용 PC 시리얼
  pinMode(16, INPUT);
  pinMode(17, OUTPUT);
  Serial2.begin(SERIAL_BAUD, SERIAL_8N1, RXD2, TXD2);  // Mega 연결용 UART

  Serial.println("ESP32 Wi-Fi 모듈 시작 중...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Wi-Fi 연결됨!");

  if (client.connect(SERVER_IP, SERVER_PORT)) {
    Serial.println("✅ 서버 연결 성공!");
    client.println("USR_BT");  // 로그인 메시지
  } else {
    Serial.println("❌ 서버 연결 실패");
  }
}

void loop() {
  // Mega2560 → 서버
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();

    if (msg.length() > 0 && client.connected()) {
      client.print(msg);
      Serial.println("📤 서버로 보냄: " + msg);
    }
  }

  // 서버 → STM32
  if (client.available()) {
    String resp = client.readStringUntil('\n');
    resp.trim();

    if (resp.length() > 0) {
      String formatted_resp = resp;
      
      Serial2.println(resp);
      Serial.println("📥 STM32로 응답: " + resp);
    }
  }

  // 연결 유지 확인 (서버 끊겼을 때 자동 재연결)
  if (!client.connected()) {
    Serial.println("❗ 서버와 연결 끊김. 재연결 시도...");
    client.stop();
    delay(1000);

    if (client.connect(SERVER_IP, SERVER_PORT)) {
      Serial.println("✅ 서버 재연결 성공!");
      client.println("USR_BT");
    } else {
      Serial.println("❌ 서버 재연결 실패");
    }
  }
}
