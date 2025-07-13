/*IoT Weather forcast and News headline text scroller for ESP8266 Node MCU V1.0 (ESP-12E) Flash size 4MB, CPU Freq 80MHz
Grabs forcast from api.weather.gov (free to use API without key, USA only)
Grabs news from newsapi.org (requires API key). Free API use has 100 request/day limit
Outputs all web data to serial for debugging or big chunk to output to other display type.

Hardware is 16x of FC16_HW LED matrix (SPI communication) split into 2 zones.
ESP32 also works (better) but I am trying to use some old hardware/parts
Requires external power (2-3A @5v) for the MAX72xx LED Matrix
released: June 23 2025, Ryan Bates
Compatible hardware is ESP8266 ScoreBoard PCB
*/

// WiFi credentials
const char* ssid = "your wifi ssid 2.4";
const char* password = "your password";
// API credentials
const char* apiKey = "your api key";  // Replace with your NewsAPI key

#include <ESP8266WiFi.h> //change this library if using ESP32
#include <ESP8266HTTPClient.h> //change this library if using ESP32
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
//#include <MD_MAX72xx.h> //not using this for LED matrix display
#include <SPI.h>

// ========== Display Config ==========
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //this may change for your hardware
#define MAX_DEVICES 16
#define NUM_ZONES   2
#define ZONE_SIZE (MAX_DEVICES / NUM_ZONES)
//SPI hardware pins
#define CLK_PIN   D5
#define DATA_PIN  D7
#define CS_PIN    D8

#define SPEED_TIME  25
#define PAUSE_TIME  1000

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WiFiClientSecure client;

// ========== News Globals ==========
unsigned long lastNewsFetch = 0;
const unsigned long NEWS_INTERVAL = 3600000; //refresh news every 1 hour
String headlines[10] = {
  "Loading headlines...", "", "", "", "", "", "", "", "", ""
};
uint8_t curHeadline = 0; //what does this do?

// ========== Weather Globals ========== //not used, but could be
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_INTERVAL = 3600000; //refresh weather every 1 hour
String forecastMessage = "Loading weather forecast...";
uint8_t curForecastZone = 1;

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Matrix setup
  P.begin(NUM_ZONES);
  for (uint8_t i = 0; i < NUM_ZONES; i++) {
    P.setZone(i, ZONE_SIZE * i, ZONE_SIZE * (i + 1) - 1);
  }
  P.setIntensity(1); // 1=low brightness, 16=max brightness. uses lots of power
  P.setInvert(false); //true = wayyyyyy more power, but kinda neat look, harder to read

