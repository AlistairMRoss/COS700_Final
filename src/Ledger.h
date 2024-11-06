#ifndef LEDGER_H
#define LEDGER_H

#include <Arduino.h>
#include <vector>
#include <mbedtls/md.h>

struct ledgerItem {
    String hash;
    unsigned long timestamp;
    String message;
};

class Ledger {
public:
    void init();
    void addEntry(const String& hash, const long& timestamp, const String& message = "");
    ledgerItem getLatestEntry() const;
    std::vector<ledgerItem> getAllEntries() const;
    String calculateNewHash(const String& msg);
private:
    std::vector<ledgerItem> entries;
    const size_t MAX_ENTRIES = 50;
};

#endif