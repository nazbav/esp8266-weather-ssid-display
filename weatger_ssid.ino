#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Настройки Wi-Fi и API
const char* wifiSsid = "...";
const char* wifiPassword = "...";
const char* apiUrl = ".../weather.php"; //(для экономия памяти ESP используется свой сайт, который подготавливает данные)

// Интервал обновления данных (10 минут)
const unsigned long dataUpdateInterval = 600000; // 10 минут
const unsigned long ssidAnimationInterval = 30000; // 15 секунд

unsigned long lastDataFetchTime = 0;
unsigned long lastSSIDUpdateTime = 0;

// NTP Client
WiFiUDP udp;
NTPClient ntpClient(udp, "pool.ntp.org", 10800);  // Время UTC +3 (Москва)

// Переменные для хранения данных
String currentWeatherSSID;
String morningWeatherSSID;
String dayWeatherSSID;
String eveningWeatherSSID;
String nightWeatherSSID;

void setup() {
  Serial.begin(115200);
  setupWiFi();
  ntpClient.begin();
  ntpClient.update();
  fetchWeatherData();
  updateSSIDAnimation();
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastDataFetchTime >= dataUpdateInterval) {
    fetchWeatherData();
    lastDataFetchTime = currentTime;
  }

  if (currentTime - lastSSIDUpdateTime >= ssidAnimationInterval) {
    updateSSIDAnimation();
    lastSSIDUpdateTime = currentTime;
  }
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WeatherStation", wifiPassword);
  Serial.println("Access Point created");

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);

  Serial.println("Connecting to WiFi...");
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(1000);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  HTTPClient http;
  WiFiClient client;
  http.begin(client, apiUrl);

  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Received payload:");
    Serial.println(payload);

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      http.end();
      return;
    }

    String timeString = ntpClient.getFormattedTime().substring(0, 5); // Только часы и минуты
    int currentHour = ntpClient.getHours();

    int temperature = doc["currentWeather"]["temperature"].as<int>();
    int humidity = doc["currentWeather"]["humidity"].as<int>();
    int windspeed = doc["currentWeather"]["windspeed"].as<int>();
    int weathercode = doc["currentWeather"]["weathercode"].as<int>();

    if (humidity < 0 || humidity > 100) humidity = 0;

    currentWeatherSSID = timeString + " " + getWeatherEmoji(weathercode) + "🌡" + String(temperature) + " " + "💧" + String(humidity) + " " + "🌪️" + String(windspeed);
    currentWeatherSSID = truncateSSID(currentWeatherSSID);

    // Определение данных для утра, дня, вечера и ночи, а также добавление смайликов для следующего дня
    morningWeatherSSID = getWeatherSSID(doc, "morning", currentHour, 6, 12);
    dayWeatherSSID = getWeatherSSID(doc, "day", currentHour, 12, 18);
    eveningWeatherSSID = getWeatherSSID(doc, "evening", currentHour, 18, 24);
    nightWeatherSSID = getWeatherSSID(doc, "night", currentHour, 0, 6);

    Serial.println("SSID data updated");
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpCode);
  }

  http.end();
}

String getWeatherSSID(DynamicJsonDocument &doc, const char* period, int currentHour, int startHour, int endHour) {
  String ssid;
  bool isNextDay = (currentHour >= endHour);
  String nextDayIndicator = isNextDay ? " 🕒" : ""; // Добавляем смайлик, если это данные на следующий день
  String emoji = getWeatherEmoji(doc[period]["weathercode"].as<int>());
  if (period == "night" && emoji == "☀") {
    emoji = "🌕"; // Луна для ясной ночи
  }
  ssid = (isNextDay ? "🕒 " : "") + getPeriodEmoji(startHour) + emoji + "🌡" +
         doc[period]["temp_min"].as<int>() + "-" + doc[period]["temp_max"].as<int>() + " " + "🌪️" + doc[period]["windspeed"].as<int>();
  return truncateSSID(ssid);
}

String truncateSSID(const String& ssid) {
  return ssid.length() > 32 ? ssid.substring(0, 32) : ssid;
}

String getWeatherEmoji(int code) {
  switch (code) {
    case 0: return "☀";   // Ясно
    case 1: return "🌤";   // Частичная облачность
    case 2: return "⛅";   // Переменная облачность
    case 3: return "☁";   // Облачно
    case 45:
    case 48: return "🌫";   // Туман, иней
    case 51:
    case 53:
    case 55: return "🌦";   // Легкий дождь
    case 56:
    case 57: return "🌧";   // Морось, ледяной дождь
    case 61:
    case 63: return "🌦";   // Проливной дождь, временами
    case 65: return "🌧";   // Проливной дождь
    case 66:
    case 67: return "🌨";   // Ледяной дождь, снег
    case 71:
    case 73: return "❄";   // Легкий снег
    case 75: return "🌨";   // Сильный снег
    case 77: return "🌬";   // Метель
    case 80: return "🌧";   // Временами дождь
    case 81: return "🌦";   // Дождь с прояснениями
    case 82: return "⛈";   // Гроза
    case 85: return "🌨";   // Легкий снегопад
    case 86: return "❄";   // Сильный снегопад
    default: return "❓";   // Неизвестное условие
  }
}

String getPeriodEmoji(int hour) {
  if (hour >= 6 && hour < 12) {
    return "🌅 ";
  } else if (hour >= 12 && hour < 18) {
    return "🌞 ";
  } else if (hour >= 18 && hour < 24) {
    return "🌇 ";
  } else {
    return "🌙 "; // Ночь
  }
}

void updateSSIDAnimation() {
  static int currentSSIDIndex = 0;
  String ssidToShow;

  switch (currentSSIDIndex) {
    case 0: ssidToShow = currentWeatherSSID; break;
    case 1: ssidToShow = morningWeatherSSID; break;
    case 2: ssidToShow = dayWeatherSSID; break;
    case 3: ssidToShow = eveningWeatherSSID; break;
    case 4: ssidToShow = nightWeatherSSID; break;
  }

  currentSSIDIndex = (currentSSIDIndex + 1) % 5;

  Serial.println("Setting SSID to:");
  Serial.println(ssidToShow);

  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(ssidToShow.c_str(), wifiPassword)) {
    Serial.println("SSID updated successfully");
  } else {
    Serial.println("Failed to update SSID");
  }
}
