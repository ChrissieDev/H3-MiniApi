#include "arduino_secrets.h" 
#include <WiFiNINA.h>  
#include <ArduinoHttpClient.h>  
#include <Arduino_JSON.h>    
#include <Arduino_MKRIoTCarrier.h>  

MKRIoTCarrier carrier;

// ---------- Countries API (HTTPS) ----------
const char SERVER[]    = "api-opgaver.mercantec.tech";
const int  SERVER_PORT = 443;
WiFiSSLClient secureClient;
HttpClient    http(secureClient, SERVER, SERVER_PORT);

// ---------- Backend (plain HTTP, local network) ----------
const char BACKEND_HOST[] = "192.168.1.191";   // may change, update here only
const int  BACKEND_PORT   = 5144;
const char BACKEND_PATH[] = "/api/countries";
WiFiClient backendClient;
HttpClient backendHttp(backendClient, BACKEND_HOST, BACKEND_PORT);

// ---------- Countries per continent (spaces as %20) ----------
const char* const EUROPE[] = {
  "Poland", "France", "Germany", "Italy", "Spain", "Norway", "Sweden",
  "Ukraine", "Greece", "Portugal", "Ireland", "Denmark", "Iceland",
  "Finland", "Austria", "Switzerland", "Hungary", "Romania",
  "Netherlands", "Croatia"
};
const char* const ASIA[] = {
  "Japan", "India", "China", "Thailand", "Vietnam", "Mongolia", "Nepal",
  "Pakistan", "Kazakhstan", "Indonesia", "Philippines", "Malaysia",
  "Jordan", "Israel", "Armenia", "Uzbekistan", "Bhutan", "Singapore",
  "Cambodia", "Laos"
};
const char* const AFRICA[] = {
  "Egypt", "Kenya", "Morocco", "Nigeria", "Ghana", "Ethiopia", "Tanzania",
  "Senegal", "Madagascar", "Namibia", "Botswana", "Uganda", "Algeria",
  "Tunisia", "Zambia", "Rwanda", "Mali", "Angola", "Mozambique", "Cameroon"
};

const char* const* const LISTS[3]   = { EUROPE, ASIA, AFRICA };
const int COUNTS[3] = {
  sizeof(EUROPE) / sizeof(EUROPE[0]),
  sizeof(ASIA)   / sizeof(ASIA[0]),
  sizeof(AFRICA) / sizeof(AFRICA[0])
};
const char* const CONTINENT_NAMES[3] = { "Europe", "Asia", "Africa" };

// ---------- Game state ----------
enum State { NO_WIFI, PICK_CONTINENT, WAIT_SHAKE, SHOW_COUNTRY };
State  state = PICK_CONTINENT;
int    continent = -1;       // 0, 1 or 2
String lastName = "";        // avoid the same country twice in a row
bool   busy = false;         // guard: one request at a time

struct Country {
  bool   valid = false;
  String choice, cca3, capital, region, subregion, flagUrl;
  double population = 0, area = 0, latitude = 0, longitude = 0;
};
Country current;

// ---------- Shake detection ----------
const float         SHAKE_THRESHOLD = 1.5;   // lower = more sensitive
const unsigned long SHAKE_COOLDOWN  = 1500;  // ms between shakes
float lastX = 0, lastY = 0, lastZ = 0;
unsigned long lastShakeTime = 0;

// Call every loop so the "previous" values stay fresh.
bool shaken() {
  float x, y, z;
  if (!carrier.IMUmodule.accelerationAvailable()) return false;
  carrier.IMUmodule.readAcceleration(x, y, z);

  float delta = fabs(x - lastX) + fabs(y - lastY) + fabs(z - lastZ);
  lastX = x; lastY = y; lastZ = z;

  if (delta > SHAKE_THRESHOLD && millis() - lastShakeTime > SHAKE_COOLDOWN) {
    lastShakeTime = millis();
    Serial.print("Shake! delta = ");
    Serial.println(delta);
    return true;
  }
  return false;
}

