#ifndef WIFI_FUNCTIONS_H
#define WIFI_FUNCTIONS_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <time.h>
#include "awsFunctions.h"
#include "MeshNetwork.h"
#include <Base64.h>

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PSW"

#define PORTAL_TIMEOUT 180
#define AP_NAME "myDevice"
#define WIFI_MAX_RECONNECT_TRIES 25
#define CONNECT_TIMEOUT 10000
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET 3600
#define DAYLIGHT_OFFSET 3600
#define FORCE_CONNECT_INTERVAL 1000 * 60 * 5 
#define REBOOT_INTERVAL 1000 * 60 * 30
#define QUERY_INTERVAL 1000 * 60 * 60
#define PREFS_NAMESPACE "cos700"
#define REQUIRED_NODES 1
#define VALIDATION_TIME 604800000UL
#define UPDATE_START_INTERVAL 1000 * 3 * 60 // 3 min
#define START_VALIDATION 1000 * 2 * 60 // 3 min

class WifiFunctions {
public:
    WifiFunctions(MeshNetwork* meshNetwork, Scheduler& scheduler);
    void setup();
    void loop();
    bool checkWifi();
    bool getConnected();
    void queryEndpoint();
    String getCurrentTime();
    bool verifyFirmwareHash();

private:
    void connectToWiFi();
    void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
    void WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
 

    void awsMessageCallback(char* topic, byte* payload, unsigned int length);
    static WifiFunctions* instance;
    static void staticAwsMessageCallback(char* topic, byte* payload, unsigned int length);

    MeshNetwork* meshNetwork;

    Scheduler& scheduler;
    Task wifiCheckTask;
    bool allNodesConnected = false;

    bool connected;
    bool WiFiOffline;
    unsigned long last_connected;
    unsigned long last_force_connected;
    unsigned long last_query_time;
    bool queried;
    bool update = false;
    Preferences prefs;
    AwsFunctions awsFunctions;
    const char * updateHash;
    String updateDevice;
    bool initialValidation = false;
    unsigned long validateTime;

    bool validate = true;

    unsigned long updateStart;
    bool updateMessageSent = false;

    const String firmware_url = "https://bro6zaxrc1.execute-api.us-east-1.amazonaws.com/v1/firmware/getFirmware";

};

#endif