#include <Arduino.h>
#include "MeshNetwork.h"
#include "Ledger.h"
#include "WifiFunctions.h"

const size_t CHUNK_SIZE = 256;

#define MESH_PREFIX     "join"
#define MESH_PASSWORD   "12345678"
#define MESH_PORT       5555

Scheduler scheduler;
MeshNetwork meshNetwork(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, scheduler);
Ledger ledger;
WifiFunctions wifiFunctions(&meshNetwork, scheduler);

void setup() {
    Serial.begin(115200);
    delay(3000);
    // meshNetwork.calculateUniqueTimestampedFirmwareHash(3);
    wifiFunctions.setup();

    Serial.println("Setting up ledger");
    ledger.init();
    
    
    // String identityMessage = "IDENTITY:" + String(meshNetwork.getNodeId());
    // meshNetwork.sendMessage(identityMessage, 0);
}





void loop() {
    scheduler.execute();
}

// check spiffs

// #include "SPIFFS.h"

// void setup() {
//   Serial.begin(115200);
//   while (!Serial) {
//     ; // Wait for serial port to connect
//   }
  
//   Serial.println("\nInitializing SPIFFS...");
  
//   if (!SPIFFS.begin(true)) {
//     Serial.println("SPIFFS Mount Failed");
//     return;
//   }
  
//   Serial.println("SPIFFS Mount Successful");
  
//   File root = SPIFFS.open("/");
//   if (!root) {
//     Serial.println("Failed to open directory");
//     return;
//   }
//   if (!root.isDirectory()) {
//     Serial.println("Not a directory");
//     return;
//   }
  
//   File file = root.openNextFile();
  
//   Serial.println("\nSPIFFS Contents:");
//   Serial.println("---------------");
  
//   while (file) {
//     Serial.print("File: ");
//     Serial.print(file.name());
//     Serial.print("\tSize: ");
//     Serial.print(file.size());
//     Serial.println(" bytes");
    
//     file = root.openNextFile();
//   }
  
//   // Print total space information
//   unsigned int totalSpace = SPIFFS.totalBytes();
//   unsigned int usedSpace = SPIFFS.usedBytes();
  
//   Serial.println("\nStorage Information:");
//   Serial.println("-------------------");
//   Serial.print("Total space: ");
//   Serial.print(totalSpace);
//   Serial.println(" bytes");
//   Serial.print("Used space: ");
//   Serial.print(usedSpace);
//   Serial.println(" bytes");
//   Serial.print("Free space: ");
//   Serial.print(totalSpace - usedSpace);
//   Serial.println(" bytes");
// }

// void loop() {}

// clear spiffs

// #include "SPIFFS.h"

// void clearSpiffs() {
//     if(!SPIFFS.begin(true)) {
//         Serial.println("SPIFFS Mount Failed");
//         return;
//     }
    
//     Serial.println("Starting SPIFFS format...");
    
//     if(SPIFFS.format()) {
//         Serial.println("SPIFFS formatted successfully");
//     } else {
//         Serial.println("SPIFFS format failed");
//     }
    
//     SPIFFS.end();
//     Serial.println("SPIFFS unmounted");
// }

// #include <Update.h>
// #include <SPIFFS.h>
// #include <esp_ota_ops.h>
// #include <mbedtls/md.h>


// String calculateHashFromOTAPartition(long setTime = 0) {
//     Serial.println("Starting OTA partition hash experiment...");
    
//     if (!SPIFFS.begin(true)) {
//         Serial.println("Failed to mount SPIFFS");
//         return "";
//     }

//     // Open firmware file from SPIFFS
//     File file = SPIFFS.open("/firmware_ESP32_otareceiver.bin", "r");
//     if (!file) {
//         Serial.println("Failed to open firmware file");
//         return "";
//     }

//     // Get the next OTA partition
//     const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
//     if (!update_partition) {
//         Serial.println("Failed to get update partition");
//         return "";
//     }

//     Serial.printf("Writing to partition subtype %d at offset 0x%x\n",
//                  update_partition->subtype, update_partition->address);

//     // Erase the partition first
//     if (esp_partition_erase_range(update_partition, 0, update_partition->size) != ESP_OK) {
//         Serial.println("Failed to erase partition");
//         return "";
//     }

//     // Read and write the firmware file directly to the partition
//     size_t fileSize = file.size();
//     size_t bytesWritten = 0;
//     uint8_t buffer[1024];
    
