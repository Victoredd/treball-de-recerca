/*
All of web.cpp was created by a Generative AI model and only verified by myself.
This is because the web functionality is not the main scope of my project.
Therefore, I have offloaded this task to AI.
However, all other code, unless stated otherwise, has been programmed by myself
*/

/**
 * @file web.cpp
 * * Handles all Wi-Fi, mDNS, and Web Server functionality for the ESP32.
 * * CONFIGURED AS ACCESS POINT (HOTSPOT).
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <vector>

// ----------------------------------------------------------------------------
// Type Definitions (from main file)
// ----------------------------------------------------------------------------

struct DataPoint {
    uint32_t timestamp;
    float sensorValue;
    int selectedSensor;
    float error;
    float controlOutput;
    int strategyUsed;
};

// ----------------------------------------------------------------------------
// External State Variables (defined in main.cpp)
// ----------------------------------------------------------------------------

extern volatile bool running;
extern volatile int selectedStrategy;
extern volatile int selectedSensor;
extern float calibMiddle; 
extern float calibLowVal;  
extern float calibHighVal; 
extern std::vector<DataPoint> dataLog;
extern unsigned long lastSensorUpdateTime;

// PID Gains (Externally linked from main.cpp)
extern float gain_p;
extern float gain_i;
extern float gain_d;

// Helper function from main.cpp
String getBufferCSV();
void calibrateLowStep(int selectedSensor);
void calibrateHighStep(int selectedSensor);

// ----------------------------------------------------------------------------
// Web Server Globals
// ----------------------------------------------------------------------------

const char* ssid = "ESP32-Control";
const char* password = "12345678"; 

WebServer server(80);

// ----------------------------------------------------------------------------
// HTML Helper
// ----------------------------------------------------------------------------

String getHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f4f4f4; }
        h1 { color: #333; text-align: center; }
        .container { max-width: 600px; margin: auto; padding: 20px; background: #fff; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        button, select, input { display: block; width: 100%; padding: 12px; margin: 10px 0; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; box-sizing: border-box; }
        input { border: 1px solid #ccc; background: #fff; }
        select { background: #eee; }
        button { color: white; }
        .start { background: #28a745; } .start:hover { background: #218838; }
        .stop { background: #dc3545; } .stop:hover { background: #c82333; }
        .calib { background: #ffc107; color: #333; } .calib:hover { background: #e0a800; }
        .download { background: #17a2b8; } .download:hover { background: #138496; }
        .btn-blue { background: #007bff; } .btn-blue:hover { background: #0056b3; }
        .status-box { text-align: center; margin-bottom: 20px; padding: 10px; background: #eee; border-radius: 5px;}
        .status-dot { height: 15px; width: 15px; border-radius: 50%; display: inline-block; margin-right: 8px; vertical-align: middle; }
        .running { background-color: #28a745; box-shadow: 0 0 8px #28a745; }
        .stopped { background-color: #dc3545; }
        .gains-display { font-size: 0.9em; margin-top: 5px; color: #555; }
        #dataCount { font-weight: bold; color: #555; text-align: center; display: block; margin-top: 10px;}
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Motor Control</h1>

        <div class="status-box">
            Status: <span id="statusDot" class="status-dot stopped"></span> <strong id="statusText">STOPPED</strong>
            <br>Current Strategy: <strong id="stratDisp">)rawliteral";
            
    html += String(selectedStrategy); 
    
    html += R"rawliteral(</strong>
            <br>Last Sensor Time: <strong>)rawliteral" + String(lastSensorUpdateTime) + R"rawliteral( ms</strong>
            <div class="gains-display">
                <strong>Current Gains:</strong> 
                P: )rawliteral" + String(gain_p) + 
                " | I: " + String(gain_i) + 
                " | D: " + String(gain_d) + R"rawliteral(
            </div>
        </div>

        <form action="/start" method="GET" onsubmit="startPolling()"><button class="start" type="submit">Start Experiment</button></form>
        <form action="/stop" method="GET" onsubmit="stopPolling()"><button class="stop" type="submit">Stop Experiment</button></form>

        <span id="dataCount">Recorded Points in Browser: 0</span>

        <hr>
        <button class="download" onclick="downloadCSV()">Download Accumulated Data</button>

        <hr>
        <h3>PID Tuning</h3>
        <form action="/setPID" method="GET">
            <label>P Gain: <input type="number" step="0.001" name="p" value=")rawliteral" + String(gain_p) + R"rawliteral("></label>
            <label>I Gain: <input type="number" step="0.001" name="i" value=")rawliteral" + String(gain_i) + R"rawliteral("></label>
            <label>D Gain: <input type="number" step="0.001" name="d" value=")rawliteral" + String(gain_d) + R"rawliteral("></label>
            <button class="btn-blue" type="submit">Update Gains</button>
        </form>

        <hr>
        <h3>Calibration</h3>
        <p>1. Move rig to LOW position (Last: <strong>)rawliteral" + String(calibLowVal) + R"rawliteral(</strong>):</p>
        <form action="/calibLow" method="GET"><button class="calib" type="submit">Record Low</button></form>
        <p>2. Move rig to HIGH position (Last: <strong>)rawliteral" + String(calibHighVal) + R"rawliteral(</strong>):</p>
        <form action="/calibHigh" method="GET"><button class="calib" type="submit">Record High</button></form>
        <p><em>Calculated Middle: )rawliteral";
        
    html += String(calibMiddle);

    html += R"rawliteral(</em></p>
        
        <hr>
        <form action="/setStrategy" method="GET">
            <label for="strategy">Control Strategy:</label>
            <select name="value" id="strategy">
                <option value="0">Strategy 0 (Idle)</option>
                <option value="1">Strategy 1 (P-Only)</option>
                <option value="2">Strategy 2 (ON/OFF)</option>
                <option value="3">Strategy 3 (PID)</option>
            </select>
            <button class="btn-blue" type="submit">Set Strategy</button>
        </form>
    </div>

    <script>
        // -- JAVASCRIPT STREAMING LOGIC --
        let allData = "timestamp,sensorValue,selectedSensor,error,controlOutput,strategyUsed\n";
        let pointCount = 0;
        let pollInterval = null;
        
        const isRunning = )rawliteral";
    html += (running ? "true" : "false");
    html += R"rawliteral(;

        if(isRunning) {
            document.getElementById("statusDot").className = "status-dot running";
            document.getElementById("statusText").innerText = "RUNNING";
            startPolling(); 
        }

        function startPolling() {
            if(pollInterval) clearInterval(pollInterval);
            // Fetch chunks every 1 second
            pollInterval = setInterval(fetchDataChunk, 1000);
        }

        function stopPolling() {
            setTimeout(fetchDataChunk, 500); // One last fetch
            if(pollInterval) clearInterval(pollInterval);
        }

        function fetchDataChunk() {
            fetch('/pollData')
                .then(response => response.text())
                .then(data => {
                    if(data.length > 5) { // If valid csv chunk
                        allData += data;
                        let lines = data.split("\n").length - 1;
                        pointCount += lines;
                        document.getElementById("dataCount").innerText = "Recorded Points in Browser: " + pointCount;
                    }
                })
                .catch(err => console.error("Poll error:", err));
        }

        function downloadCSV() {
            const blob = new Blob([allData], { type: 'text/csv' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.setAttribute('hidden', '');
            a.setAttribute('href', url);
            a.setAttribute('download', 'experiment_log.csv');
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
        }
    </script>
</body>
</html>
)rawliteral";
    return html;
}

// ----------------------------------------------------------------------------
// Web Server Route Handlers
// ----------------------------------------------------------------------------

void handleRoot() {
    server.send(200, "text/html", getHTML());
}

void handleStart() {
    running = true;
    server.sendHeader("Location", "/"); 
    server.send(302, "text/plain", "Starting...");
}

void handleStop() {
    running = false;
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Stopping...");
}

// NEW: Polling Endpoint
void handlePollData() {
    String chunk = getBufferCSV();
    server.send(200, "text/plain", chunk);
}

void handleCalibLow() {
    calibrateLowStep(selectedSensor);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Low Set");
}

void handleCalibHigh() {
    calibrateHighStep(selectedSensor);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "High Set");
}

void handleSetStrategy() {
    if (server.hasArg("value")) {
        int strategy = server.arg("value").toInt();
        selectedStrategy = strategy;
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Strategy set.");
}

// NEW: PID Update Endpoint
void handleSetPID() {
    if (server.hasArg("p")) gain_p = server.arg("p").toFloat();
    if (server.hasArg("i")) gain_i = server.arg("i").toFloat();
    if (server.hasArg("d")) gain_d = server.arg("d").toFloat();
    
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "PID Updated");
}

void handleDownload() {
    // Legacy download handler (optional, but kept for compatibility)
    // The JS downloadCSV() is now the primary method.
    server.send(200, "text/plain", "Use the button on the page to download streamed data.");
}

void handleNotFound() {
    server.send(404, "text/plain", "404: Not Found");
}

// ----------------------------------------------------------------------------
// Public Functions
// ----------------------------------------------------------------------------

void initWebServer() {
    WiFi.mode(WIFI_AP);
    Serial.println("Creating Access Point...");
    WiFi.softAP(ssid, password);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP Created! Network Name: ");
    Serial.println(ssid);
    Serial.print("Connect to this network and visit: http://");
    Serial.println(myIP);

    if (MDNS.begin("esprig")) {
        Serial.println("mDNS responder started: http://esprig.local");
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/start", HTTP_GET, handleStart);
    server.on("/stop", HTTP_GET, handleStop);
    server.on("/calibLow", HTTP_GET, handleCalibLow);
    server.on("/calibHigh", HTTP_GET, handleCalibHigh);
    server.on("/setStrategy", HTTP_GET, handleSetStrategy);
    server.on("/setPID", HTTP_GET, handleSetPID); // New PID handler
    server.on("/pollData", HTTP_GET, handlePollData); 
    server.on("/download", HTTP_GET, handleDownload);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void handleWebServer() {
    server.handleClient();
}