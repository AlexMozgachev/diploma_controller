#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>

// Настройки сети
const char* ssid = "MGTS_GPON_8453";
const char* password = "7tkYQchY";

WebServer server(80);

// Состояния реле
bool relay1State = false;
bool relay2State = false;

// Пины управления
const int RELAY1_PIN = 5;
const int RELAY2_PIN = 18;

// Данные с датчика ATC
float currentTemp = 0;
float currentHum = 0;
int currentBatt = 0;
unsigned long lastSensorTime = 0;

NimBLEScan* pBLEScan;
const int scanTime = 3;

// ================== BLE ОБРАБОТЧИК (исправлено для 1.4.3) ==================
class MyCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* device) override {
    String name = device->getName().c_str();
    
    // Ищем датчики ATC
    if (name.indexOf("ATC_") == 0 && device->haveServiceData()) {
      std::string serviceData = device->getServiceData();
      uint8_t* data = (uint8_t*)serviceData.data();
      size_t len = serviceData.length();
      
      if (len >= 15) {
        // Температура: байты 6-7 (little-endian) / 100
        uint16_t tempRaw = (data[7] << 8) | data[6];
        currentTemp = tempRaw / 100.0;
        
        // Влажность: байты 8-9 (little-endian) / 100
        uint16_t humRaw = (data[9] << 8) | data[8];
        currentHum = humRaw / 100.0;
        
        // Батарея: байт 12
        currentBatt = data[12];
        lastSensorTime = millis();
        
        Serial.printf("📡 ATC: T=%.1f°C H=%.1f%% B=%d%%\n", 
                      currentTemp, currentHum, currentBatt);
      }
    }
  }
};

// ================== НАСТРОЙКА ВЕБ-СЕРВЕРА ==================
void setupWebServer() {
  // Главная страница
  server.on("/", HTTP_GET, []() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
      server.send(200, "text/html", "<h1>ESP32 работает!</h1><p>Файл index.html не найден</p>");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });

  // Управление реле 1
  server.on("/relay1/toggle", HTTP_GET, []() {
    relay1State = !relay1State;
    digitalWrite(RELAY1_PIN, relay1State ? HIGH : LOW);
    server.send(200, "text/plain", relay1State ? "on" : "off");
  });

  // Управление реле 2
  server.on("/relay2/toggle", HTTP_GET, []() {
    relay2State = !relay2State;
    digitalWrite(RELAY2_PIN, relay2State ? HIGH : LOW);
    server.send(200, "text/plain", relay2State ? "on" : "off");
  });

  // Статус для AJAX
  server.on("/status", HTTP_GET, []() {
    String json = "{";
    json += "\"relay1\":\"" + String(relay1State ? "on" : "off") + "\",";
    json += "\"relay2\":\"" + String(relay2State ? "on" : "off") + "\",";
    
    if (millis() - lastSensorTime < 30000) {
      json += "\"temp\":" + String(currentTemp, 1) + ",";
      json += "\"hum\":" + String(currentHum, 0) + ",";
      json += "\"battery\":" + String(currentBatt);
    } else {
      json += "\"temp\":0,\"hum\":0,\"battery\":0";
    }
    
    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("✅ Веб-сервер запущен на http://" + WiFi.localIP().toString());
}

// ================== НАСТРОЙКА ==================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n==================================");
  Serial.println("ESP32 + Веб-интерфейс + ATC Sensor");
  Serial.println("==================================");
  
  // Настройка пинов реле
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  Serial.println("✅ Реле настроены");

  // Подключение к Wi-Fi
  Serial.printf("\n📡 Подключение к Wi-Fi: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi подключен!");
    Serial.printf("   IP адрес: %s\n", WiFi.localIP().toString().c_str());
    
    // Монтируем LittleFS
    if(!LittleFS.begin(true)) {
      Serial.println("❌ Ошибка монтирования LittleFS");
    } else {
      Serial.println("✅ LittleFS смонтирована");
    }
    
    // Запускаем веб-сервер
    setupWebServer();
    
    Serial.println("\n📱 Откройте браузер: http://" + WiFi.localIP().toString());
    
  } else {
    Serial.println("\n❌ Ошибка подключения к Wi-Fi");
  }

  // Инициализация BLE (исправлено для 1.4.3)
  NimBLEDevice::init("ESP32_Relay");
  pBLEScan = NimBLEDevice::getScan();
  
  // В версии 1.4.3 используется setAdvertisedDeviceCallbacks
  pBLEScan->setAdvertisedDeviceCallbacks(new MyCallbacks());
  
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setMaxResults(0);
  Serial.println("✅ BLE инициализирован");
  
  Serial.println("==================================\n");
}

// ================== ГЛАВНЫЙ ЦИКЛ ==================
void loop() {
  server.handleClient();
  
  // Сканируем BLE каждые 3 секунды
  static unsigned long lastScan = 0;
  if (millis() - lastScan > 3000) {
    if (!pBLEScan->isScanning()) {
      pBLEScan->start(scanTime, false);
    }
    lastScan = millis();
  }
  
  delay(10);
}