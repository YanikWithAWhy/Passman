
#include "PasswordDatabase.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <aclapi.h>
#  include <io.h>
#else
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

static std::string escapeNewlines(const std::string& s) {
    std::string r; r.reserve(s.size() + 8);
    for (char c : s) r += (c == '\n') ? "\\n" : std::string(1, c);
    return r;
}
static std::string unescapeNewlines(const std::string& s) {
    std::string r; r.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { r += '\n'; ++i; }
        else r += s[i];
    }
    return r;
}

std::string PasswordEntry::serialize() const {
    std::ostringstream oss;
    oss << escapeNewlines(title)    << '\t' << escapeNewlines(username) << '\t'
        << escapeNewlines(password) << '\t' << escapeNewlines(notes)    << '\t'
        << escapeNewlines(url)      << '\t' << created << '\t' << modified << '\n';
    return oss.str();
}

bool PasswordEntry::deserialize(const std::string& line) {
    std::vector<std::string> f;
    f.reserve(7);
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == '\t') {
            f.push_back(line.substr(start, i - start));
            start = i + 1;
            if (f.size() > 7) return false;
        }
    }
    if (f.size() != 7) return false;

    std::string tTitle    = unescapeNewlines(f[0]);
    std::string tUser     = unescapeNewlines(f[1]);
    std::string tPass     = unescapeNewlines(f[2]);
    std::string tNotes    = unescapeNewlines(f[3]);
    std::string tUrl      = unescapeNewlines(f[4]);

    if (tTitle.size()    > MAX_FIELD_TITLE)    return false;
    if (tUser.size()     > MAX_FIELD_USERNAME) return false;
    if (tPass.size()     > MAX_FIELD_PASSWORD) return false;
    if (tNotes.size()    > MAX_FIELD_NOTES)    return false;
    if (tUrl.size()      > MAX_FIELD_URL)      return false;

    uint64_t tCreated = 0, tModified = 0;
    try {
        tCreated  = std::stoull(f[5]);
        tModified = std::stoull(f[6]);
    } catch (...) { return false; }

    if (tCreated == 0 || tModified == 0)    return false;
    if (tCreated > tModified)               return false;
    if (tCreated  > 7258118400ULL)          return false;
    if (tModified > 7258118400ULL)          return false;

    title    = std::move(tTitle);
    username = std::move(tUser);
    password = std::move(tPass);
    notes    = std::move(tNotes);
    url      = std::move(tUrl);
    created  = tCreated;
    modified = tModified;
    return true;
}

#ifdef _WIN32

static HANDLE createOwnerOnlyFile(const std::wstring& path) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return INVALID_HANDLE_VALUE;

    DWORD infoSize = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &infoSize);
    std::vector<BYTE> tokenInfo(infoSize);
    bool gotInfo = GetTokenInformation(token, TokenUser,
                                        tokenInfo.data(), infoSize, &infoSize);
    CloseHandle(token);
    if (!gotInfo) return INVALID_HANDLE_VALUE;

    PSID userSid = reinterpret_cast<PTOKEN_USER>(tokenInfo.data())->User.Sid;

    EXPLICIT_ACCESSW ea{};
    ea.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
    ea.grfAccessMode        = SET_ACCESS;
    ea.grfInheritance       = NO_INHERITANCE;
    ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType  = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName    = reinterpret_cast<LPWSTR>(userSid);

    PACL acl = nullptr;
    if (SetEntriesInAclW(1, &ea, nullptr, &acl) != ERROR_SUCCESS)
        return INVALID_HANDLE_VALUE;

    PSECURITY_DESCRIPTOR sd = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(sd, TRUE, acl, FALSE);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength              = sizeof(sa);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle       = FALSE;

    HANDLE h = CreateFileW(
        path.c_str(), GENERIC_WRITE, 0, &sa,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);

    LocalFree(acl);
    LocalFree(sd);
    return h;
}
#endif // _WIN32

