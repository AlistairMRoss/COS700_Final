#include "Ledger.h"

void Ledger::init() {}

void Ledger::addEntry(const String& hash, const long& timestamp, const String& message) {
    ledgerItem newItem = {hash, timestamp, message};
    entries.push_back(newItem);
    if (entries.size() > MAX_ENTRIES) {
        entries.erase(entries.begin());
    }
}

ledgerItem Ledger::getLatestEntry() const {
    if (!entries.empty()) {
        return entries.back();
    }
    return {"", 0, ""};
}

std::vector<ledgerItem> Ledger::getAllEntries() const {
    return entries;
}

String Ledger::calculateNewHash(const String& message) {
    String previousHash = getLatestEntry().hash;
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);

    mbedtls_md_update(&ctx, (uint8_t*)previousHash.c_str(), previousHash.length());

    
    mbedtls_md_update(&ctx, (uint8_t*)message.c_str(), message.length());

    uint8_t hash[32];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    String hashString;
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        hashString += hex;
    }
    
    return hashString;
}