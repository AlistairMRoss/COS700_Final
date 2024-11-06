#include "WifiFunctions.h"

WifiFunctions* WifiFunctions::instance = nullptr;

WifiFunctions::WifiFunctions(MeshNetwork* meshNetwork, Scheduler& _scheduler) 
    : connected(false), WiFiOffline(true), last_connected(millis()), 
      last_force_connected(0), last_query_time(0), queried(false),
      awsFunctions(staticAwsMessageCallback), meshNetwork(meshNetwork), scheduler(_scheduler),
      wifiCheckTask(TASK_SECOND * 5, TASK_FOREVER, [this]() { this->loop(); }) {
    instance = this;
}

void WifiFunctions::setup() {
    delay(1000);
    validateTime = millis();
    
    prefs.begin(PREFS_NAMESPACE, false);
    String root = prefs.getString("root", "");

    prefs.end();

    if (root.indexOf("true") != -1) {
        WiFi.onEvent(std::bind(&WifiFunctions::WiFiGotIP, this, std::placeholders::_1, std::placeholders::_2), 
                    WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
        WiFi.onEvent(std::bind(&WifiFunctions::WiFiDisconnected, this, std::placeholders::_1, std::placeholders::_2), 
                    WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    }

    if (meshNetwork && !meshNetwork->isInitialized()) {
        Serial.println("Initializing mesh network first...");
        while(!meshNetwork->init()){}
    }

    while (!allNodesConnected) {
        meshNetwork->update();
        if (meshNetwork->getConnectedNodes() >= REQUIRED_NODES - 1) {
            allNodesConnected = true;
            Serial.println("All nodes connected!");
        }
        delay(100);
    }
    Serial.println("WiFi Setup");
    if (root.indexOf("true") != -1) {
        scheduler.addTask(wifiCheckTask);
        wifiCheckTask.enable();
    }
}

void WifiFunctions::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (last_connected + REBOOT_INTERVAL < millis()) {
            Serial.println("Rebooting");
            ESP.restart();
        }
        if (last_force_connected + FORCE_CONNECT_INTERVAL < millis()) {
            connectToWiFi();
        }
    } else {
        if (!awsFunctions.getAwsConnected()) {
            Serial.println("attempting MQTT");
            if (awsFunctions.connectAWS()) {
                Serial.println("AWS connection successful!");
                delay(500);
            } else {
                Serial.println("AWS connection failed!");
            }
        } else {
            awsFunctions.checkAWS();
        }
        if ((millis() - validateTime) < START_VALIDATION && validate) {
            meshNetwork->sendMessage("VALIDATE:", 0);
            vTaskDelay(1000);
            meshNetwork->validate();
            validate = false;
        }
    }

    if (updateMessageSent && (millis() - updateStart < UPDATE_START_INTERVAL)) {
        meshNetwork->startOTA();
        updateMessageSent =  false;
    }

}

void WifiFunctions::queryEndpoint() {
    Serial.println("Mounting SPIFFS...");
    
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed. Trying to format...");
        if (SPIFFS.format()) {
            Serial.println("SPIFFS formatted successfully");
            if (!SPIFFS.begin(true)) {
                Serial.println("SPIFFS mount failed even after formatting");
                return;
            }
        } else {
            Serial.println("SPIFFS format failed");
            return;
        }
    }
    
    HTTPClient http;

    Serial.println("Starting firmware download...");


    if (!http.begin(firmware_url)) {
        Serial.println("Failed to begin HTTP connection");
    }
    
    http.addHeader("Accept", "application/octet-stream");
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);

    int httpCode = http.GET();
    Serial.print("This is the http code: ");
    Serial.println(httpCode);
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed, error: %s\n", 
            http.errorToString(httpCode).c_str());
        http.end();
        Serial.println("error getting file");
        return;
    }

    int totalSize = http.getSize();
    if (totalSize <= 0) {
        Serial.println("Error: Invalid firmware size");
        http.end();
        return;
    }

    File firmwareFile = SPIFFS.open("/firmware_ESP32_otareceiver.bin", "w");
    if (!firmwareFile) {
        Serial.println("Failed to open file for writing");
        http.end();
        return;
    }

    WiFiClient * stream = http.getStreamPtr();
    
    uint8_t buff[1024] = { 0 };
    int bytesWritten = 0;
    
    while (http.connected() && (bytesWritten < totalSize)) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            if (firmwareFile.write(buff, c) != c) {
                Serial.println("Failed to write to file");
                firmwareFile.close();
                http.end();
                return;
            }
            
            bytesWritten += c;
            
            Serial.printf("Downloaded %d of %d bytes\n", bytesWritten, totalSize);
        }
        delay(10);
    }

    Serial.println("Firmware download complete!");
    Serial.printf("Total bytes written: %d\n", bytesWritten);

    firmwareFile.close();
    http.end();

    File verifyFile = SPIFFS.open("/firmware_ESP32_otareceiver.bin", "r");
    if (verifyFile) {
        int fileSize = verifyFile.size();
        verifyFile.close();
        if (fileSize == totalSize) {
            Serial.println("File size verification successful");
        } else {
            Serial.printf("File size mismatch. Expected: %d, Got: %d\n", totalSize, fileSize);
        }
    }
    if (verifyFirmwareHash()) {
        Serial.println("Firmware validation good");
        meshNetwork->sendMessage("UPDATE", 0);
        updateMessageSent = true;
        updateStart = millis();
    } else {
        Serial.println("FIRMWARE VALIDATION ERROR");
        Serial.println("sending message back to aws");
        awsFunctions.publishMessage("bad firmware hash");
    }

}

