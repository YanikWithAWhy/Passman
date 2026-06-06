#ifndef PASSWORD_DATABASE_H
#define PASSWORD_DATABASE_H
#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

#include "SecureMemory.h"
#include "AuditLog.h"

extern "C" {
#include <sodium.h>
#include <oqs/oqs.h>
}

static constexpr uint64_t ARGON_OPS = 4;
static constexpr size_t   ARGON_MEM = 512ULL * 1024 * 1024;

static constexpr size_t PQ_KEM_SK_LEN = 2400;
static constexpr size_t PQ_KEM_CT_LEN = 1088;

static constexpr size_t PQ_NONCE_LEN = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES; // 24
static constexpr size_t PQ_TAG_LEN   = crypto_aead_xchacha20poly1305_ietf_ABYTES;    // 16
static constexpr size_t PQ_KEY_LEN   = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;  // 32

static constexpr size_t PQ_AAD_LEN =
    crypto_pwhash_SALTBYTES        // 16
    + PQ_NONCE_LEN                 // 24
    + (PQ_KEM_SK_LEN + PQ_TAG_LEN) // 2416
    + PQ_KEM_CT_LEN;               // 1088

static constexpr size_t MAX_FIELD_TITLE    =  512;
static constexpr size_t MAX_FIELD_USERNAME = 1024;
static constexpr size_t MAX_FIELD_PASSWORD = 1024;
static constexpr size_t MAX_FIELD_NOTES    = 8192;
static constexpr size_t MAX_FIELD_URL      = 2048;

struct PasswordEntry {
    std::string title, username, password, notes, url;
    uint64_t    created = 0, modified = 0;

    std::string serialize() const;

    bool deserialize(const std::string& line);

    void wipe() noexcept {
        auto w = [](std::string& s) {
            if (!s.empty()) { sodium_memzero(&s[0], s.size()); s.clear(); }
        };
        w(title); w(username); w(password); w(notes); w(url);
    }

    ~PasswordEntry() { wipe(); }

    PasswordEntry() = default;
    PasswordEntry(const PasswordEntry&) = default;
    PasswordEntry& operator=(const PasswordEntry&) = default;
    PasswordEntry(PasswordEntry&&) = default;
    PasswordEntry& operator=(PasswordEntry&&) = default;
};

class PasswordDatabase {
public:
    explicit PasswordDatabase(const std::string& filename);
    ~PasswordDatabase();

    bool createNewDatabase(const std::string& masterPassword);
    bool unlock(const std::string& masterPassword);
    bool save();
    void lock();

    bool addEntry(const PasswordEntry& entry);
    bool deleteEntry(size_t index);
    bool updateEntry(size_t index, const PasswordEntry& entry);

    const std::vector<PasswordEntry>& getEntries() const { return entries; }
    size_t size()       const { return entries.size(); }
    bool   isUnlocked() const { return unlocked; }

private:
    std::string filename;
    AuditLog    auditLog;
    std::vector<PasswordEntry> entries;
    bool unlocked = false;

    std::array<unsigned char, crypto_pwhash_SALTBYTES>     salt{};    // 16 B
    std::array<unsigned char, PQ_NONCE_LEN>                skNonce{}; // 24 B
    std::array<unsigned char, PQ_KEM_SK_LEN + PQ_TAG_LEN>  encSk{};   // 2416 B
    std::array<unsigned char, PQ_KEM_CT_LEN>               kemCt{};   // 1088 B

    unsigned char* dbKey     = nullptr;
    bool           keyLocked = false;

    std::vector<unsigned char> buildAAD() const;

    bool saveToFile(const SecureString& plaintext);
    void freeKey();
};

#endif // PASSWORD_DATABASE_H