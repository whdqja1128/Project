#define DEBUG
#define DEBUG_WIFI
#define AP_SSID "embA"
#define AP_PASS "embA1234"
#define SERVER_NAME "10.10.141.73"
#define SERVER_PORT 5000
#define LOGID "USR_ARD"

#define BUTTON_PIN 12
#define BED_PIN 11
#define BATH_PIN 10
#define LIVING_PIN 9
#define WATER_PIN 8
#define LED_PIN 2

#define CMD_SIZE 50
#define ARR_CNT 5

#include <TimerOne.h>
#include <avr/pgmspace.h>

char sendBuf[CMD_SIZE];
bool timerIsrFlag = false;
bool buttonPrev = LOW;
bool buttonBPrev = LOW;
bool buttonLPrev = LOW;
bool buttonWPrev = LOW;
bool buttonEPrev = LOW;

unsigned int secCount;
int sensorTime;

// ✅ MEGA는 하드웨어 시리얼 사용!

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BATH_PIN, INPUT);
  pinMode(LIVING_PIN, INPUT);
  pinMode(WATER_PIN, INPUT);
  pinMode(BED_PIN, INPUT);

  Serial.begin(115200);  // 시리얼 모니터용
  Serial1.begin(38400);  // ESP8266과 연결 (TX1=18, RX1=19)

  Timer1.initialize(1000000);
  Timer1.attachInterrupt(timerIsr);  // 매 1초마다 인터럽트 발생
  

  buttonPrev = digitalRead(BUTTON_PIN);
  buttonBPrev = digitalRead(BATH_PIN);
  buttonLPrev = digitalRead(LIVING_PIN);
  buttonWPrev = digitalRead(WATER_PIN);
  buttonEPrev = digitalRead(BED_PIN);

}

void loop() {
  static bool wasConnected = false;

  if (timerIsrFlag) {
    timerIsrFlag = false;
  }
  socketEvent();
}

void socketEvent() {
  bool buttonNow = digitalRead(BUTTON_PIN);
  bool buttonBNow = digitalRead(BATH_PIN);
  bool buttonLNow = digitalRead(LIVING_PIN);
  bool buttonWNow = digitalRead(WATER_PIN);
  bool buttonENow = digitalRead(BED_PIN);

  // HIGH로 바뀌는 순간 → 버튼 눌림
  if (buttonPrev == LOW && buttonNow == HIGH) {
    digitalWrite(LED_PIN, HIGH);  // LED 켜기
    sprintf(sendBuf, "[USR_STM32]SOUND@OFF\n");
    Serial1.print(sendBuf);
    //client.write(sendBuf, strlen(sendBuf));

#ifdef DEBUG
    Serial.print(", send : ");
    Serial.print(sendBuf);
#endif
  }

  if (buttonBPrev == LOW && buttonBNow == HIGH) {
    digitalWrite(LED_PIN, HIGH);  // LED 켜기
    sprintf(sendBuf, "[USR_LIN]BATHGO\n");
    Serial1.print(sendBuf);
    //client.write(sendBuf, strlen(sendBuf));

#ifdef DEBUG
    Serial.print(", send : ");
    Serial.print(sendBuf);
#endif
  }

  if (buttonLPrev == LOW && buttonLNow == HIGH) {
    digitalWrite(LED_PIN, HIGH);  // LED 켜기
    sprintf(sendBuf, "[USR_LIN]LIVINGGO\n");
    Serial1.print(sendBuf);
    //client.write(sendBuf, strlen(sendBuf));

#ifdef DEBUG
    Serial.print(", send : ");
    Serial.print(sendBuf);
#endif
  }

  if (buttonWPrev == LOW && buttonWNow == HIGH) {
    digitalWrite(LED_PIN, HIGH);  // LED 켜기
    sprintf(sendBuf, "[USR_LIN]WATERGO\n");
    Serial1.print(sendBuf);
    // client.write(sendBuf, strlen(sendBuf));

#ifdef DEBUG
    Serial.print(", send : ");
    Serial.print(sendBuf);
#endif
  }

  if (buttonEPrev == LOW && buttonENow == HIGH) {
    digitalWrite(LED_PIN, HIGH);  // LED 켜기
    sprintf(sendBuf, "[USR_LIN]BEDGO\n");
    Serial1.print(sendBuf);
    // client.write(sendBuf, strlen(sendBuf));

#ifdef DEBUG
    Serial.print(", send : ");
    Serial.print(sendBuf);
#endif
  }

  // 버튼에서 손 뗐을 때 (HIGH → LOW)
  if (buttonPrev == HIGH && buttonNow == LOW) {
    digitalWrite(LED_PIN, LOW);  // LED 끄기
  }
  if (buttonBPrev == HIGH && buttonBNow == LOW) {
    digitalWrite(LED_PIN, LOW);  // LED 끄기
  }
  if (buttonLPrev == HIGH && buttonLNow == LOW) {
    digitalWrite(LED_PIN, LOW);  // LED 끄기
  }
  if (buttonWPrev == HIGH && buttonWNow == LOW) {
    digitalWrite(LED_PIN, LOW);  // LED 끄기
  }
  if (buttonEPrev == HIGH && buttonENow == LOW) {
    digitalWrite(LED_PIN, LOW);  // LED 끄기
  }

  buttonPrev  = buttonNow;
  buttonBPrev = buttonBNow;
  buttonLPrev = buttonLNow;
  buttonWPrev = buttonWNow;
  buttonEPrev = buttonENow;


}




void timerIsr() {
  timerIsrFlag = true;
  secCount++;
}

// void wifi_Setup() {
//   // ✅ MEGA에선 SoftwareSerial 안 쓰고 Serial1 사용!
//   WiFi.init(&Serial1);
//   server_Connect();

//   // 연결 성공 여부 출력
//   while (WiFi.status() != WL_CONNECTED) {
// #ifdef DEBUG_WIFI
//     Serial.print("Attempting to connect to WPA SSID: ");
//     Serial.println(AP_SSID);
// #endif
//     WiFi.begin(AP_SSID, AP_PASS);
//     delay(1000);
//   }

// #ifdef DEBUG_WIFI
//   Serial.println("You're connected to the network");
//   printWifiStatus();
// #endif
// }

// int server_Connect() {
// #ifdef DEBUG_WIFI
//   Serial.println("Starting connection to server...");
// #endif

//   if (client.connect(SERVER_NAME, SERVER_PORT)) {
// #ifdef DEBUG_WIFI
//     Serial.println("Connected to server");
// #endif
//     client.print(LOGID);
//   } else {
// #ifdef DEBUG_WIFI
//     Serial.println("server connection failure");
// #endif
//   }
// }

// void printWifiStatus() {
//   Serial.print("SSID: ");
//   Serial.println(WiFi.SSID());

//   IPAddress ip = WiFi.localIP();
//   Serial.print("IP Address: ");
//   Serial.println(ip);

//   long rssi = WiFi.RSSI();
//   Serial.print("Signal strength (RSSI):");
//   Serial.print(rssi);
//   Serial.println(" dBm");
// }