//===========pushing the hardware MAC address to the screen for posterity==================== can delete this section if you want
  String macStr = WiFi.macAddress(); //get MAC as a string
  static char macChar[20]; //store mac as array and make it static (retain value between function calls, stays forver after void setup)
  macStr.toCharArray(macChar, sizeof(macChar));
  P.displayZoneText(0, "Connecting to WiFi...", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.displayZoneText(1, macChar, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.displayReset();

  unsigned long start = millis(); //special non locking coop to run the matrix during setup and avoid a crash on boot
  while (!P.getZoneStatus(0) || !P.getZoneStatus(1)) {
    P.displayAnimate();
    delay(10);
    yield();  // 🧠 Prevent watchdog reset. special to the ESP8266; i dont understand this hardware
  }



  //  WiFi connect===================================================================================
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("MAC: "); 
  Serial.println(WiFi.macAddress()); //delete for enteprise security, helpful for network troubleshooting
  delay(1000);
  Serial.println("\n✅ WiFi connected!");
  P.displayZoneText(0, "Wifi connected!", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.displayZoneText(1, "Fetching News!", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  client.setInsecure();  // Skip cert validation
  
  fetchHeadlines(); // newsapi
  fetchForecast();  //weather.gov
  printStatusToSerial(); // <-- Optional for startup
}

// ========== Loop ==========
void loop() {
  checkWiFiConnection(); //re-establish wifi connection if dropped

  unsigned long now = millis();
 //checks to refresh news and weather
  if (now - lastNewsFetch >= NEWS_INTERVAL || now < lastNewsFetch) {
    fetchHeadlines();
    lastNewsFetch = now;
  }

  if (now - lastWeatherFetch >= WEATHER_INTERVAL || now < lastWeatherFetch) {
    fetchForecast();
    lastWeatherFetch = now;
  }

  // ===================Display News on Zone 0===============================================================
  static bool displayingNews = false;
  if (!displayingNews && P.getZoneStatus(0)) {
    P.displayZoneText(0, headlines[curHeadline].c_str(), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    displayingNews = true;
  }

  //=================== Display Forecast on Zone 1================================================================
  static bool displayingWeather = false;
  if (!displayingWeather && P.getZoneStatus(curForecastZone)) {
    P.displayZoneText(curForecastZone, forecastMessage.c_str(), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    displayingWeather = true;
  }

  if (P.displayAnimate()) {
    curHeadline = (curHeadline + 1) % 10;
    displayingNews = false;
    displayingWeather = false;
  }
}

// ========== Sanitize Headlines ==========
//need to remove some special/fancy characters that the parola library cannot display
String sanitizeHeadline(String input) {
  input.replace("’", "'"); input.replace("‘", "'");
  input.replace("“", "\""); input.replace("”", "\"");
  input.replace("—", "-"); input.replace("–", "-");
  input.replace("…", "..."); input.replace("•", "-");
  input.replace("é", "e"); input.replace("á", "a");
  input.replace("ó", "o");
  return input;
}

// ========== Fetch News ==========
void fetchHeadlines() {
  WiFiClientSecure client;
  client.setInsecure();
// base URL for news headlines. needs API key (see top of code)
  String url = String("https://newsapi.org/v2/top-headlines?country=us&apiKey=") + apiKey;
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("User-Agent", "ESP8266Client"); //this isnt needed, but some sites want a hardware name when OK'ing a connection
  int httpCode = http.GET();
  Serial.print("[HTTP] Status code: "); //just HTML/API debugging
  Serial.println(httpCode);
  
  if (httpCode > 0) { //this could be larger and grab more headlines on better hardware
      //print how large the json payload when grabbing 10 headlines.
    int len = http.getSize();
    Serial.print("Payload size: ");
    Serial.print(len / 1024.0, 2);  // size in KB with 2 decimals
    Serial.println(" KB");
    //should do some better memeory allocating for ESP8266, but eveything is working just fine and i dont want to break stuff.

    const size_t capacity = JSON_ARRAY_SIZE(20) + JSON_OBJECT_SIZE(3) + 20 * JSON_OBJECT_SIZE(6) + 3500;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, http.getStream());

    JsonArray articles = doc["articles"].as<JsonArray>();
    uint8_t count = 0;
    for (JsonObject article : articles) {
      if (count >= 10) break;
      const char* title = article["title"];
      if (title) headlines[count++] = sanitizeHeadline(String(title));
    }
    for (; count < 10; count++) headlines[count] = "";

    Serial.println("✅ Headlines updated.");
    printStatusToSerial(); // <-- debug, this is optional
  } else {
    Serial.println("❌ Failed to fetch headlines.");
  }
  http.end();
}

// ========== Fetch Weather ==========
void fetchForecast() {
  if (!client.connect("api.weather.gov", 443)) {
    Serial.println("❌ Connection to weather.gov failed.");
    return;
  }

  // Step 1: Get forecast URL with lat and long for the city you want (Pittsburgh, PA in this example)
  client.print(String("GET /points/40.4406,-79.9959 HTTP/1.1\r\n") +
               "Host: api.weather.gov\r\nUser-Agent: ESP8266\r\nConnection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, client);
  const char* forecastUrl = doc["properties"]["forecast"];
  client.stop();

  if (!forecastUrl) {
    Serial.println("❌ Forecast URL not found.");
    return;
  }

  delay(500);
  if (!client.connect("api.weather.gov", 443)) {
    Serial.println("❌ Connection failed (forecast fetch).");
    return;
  }

  String path = String(forecastUrl);
  path.replace("https://api.weather.gov", "");
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: api.weather.gov\r\nUser-Agent: ESP8266\r\nConnection: close\r\n\r\n");

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
    filter["properties"]["periods"][i]["windSpeed"] = true;
    filter["properties"]["periods"][i]["windDirection"] = true;
    filter["properties"]["periods"][i]["probabilityOfPrecipitation"]["value"] = true;
  }

  DynamicJsonDocument forecastDoc(2048);
  deserializeJson(forecastDoc, client, DeserializationOption::Filter(filter));
  client.stop();

  JsonArray periods = forecastDoc["properties"]["periods"];
  if (periods.size() < 3) {
    forecastMessage = "Forecast data unavailable.";
    return;
  }

  String msg = "";
  for (int i = 0; i < 3; i++) {
    JsonObject p = periods[i];
    msg += String(p["name"].as<const char*>()) + ": ";
    msg += String(p["temperature"].as<int>()) + p["temperatureUnit"].as<const char*>() + ", ";
    msg += p["shortForecast"].as<const char*>();
    if (p["probabilityOfPrecipitation"]["value"].is<int>()) {
      int precip = p["probabilityOfPrecipitation"]["value"];
      if (precip > 0) msg += ", " + String(precip) + "% precip";
    }
    msg += " | ";
  }
  forecastMessage = msg;
   printStatusToSerial(); // <-- optional, debug use
}

// ========== Check WiFi ==========
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🔄 Reconnecting WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
      Serial.println("\n✅ Reconnected.");
    else
      Serial.println("\n❌ Failed to reconnect.");
  }
}

// ========== Print All Headlines and Forecast over serial ========== Use this to push on other displays
void printStatusToSerial() {
  Serial.println("\n==== Headlines ====");
  for (int i = 0; i < 10; i++) {
    if (headlines[i].length() > 0) {
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.println(headlines[i]);
    }
  }

  Serial.println("\n==== Weather Forecast ====");
  Serial.println(forecastMessage);
}