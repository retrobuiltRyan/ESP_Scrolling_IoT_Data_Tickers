/* IoT Stock Price text scroller for ESP8266 | Node MCU V1.0 (ESP-12E) Flash size 4MB, CPU Freq 80MHz
- Fetches 25 real-time stock prices using Finnhub.io (60 calls/minute free API)
- Parses stock data from JSON littered with HTTP messages
- Displays prices on 8x64 LED matrix (MAX72xx) in 2 zones and sorts by gain or loss.
+-------------------------------+
|           Zone 0  (tops)      |  
|    Scrolling stock ↑ gain     | 
+-------------------------------+
|           Zone 1  (bottom)    |  
|   Scrolling stock ↓ loss      | 
+-------------------------------+
- Scrolls stock data with custom up/down arrows
- Uses non-blocking logic to fetch data smoothly
- Separates the upStock and downStock into chunks to avoid overflowing the Parola library scroll buffer. 
-this is a huge pain in the ass to get this working with the Parola hardware. scrolling buffer is max199
characters when using 2 zones, so this code is 2x longer to impliment. (just use the esp32/ and RGB Pmatrix displays)
- Automatically reconnects to WiFi if disconnected
*/

// =========================== WiFi Config =============================
//#define USE_DEVICE_WIFI  // Comment this line to use the second WiFi config
#ifdef USE_DEVICE_WIFI
const char* ssid = "special wifi";
const char* password = "";
#else
const char* ssid = "your wifi name";
const char* password = "you password";
#endif

// =========================== API Config =============================
const char* apiKey = "replace with your Api key";    // Replace with your Finnhub.io API key
const char* finnhubHost = "finnhub.io";
const int httpsPort = 443;

// =========================== Libraries =============================
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <SPI.h>

// =========================== Custom Arrows GFX =============================
const uint8_t downArrow[] = {
  7,  // array/arrow draw is rotated 90deg because the because this hardware is weird
  0b0001000,
  0b0011000,
  0b0111111,
  0b1111111,
  0b0111111,
  0b0011000,
  0b0001000
};

const uint8_t upArrow[] = {
  7,  // also drawn sideways
  0b0001000,
  0b0001100,
  0b1111110,
  0b1111111,
  0b1111110,
  0b0001100,
  0b0001000
};
// #define DEBUG_MODE  // Comment out this line to disable debug output
// =========================== Debug Config =============================
#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x)     Serial.print(x)
  #define DEBUG_PRINTLN(x)   Serial.println(x)
  #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif
// =========================== Matrix Config =============================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 16 //change this to fit your harware
#define NUM_ZONES 2
#define ZONE_SIZE (MAX_DEVICES / NUM_ZONES)

#define CLK_PIN   D5   //SPI connections
#define DATA_PIN  D7
#define CS_PIN    D8

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

int scrollSpeed = 25; // Lower is faster

// =========================== Globals =============================
unsigned long lastFetch = 0;
const unsigned long interval = 600000; // 10 minutes then fetch new stock numbers
#define MSG_MAX_LEN 128
char currentMsg[MSG_MAX_LEN] = "Loading stock prices...";
char lastKnownScrollMsg[MSG_MAX_LEN] = "Waiting for data...";

#define MAX_SCROLL_CHUNK 200 //max number of characters in a string chunk
char upChunks[4][MAX_SCROLL_CHUNK];
char downChunks[4][MAX_SCROLL_CHUNK];
int numUpChunks = 0;
int numDownChunks = 0;
int currentChunkIndex = 0;
unsigned long lastChunkTime = 0;
const unsigned long chunkDisplayDuration = 15000; // 15 seconds per chunk


bool newMessage = true;

const char* symbols[] = { // don't fetch more than 25 stocks. API freebie limit is 60 requests per minute. else key will be blocked.
"AAPL", "AMD", "AMZN", "BABA", "CMCSA",
"EA", "GME", "GOOGL", "IBM", "INTC",
"META", "MMM", "MSFT", "NFLX", "NVDA",
"RIVN", "QCOM", "TSLA", "TTWO", "TSM"
};
const int numSymbols = sizeof(symbols) / sizeof(symbols[0]);

