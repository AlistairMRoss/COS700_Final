#include "MeshNetwork.h"


IPAddress getlocalIP();

IPAddress myIP(0,0,0,0);

painlessMesh  mesh;
WiFiClient wifiClient;

MeshNetwork::MeshNetwork(const char* prefix, const char* password, uint16_t port, Scheduler& _scheduler)
    : meshPrefix(prefix), meshPassword(password), meshPort(port), initialized(false), scheduler(_scheduler),
      meshUpdateTask(TASK_MILLISECOND * 100, TASK_FOREVER, [this]() { 
          this->update(); }) {}

bool MeshNetwork::init() {
    if (initialized) {
        Serial.println("Mesh already initialized");
        return true;
    }

    
    mesh.setDebugMsgTypes(ERROR | STARTUP);

    mesh.init(meshPrefix, meshPassword,  meshPort, WIFI_AP_STA, 6);

     
    mesh.onReceive(std::bind(&MeshNetwork::receivedCallback, this, std::placeholders::_1, std::placeholders::_2));
    mesh.onNewConnection(std::bind(&MeshNetwork::newConnectionCallback, this, std::placeholders::_1));
    mesh.onChangedConnections(std::bind(&MeshNetwork::changedConnectionCallback, this));

 
    prefs.begin(PREFS_NAMESPACE, false);
    root = prefs.getString("root", "");
    deviceId =  prefs.getString("deviceId", "");
    prefs.end();

    if (root.indexOf("true") != -1) {
        Serial.println("setting to root");
                
        mesh.stationManual("SSID", "PSW");
        mesh.setHostname(deviceId.c_str());
        mesh.setRoot(true);
    }
    
    mesh.setContainsRoot(true);

    scheduler.addTask(meshUpdateTask);
    meshUpdateTask.enable();

    Serial.println("Mesh initialized");
    Serial.print("Node ID: ");
    Serial.println(mesh.getNodeId());

    initialized = true;

    return true;
}

void MeshNetwork::update() {
  mesh.update();

  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
  }

  processMessage();
}

void MeshNetwork::sendMessage(const String& msg, unsigned long time) {
    ledgerMessage ldgMsg = {};
    if (msg.startsWith("LEDGER")) {
        String hash = ledger.calculateNewHash(msg);
        ldgMsg = {
            hashValue: hash,
            timeStamp: time,
            message: msg
        };
    } else {
        ldgMsg = {
            hashValue: "",
            timeStamp: time,
            message: msg
        };
    }

    String jsonString = serializeLedgerMessage(ldgMsg);
    Serial.print("sending message: ");
    Serial.println(jsonString);
    mesh.sendBroadcast(jsonString);
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}

uint32_t MeshNetwork::getNodeId() {
    return mesh.getNodeId();
}

size_t MeshNetwork::getConnectedNodes() {
    return connectedNodes.size();
}


void MeshNetwork::receivedCallback(uint32_t from, String &msg) {
    pendingMessage newMessage(from, msg);
    
    printf("Message from node %lu: %s\n", from, msg.c_str());
    
    pendingMessages.push_back(newMessage);
}

void MeshNetwork::processMessage() {
    while (!pendingMessages.empty()) {
        delay(300);
        pendingMessage message = pendingMessages.back();
        pendingMessages.pop_back();
        ledgerMessage receivedMsg = deserializeLedgerMessage(message.msg);

        if (receivedMsg.message.startsWith("TIME:")) {
            String timeStr = receivedMsg.message.substring(5);

            struct tm timeinfo = {0};
            if (strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &timeinfo)) {
                time_t epochTime = mktime(&timeinfo);
                struct timeval tv = { .tv_sec = epochTime, .tv_usec = 0 };
                settimeofday(&tv, NULL);

                if (getLocalTime(&timeinfo)) {
                    char timeStringBuff[50];
                    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
                    Serial.printf("Time set successfully to: %s\n", timeStringBuff);
                } else {
                    Serial.println("Failed to set time");
                }
            } else {
                Serial.println("Failed to parse time string");
            }
        } else if (receivedMsg.message.startsWith("IDENTITY:")) {
            processIdentityMessage(message.from, message.msg);
        } else if (receivedMsg.message.startsWith("LEDGER:")) {
            processLedgerMessage(message.from, message.msg);
        } else if (receivedMsg.message.startsWith("VALIDATE:")) {
            validate();
        } else if (receivedMsg.message.startsWith("VALIDATED:")) {
            checkForMyValidation(message.msg);
        } else if (receivedMsg.message.startsWith("VALIDATING:")) {
            validateOthers(message.from, message.msg);
        } else if (receivedMsg.message.startsWith("VALIDATIONFAILED")) {
            validatationFailed(message.from);
        } else if (receivedMsg.message.startsWith("UPDATE")) {
            if (root.indexOf("false") != -1) {
                mesh.initOTAReceive("otareceiver");
            }
        }
    }
}

void MeshNetwork::newConnectionCallback(uint32_t nodeId) {
    Serial.printf("New Connection, nodeId = %u\n", nodeId);
    connectedNodes.push_back(nodeId);
}

void MeshNetwork::changedConnectionCallback() {
    Serial.printf("Changed connections\n");
    auto nodes = mesh.getNodeList();
    connectedNodes.clear();
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        connectedNodes.push_back(*it);
    }
}

