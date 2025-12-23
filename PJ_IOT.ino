#include <WiFi.h>           // Library for Wi-Fi connection
#include <PubSubClient.h>   // Library for MQTT communication (publish/subscribe system)

// ===== WiFi & MQTT Configuration =====
const char* ssid = "Galaxiuuuu";       // Wi-Fi network name
const char* password = "12345678";     // Wi-Fi password
const char* mqtt_server = "192.168.";  // MQTT broker IP address (to be completed)
WiFiClient espClient;                  // Creates Wi-Fi client object
PubSubClient client(espClient);        // Creates MQTT client using Wi-Fi connection

// ===== Pin Definitions =====
const int sensorIn = 34;     // IR sensor pin for entry detection
const int sensorOut = 35;    // IR sensor pin for exit detection
const int buttonPin = 13;    // Button pin to start/stop system

// ===== System Variables =====
bool systemRunning = false;            // Flag to indicate if the system is running
int producedCount = 0;                 // Total number of produced pieces
unsigned long systemStartTime = 0;     // Time when system starts (in ms)
unsigned long systemStopTime = 0;      // Time when system stops (in ms)
unsigned long totalCycleTime = 0;      // Sum of all cycle times (ms)

// ===== FIFO Structure for Pieces =====
// Each "Piece" represents one item that passes through the production system
struct Piece {
  int id;                    // Unique ID of the piece
  unsigned long entryTime;   // Timestamp when piece enters system
  unsigned long exitTime;    // Timestamp when piece exits system
  unsigned long cycleTime;   // Time the piece spent in system = exitTime - entryTime
};

// ===== FIFO Queue Parameters =====
const int MAX_PIECES = 200;  // Maximum number of pieces that can be tracked
Piece pieces[MAX_PIECES];    // Array to store pieces
int fifoHead = 0;            // Points to first unprocessed piece (oldest)
int fifoTail = 0;            // Points to position where next piece will be added

// ===== Time Tracking Variables =====
unsigned long totalWipTime = 0;        // Total Work In Progress (WIP) time accumulator
unsigned long lastWipChangeTime = 0;   // Last time WIP count changed

// ===== Busy/Idle Tracking =====
unsigned long busyStartTime = 0;       // When system became busy
unsigned long busyTime = 0;            // Total busy time
bool systemWasBusy = false;            // Flag to know if machine was active

// ===== Sensor States =====
int lastInState = HIGH;                // Previous state of entry sensor
int lastOutState = HIGH;               // Previous state of exit sensor

// ===== Function: Connect to Wi-Fi =====
void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);                // Start Wi-Fi connection
  while (WiFi.status() != WL_CONNECTED) {    // Wait until connected
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());            // Display IP assigned to ESP32
}