static bool atomicWrite(const std::string& dest,
                        const unsigned char* data, size_t len)
{
    const std::string tmp = dest + ".tmp";

#ifdef _WIN32
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring w(n - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
        return w;
    };
    std::wstring tmpW  = toWide(tmp);
    std::wstring destW = toWide(dest);

    HANDLE h = createOwnerOnlyFile(tmpW);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    bool ok = WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr)
              && written == static_cast<DWORD>(len);
    FlushFileBuffers(h);
    CloseHandle(h);

    if (!ok) { DeleteFileW(tmpW.c_str()); return false; }

    if (!MoveFileExW(tmpW.c_str(), destW.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmpW.c_str());
        return false;
    }
    return true;

#else
    int fd = open(tmp.c_str(),
                  O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC,
                  S_IRUSR | S_IWUSR);
    if (fd < 0) return false;

    bool ok = true;
    size_t pos = 0;
    while (pos < len) {
        ssize_t n = write(fd, data + pos, len - pos);
        if (n <= 0) { ok = false; break; }
        pos += static_cast<size_t>(n);
    }
    if (ok) ok = (fsync(fd) == 0);
    close(fd);

    if (!ok) { ::unlink(tmp.c_str()); return false; }
    if (::rename(tmp.c_str(), dest.c_str()) != 0) {
        ::unlink(tmp.c_str()); return false;
    }
    return true;
#endif
}

PasswordDatabase::PasswordDatabase(const std::string& file)
    : filename(file), auditLog(file)
{
    if (sodium_init() < 0)
        throw std::runtime_error("libsodium initialisation failed");
    OQS_init();
}

PasswordDatabase::~PasswordDatabase() { lock(); OQS_destroy(); }

void PasswordDatabase::freeKey() {
    if (dbKey) {
        if (keyLocked) { sodium_munlock(dbKey, PQ_KEY_LEN); keyLocked = false; }
        sodium_free(dbKey);
        dbKey = nullptr;
    }
}

void PasswordDatabase::lock() {
    if (unlocked) auditLog.logLock();
    freeKey();
    sodium_memzero(skNonce.data(), skNonce.size());
    sodium_memzero(encSk.data(),   encSk.size());
    sodium_memzero(kemCt.data(),   kemCt.size());
    for (auto& e : entries) e.wipe();
    entries.clear();
    entries.shrink_to_fit();
    unlocked = false;
}

std::vector<unsigned char> PasswordDatabase::buildAAD() const {
    std::vector<unsigned char> aad;
    aad.reserve(PQ_AAD_LEN);
    aad.insert(aad.end(), salt.begin(),    salt.end());
    aad.insert(aad.end(), skNonce.begin(), skNonce.end());
    aad.insert(aad.end(), encSk.begin(),   encSk.end());
    aad.insert(aad.end(), kemCt.begin(),   kemCt.end());
    return aad;
}

bool PasswordDatabase::saveToFile(const SecureString& plaintext) {
    if (!dbKey) return false;

    std::array<unsigned char, PQ_NONCE_LEN> dbNonce;
    randombytes_buf(dbNonce.data(), PQ_NONCE_LEN);

    const auto aad = buildAAD();

    SecureBytes ct(plaintext.size() + PQ_TAG_LEN);
    unsigned long long ctLen = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ct.data(), &ctLen,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            plaintext.size(),
            aad.data(), aad.size(),
            nullptr,
            dbNonce.data(), dbKey) != 0)
        return false;

    const size_t total =
        6 + salt.size() + PQ_NONCE_LEN + encSk.size() + kemCt.size() +
        PQ_NONCE_LEN + static_cast<size_t>(ctLen);

    SecureBytes buf(total);
    size_t pos = 0;
    auto append = [&](const void* src, size_t n) {
        memcpy(buf.data() + pos, src, n); pos += n;
    };

    append("PMDB\x04\x00", 6);
    append(salt.data(),    salt.size());
    append(skNonce.data(), PQ_NONCE_LEN);
    append(encSk.data(),   encSk.size());
    append(kemCt.data(),   kemCt.size());
    append(dbNonce.data(), PQ_NONCE_LEN);
    append(ct.data(),      static_cast<size_t>(ctLen));

    return atomicWrite(filename, buf.data(), total);
}