//     while (bytesWritten < fileSize) {
//         size_t bytesToRead = min((size_t)1024, fileSize - bytesWritten);
//         size_t bytesRead = file.read(buffer, bytesToRead);
        
//         if (bytesRead == 0) break;
        
//         if (esp_partition_write(update_partition, bytesWritten, buffer, bytesRead) != ESP_OK) {
//             Serial.println("Failed to write to partition");
//             file.close();
//             return "";
//         }
        
//         bytesWritten += bytesRead;
//     }

//     file.close();

//     if (bytesWritten != fileSize) {
//         Serial.println("Failed to write all data");
//         return "";
//     }

//     // Calculate SHA-256 hash of the partition
//     uint8_t hash[32];
//     esp_err_t result;
    
//     if (setTime != 0) {
//         // First get the partition hash
//         result = esp_partition_get_sha256(update_partition, hash);
//         if (result != ESP_OK) {
//             Serial.println("Failed to calculate partition hash");
//             return "";
//         }
        
//         // Create a new SHA-256 context to include the timestamp
//         mbedtls_md_context_t ctx;
//         mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
        
//         mbedtls_md_init(&ctx);
//         mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
//         mbedtls_md_starts(&ctx);
        
//         // Include the partition hash
//         mbedtls_md_update(&ctx, hash, 32);
        
//         // Add timestamp
//         time_t now = setTime;
//         Serial.printf("This is the time now: %lu\n", now);
//         mbedtls_md_update(&ctx, (uint8_t*)&now, sizeof(now));
        
//         // Get final hash
//         mbedtls_md_finish(&ctx, hash);
//         mbedtls_md_free(&ctx);
//     } else {
//         result = esp_partition_get_sha256(update_partition, hash);
//         if (result != ESP_OK) {
//             Serial.println("Failed to calculate partition hash");
//             return "";
//         }
//     }

//     // Convert hash to string
//     String hashString;
//     for (int i = 0; i < 32; i++) {
//         char hex[3];
//         sprintf(hex, "%02x", hash[i]);
//         hashString += hex;
//     }

//     Serial.printf("\nUpdate partition: %s\n", update_partition->label);
//     Serial.printf("Partition size: %u bytes\n", update_partition->size);
//     Serial.print("Calculated SHA-256: ");
//     Serial.println(hashString);

//     return hashString;
// }

// void setup() {
//     Serial.begin(115200);
//     delay(1000);
//     calculateHashFromOTAPartition(3);
// }

// void loop() {
//     // Main loop code here
//     delay(1000);
//     Serial.println("cool finished doing that onto the next thing");
// }

// #include <mbedtls/md.h>
// #include <SPIFFS.h>

// bool verifyFirmwareHash() {
//     if (!SPIFFS.begin(true)) {
//         Serial.println("Failed to mount SPIFFS for hash verification");
//         return false;
//     }

//     File firmwareFile = SPIFFS.open("/firmware_ESP32_otareceiver.bin", "r");
//     if (!firmwareFile) {
//         Serial.println("Failed to open firmware file for hash verification");
//         return false;
//     }

//     mbedtls_md_context_t ctx;
//     mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
//     mbedtls_md_init(&ctx);
//     mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
//     mbedtls_md_starts(&ctx);

//     uint8_t buff[1024];
//     size_t bytesRead;
//     while ((bytesRead = firmwareFile.read(buff, sizeof(buff))) > 0) {
//         mbedtls_md_update(&ctx, (const unsigned char*)buff, bytesRead);
//     }

//     unsigned char shaResult[32];
//     mbedtls_md_finish(&ctx, shaResult);
//     mbedtls_md_free(&ctx);
    
//     firmwareFile.close();

//     char calculatedHash[65];
//     for(int i = 0; i < 32; i++) {
//         sprintf(&calculatedHash[i * 2], "%02x", (int)shaResult[i]);
//     }
//     calculatedHash[64] = '\0';

//     // bool hashesMatch = (strcmp(calculatedHash, updateHash) == 0);
    
//     Serial.println("Calculated hash: " + String(calculatedHash));
//     // Serial.println("Received hash:   " + String(updateHash));
//     // Serial.println(hashesMatch ? "Hash verification SUCCESS" : "Hash verification FAILED");

//     return true;
// }

// void setup() {
//     Serial.begin(115200);
//     delay(2000);
//     verifyFirmwareHash();
// }

// void loop() {}
