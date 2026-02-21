#ifndef PASSWORD_DATABASE_H
#define PASSWORD_DATABASE_H

#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
extern "C" {
#include <sodium.h>
}

struct PasswordEntry {
    std::string title, username, password, notes, url;
    uint64_t created, modified;

    PasswordEntry() : created(0), modified(0) {}
    std::string serialize() const;
    bool deserialize(const std::string& line);
};

class PasswordDatabase {
public:
    PasswordDatabase(const std::string& filename);

    bool createNewDatabase(const std::string& masterPassword);

    bool unlock(const std::string& masterPassword);
    bool save();
    bool addEntry(const PasswordEntry& entry);
    bool deleteEntry(size_t index);
    const std::vector<PasswordEntry>& getEntries() const { return entries; }
    size_t size() const { return entries.size(); }
    bool isUnlocked() const { return unlocked; }
    void lock();
    bool updateEntry(size_t index, const PasswordEntry& entry);

    ~PasswordDatabase();

private:
    std::vector<PasswordEntry> entries;
    unsigned char* key;
    std::array<unsigned char, crypto_pwhash_SALTBYTES> salt;
    bool unlocked = false;
    std::string filename;
    bool keyLocked = false;

    bool deriveKey(const std::string& password);
    bool cryptoSave();
    void lockKey();
    void unlockKey();
};

#endif
