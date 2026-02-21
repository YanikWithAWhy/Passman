#include "PasswordDatabase.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <cstring>

std::string PasswordEntry::serialize() const {
    std::ostringstream oss;
    oss << title << '\t' << username << '\t' << password
        << '\t' << notes << '\t' << url << '\t'
        << created << '\t' << modified << '\n';
    return oss.str();
}

bool PasswordEntry::deserialize(const std::string& line) {
    std::istringstream iss(line);
    return (iss >> title >> username >> password >> notes >> url
            >> created >> modified) && iss.eof();
}

PasswordDatabase::PasswordDatabase(const std::string& file) : filename(file), key(nullptr) {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }
}

bool PasswordDatabase::createNewDatabase(const std::string& masterPassword) {
    // Salt generieren
    randombytes_buf(salt.data(), salt.size());

    // Key ableiten
    if (!deriveKey(masterPassword)) {
        return false;
    }

    // Leeren Plaintext (genau 0 Bytes)
    const std::string emptyData = "";

    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    randombytes_buf(nonce.data(), nonce.size());

    // Ciphertext = MAC (16 Bytes) für leeren Plaintext
    std::vector<unsigned char> ciphertext(crypto_secretbox_MACBYTES);
    if (crypto_secretbox_easy(ciphertext.data(),
                            reinterpret_cast<const unsigned char*>(emptyData.data()),
                            emptyData.size(),  // 0!
                            nonce.data(),
                            key) != 0) {
        unlockKey();
        sodium_free(key);
        key = nullptr;
        return false;
                            }

    // Datei schreiben
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        unlockKey();
        sodium_free(key);
        key = nullptr;
        return false;
    }

    file.write("PMDB\x02\x00", 6);                    // Header (6)
    file.write((char*)salt.data(), salt.size());      // Salt (32)
    file.write((char*)nonce.data(), nonce.size());    // Nonce (24)
    file.write((char*)ciphertext.data(), ciphertext.size()); // MAC (16)

    bool success = file.good();
    unlockKey();
    sodium_free(key);
    key = nullptr;

    return success;
}


bool PasswordDatabase::deriveKey(const std::string& password) {

    key = (unsigned char*)sodium_malloc(crypto_secretbox_KEYBYTES);
    if (!key) return false;

    bool success = crypto_pwhash(
        key,
        crypto_secretbox_KEYBYTES,
        password.c_str(),
        password.size(),
        salt.data(),
        crypto_pwhash_OPSLIMIT_MODERATE,
        crypto_pwhash_MEMLIMIT_MODERATE,
        crypto_pwhash_ALG_DEFAULT
    ) == 0;

    if (success) {
        lockKey();
    }
    return success;
}