// ===== Function: Reconnect to MQTT Broker =====
void reconnect() {
  while (!client.connected()) {              // Loop until connection succeeds
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {     // Connect with client name "ESP32Client"
      Serial.println("connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());          // Print connection error code
      Serial.println(" retrying in 5 seconds");
      delay(5000);                           // Wait 5 seconds before retry
    }
  }
}

// ===== Setup Function =====
void setup() {
  Serial.begin(115200);                      // Start serial monitor
  pinMode(sensorIn, INPUT);                  // IR entry sensor as input
  pinMode(sensorOut, INPUT);                 // IR exit sensor as input
  pinMode(buttonPin, INPUT_PULLUP);          // Button input with internal pull-up resistor
  setup_wifi();                              // Connect to Wi-Fi
  client.setServer(mqtt_server, 1883);       // Connect to MQTT broker at port 1883
  Serial.println("System ready. Press button to START/STOP.");
}

// ===== Helper Function: Current Work In Progress =====
// Returns number of pieces currently in system (entered but not exited)
int currentWIP() {
  return fifoTail - fifoHead;
}

// ===== Helper Function: Add new piece when it enters =====
void pushPieceEntry(unsigned long entryMillis) {
  if (fifoTail < MAX_PIECES) {               // If queue not full
    pieces[fifoTail].id = fifoTail + 1;      // Assign ID
    pieces[fifoTail].entryTime = entryMillis;
    pieces[fifoTail].exitTime = 0;
    pieces[fifoTail].cycleTime = 0;
    fifoTail++;                              // Move tail to next position
  } else {
    Serial.println("FIFO full! Increase MAX_PIECES.");
  }
}

// ===== Helper Function: Remove oldest piece when it exits =====
Piece popOldestPiece() {
  Piece p = { -1, 0, 0, 0 };                 // Default piece if none available
  if (fifoHead < fifoTail) {                 // Check if queue not empty
    p = pieces[fifoHead];                    // Take oldest piece
    fifoHead++;                              // Move head forward
  }
  return p;                                  // Return that piece
}

// ===== Helper Function: Publish JSON message to MQTT =====
void publishJSON(const char* topic, const char* payload) {
  client.publish(topic, payload);            // Publish data to MQTT broker
}

// ===== MAIN LOOP =====
void loop() {
  if (!client.connected()) reconnect();      // Ensure MQTT connection is alive
  client.loop();                             // Maintain MQTT communication

  // --- Handle Start/Stop Button ---
  static bool lastButtonState = HIGH;        // Remember previous state (static keeps its value)
  bool currentButtonState = digitalRead(buttonPin);  // Read current button state

  // Detect button press (HIGH → LOW transition)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    systemRunning = !systemRunning;          // Toggle system on/off
    delay(200);                              // Small delay for debouncing

    // --- If system just started ---
    if (systemRunning) {
      Serial.println("System started!");
      systemStartTime = millis();            // Record start time
      producedCount = 0;                     // Reset counters
      totalCycleTime = 0;
      fifoHead = 0;
      fifoTail = 0;
      totalWipTime = 0;
      lastWipChangeTime = millis();
      busyTime = 0;
      busyStartTime = 0;
      systemWasBusy = false; } 
    else {
      // --- If system just stopped ---
      systemStopTime = millis();
      unsigned long totalTimeMs = systemStopTime - systemStartTime;
      float totalTimeS = totalTimeMs / 1000.0;  // Convert ms → seconds

      // Finalize WIP time calculation
      unsigned long now = millis();
      totalWipTime += (unsigned long)currentWIP() * (now - lastWipChangeTime);1

      // Finalize busy time if system was busy at stop
      if (systemWasBusy) {
        busyTime += now - busyStartTime;
        systemWasBusy = false;
      }

      // --- Calculate performance metrics ---
      float avgCycle = producedCount > 0 ? (float)totalCycleTime / producedCount / 1000.0 : 0.0;
      float avgWip = totalTimeMs > 0 ? (float)totalWipTime / (float)totalTimeMs : 0.0;
      float throughput = totalTimeMs > 0 ? (float)producedCount / (totalTimeMs / 1000.0) : 0.0;
      float busyTimeS = busyTime / 1000.0;
      float idleTimeS = (totalTimeMs - busyTime) / 1000.0;
      float utilization = totalTimeMs > 0 ? (float)busyTime / totalTimeMs * 100.0 : 0.0;

      // --- Print summary to Serial Monitor ---
      Serial.println("System stopped.");
      Serial.print("Total system time (s): "); Serial.println(totalTimeS, 2);
      Serial.print("Total produced (pieces): "); Serial.println(producedCount);
      Serial.print("Average cycle time (s): "); Serial.println(avgCycle, 2);
      Serial.print("Average WIP: "); Serial.println(avgWip, 2);
      Serial.print("Throughput (parts/s): "); Serial.println(throughput, 4);
      Serial.print("Busy time (s): "); Serial.println(busyTimeS, 2);
      Serial.print("Idle time (s): "); Serial.println(idleTimeS, 2);
      Serial.print("Utilization (%): "); Serial.println(utilization, 2);

      // --- Print each piece’s details ---
      Serial.println("Detailed pieces:");
      int printed = 0;
      for (int i = 0; i < fifoTail && printed < producedCount; i++) {
        if (pieces[i].exitTime != 0) {  // Only for finished pieces
          Serial.print("Piece "); Serial.print(pieces[i].id);
          Serial.print(" | Entry: "); Serial.print((pieces[i].entryTime - systemStartTime) / 1000.0, 2);
          Serial.print("s | Exit: "); Serial.print((pieces[i].exitTime - systemStartTime) / 1000.0, 2);
          Serial.print("s | Cycle: "); Serial.print(pieces[i].cycleTime / 1000.0, 2);
          Serial.println("s");
          printed++;
        }
      }
      Serial.println("----------------------");

      // --- Create JSON summary and send via MQTT ---
      char summary[300];
      snprintf(summary, sizeof(summary),
               "{\"total_time_s\":%.2f,\"produced\":%d,\"avg_cycle_s\":%.2f,\"avg_wip\":%.2f,"
               "\"throughput_ps\":%.4f,\"busy_s\":%.2f,\"idle_s\":%.2f,\"utilization\":%.2f}",
               totalTimeS, producedCount, avgCycle, avgWip, throughput,
               busyTimeS, idleTimeS, utilization);
      publishJSON("esp32/system/summary", summary);  // Send summary to MQTT
    }
  }
  lastButtonState = currentButtonState;  // Remember last button state

  // --- If system is running, check sensors ---
  if (systemRunning) {
    int inState = digitalRead(sensorIn);   // Read entry sensor
    int outState = digitalRead(sensorOut); // Read exit sensor
    unsigned long now = millis();          // Current time

    // ===== ENTRY DETECTION =====
    if (lastInState == HIGH && inState == LOW) {     // Detect falling edge (object passes)
      totalWipTime += (unsigned long)currentWIP() * (now - lastWipChangeTime);
      lastWipChangeTime = now;

      int oldWip = currentWIP();
      pushPieceEntry(now);                            // Add new piece in system
      int newWip = currentWIP();

      // Start busy time if system was idle
      if (oldWip == 0 && newWip > 0) {
        systemWasBusy = true;
        busyStartTime = now;
      }

      // Print and send entry info
      Serial.print("Piece entered at "); Serial.print((now - systemStartTime)/1000.0, 2); Serial.println(" s");
      Serial.print("WIP: "); Serial.println(newWip);
      publishJSON("esp32/sensor/in", "{\"in\":1}");
    }

    // ===== EXIT DETECTION =====
    if (lastOutState == HIGH && outState == LOW) {    // Detect falling edge (object exits)
      if (currentWIP() > 0) {                         // If at least one piece is in system
        totalWipTime += (unsigned long)currentWIP() * (now - lastWipChangeTime);
        lastWipChangeTime = now;

        int oldWip = currentWIP();
        Piece p = popOldestPiece();                   // Get oldest piece (FIFO)
        int newWip = currentWIP();

        producedCount++;                              // Increment piece counter
        p.id = producedCount;
        p.exitTime = now;
        p.cycleTime = p.exitTime - p.entryTime;
        totalCycleTime += p.cycleTime;

        // Update record
        int storeIndex = fifoHead - 1; 
        if (storeIndex >= 0 && storeIndex < MAX_PIECES) pieces[storeIndex] = p;

        // Stop busy time if system becomes idle
        if (oldWip > 0 && newWip == 0 && systemWasBusy) {
          busyTime += now - busyStartTime;
          systemWasBusy = false;
        }

        // Calculate throughput
        float throughput = (float)producedCount / ((now - systemStartTime) / 1000.0);

        // Print piece summary
        Serial.print("Piece exited (id "); Serial.print(p.id); Serial.print(") at ");
        Serial.print((now - systemStartTime)/1000.0, 2); Serial.println(" s");
        Serial.print("Cycle time (s): "); Serial.println(p.cycleTime / 1000.0, 2);
        Serial.print("WIP: "); Serial.println(newWip);
        Serial.print("Throughput (parts/s): "); Serial.println(throughput, 4);
        Serial.println("----------------------");

        // Prepare JSON payload and publish
        char payload[250];
        snprintf(payload, sizeof(payload),
                 "{\"piece\":%d,\"entry\":%.2f,\"exit\":%.2f,\"cycle\":%.2f,"
                 "\"wip\":%d,\"throughput\":%.4f}",
                 p.id,
                 (p.entryTime - systemStartTime) / 1000.0,
                 (p.exitTime - systemStartTime) / 1000.0,
                 p.cycleTime / 1000.0,
                 newWip,
                 throughput);
        publishJSON("esp32/system/piece", payload);   // Send piece data to MQTT
      } else {
        Serial.println("Exit detected but no piece in system (WIP=0)");
      }
    }

    // Update last sensor states
    lastInState = inState;
    lastOutState = outState;
  }
}