// ---------- Screen helpers (centered, fits the round window) ----------
void showLine(const String &text, int y, uint16_t color = ST77XX_WHITE, int maxWidth = 190) {
  int16_t x1, y1;
  uint16_t w, h;
  carrier.display.setTextColor(color);
  carrier.display.setTextSize(2);
  carrier.display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  if (w > maxWidth) {
    carrier.display.setTextSize(1);
    carrier.display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  }
  carrier.display.setCursor((240 - (int)w) / 2, y);
  carrier.display.print(text);
}

void showMessage(const String &text) {
  carrier.display.fillScreen(ST77XX_BLACK);
  showLine(text, 100);
}

void showPickScreen() {
  carrier.display.fillScreen(ST77XX_BLACK);
  showLine("Pick continent", 50, ST77XX_YELLOW);
  showLine("01 Europe", 90);
  showLine("02 Asia",   120);
  showLine("03 Africa", 150);
}

void showShakePrompt() {
  carrier.display.fillScreen(ST77XX_BLACK);
  showLine(CONTINENT_NAMES[continent], 70, ST77XX_YELLOW);
  showLine("Shake me!", 110);
  showLine("04 = back", 160, ST77XX_CYAN);
}

void showCountry() {
  carrier.display.fillScreen(ST77XX_BLACK);
  showLine(current.choice,                          45, ST77XX_YELLOW);
  showLine("Cap: " + current.capital,               80);
  showLine("Reg: " + current.region,               110);
  showLine("Pop: " + String((long)(current.population / 1000)) + "k", 140);
  showLine("00 = save",                            175, ST77XX_GREEN);
}

// ---------- JSON helpers ----------
String textOr(JSONVar value, const String &fallback) {
  if (JSON.typeof(value) == "string") return String((const char*)value);
  return fallback;
}

double numberOr(JSONVar value, double fallback) {
  if (JSON.typeof(value) == "number") return (double)value;
  return fallback;
}

String jsonText(const String &s) {
  String out = "\"";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  out += "\"";
  return out;
}

