#ifndef MESH_NETWORK_H
#define MESH_NETWORK_H

#define FIRMWARE_VERSION 1

#include <painlessMesh.h>
#include <vector>
#include "Ledger.h"
#include <mbedtls/md.h>
#include <time.h>
#include <Update.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <ArduinoJson.h>
#include <algorithm>
#define OTA_PART_SIZE 1024
#define REQUIRED_NODES 3

#define PREFS_NAMESPACE "cos700"

struct ledgerMessage {
    String hashValue;
    unsigned long timeStamp;
    String message;
};

struct pendingMessage {
    uint32_t from;
    String msg;

    pendingMessage(uint32_t sender, String& message)
        : from(sender)
        , msg(message)
    {}
};

class Ledger;
class MeshNetwork {
public:
    MeshNetwork(const char* prefix, const char* password, uint16_t port, Scheduler& scheduler);
    bool init();
    void update();
    void sendMessage(const String& msg, unsigned long time);
    uint32_t getNodeId();
    size_t getConnectedNodes();

    bool isInitialized() const;

    void startOTA();
    String calculateUniqueTimestampedFirmwareHash(long time = 0);

    void validate();
    void processMessage();

    
private:
    void receivedCallback(uint32_t from, String &msg);
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionCallback();
    void nodeTimeAdjustedCallback(int32_t offset);
    void processIdentityMessage(uint32_t from, String& msg);
    void processLedgerMessage(uint32_t from, String& msg);
    void checkForMyValidation(String &msg);
    void validateOthers(uint32_t from, String &msg);
    void validatationFailed(uint32_t from);
    bool initialized;
    std::vector<pendingMessage> pendingMessages;
    painlessMesh mesh;
    const char* meshPrefix;
    const char* meshPassword;
    uint16_t meshPort;
    std::vector<uint32_t> connectedNodes;

    Scheduler& scheduler;
    Task meshUpdateTask;
    Preferences prefs;

    const size_t CHUNK_SIZE = 512;
    String incomingFirmwareFilename;
    std::vector<String> firmwareChunks;
    size_t expectedChunks;
    size_t receivedChunks;

    bool validated;

    String serializeLedgerMessage(ledgerMessage& msg);
    ledgerMessage deserializeLedgerMessage(String& jsonString);

    String root;
    String deviceId;

    int consensusNumber = floor(REQUIRED_NODES * 0.6);
    int validationMessages = 0;

    std::vector<String> validNodes;


    String secretValue = "This is a secret";
};



extern Ledger ledger;

#endif