bool PasswordDatabase::unlock(const std::string& masterPassword) {
    printf("DEBUG: Trying to unlock %s\n", filename.c_str());

    // Datei öffnen
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("❌ Could not open file: %s\n", filename.c_str());
        return false;
    }

    // Filesize korrekt ermitteln
    auto filesize = file.tellg();
    printf("File size: %lld bytes\n", (long long)filesize);

    if (filesize < 62) {  // 6+32+24 = 62 bytes minimum
        printf("❌ FILE TOO SMALL: %lld bytes\n", (long long)filesize);
        return false;
    }

    file.seekg(0);

    // HEADER prüfen (6 bytes)
    char header[6];
    if (!file.read(header, 6) || memcmp(header, "PMDB\x02\x00", 6) != 0) {
        printf("❌ HEADER INVALID\n");
        return false;
    }
    printf("✅ HEADER OK\n");

    // SALT laden (32 bytes)
    if (!file.read((char*)salt.data(), salt.size())) {
        printf("❌ SALT READ FAILED\n");
        return false;
    }
    printf("✅ SALT LOADED\n");

    // NONCE laden (24 bytes)
    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    if (!file.read((char*)nonce.data(), nonce.size())) {
        printf("❌ NONCE READ FAILED\n");
        return false;
    }
    printf("✅ NONCE LOADED\n");

    // Rest = Ciphertext (inkl. MAC)
    std::vector<unsigned char> ciphertext((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
    printf("Ciphertext size: %zu bytes\n", ciphertext.size());

    if (ciphertext.size() < crypto_secretbox_MACBYTES) {
        printf("❌ CIPHERTEXT TOO SMALL\n");
        return false;
    }

    // Key ableiten
    if (!deriveKey(masterPassword)) {
        printf("❌ KEY DERIVATION FAILED\n");
        return false;
    }
    printf("✅ KEY DERIVED\n");

    // Entschlüsseln
    std::vector<unsigned char> decrypted(ciphertext.size() - crypto_secretbox_MACBYTES);
    int result = crypto_secretbox_open_easy(
        decrypted.data(),
        ciphertext.data(),
        ciphertext.size(),
        nonce.data(),
        key
    );

    if (result != 0) {
        printf("❌ DECRYPTION FAILED (wrong password/corrupted!) code=%d\n", result);
        unlockKey();
        sodium_free(key);
        key = nullptr;
        return false;
    }
    printf("✅ DECRYPTION SUCCESS\n");

    // Plaintext parsen (leer ist OK)
    std::string plaintext(reinterpret_cast<char*>(decrypted.data()), decrypted.size());
    entries.clear();

    if (!plaintext.empty()) {
        std::istringstream iss(plaintext);
        std::string line;
        while (std::getline(iss, line)) {
            PasswordEntry entry;
            if (entry.deserialize(line)) {
                entries.push_back(entry);
            }
        }
    }
    printf("✅ Loaded %zu entries\n", entries.size());

    unlocked = true;
    printf("🎉 FULL UNLOCK SUCCESS\n");
    return true;
}

bool PasswordDatabase::cryptoSave() {

    if (!unlocked || !key) return false;

    // Plaintext serialisieren
    std::string plaintext;
    for (const auto& entry : entries) {
        plaintext += entry.serialize();
    }

    // **WICHTIG: NEUER NONCE pro Save!**
    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext(plaintext.size() + crypto_secretbox_MACBYTES);
    if (crypto_secretbox_easy(
        ciphertext.data(),
        reinterpret_cast<const unsigned char*>(plaintext.data()),
        plaintext.size(),
        nonce.data(),
        key
    ) != 0) {
        printf("❌ ENCRYPTION FAILED\n");
        return false;
    }

    // Datei überschreiben (Header + Salt + neuer Nonce + neuer Ciphertext)
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        printf("❌ CANNOT WRITE FILE\n");
        return false;
    }

    file.write("PMDB\x02\x00", 6);                    // Header bleibt
    file.write((char*)salt.data(), salt.size());      // Salt bleibt (Key-Konsistenz!)
    file.write((char*)nonce.data(), nonce.size());    // ← NEUER NONCE!
    file.write((char*)ciphertext.data(), ciphertext.size());

    bool success = file.good();
    printf("✅ SAVED: %zu bytes plaintext → %zu bytes ciphertext\n",
           plaintext.size(), ciphertext.size());

    return success;

}

void PasswordDatabase::lockKey() {
    if (!keyLocked && key != nullptr) {
        if (sodium_mlock(key, crypto_secretbox_KEYBYTES) == 0) {
            keyLocked = true;
        }
    }
}

void PasswordDatabase::unlockKey() {
    if (keyLocked && key != nullptr) {
        sodium_munlock(key, crypto_secretbox_KEYBYTES);
        keyLocked = false;
    }
}

bool PasswordDatabase::save() {
    if (!unlocked || !key) {
        printf("❌ Cannot save: not unlocked\n");
        return false;
    }
    bool result = cryptoSave();
    // NICHT unlockKey() hier aufrufen - Key muss für nächste Saves verfügbar bleiben!
    return result;
}

bool PasswordDatabase::addEntry(const PasswordEntry& entry) {
    if (!unlocked) return false;
    PasswordEntry e = entry;
    e.created = e.modified = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entries.push_back(e);
    return true;
}

bool PasswordDatabase::deleteEntry(size_t index) {
    if (!unlocked || index >= entries.size()) return false;
    entries.erase(entries.begin() + index);
    return true;
}

void PasswordDatabase::lock() {
    unlockKey();
    sodium_free(key);
    key = nullptr;
    entries.clear();
    unlocked = false;
}

PasswordDatabase::~PasswordDatabase() {
    lock();
}