// ---------- Wi-Fi ----------
bool connectWifi() {
  Serial.print("Connecting to Wi-Fi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 5) {
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(5000);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

// ---------- Fetch one country. true = OK, 'current' updated ----------
bool fetchCountry(const String &name) {
  String path = "/api/Countries/name/" + name;
  Serial.println("GET https://" + String(SERVER) + path);

  http.beginRequest();
  http.get(path);
  http.sendHeader("Accept", "application/json");
  http.endRequest();

  int status = http.responseStatusCode();
  Serial.print("HTTP status: ");
  Serial.println(status);
  if (status != 200) {
    http.stop();
    showMessage("HTTP " + String(status));
    return false;
  }

  String body = http.responseBody();
  http.stop();

  JSONVar obj = JSON.parse(body);
  body = "";
  if (JSON.typeof(obj) == "undefined") {
    Serial.println("JSON parse failed");
    showMessage("JSON error");
    return false;
  }

  current.choice     = textOr(obj["name"]["common"], "Not stated");
  current.cca3       = textOr(obj["cca3"],           "Not stated");
  current.capital    = textOr(obj["capital"][0],     "Not stated");
  current.region     = textOr(obj["region"],         "Not stated");
  current.subregion  = textOr(obj["subregion"],      "Not stated");
  current.population = numberOr(obj["population"],   0);
  current.area       = numberOr(obj["area"],         0);
  current.latitude   = numberOr(obj["latlng"][0],    0);
  current.longitude  = numberOr(obj["latlng"][1],    0);
  current.flagUrl    = textOr(obj["flags"]["png"],   "Not stated");
  current.valid      = true;

  Serial.println("Got: " + current.choice + " (" + current.region + ")");
  return true;
}

// ---------- Save current country to backend. Returns HTTP status ----------
int saveCountry() {
  String json = "{";
  json += "\"apiName\":"    + jsonText("Mercantec Countries API") + ",";
  json += "\"choice\":"     + jsonText(current.choice)    + ",";
  json += "\"cca3\":"       + jsonText(current.cca3)      + ",";
  json += "\"capital\":"    + jsonText(current.capital)   + ",";
  json += "\"region\":"     + jsonText(current.region)    + ",";
  json += "\"subregion\":"  + jsonText(current.subregion) + ",";
  json += "\"population\":" + String((long)current.population) + ",";
  json += "\"area\":"       + String(current.area, 1)      + ",";
  json += "\"latitude\":"   + String(current.latitude, 4)  + ",";
  json += "\"longitude\":"  + String(current.longitude, 4) + ",";
  json += "\"flagUrl\":"    + jsonText(current.flagUrl);
  json += "}";

  Serial.println("POST " + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + BACKEND_PATH);
  Serial.println(json);

  int err = backendHttp.post(BACKEND_PATH, "application/json", json);
  if (err != 0) {
    Serial.print("Connection error: ");
    Serial.println(err);
    backendHttp.stop();
    return -1;
  }
  int status = backendHttp.responseStatusCode();
  Serial.print("Backend status: ");
  Serial.println(status);
  Serial.println(backendHttp.responseBody());
  backendHttp.stop();
  return status;
}

// ---------- Game actions ----------
void goToStart() {
  continent = -1;
  current.valid = false;
  lastName = "";
  state = PICK_CONTINENT;
  showPickScreen();
}

void travel() {
  busy = true;
  showMessage("Travelling...");

  String name;
  do {
    name = LISTS[continent][random(COUNTS[continent])];
  } while (name == lastName && COUNTS[continent] > 1);

  if (fetchCountry(name)) {
    lastName = name;
    state = SHOW_COUNTRY;
    showCountry();
  } else {
    delay(1500);                       // let the error be read, then go back
    if (current.valid) showCountry(); else showShakePrompt();
  }
  lastShakeTime = millis();            // ignore the shake echo after the request
  busy = false;
}

void saveAndFinish() {
  busy = true;
  showMessage("Saving...");
  int status = saveCountry();

  if (status == 201) {
    showMessage("Saved!");
    delay(1500);
    goToStart();                       // success -> new game
  } else {
    if (status == 400)     showMessage("Rejected");
    else if (status == -1) showMessage("No server");
    else                   showMessage("Err " + String(status));
    delay(1500);
    showCountry();                     // failure -> keep country, can retry
  }
  busy = false;
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 5000) {}

  carrier.noCase();
  carrier.begin();
  carrier.display.setRotation(0);
  randomSeed(analogRead(A5) + micros());

  showMessage("Wi-Fi...");
  if (!connectWifi()) {
    showMessage("No Wi-Fi");
    state = NO_WIFI;
    return;
  }
  goToStart();
}

void loop() {
  if (state == NO_WIFI || busy) return;

  carrier.Buttons.update();
  bool shake = shaken();

  // 04 = start over, from any screen
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    goToStart();
    return;
  }

  if (state == PICK_CONTINENT) {
    int pick = -1;
    if (carrier.Buttons.onTouchDown(TOUCH1)) pick = 0;
    if (carrier.Buttons.onTouchDown(TOUCH2)) pick = 1;
    if (carrier.Buttons.onTouchDown(TOUCH3)) pick = 2;
    if (pick >= 0) {
      continent = pick;
      state = WAIT_SHAKE;
      showShakePrompt();
    }
  }
  else if (state == WAIT_SHAKE) {
    if (shake) travel();
  }
  else if (state == SHOW_COUNTRY) {
    if (shake) travel();                                   // another country
    else if (carrier.Buttons.onTouchDown(TOUCH0)) saveAndFinish();
  }
}