bool PasswordDatabase::createNewDatabase(const std::string& masterPassword) {
    randombytes_buf(salt.data(), salt.size());

    SecureBytes wrapKey(PQ_KEY_LEN);
    if (crypto_pwhash(
            wrapKey.data(), PQ_KEY_LEN,
            masterPassword.c_str(), masterPassword.size(),
            salt.data(), ARGON_OPS, ARGON_MEM,
            crypto_pwhash_ALG_DEFAULT) != 0)
        return false;

    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (!kem) return false;

    SecureBytes pk(kem->length_public_key);
    SecureBytes sk(kem->length_secret_key);

    if (OQS_KEM_keypair(kem, pk.data(), sk.data()) != OQS_SUCCESS) {
        OQS_KEM_free(kem); return false;
    }

    randombytes_buf(skNonce.data(), PQ_NONCE_LEN);
    unsigned long long encLen = 0;
    bool ok = (crypto_aead_xchacha20poly1305_ietf_encrypt(
                   encSk.data(), &encLen,
                   sk.data(), sk.size(),
                   nullptr, 0, nullptr,
                   skNonce.data(), wrapKey.data()) == 0)
              && (encLen == encSk.size());
    if (!ok) { OQS_KEM_free(kem); return false; }

    SecureBytes ss(kem->length_shared_secret);
    ok = (OQS_KEM_encaps(kem, kemCt.data(), ss.data(), pk.data()) == OQS_SUCCESS);
    OQS_KEM_free(kem);
    if (!ok) return false;

    dbKey = static_cast<unsigned char*>(sodium_malloc(PQ_KEY_LEN));
    if (!dbKey) return false;
    memcpy(dbKey, ss.data(), PQ_KEY_LEN);
    sodium_mlock(dbKey, PQ_KEY_LEN); keyLocked = true;

    bool result = saveToFile(SecureString{});
    auditLog.logCreate();
    lock();
    return result;
}

