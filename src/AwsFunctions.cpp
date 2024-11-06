#include "awsFunctions.h"

AwsFunctions::AwsFunctions(void (*messageHandler)(char*, byte*, unsigned int)) {
    client.setClient(net);
    client.setCallback(messageHandler);
    AWSConnected = false;
    last_connect_attempt = 0;
    retries = 0;
}

void AwsFunctions::publishMessage(const char *message) {
    if (!client.connected()) {
        Serial.println("Cannot publish - not connected");
        return;
    }
    
    StaticJsonDocument<512> doc;
    doc["reply"] = message;
    String pubtopic = TOPIC_BASE + deviceId + "/info";
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    
    Serial.print("Publishing to ");
    Serial.print(pubtopic);
    Serial.print(" message: ");
    Serial.println(jsonBuffer);
    
    if (!client.publish(pubtopic.c_str(), jsonBuffer)) {
        Serial.println("Publish failed");
    }
}

bool AwsFunctions::connectSubscribe() {
    if (client.connected()) {
        Serial.println("Already Connected");
        return true;
    }
    
    // Check if we need to wait before retrying
    if (millis() - last_connect_attempt < AWS_CONNECT_TIMEOUT) {
        delay(100);
        return false;
    }
    
    // Reset retries if we've hit the limit
    if (retries >= 5) {
        Serial.println("Max retries reached, resetting counter");
        retries = 0;
        delay(5000); // Additional delay before starting fresh
        return false;
    }
    
    Serial.println("Connecting to AWS IoT");
    last_connect_attempt = millis();
    
    if (client.connect(deviceId.c_str())) {
        Serial.println("AWS IoT Connected!");
        String subtopic = TOPIC_BASE + deviceId + "/commands";
        
        if (client.subscribe(subtopic.c_str())) {
            Serial.print("Subscribed to: ");
            Serial.println(subtopic);
            AWSConnected = true;
            publishMessage("Connected to AWS");
            return true;
        } else {
            Serial.println("Subscribe failed");
            client.disconnect();
            AWSConnected = false;
            return false;
        }
    }
    
    Serial.print("Connection failed, error code: ");
    Serial.println(client.state());
    AWSConnected = false;
    retries++;
    return false;
}

bool AwsFunctions::connectAWS() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return false;
    }
    
    prefs.begin(PREFS_NAMESPACE, false);
    deviceId = prefs.getString("deviceId", "");
    String cacert = prefs.getString("ca", "");
    String certificate = prefs.getString("cert", "");
    String privateKey = prefs.getString("private", "");
    String endPoint = prefs.getString("endPoint", "");
    prefs.end();
    
    if (deviceId.isEmpty() || certificate.isEmpty() || privateKey.isEmpty() || endPoint.isEmpty()) {
        Serial.println("Missing required AWS credentials");
        return false;
    }

    Serial.print("Device Id: ");
    Serial.println(deviceId);
    Serial.print("Endpoint: ");
    Serial.println(endPoint);

    // net.stop();
    // net.flush();
    // delay(100);

    net.setCACert(AWS_CERT_CA);
    net.setCertificate(certificate.c_str());
    net.setPrivateKey(privateKey.c_str());
    net.setTimeout(30000);

    client.setServer(endPoint.c_str(), 8883);
    client.setKeepAlive(60);
    
    return connectSubscribe();
}

bool AwsFunctions::getAwsConnected() {
    if (client.connected() != AWSConnected) {
        AWSConnected = client.connected();
    }
    return AWSConnected;
}

bool AwsFunctions::checkAWS() {
  if (client.loop()) {
    delay(100);
    return true;
  }
  else {
    return connectSubscribe();
  }
}