unsigned long wifiReconnectLastAttempt = 0;
const unsigned long wifiReconnectInterval = 5000; // try reconnect every 5 seconds
bool wifiReconnecting = false;

// =========================== Setup =============================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("System starting...");
  delay(1000);
  P.begin(NUM_ZONES);

  for (uint8_t i = 0; i < NUM_ZONES; i++) {
    P.setZone(i, ZONE_SIZE * i, (ZONE_SIZE * (i + 1)) - 1);
  }

  P.setIntensity(1); // 1=low brightness, 16=max brightness. uses lots of power
  P.setInvert(false); //true = wayyyyyy more power, but kinda neat look, harder to read
  P.setSpeed(scrollSpeed);
  P.setPause(1000);
  P.setTextAlignment(PA_LEFT);

  P.addChar('^', upArrow);   // Use '^' for up
  P.addChar('v', downArrow); // Use 'v' for down

  WiFi.begin(ssid, password);
  Serial.println(WiFi.macAddress()); //cannot call this before WiFi is initialized

  // Displaysomething on boot .
  P.displayText("Connecting...", PA_LEFT, 10, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  // Wait for WiFi connection but keep animating display and letting WiFi run
  while (WiFi.status() != WL_CONNECTED) {
    P.displayAnimate();
    delay(50); // small delay for WiFi stack and display refresh
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  fetchAllStockPrices(); // Initial cache population
}

void loop() {
  unsigned long now = millis();

  // Check WiFi status and attempt reconnect if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiReconnecting) {
      Serial.println("WiFi disconnected, attempting reconnect...");
      wifiReconnecting = true;
      P.displayZoneText(0, "Reconnecting WiFi...", PA_LEFT, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      P.displayZoneText(1, "", PA_LEFT, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      P.displayReset();
    }

    if (now - wifiReconnectLastAttempt >= wifiReconnectInterval) {
      wifiReconnectLastAttempt = now;
      WiFi.disconnect();
      WiFi.reconnect();
      Serial.println("Reconnecting to WiFi...");
    }

    P.displayAnimate();
    delay(10);

    // RETURN EARLY to skip normal operation when disconnected
    return;
  } else {
    if (wifiReconnecting) {
      Serial.println("WiFi reconnected!");
      wifiReconnecting = false;
      lastFetch = 0;
      scrollText("WiFi connected, fetching stock prices...");
    }
  }

  // Animate both zones
  P.displayAnimate();

  // Rotate through up/down chunks every 15 seconds
  if ((numUpChunks > 1 || numDownChunks > 1) && millis() - lastChunkTime >= chunkDisplayDuration) {
    currentChunkIndex++;
    if (currentChunkIndex >= max(numUpChunks, numDownChunks)) {
      currentChunkIndex = 0;
    }

    if (currentChunkIndex < numUpChunks) {
      P.displayZoneText(1, upChunks[currentChunkIndex], PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      P.displayReset(1);
    }

    if (currentChunkIndex < numDownChunks) {
      P.displayZoneText(0, downChunks[currentChunkIndex], PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      P.displayReset(0);
    }

    lastChunkTime = millis();
  }

  // Restart scrolling when both zones finish
  if (P.getZoneStatus(0) && P.getZoneStatus(1)) {
    P.displayReset(0);
    P.displayReset(1);
  }

  // Refresh quotes every 10 minutes
  if (now - lastFetch >= interval || lastFetch == 0) {
    Serial.println("\nFetching all stock prices...");
    fetchAllStockPrices();
    lastFetch = now;
  }

  if (newMessage) { //maybe this helps, i doubt it
    Serial.println("Resetting display zones...");
    P.displayReset(0);
    P.displayReset(1);
    newMessage = false;
  }
}


// =========================== Scroll Update ==================================
void scrollText(const char* p) {
  strncpy(currentMsg, p, sizeof(currentMsg) - 1);
  newMessage = true;
}

// =========================== Symbol Mapping Helper ====================================
const char* symbolToQuery(const char* sym) {
  if (strcmp(sym, "BRK.B") == 0) return "BRKB";
  return sym;
}

// =========================== Fetch SINGLE Stock ========================================
bool fetchSingleStockPrice(const char* symbol, String &result) {
  WiFiClientSecure client;
  client.setInsecure();  // Warning: insecure mode used for simplicity

  String querySymbol = symbolToQuery(symbol);
  String path = "/api/v1/quote?symbol=" + String(querySymbol) + "&token=" + apiKey;

  if (!client.connect(finnhubHost, httpsPort)) {
    Serial.printf("Connection failed for %s\n", symbol);
    return false;
  }

  // Send HTTP GET request
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + finnhubHost + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  // Read status line
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  //Serial.println("Status Line: " + statusLine); debug, checking if the API is happy with the request HTTP 200 is good
  if (!statusLine.startsWith("HTTP/1.1 200")) {
    Serial.println("HTTP error or unexpected status");
    return false;
  }

  // Skip headers safely
  int headerLines = 0;
  while (true) {
    String headerLine = client.readStringUntil('\n');
    headerLine.trim();
    //Serial.println("Header: " + headerLine); //debug stuff
    headerLines++;

    if (headerLine.length() == 0) {
      //Serial.println("End of headers after " + String(headerLines) + " lines."); //debug stuff
      break;
    }

    if (headerLines > 30) {
      Serial.println("Too many headers, aborting.");
      return false;
    }
  }

  // Read JSON payload with timeout
  String payload = "";
  unsigned long timeout = millis() + 5000; // 5 second timeout
  while (millis() < timeout) {
    while (client.available()) {
      char c = client.read();
      payload += c;
      timeout = millis() + 5000;  // Reset timeout on every byte
    }
    if (payload.length() > 0) break;
    delay(10); // Prevent busy wait
  }

  payload.trim();
  //Serial.println("Payload received: \"" + payload + "\""); //debug

  if (payload.length() == 0) {
    Serial.printf("JSON error for %s: EmptyInput\n", symbol);
    return false;
  }

  // Parse JSON
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("JSON error for %s: %s\n", symbol, err.c_str());
    return false;
  }

  if (doc.containsKey("error")) {
    Serial.printf("API error for %s: %s\n", symbol, doc["error"].as<const char*>());
    return false;
  }

  if (!doc.containsKey("c") || doc["c"].isNull()) {
    Serial.printf("Invalid data for %s: missing 'c'\n", symbol);
    return false;
  }

  // Extract and format result
  float current = doc["c"];
  float change = doc["d"];
  float percent = doc["dp"];

  String arrow = (change >= 0) ? "^" : "v";
  String percentSign = (percent >= 0) ? "+" : "";
  String changeSign = (change >= 0) ? "+" : "";

  result = String(symbol) + ": $" + String(current, 2) + " " + arrow
           + " " + changeSign + String(change, 2) + " (" + percentSign + String(percent, 2) + " % )   ";
  
  Serial.println(result);
  return true;
}

// =========================== Fetch All Stock Prices =============================
//to fit within the API's freebie limits what happens here is not "efficient" but does the job without getting blocked from the API
//Sends an HTTP GET request for that one symbol
//Waits for and reads the response JSON payload
//Parses and formats that data into a display string

// =========================== Fetch All Stock Prices =============================
void fetchAllStockPrices() {
  String upStocks = "";
  String downStocks = "";
  bool success = false;

  for (int i = 0; i < numSymbols; i++) {
    String stockMsg;
    const char* sym = symbols[i];

    if (fetchSingleStockPrice(sym, stockMsg)) {
      success = true;
      if (stockMsg.indexOf('^') >= 0) {
        upStocks += stockMsg;
        upStocks += "     ";
      } else {
        downStocks += stockMsg;
        downStocks += "     ";
      }
    } else {
      downStocks += String(sym) + ": N/A       ";
    }

    delay(780);
    //toggle serial print for size of string length when building the gain and loss stock strings 
    DEBUG_PRINT("upStocks / downStocks length: ");
    DEBUG_PRINT(upStocks.length()); DEBUG_PRINT(" / "); DEBUG_PRINT(downStocks.length());
    DEBUG_PRINT("   Free heap: ");
    DEBUG_PRINTLN(ESP.getFreeHeap());
  }

  if (success) {
    upStocks.trim();
    downStocks.trim();

    // Chunk upStocks manually, breaking on complete stock messages
    numUpChunks = 0;
    int upLen = upStocks.length();
    int upStart = 0;
    String upChunk = "";
    while (upStart < upLen && numUpChunks < 4) {
      int nextSpace = upStocks.indexOf("     ", upStart);
      if (nextSpace == -1) nextSpace = upLen;

      String stockEntry = upStocks.substring(upStart, nextSpace + 5); // include 5-space delimiter
      if (upChunk.length() + stockEntry.length() >= MAX_SCROLL_CHUNK - 1) {
        upChunk.toCharArray(upChunks[numUpChunks], MAX_SCROLL_CHUNK);
        upChunks[numUpChunks][MAX_SCROLL_CHUNK - 1] = '\0';
        Serial.println("=============Chunk Formation==============");
        Serial.print("Up Chunk "); Serial.print(numUpChunks); Serial.print(": ");
        Serial.println(upChunks[numUpChunks]);
        numUpChunks++;
        upChunk = "";
        if (numUpChunks >= 4) break;
      }
      upChunk += stockEntry;
      upStart = nextSpace + 5;
    }

    if (upChunk.length() > 0 && numUpChunks < 4) {
      upChunk.toCharArray(upChunks[numUpChunks], MAX_SCROLL_CHUNK);
      upChunks[numUpChunks][MAX_SCROLL_CHUNK - 1] = '\0';
      Serial.println("=============Chunk Formation==============");
      Serial.print("Up Chunk "); Serial.print(numUpChunks); Serial.print(": ");
      Serial.println(upChunks[numUpChunks]);
      numUpChunks++;
    }

    // Chunk downStocks manually, breaking on complete stock messages
    numDownChunks = 0;
    int downLen = downStocks.length();
    int downStart = 0;
    String downChunk = "";
    while (downStart < downLen && numDownChunks < 4) {
      int nextSpace = downStocks.indexOf("     ", downStart);
      if (nextSpace == -1) nextSpace = downLen;

      String stockEntry = downStocks.substring(downStart, nextSpace + 5); // include 5-space delimiter
      if (downChunk.length() + stockEntry.length() >= MAX_SCROLL_CHUNK - 1) {
        downChunk.toCharArray(downChunks[numDownChunks], MAX_SCROLL_CHUNK);
        downChunks[numDownChunks][MAX_SCROLL_CHUNK - 1] = '\0';
        Serial.println("=============Chunk Formation==============");
        Serial.print("Down Chunk "); Serial.print(numDownChunks); Serial.print(": ");
        Serial.println(downChunks[numDownChunks]);
        numDownChunks++;
        downChunk = "";
        if (numDownChunks >= 4) break;
      }
      downChunk += stockEntry;
      downStart = nextSpace + 5;
    }

    if (downChunk.length() > 0 && numDownChunks < 4) {
      downChunk.toCharArray(downChunks[numDownChunks], MAX_SCROLL_CHUNK);
      downChunks[numDownChunks][MAX_SCROLL_CHUNK - 1] = '\0';
      Serial.println("=============Chunk Formation==============");
      Serial.print("Down Chunk "); Serial.print(numDownChunks); Serial.print(": ");
      Serial.println(downChunks[numDownChunks]);
      numDownChunks++;
    }

    currentChunkIndex = 0;
    lastChunkTime = millis();

    // Load first chunk into zones
    P.displayZoneText(1, upChunks[currentChunkIndex], PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    P.displayZoneText(0, downChunks[currentChunkIndex], PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    newMessage = true;
    strncpy(lastKnownScrollMsg, currentMsg, sizeof(lastKnownScrollMsg) - 1);
  } else {
    Serial.println("Fetch failed — using cached data.");
    strncpy(currentMsg, lastKnownScrollMsg, sizeof(currentMsg) - 1);
    newMessage = true;
  }
}
