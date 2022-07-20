#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>

#include <ArduinoJson.h>        // json라이브러리
#include <ESP8266HTTPClient.h>  //http 라이브러리

#define STASSID "teraenergy 2.4"
#define STAPSK  "owen910725"
#define URL "http://192.168.0.164:10032/keyCheck"

SoftwareSerial BTSerial(D3, D2); // Arduino(RX, TX) -> module(TX, RX)

int requestCount = 0;             // requestCount : 1 초
int requestMaxCount= 30 * 60;     // 서버에서 데이터 전송 요청 시각 - 30 분/개수 or 1800 초/개수

int commute_key = 0;

String ssid = "";
String password = "";

String UUID0 = "";
String UUID1 = "";
String UUID2 = "";
String UUID3 = "";
String newLine = "\r\n";

/* BLUETOOTH - BLE Serial */

// 비콘 UUID 설정
void setBeaconUUID(String data){
  int totalLen = data.length();
  UUID0 = data.substring(0, 8);
  UUID1 = data.substring(8, 16);
  UUID2 = data.substring(16, 24);
  UUID3 = data.substring(24, 32);

  Serial.print("uuid=");
  Serial.print(UUID0);
  Serial.print("-");
  Serial.print(UUID1);
  Serial.print("-");
  Serial.print(UUID2);
  Serial.print("-");
  Serial.print(UUID3);
  Serial.println("");

  setUuid0();
}

void setUuid0(){
  sendATCommand("AT+IBE0" + UUID0);
}

void setUuid1(){
  sendATCommand("AT+IBE1" + UUID1);
}

void setUuid2(){
  sendATCommand("AT+IBE2" + UUID2);
}

void setUuid3(){
  sendATCommand("AT+IBE3" + UUID3);
}

// 비콘 활성화 설정
void setBeaconActive() {
  sendATCommand("AT+IBEA1");
}

// 비콘 브로드 케스트 모드 전환
void setBeaconBroadcast() {
  sendATCommand("AT+DELO2");
}

// 비콘 AT Command 응답 체크
void checkResponse(){
  sendATCommand("AT");
}

// 블루투스 설정에 대한 결과 동작
void resultBleSet(String data) {
  data.trim();
  Serial.println("result data = " + data);
  
  if ( data.equals("OK+Set:0x" + UUID0) ) {
      setUuid1();
  } else if ( data.equals("OK+Set:0x" + UUID1) ) {
      setUuid2();
  } else if ( data.equals("OK+Set:0x" + UUID2) ) {
      setUuid3();
  } else if ( data.equals("OK+Set:0x" + UUID3) ) {
     Serial.println(" uuid complete .... ");
  } else {
     Serial.println( data + " complete ....  ");
  }

  delay(1000);
}

// 인증 키 전송
void setKey() {
  if (commute_key == 0) return;
  
  char buf[6];
  sprintf(buf, "0x%04X", commute_key);
  String data = String(buf);
  sendATCommand("AT+MINO" + data);
}

// 전송 AT Command
void sendATCommand(String command){
  BTSerial.print(command + newLine);
  Serial.println(command);
  Serial.println("");
}

/* WIFI */

// wifi 설정
void setWifi(String data) {
  int totalLen = data.length();
  if ( totalLen > 0 ) {
    int nGetIndex = 0;
    
    nGetIndex = data.indexOf('-');
    if ( nGetIndex != -1 ) {
      ssid = data.substring(0, nGetIndex);
      password = data.substring(nGetIndex+1, totalLen);
      
    } else {
      Serial.println(" error : Unable to set wifi.");
    }  
  } else {
    ssid = STASSID;
    password = STAPSK;
  }

  Serial.print("ssid = ");
  Serial.print(ssid);
  Serial.print(":");
  Serial.print("password = ");
  Serial.print(password);
  Serial.println("");

  _setWifi();
}

