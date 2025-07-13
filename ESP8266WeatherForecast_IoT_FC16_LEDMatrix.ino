/*IoT Weather forecast text scroller for ESP8266 Node MCU V1.0 (ESP-12E) Flash size 4MB, CPU Freq 80MHz
Grabs forecast from api.weather.gov (free to use API without key, USA only)
Outputs all web data to serial for debugging or big chunk to output to other display type.
Grab forecast (refresh) again after 1 hour.
Hardware is 8 of FC16_HW LED matrix (SPI communication) split into 2 zones.
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <SPI.h>

// =========================== WiFi Config =============================
const char* ssid = "your wifi ssid 2.4";
const char* password = "your password";

// =========================== Weather API Config =============================
const char* host = "api.weather.gov";
const int httpsPort = 443;
WiFiClientSecure client;

// =========================== Matrix Config =============================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 8
#define CLK_PIN   D5
#define DATA_PIN  D7
#define CS_PIN    D8
int scrollSpeed = 20;


MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// =========================== Globals =============================
String forecastMessage = "Loading forecast...";
String wifiConnect = " Connected to WiFi!...";
unsigned long lastFetch = 0;
const unsigned long interval = 3600000; // 1 hour

char currentMsg[512] = {0};  // Display buffer
bool isFirstLoop = true;

// =========================== Setup =============================
void setup() {
  Serial.begin(115200);
  delay(100);

  P.begin();
  P.setIntensity(1);           // Range 1-15 (15 is max bright)
  P.setInvert(false); //true = wayyyyyy more power, but kinda neat look, harder to read
  P.setSpeed(scrollSpeed);
  P.setPause(1000);
  P.setTextAlignment(PA_LEFT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  // Show connection message
  strncpy(currentMsg, wifiConnect.c_str(), sizeof(currentMsg) - 1);
  P.displayText(currentMsg, PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.displayReset();

  //=========== Display hardware MAC address ====================
  String mac = WiFi.macAddress();
  char macChar[20];
  mac.toCharArray(macChar, sizeof(macChar));
  delay(3000);  // Let the user read the WiFi message first
  strncpy(currentMsg, macChar, sizeof(currentMsg) - 1);
  P.displayText(currentMsg, PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.displayReset();

  client.setInsecure(); // Skip certificate validation

  fetchForecast(); // Initial forecast
}

// =========================== Loop =============================
void loop() {
  unsigned long now = millis();

  // Display scrolling
  if (P.displayAnimate()) {
    P.displayReset();
  }

  // Fetch forecast every hour
  if (now - lastFetch >= interval || lastFetch == 0) {
    Serial.println("\nFetching forecast update...");
    fetchForecast();
    lastFetch = now;
  }
}

// =========================== Scroll Message =============================
void scrollText(char *p) {
  strncpy(currentMsg, p, sizeof(currentMsg) - 1);
  P.displayText(currentMsg, PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.displayReset();
}

// =========================== Format Forecast for Display =============================
void printFormattedForecast(JsonArray periods) {
  String msg = "";

  for (int i = 0; i < 3; i++) {
    JsonObject period = periods[i];

    String name = period["name"] | "N/A";
    int temp = period["temperature"] | 0;
    String unit = period["temperatureUnit"] | "F";
    String shortForecast = period["shortForecast"] | "";
    String detailedForecast = period["detailedForecast"] | "";
    String wind = period["windSpeed"] | "";
    String windDir = period["windDirection"] | "";
    int precip = period["probabilityOfPrecipitation"]["value"] | 0;

    //====Serial output formatting with emoticons==============================
    Serial.println("--------------------------------------------------");
    Serial.printf("📅 %s\n", name.c_str());
    Serial.printf("🌡️  Temp: %d°%s\n", temp, unit.c_str());
    Serial.printf("💧 Precip Chance: %d%%\n", precip);
    Serial.printf("🌬️  Wind: %s %s\n", windDir.c_str(), wind.c_str());
    Serial.printf("⛅ Short Forecast: %s\n", shortForecast.c_str());
    Serial.printf("📝 Details: %s\n", detailedForecast.c_str());
    Serial.println("--------------------------------------------------\n");

    msg += name + ": " + temp + unit + ", " + shortForecast;
    if (precip > 0) msg += ", Precip " + String(precip) + "%";
    if (wind != "") msg += ", Wind " + windDir + " " + wind;
    msg += " | ";

    // Add scrolling break between periods:
    if (i < 2) msg += "    ***    ";  // Separator with spaces for scrolling pause effect
  }

  msg.trim();
  forecastMessage = msg;
  scrollText((char*)forecastMessage.c_str());
}

// =========================== Fetch Weather Forecast =============================
void fetchForecast() {
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed (step 1)");
    return;
  }

  String url = "/points/40.4406,-79.9959"; // Pittsburgh PA
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, client);
  if (err) {
    Serial.print("Failed to parse step 1 JSON: ");
    Serial.println(err.c_str());
    client.stop();
    return;
  }

  const char* forecastUrl = doc["properties"]["forecast"];
  if (!forecastUrl) {
    Serial.println("Forecast URL not found.");
    client.stop();
    return;
  }

  Serial.print("Forecast URL: ");
  Serial.println(forecastUrl);
  client.stop();
  delay(500);

  // Step 2: Get forecast data
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed (step 2)");
    return;
  }

  String path = String(forecastUrl);
  path.replace("https://api.weather.gov", "");
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  StaticJsonDocument<768> filter;
  for (int i = 0; i < 3; i++) {
    filter["properties"]["periods"][i]["name"] = true;
    filter["properties"]["periods"][i]["temperature"] = true;
    filter["properties"]["periods"][i]["temperatureUnit"] = true;
    filter["properties"]["periods"][i]["shortForecast"] = true;
    filter["properties"]["periods"][i]["detailedForecast"] = true;
    filter["properties"]["periods"][i]["windSpeed"] = true;
    filter["properties"]["periods"][i]["windDirection"] = true;
    filter["properties"]["periods"][i]["probabilityOfPrecipitation"]["value"] = true;
  }

  DynamicJsonDocument forecastDoc(2048);
  err = deserializeJson(forecastDoc, client, DeserializationOption::Filter(filter));
  client.stop();

  if (err) {
    Serial.print("Forecast JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  JsonArray periods = forecastDoc["properties"]["periods"];
  if (periods.isNull() || periods.size() < 3) {
    Serial.println("Forecast data incomplete.");
    return;
  }

  printFormattedForecast(periods);
}