bool PasswordDatabase::unlock(const std::string& masterPassword) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) { auditLog.logUnlockFail(); return false; }

    constexpr size_t MIN_SIZE =
        6 + crypto_pwhash_SALTBYTES + PQ_NONCE_LEN +
        (PQ_KEM_SK_LEN + PQ_TAG_LEN) + PQ_KEM_CT_LEN + PQ_NONCE_LEN + PQ_TAG_LEN;

    if (static_cast<size_t>(file.tellg()) < MIN_SIZE) {
        auditLog.logUnlockFail(); return false;
    }
    file.seekg(0);

    char magic[6];
    if (!file.read(magic, 6) || memcmp(magic, "PMDB\x04\x00", 6) != 0) {
        auditLog.logUnlockFail(); return false;
    }

    std::array<unsigned char, crypto_pwhash_SALTBYTES>    tmpSalt;
    std::array<unsigned char, PQ_NONCE_LEN>               tmpSkNonce;
    std::array<unsigned char, PQ_KEM_SK_LEN + PQ_TAG_LEN> tmpEncSk;
    std::array<unsigned char, PQ_KEM_CT_LEN>              tmpKemCt;
    std::array<unsigned char, PQ_NONCE_LEN>               dbNonce;

    if (!file.read(reinterpret_cast<char*>(tmpSalt.data()),    tmpSalt.size()))    { auditLog.logUnlockFail(); return false; }
    if (!file.read(reinterpret_cast<char*>(tmpSkNonce.data()), tmpSkNonce.size())) { auditLog.logUnlockFail(); return false; }
    if (!file.read(reinterpret_cast<char*>(tmpEncSk.data()),   tmpEncSk.size()))   { auditLog.logUnlockFail(); return false; }
    if (!file.read(reinterpret_cast<char*>(tmpKemCt.data()),   tmpKemCt.size()))   { auditLog.logUnlockFail(); return false; }
    if (!file.read(reinterpret_cast<char*>(dbNonce.data()),    dbNonce.size()))    { auditLog.logUnlockFail(); return false; }

    std::istreambuf_iterator<char> dbBegin(file), dbEnd;
    std::vector<unsigned char> dbCt(dbBegin, dbEnd);
    if (dbCt.size() < PQ_TAG_LEN) { auditLog.logUnlockFail(); return false; }

    std::vector<unsigned char> aad;
    aad.reserve(PQ_AAD_LEN);
    aad.insert(aad.end(), tmpSalt.begin(),    tmpSalt.end());
    aad.insert(aad.end(), tmpSkNonce.begin(), tmpSkNonce.end());
    aad.insert(aad.end(), tmpEncSk.begin(),   tmpEncSk.end());
    aad.insert(aad.end(), tmpKemCt.begin(),   tmpKemCt.end());

    SecureBytes wrapKey(PQ_KEY_LEN);
    if (crypto_pwhash(
            wrapKey.data(), PQ_KEY_LEN,
            masterPassword.c_str(), masterPassword.size(),
            tmpSalt.data(), ARGON_OPS, ARGON_MEM,
            crypto_pwhash_ALG_DEFAULT) != 0) {
        auditLog.logUnlockFail(); return false;
    }

    SecureBytes sk(PQ_KEM_SK_LEN);
    unsigned long long skLen = 0;
    bool ok = (crypto_aead_xchacha20poly1305_ietf_decrypt(
                   sk.data(), &skLen, nullptr,
                   tmpEncSk.data(), tmpEncSk.size(),
                   nullptr, 0,
                   tmpSkNonce.data(), wrapKey.data()) == 0)
              && (skLen == PQ_KEM_SK_LEN);
    if (!ok) { auditLog.logUnlockFail(); return false; }

    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (!kem) { auditLog.logUnlockFail(); return false; }

    SecureBytes ss(kem->length_shared_secret);
    ok = (OQS_KEM_decaps(kem, ss.data(), tmpKemCt.data(), sk.data()) == OQS_SUCCESS);
    OQS_KEM_free(kem);
    if (!ok) { auditLog.logUnlockFail(); return false; }

    SecureBytes plain(dbCt.size() - PQ_TAG_LEN);
    unsigned long long plainLen = 0;
    ok = (crypto_aead_xchacha20poly1305_ietf_decrypt(
              plain.data(), &plainLen, nullptr,
              dbCt.data(), dbCt.size(),
              aad.data(), aad.size(),
              dbNonce.data(), ss.data()) == 0);
    if (!ok) { auditLog.logUnlockFail(); return false; }

    unsigned char* newKey = static_cast<unsigned char*>(sodium_malloc(PQ_KEY_LEN));
    if (!newKey) { auditLog.logUnlockFail(); return false; }
    memcpy(newKey, ss.data(), PQ_KEY_LEN);
    sodium_mlock(newKey, PQ_KEY_LEN);

    entries.clear();
    const char* cur = reinterpret_cast<const char*>(plain.data());
    const char* end = cur + static_cast<size_t>(plainLen);
    while (cur < end) {
        const char* nl = static_cast<const char*>(memchr(cur, '\n', end - cur));
        size_t lineLen = nl ? static_cast<size_t>(nl - cur) : static_cast<size_t>(end - cur);
        if (lineLen > 0) {
            std::string line(cur, lineLen);
            PasswordEntry e;
            if (e.deserialize(line)) entries.push_back(e);
            sodium_memzero(&line[0], line.size());
        }
        if (!nl) break;
        cur = nl + 1;
    }

    freeKey();
    dbKey     = newKey;
    keyLocked = true;
    salt      = tmpSalt;
    skNonce   = tmpSkNonce;
    encSk     = tmpEncSk;
    kemCt     = tmpKemCt;
    unlocked  = true;

    auditLog.logUnlockOk();
    return true;
}

bool PasswordDatabase::save() {
    if (!unlocked || !dbKey) return false;
    SecureString plaintext;
    for (const auto& e : entries)
        plaintext += e.serialize().c_str();
    bool ok = saveToFile(plaintext);
    if (ok) auditLog.logSave();
    return ok;
}

bool PasswordDatabase::addEntry(const PasswordEntry& entry) {
    if (!unlocked) return false;
    PasswordEntry e = entry;
    e.created = e.modified = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    entries.push_back(e);
    auditLog.logAdd(e.title);
    return true;
}

bool PasswordDatabase::deleteEntry(size_t index) {
    if (!unlocked || index >= entries.size()) return false;
    entries[index].wipe();
    entries.erase(entries.begin() + index);
    auditLog.logDelete(index);
    return true;
}

bool PasswordDatabase::updateEntry(size_t index, const PasswordEntry& newEntry) {
    if (!unlocked || index >= entries.size()) return false;
    entries[index].wipe();
    entries[index] = newEntry;
    entries[index].modified = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    auditLog.logUpdate(index);
    return true;
}