void MeshNetwork::nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void MeshNetwork::processIdentityMessage(uint32_t from, String& msg) {
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)) {
        char timeStringBuff[50];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
        
        String meshMessage = "TIME:" + String(timeStringBuff);
        
        sendMessage(meshMessage, 0);
        
        Serial.print("Sent time to mesh network: ");
        Serial.println(meshMessage);
    } else {
        Serial.println("Failed to obtain time");
    }
}

void MeshNetwork::processLedgerMessage(uint32_t from, String& msg) {
    Serial.println("Checking for ledger");
    ledgerMessage receivedMsg = deserializeLedgerMessage(msg);

    if (std::find(validNodes.begin(), validNodes.end(), String(from)) != validNodes.end()) {
        ledger.addEntry(receivedMsg.hashValue, receivedMsg.timeStamp, receivedMsg.message);
        Serial.println("Ledger entry added");
    } else {
        Serial.println("Failed to add to ledger");
    }
}

String MeshNetwork::calculateUniqueTimestampedFirmwareHash(long setTime) {
    Serial.println("Starting firmware hash calculation...");
    
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        Serial.println("Failed to get running partition");
        return "";
    }

    uint8_t hash[32];
    esp_err_t result;
    
    result = esp_partition_get_sha256(running, hash);
    if (result != ESP_OK) {
        Serial.println("Failed to calculate partition hash");
        return "";
    }
    
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    
    mbedtls_md_update(&ctx, hash, 32);
    
    time_t now;
    if (setTime != 0) {
        now = setTime;
    } else {
        time(&now);
    }
    Serial.printf("This is the time now: %lu\n", now);
    mbedtls_md_update(&ctx, (uint8_t*)&now, sizeof(now));

    const char* secretChars = secretValue.c_str();
    mbedtls_md_update(&ctx, (const unsigned char*)secretChars, strlen(secretChars));
    
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    Serial.printf("\nRunning partition: %s\n", running->label);
    Serial.printf("Partition size: %u bytes\n", running->size);
    
    String hashString;
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        hashString += hex;
    }

    Serial.print("Firmware SHA-256: ");
    Serial.println(hashString);
    return hashString;
}

void MeshNetwork::validate() {
    int randomDelay = random(100, 1000);
    Serial.printf("Delaying for %d ms\n", randomDelay);
    vTaskDelay(pdMS_TO_TICKS(randomDelay));

    time_t now;
    unsigned long timeNow = time(&now);
    String hashResult = "VALIDATING:" + calculateUniqueTimestampedFirmwareHash(timeNow);
    sendMessage(hashResult, timeNow);
}

void MeshNetwork::checkForMyValidation(String &msg) {
    Serial.println("Checking if im valid");
    ledgerMessage receivedMsg = deserializeLedgerMessage(msg);
    String content = receivedMsg.message.substring(10);

    if (content.equals(String(getNodeId()))) {
        validationMessages++;
        if (validationMessages >= consensusNumber) {
            sendMessage("LEDGER:", 0);
        }
    }
}

void MeshNetwork::validateOthers(uint32_t from, String &msg) {
    Serial.println("Validating others");
    ledgerMessage receivedMsg = deserializeLedgerMessage(msg);
    String content = receivedMsg.message.substring(11); 

    String hashValue = content;
    unsigned long timestampValue = receivedMsg.timeStamp;
    
    Serial.printf("From Node %u - Hash: %s, Timestamp: %lu\n", from, hashValue.c_str(), timestampValue);

    String hashResult = calculateUniqueTimestampedFirmwareHash(timestampValue);

    if (hashResult.equals(hashValue)) {
        String result = "VALIDATED:" + String(from);
        validNodes.push_back(String(from));
        sendMessage(result, 0);
    } else {
        String result = "VALIDATIONFAILED:" + String(from);
        sendMessage(result, 0);
    }
}

void MeshNetwork::validatationFailed(uint32_t from) {
    // ledger.addEntry()
}

bool MeshNetwork::isInitialized() const { return initialized; }

String MeshNetwork::serializeLedgerMessage(ledgerMessage& msg) {
    StaticJsonDocument<512> doc; 
    
    doc["hash"] = msg.hashValue;
    doc["time"] = msg.timeStamp;
    doc["msg"] = msg.message;
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

ledgerMessage MeshNetwork::deserializeLedgerMessage(String& jsonString) {
    StaticJsonDocument<512> doc;
    ledgerMessage msg;
    
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return msg;
    }
    
    msg.hashValue = doc["hash"].as<String>();
    msg.timeStamp = doc["time"].as<long>();
    msg.message = doc["msg"].as<String>();
    
    return msg;
}

void MeshNetwork::startOTA() {
    if(!SPIFFS.begin()){
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    Serial.println("Starting OTA");

    File firmware = SPIFFS.open("/firmware_ESP32_otareceiver.bin", "r");
    if(!firmware) {
        Serial.println("Failed to open firmware file");
        return;
    }
    
    Serial.printf("Firmware file opened, size: %d bytes\n", firmware.size());

    mesh.initOTASend(
        [&firmware](painlessmesh::plugin::ota::DataRequest pkg, char* buffer) {
            firmware.seek(OTA_PART_SIZE * pkg.partNo);
            size_t read = firmware.readBytes(buffer, OTA_PART_SIZE);
            return read;
        },
        OTA_PART_SIZE
    );

    MD5Builder md5;
    md5.begin();
    md5.addStream(firmware, firmware.size());
    md5.calculate();

    mesh.offerOTA("otareceiver", "ESP32", md5.toString(),
                  ceil(((float)firmware.size()) / OTA_PART_SIZE), false);

    while (true) {
        mesh.update();
        usleep(1000);
    }
}
