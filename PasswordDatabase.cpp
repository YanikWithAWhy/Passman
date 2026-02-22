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

bool PasswordEntry::deserialize(const std::string &line) {
    std::istringstream iss(line);
    return (iss >> title >> username >> password >> notes >> url
            >> created >> modified) && iss.eof();
}

PasswordDatabase::PasswordDatabase(const std::string &file) : filename(file), key(nullptr) {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }
}

bool PasswordDatabase::createNewDatabase(const std::string &masterPassword) {
    randombytes_buf(salt.data(), salt.size());

    if (!deriveKey(masterPassword)) {
        return false;
    }

    const std::string emptyData = "";

    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext(crypto_secretbox_MACBYTES);
    if (crypto_secretbox_easy(ciphertext.data(),
                              reinterpret_cast<const unsigned char *>(emptyData.data()),
                              emptyData.size(),
                              nonce.data(),
                              key) != 0) {
        unlockKey();
        sodium_free(key);
        key = nullptr;
        return false;
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        unlockKey();
        sodium_free(key);
        key = nullptr;
        return false;
    }

    file.write("PMDB\x02\x00", 6); // Header (6B)
    file.write((char *) salt.data(), salt.size()); // Salt (32B)
    file.write((char *) nonce.data(), nonce.size()); // Nonce (24B)
    file.write((char *) ciphertext.data(), ciphertext.size()); // MAC (16B)

    bool success = file.good();
    unlockKey();
    sodium_free(key);
    key = nullptr;

    return success;
}


bool PasswordDatabase::deriveKey(const std::string &password) {
    key = (unsigned char *) sodium_malloc(crypto_secretbox_KEYBYTES);
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

bool PasswordDatabase::unlock(const std::string &masterPassword) {
    printf("DEBUG: Trying to unlock %s\n", filename.c_str());

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("DEBUG: Could not open file: %s\n", filename.c_str());
        return false;
    }

    auto filesize = file.tellg();
    printf("DEBUG: File size: %lld bytes\n", (long long) filesize);

    if (filesize < 62) {
        printf("DEBUG: FILE TOO SMALL: %lld bytes\n", (long long) filesize);
        return false;
    }

    file.seekg(0);

    char header[6];
    if (!file.read(header, 6) || memcmp(header, "PMDB\x02\x00", 6) != 0) {
        printf("DEBUG: HEADER INVALID\n");
        return false;
    }
    printf("DEBUG: HEADER OK\n");

    if (!file.read((char *) salt.data(), salt.size())) {
        printf("DEBUG: SALT READ FAILED\n");
        return false;
    }
    printf("DEBUG: SALT LOADED\n");

    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    if (!file.read((char *) nonce.data(), nonce.size())) {
        printf("DEBUG: NONCE READ FAILED\n");
        return false;
    }
    printf("DEBUG: NONCE LOADED\n");

    std::vector<unsigned char> ciphertext((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
    printf("DEBUG: Ciphertext size: %zu bytes\n", ciphertext.size());

    if (ciphertext.size() < crypto_secretbox_MACBYTES) {
        printf("DEBUG: CIPHERTEXT TOO SMALL\n");
        return false;
    }

    if (!deriveKey(masterPassword)) {
        printf("DEBUG: KEY DERIVATION FAILED\n");
        return false;
    }
    printf("DEBUG: KEY DERIVED\n");

    std::vector<unsigned char> decrypted(ciphertext.size() - crypto_secretbox_MACBYTES);
    int result = crypto_secretbox_open_easy(
        decrypted.data(),
        ciphertext.data(),
        ciphertext.size(),
        nonce.data(),
        key
    );

    if (result != 0) {
        printf("DEBUG: DECRYPTION FAILED (wrong password/corrupted!) code=%d\n", result);
        unlockKey();
        sodium_free(key);
        key = nullptr;
        return false;
    }
    printf("DEBUG: DECRYPTION SUCCESS\n");

    std::string plaintext(reinterpret_cast<char *>(decrypted.data()), decrypted.size());
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
    printf("DEBUG: Loaded %zu entries\n", entries.size());

    unlocked = true;
    printf("DEBUG: UNLOCK SUCCESS\n");
    return true;
}

bool PasswordDatabase::cryptoSave() {
    if (!unlocked || !key) return false;

    std::string plaintext;
    for (const auto &entry: entries) {
        plaintext += entry.serialize();
    }

    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext(plaintext.size() + crypto_secretbox_MACBYTES);
    if (crypto_secretbox_easy(
            ciphertext.data(),
            reinterpret_cast<const unsigned char *>(plaintext.data()),
            plaintext.size(),
            nonce.data(),
            key
        ) != 0) {
        printf("DEBUG: ENCRYPTION FAILED\n");
        return false;
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        printf("DEBUG: CANNOT WRITE FILE\n");
        return false;
    }

    file.write("PMDB\x02\x00", 6);
    file.write((char *) salt.data(), salt.size());
    file.write((char *) nonce.data(), nonce.size());
    file.write((char *) ciphertext.data(), ciphertext.size());

    bool success = file.good();
    printf("DEBUG: SAVED: %zu bytes plaintext → %zu bytes ciphertext\n",
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
        printf("DEBUG: Cannot save: not unlocked\n");
        return false;
    }
    bool result = cryptoSave();

    return result;
}

bool PasswordDatabase::addEntry(const PasswordEntry &entry) {
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

bool PasswordDatabase::updateEntry(size_t index, const PasswordEntry &newEntry) {
    if (!unlocked || index >= entries.size()) return false;

    entries[index] = newEntry;
    entries[index].modified = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return true;
}

PasswordDatabase::~PasswordDatabase() {
    lock();
}
