#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ArduinoJson.h> // Install this library!

// --- 1. WI-FI SETTINGS ---
const char* ssid = "Excitel_lightmorphic4g";     // <--- CHANGE THIS
const char* password = "Wildshriyash@"; // <--- CHANGE THIS
String camera_ip = "192.168.1.105";      // <--- CHANGE THIS if you have a camera

// --- 2. PIN DEFINITIONS (Matches your working setup) ---
#define DHTPIN 4
#define DHTTYPE DHT11
#define LDR_PIN 26
#define TRIG_PIN 5
#define ECHO_PIN 18
#define PUMP_PIN 27      // Relay 1
#define LIGHT_PIN 14     // Relay 2

// --- 3. OBJECTS ---
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

// --- 4. VARIABLES ---
float temp = 0.0;
float hum = 0.0;
int waterLevelPercent = 0;
bool pumpState = false;
bool lightState = false;
bool manualMode = false; // If true, automation stops and you control it

// --- 5. DASHBOARD HTML ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Hydro-Grow Smart Farm</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css" rel="stylesheet">
    <style>body{background:#0f172a;color:white;font-family:sans-serif;}</style>
</head>
<body class="p-4 max-w-lg mx-auto">
    
    <div class="flex items-center gap-3 mb-6 bg-slate-800 p-4 rounded-xl border border-slate-700">
        <div class="bg-green-500 p-2 rounded-lg"><i class="fas fa-leaf text-white text-xl"></i></div>
        <div><h1 class="text-xl font-bold">Hydro-Grow</h1><p class="text-xs text-green-400">System Online</p></div>
    </div>

    <div class="bg-slate-800 p-1 rounded-xl mb-4 border border-slate-700 overflow-hidden relative">
        <div class="absolute top-2 left-2 bg-red-600 text-xs px-2 py-1 rounded font-bold animate-pulse">LIVE</div>
        <img id="cam" src="" class="w-full rounded-lg" alt="Camera Feed Loading...">
    </div>

    <div class="grid grid-cols-2 gap-4 mb-4">
        <div class="bg-slate-800 p-4 rounded-xl border border-slate-700">
            <div class="flex justify-between text-orange-400 mb-2"><i class="fas fa-temperature-high"></i><span>Temp</span></div>
            <p class="text-3xl font-bold"><span id="t">--</span>°C</p>
        </div>
        <div class="bg-slate-800 p-4 rounded-xl border border-slate-700">
            <div class="flex justify-between text-blue-400 mb-2"><i class="fas fa-water"></i><span>Water</span></div>
            <p class="text-3xl font-bold"><span id="w">--</span>%</p>
        </div>
    </div>

    <div class="bg-slate-800 p-6 rounded-xl border border-slate-700">
        <h3 class="font-bold mb-4 text-gray-400 border-b border-gray-600 pb-2">Manual Override</h3>
        
        <div class="flex justify-between items-center mb-6">
            <div class="flex items-center gap-3">
                <div class="p-2 bg-indigo-500/20 rounded text-indigo-400"><i class="fas fa-tint"></i></div>
                <span>Pump</span>
            </div>
            <button id="btn-p" onclick="toggle('pump')" class="bg-gray-600 px-4 py-2 rounded-full font-bold transition">OFF</button>
        </div>

        <div class="flex justify-between items-center">
            <div class="flex items-center gap-3">
                <div class="p-2 bg-yellow-500/20 rounded text-yellow-400"><i class="fas fa-lightbulb"></i></div>
                <span>Lights</span>
            </div>
            <button id="btn-l" onclick="toggle('light')" class="bg-gray-600 px-4 py-2 rounded-full font-bold transition">OFF</button>
        </div>
    </div>

    <script>
        // Set Camera URL
        document.getElementById('cam').src = "http://" + "%CAMERA_IP%" + ":81/stream";

        setInterval(() => {
            fetch('/status').then(r => r.json()).then(d => {
                document.getElementById('t').innerText = d.t;
                document.getElementById('w').innerText = d.w;
                updateBtn('btn-p', d.p);
                updateBtn('btn-l', d.l);
            });
        }, 1000);

        function updateBtn(id, state) {
            const el = document.getElementById(id);
            if(state) {
                el.innerText = "ON";
                el.className = "bg-green-500 px-4 py-2 rounded-full font-bold shadow-[0_0_10px_rgba(34,197,94,0.5)] transition";
            } else {
                el.innerText = "OFF";
                el.className = "bg-gray-600 px-4 py-2 rounded-full font-bold text-gray-400 transition";
            }
        }

        function toggle(dev) { fetch('/toggle?d=' + dev); }
    </script>
</body>
</html>
)rawliteral";

// --- 6. SERVER HANDLERS ---
void handleRoot() {
  String s = index_html;
  s.replace("%CAMERA_IP%", camera_ip); 
  server.send(200, "text/html", s);
}

void handleStatus() {
  StaticJsonDocument<200> doc;
  doc["t"] = temp; 
  doc["w"] = waterLevelPercent;
  doc["p"] = pumpState; 
  doc["l"] = lightState;
  String json; 
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleToggle() {
  String d = server.arg("d");
  manualMode = true; // Disable automation if user clicks a button
  
  if (d == "pump") pumpState = !pumpState;
  if (d == "light") lightState = !lightState;
  
  // Apply changes immediately (Active LOW Relays)
  digitalWrite(PUMP_PIN, pumpState ? LOW : HIGH);
  digitalWrite(LIGHT_PIN, lightState ? LOW : HIGH);
  
  server.send(200, "text/plain", "OK");
}

// --- 7. SETUP ---
void setup() {
  Serial.begin(115200);
  
  // Hardware Init
  dht.begin();
  pinMode(LDR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT); pinMode(LIGHT_PIN, OUTPUT);
  
  digitalWrite(PUMP_PIN, HIGH); // Off
  digitalWrite(LIGHT_PIN, HIGH); // Off

  // Wi-Fi Connect
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("");
  Serial.print("Connected! IP Address: ");
  Serial.println(WiFi.localIP()); // <--- IMPORTANT: Type this IP in your browser!

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/toggle", handleToggle);
  server.begin();
}

// --- 8. LOOP ---
void loop() {
  server.handleClient();

  // Read Sensors
  float t = dht.readTemperature();
  if (!isnan(t)) temp = t;
  
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH);
  int dist = dur * 0.034 / 2;
  waterLevelPercent = map(dist, 15, 2, 0, 100); // 15cm=Empty, 2cm=Full
  waterLevelPercent = constrain(waterLevelPercent, 0, 100);

  int lightVal = analogRead(LDR_PIN);

  // AUTOMATION LOGIC (Only runs if manual mode is OFF)
  if (!manualMode) {
    // Pump Logic
    if (temp > 30 && waterLevelPercent > 10) pumpState = true;
    else pumpState = false;

    // Light Logic
    if (lightVal < 2000) lightState = true;
    else lightState = false;

    // Apply Logic
    digitalWrite(PUMP_PIN, pumpState ? LOW : HIGH);
    digitalWrite(LIGHT_PIN, lightState ? LOW : HIGH);
  }
  
  delay(100);
}