bool WifiFunctions::verifyFirmwareHash() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS for hash verification");
        return false;
    }

    File firmwareFile = SPIFFS.open("/firmware_ESP32_otareceiver.bin", "r");
    if (!firmwareFile) {
        Serial.println("Failed to open firmware file for hash verification");
        return false;
    }

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);

    uint8_t buff[1024];
    size_t bytesRead;
    while ((bytesRead = firmwareFile.read(buff, sizeof(buff))) > 0) {
        mbedtls_md_update(&ctx, (const unsigned char*)buff, bytesRead);
    }

    unsigned char shaResult[32];
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);
    
    firmwareFile.close();

    char calculatedHash[65];
    for(int i = 0; i < 32; i++) {
        sprintf(&calculatedHash[i * 2], "%02x", (int)shaResult[i]);
    }
    calculatedHash[64] = '\0';

    bool hashesMatch = (strcmp(calculatedHash, updateHash) == 0);
    
    Serial.println("Calculated hash: " + String(calculatedHash));
    Serial.println("Received hash:   " + String(updateHash));
    Serial.println(hashesMatch ? "Hash verification SUCCESS" : "Hash verification FAILED");

    return hashesMatch;
}

bool WifiFunctions::checkWifi() {
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < WIFI_MAX_RECONNECT_TRIES) {
        Serial.print(".");
        retries++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return false;
    } else {
        return true;
    }
}

bool WifiFunctions::getConnected() {
    return connected;
}

void WifiFunctions::connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    connected = true;
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    connected = false;
    Serial.println("\nFailed to connect to WiFi");
  }
}

void WifiFunctions::WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("WiFi Connected: ");
    last_connected = millis();
    connected = true;
    // Configure and get time from NTP
    configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER);
    
    struct tm timeinfo;
    int retry = 0;
    const int maxRetries = 10;
    
    while(!getLocalTime(&timeinfo) && retry < maxRetries) {
        Serial.println("Waiting for NTP time sync...");
        delay(1000);
        retry++;
    }
    
    if (retry < maxRetries) {
        char timeStringBuff[50];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.print("Current time: ");
        Serial.println(timeStringBuff);
        
        String meshMessage = "TIME:" + String(timeStringBuff);
        meshNetwork->sendMessage(meshMessage, 0);
        Serial.print("Broadcasting time to mesh: ");
        Serial.println(meshMessage);
    } else {
        Serial.println("Failed to obtain time");
    }
    
    WiFiOffline = false;
    Serial.print("These are the connected nodes: ");
    Serial.println(meshNetwork->getConnectedNodes());
    char str[100];
    sprintf(str, "Connected to %s @ %i dBm", WiFi.SSID().c_str(), WiFi.RSSI());
    Serial.println(str);
}

String WifiFunctions::getCurrentTime() {
    if (!connected) {
        return "WiFi not connected";
    }

    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return "Failed to obtain time";
    }

    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

void WifiFunctions::WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("WiFi Disconnected: ");
    if (info.wifi_sta_disconnected.reason == 15) {
        Serial.println("Wrong Password");
    } else if (info.wifi_sta_disconnected.reason == 201) {
        Serial.println("No AP Found");
    }
    if (!WiFiOffline) {
        WiFiOffline = true;
        connected = false;
        Serial.println(info.wifi_sta_disconnected.reason);
        WiFi.reconnect();
    }
}

void WifiFunctions::staticAwsMessageCallback(char* topic, byte* payload, unsigned int length) {
    if (instance) {
        instance->awsMessageCallback(topic, payload, length);
    }
}

void WifiFunctions::awsMessageCallback(char* topic, byte* payload, unsigned int length) {
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println(message);

    StaticJsonDocument<100> doc;

    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }
    
    const char* command = doc["command"];
    Serial.println(command);

    if (strcmp(command, "update") == 0){
        const char* hash = doc["hash"];
        updateHash = hash;
        Serial.println("Update command received. Querying endpoint...");
        printf("This is the hash %s\n", updateHash);
        queryEndpoint();
    } else if (strcmp(command, "updateSent") == 0) {
        const char* deviceId = doc["deviceId"];
        updateDevice = deviceId;
        Serial.println("Update device Id command received. Waiting for distribution");
    }
}