// wifi 설정
void _setWifi(){

  int reConnect = 0;
  if ((WiFi.status() == WL_CONNECTED)) {
    WiFi.disconnect();
    reConnect = 1;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    count ++;

    if ( count == 20 ) break;
  }

  Serial.println("");
  Serial.println("connecting ... ");

  if (reConnect == 1) {
    delay(8000);
  }
  
  if ((WiFi.status() == WL_CONNECTED)) {
    Serial.println("connected Wifi .... ");
    Serial.print("Arduino IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("disconnected Wifi .... ");
  }
  Serial.println("");
}

// 출근 인증 key 전송
void requestKey() {
  int key = 0;
  if ((WiFi.status() == WL_CONNECTED)) {
    
    WiFiClient client;
    HTTPClient http;
    
    http.begin(client,URL); // Specify the URL
    int httpCode = http.GET();
    
    if (httpCode == 200) { 
      String payload = http.getString();
      DynamicJsonDocument jsonDoc(1024);
      
      auto error = deserializeJson(jsonDoc, payload);
      
      if (error) {
          Serial.println(F("deserializeJson() failed with code "));
          Serial.println(error.c_str());
      } else {
        // Get body
        key = jsonDoc["commute_key"];
    
        Serial.println(payload);
        Serial.println(httpCode);        
      }
    }
  
    http.end(); //Free the resources
    
  } else {
    Serial.println("Error on HTTP request");
  }
  
  commute_key = key;
  Serial.print("Minor Number (commute_key) : "); 
  Serial.println(commute_key);
  Serial.println("");

  setKey();
}

/* Command */

// 도움말 출력
void printHelp() {
  Serial.println("");
  Serial.println("[Command]");
  Serial.println("  AT : Check for beacon response. ( ex> AT )");
  Serial.println("  US [UUID] : UUID Set ( ex> US12345678901234567890123456789012 )");
  Serial.println("   * UUID should be capitalized. *");
  Serial.println("   * The total number of UUIDs is 32. *");
  Serial.println("   * The value of the UUID consists of hexadecimal digits. *");
  Serial.println("  BA : Beacon Active Set ( ex> BA )");
  Serial.println("  BB : Beacon Broadcast Active Set ( ex> BB )");
  Serial.println("  WS [SSID-PASSWORD] : WIFI SSID & PASSWORD Set ( ex> WSteraenergy-qwer1234 )");
  Serial.println("  WR : WIFI Restart ( ex> WR )");
  Serial.println("  KS : Key Set ( ex> KS )");
  Serial.println("");
}

// 명령어 추출
String extractCmd(String lowData) {
  String cmd= "";
  cmd += lowData[0];
  cmd+= lowData[1];
  return cmd;
}

// 데이터 추출
String extractData(String lowData) {
  int totalLen = lowData.length();
  String data= lowData.substring(2, totalLen);
  return data;
}

// 명령어 실행
void excute(String cmd, String data) {
  if(cmd.equals("US")){
    setBeaconUUID(data);
  } else if(cmd.equals("BA")){
    setBeaconActive();
  } else if(cmd.equals("BB")){
    setBeaconBroadcast();
  } else if(cmd.equals("AT")){
    checkResponse();
  } else if (cmd.equals("WS")){
    setWifi(data);
  } else if (cmd.equals("WR")){
    _setWifi();
  } else if (cmd.equals("KS")){
    requestKey();
  }
}

void setup() {
  Serial.begin(9600);            // 통신속도 9600으로 설정 ( 모니터링 하기 위한 Serial )
  BTSerial.begin(9600);          // 통신속도 9600으로 설정 ( BLE 에 AT Command 를 위한 Serial )
  setWifi("");
  requestKey();
}

void loop() {

  if (BTSerial.available()) {
    String data = BTSerial.readString();
    resultBleSet(data);
  } 
  
  if(Serial.available()){
    String lowData = Serial.readString();    
    lowData.trim(); // 처음 과 끝의 공백 제거
    Serial.println(lowData);
    
    if ( lowData.equals("help") ) {
      printHelp();
    } else {
      String cmd = extractCmd(lowData);
      String data = extractData(lowData);
      excute(cmd, data);
    }
    
  } else {

    if ( commute_key == 0 ) {
      if ( requestCount == 5) {
        requestKey();
        requestCount = 0;
      }
      requestCount ++;
    } else if ( requestCount == requestMaxCount ) {
      requestKey();
      requestCount = 0;
    } else {
      requestCount ++;
    }

    if ( WiFi.status() != WL_CONNECTED ) {
      Serial.println("disconnected Wifi .... ");
    }
    
    delay(1000);
  }
  
}
