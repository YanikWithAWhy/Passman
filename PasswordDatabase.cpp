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

PasswordDatabase::PasswordDatabase(const std::string& file) : filename(file) {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }
}

bool PasswordDatabase::deriveKey(const std::string& password) {

    return crypto_pwhash(
        key.data(),
        crypto_secretbox_KEYBYTES,
        password.c_str(),
        password.size(),
        salt.data(),
        crypto_pwhash_OPSLIMIT_MODERATE,
        crypto_pwhash_MEMLIMIT_MODERATE,
        crypto_pwhash_ALG_DEFAULT
    ) == 0;
}

bool PasswordDatabase::unlock(const std::string& masterPassword) {

    randombytes_buf(salt.data(), salt.size());

    if (!deriveKey(masterPassword)) {
        return false;
    }

    unlocked = cryptoLoad();
    return unlocked;
}

bool PasswordDatabase::cryptoLoad() {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 74) return false;  // Header + Salt + Nonce

    file.seekg(0);
    char header[6];
    file.read(header, 6);
    if (memcmp(header, "PMDB\x02\x00", 6) != 0) return false;

    file.read((char*)salt.data(), salt.size());

    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    file.read((char*)nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());

    std::vector<unsigned char> decrypted(ciphertext.size());
    if (crypto_secretbox_open_easy(
        decrypted.data(),
        ciphertext.data(),
        ciphertext.size(),
        nonce.data(),
        key.data()
    ) != 0) {
        return false;
    }

    std::string content(reinterpret_cast<char*>(decrypted.data()));
    std::istringstream iss(content);
    std::string line;
    entries.clear();

    while (std::getline(iss, line)) {
        PasswordEntry entry;
        if (entry.deserialize(line)) {
            entries.push_back(entry);
        }
    }
    return true;
}

bool PasswordDatabase::cryptoSave() {

    std::string plaintext;
    for (const auto& entry : entries) {
        plaintext += entry.serialize();
    }

    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext(plaintext.size() + crypto_secretbox_MACBYTES);
    if (crypto_secretbox_easy(
        ciphertext.data(),
        reinterpret_cast<const unsigned char*>(plaintext.data()),
        plaintext.size(),
        nonce.data(),
        key.data()
    ) != 0) {
        return false;
    }

    std::ofstream file(filename, std::ios::binary);
    file.write("PMDB\x02\x00", 6);
    file.write((char*)salt.data(), salt.size());
    file.write((char*)nonce.data(), nonce.size());
    file.write((char*)ciphertext.data(), ciphertext.size());

    return file.good();
}

bool PasswordDatabase::save() {
    if (!unlocked) return false;
    return cryptoSave();
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
    entries.clear();
    unlocked = false;
}
