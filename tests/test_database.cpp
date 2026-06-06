
using TestFn = std::function<void()>;
struct TestCase { std::string name; TestFn fn; };

static std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct TestRegistrar {
    TestRegistrar(const char* name, TestFn fn) {
        registry().push_back({name, std::move(fn)});
    }
};

#define ASSERT(cond) \
    do { if (!(cond)) throw std::runtime_error( \
        "Assertion failed: " #cond " at line " + std::to_string(__LINE__)); \
    } while(0)

#define PASTE_(a, b) a##b
#define PASTE(a, b)  PASTE_(a, b)

#define TEST(name) \
    static void PASTE(test_fn_, __LINE__)(); \
    static TestRegistrar PASTE(test_reg_, __LINE__)( \
        name, PASTE(test_fn_, __LINE__)); \
    static void PASTE(test_fn_, __LINE__)()

static std::string tmpPath(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() /
            ("passman_test_" + suffix + ".pmdb")).string();
}

static void cleanUp(const std::string& p) {
    std::error_code ec;
    std::filesystem::remove(p, ec);
    std::filesystem::remove(p + ".tmp", ec);
    std::filesystem::remove(p + ".audit.log", ec);
}

static PasswordEntry makeEntry(const std::string& title,
                                const std::string& user  = "u",
                                const std::string& pass  = "p",
                                const std::string& notes = "n",
                                const std::string& url   = "http://x") {
    PasswordEntry e;
    e.title = title; e.username = user; e.password = pass;
    e.notes = notes; e.url = url;
    e.created = e.modified = 1000000000ULL;
    return e;
}

TEST("T01 Entry round-trip") {
    PasswordEntry orig = makeEntry("GitHub", "alice@example.com", "S3cr3t!",
                                    "my notes\nline2", "https://github.com");
    orig.created  = 1700000000ULL;
    orig.modified = 1700000001ULL;

    std::string line = orig.serialize();
    if (!line.empty() && line.back() == '\n') line.pop_back();

    PasswordEntry parsed;
    ASSERT(parsed.deserialize(line));
    ASSERT(parsed.title    == orig.title);
    ASSERT(parsed.username == orig.username);
    ASSERT(parsed.password == orig.password);
    ASSERT(parsed.notes    == orig.notes);
    ASSERT(parsed.url      == orig.url);
    ASSERT(parsed.created  == orig.created);
    ASSERT(parsed.modified == orig.modified);
}

TEST("T02 deserialize rejects too many fields") {
    PasswordEntry e;
    ASSERT(!e.deserialize("a\tb\tc\td\te\t1\t2\textra"));
}

TEST("T03 deserialize rejects field-length violation") {
    std::string longPass(1025, 'x');
    PasswordEntry e;
    std::string line = "title\tuser\t" + longPass +
                       "\tnotes\turl\t1000000000\t1000000000";
    ASSERT(!e.deserialize(line));
}

TEST("T04 deserialize rejects zero timestamps") {
    PasswordEntry e;
    ASSERT(!e.deserialize("t\tu\tp\tn\turl\t0\t0"));
    ASSERT(!e.deserialize("t\tu\tp\tn\turl\t1000000000\t0"));
}

TEST("T05 deserialize rejects created > modified") {
    PasswordEntry e;
    ASSERT(!e.deserialize("t\tu\tp\tn\turl\t2000000000\t1000000000"));
}

TEST("T06 deserialize rejects far-future timestamps") {
    PasswordEntry e;
    ASSERT(!e.deserialize("t\tu\tp\tn\turl\t7258118401\t7258118401"));
}

TEST("T07 wipe() zeroes all string fields") {
    PasswordEntry e = makeEntry("title", "username",
                                std::string(64, 'X'),  // > SSO threshold
                                "notes", "url");
    const char* passData = e.password.data();
    size_t      passSize = e.password.size();
    e.wipe();
    ASSERT(e.title.empty());
    ASSERT(e.username.empty());
    ASSERT(e.password.empty());
    ASSERT(e.notes.empty());
    ASSERT(e.url.empty());
    bool allZero = true;
    for (size_t i = 0; i < passSize; ++i)
        if (passData[i] != '\0') { allZero = false; break; }
    ASSERT(allZero);
}

TEST("T08 createNewDatabase + unlock round-trip") {
    std::string path = tmpPath("t08");
    cleanUp(path);
    {
        PasswordDatabase db(path);
        ASSERT(db.createNewDatabase("masterPass123!"));
        ASSERT(!db.isUnlocked());
    }
    {
        PasswordDatabase db(path);
        ASSERT(db.unlock("masterPass123!"));
        ASSERT(db.isUnlocked());
        ASSERT(db.size() == 0);
    }
    cleanUp(path);
}

TEST("T09 unlock rejects wrong password") {
    std::string path = tmpPath("t09");
    cleanUp(path);
    {
        PasswordDatabase db(path);
        ASSERT(db.createNewDatabase("correct-horse-battery-staple"));
    }
    {
        PasswordDatabase db(path);
        ASSERT(!db.unlock("wrongpassword"));
        ASSERT(!db.isUnlocked());
    }
    cleanUp(path);
}

TEST("T10 unlock rejects truncated file") {
    std::string path = tmpPath("t10");
    cleanUp(path);
    {
        std::ofstream f(path, std::ios::binary);
        f.write("PMDB\x04\x00\xDE\xAD\xBE\xEF", 10);
    }
    PasswordDatabase db(path);
    ASSERT(!db.unlock("anypassword"));
    cleanUp(path);
}

TEST("T11 unlock rejects bit-flipped header (AAD tamper)") {
    std::string path = tmpPath("t11");
    cleanUp(path);
    {
        PasswordDatabase db(path);
        ASSERT(db.createNewDatabase("securePass!99"));
    }
    {
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        f.seekg(2500);
        char b = 0;
        f.read(&b, 1);
        f.seekp(2500);
        b ^= 0x01;
        f.write(&b, 1);
    }
    PasswordDatabase db(path);
    ASSERT(!db.unlock("securePass!99"));
    cleanUp(path);
}

TEST("T12 unlock rejects bit-flipped ciphertext") {
    std::string path = tmpPath("t12");
    cleanUp(path);
    {
        PasswordDatabase db(path);
        ASSERT(db.createNewDatabase("passw0rd!"));
    }
    {
        auto size = std::filesystem::file_size(path);
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        std::streamoff offset = static_cast<std::streamoff>(size) - 5;
        f.seekg(offset);
        char b = 0;
        f.read(&b, 1);
        f.seekp(offset);
        b ^= 0xFF;
        f.write(&b, 1);
    }
    PasswordDatabase db(path);
    ASSERT(!db.unlock("passw0rd!"));
    cleanUp(path);
}

TEST("T13 entry CRUD operations") {
    std::string path = tmpPath("t13");
    cleanUp(path);
    PasswordDatabase db(path);
    ASSERT(db.createNewDatabase("crudPass!"));
    ASSERT(db.unlock("crudPass!"));

    ASSERT(db.addEntry(makeEntry("Site A", "alice", "pass1")));
    ASSERT(db.addEntry(makeEntry("Site B", "bob",   "pass2")));
    ASSERT(db.size() == 2);
    ASSERT(db.getEntries()[0].title == "Site A");
    ASSERT(db.getEntries()[1].title == "Site B");

    PasswordEntry updated = makeEntry("Site A (updated)", "alice2", "pass1new");
    updated.created  = db.getEntries()[0].created;
    updated.modified = db.getEntries()[0].modified;
    ASSERT(db.updateEntry(0, updated));
    ASSERT(db.getEntries()[0].title    == "Site A (updated)");
    ASSERT(db.getEntries()[0].username == "alice2");

    ASSERT(db.deleteEntry(1));
    ASSERT(db.size() == 1);
    ASSERT(!db.deleteEntry(99));

    cleanUp(path);
}

TEST("T14 save + re-unlock preserves entries") {
    std::string path = tmpPath("t14");
    cleanUp(path);
    const std::string pw = "persistenceTest!";
    {
        PasswordDatabase db(path);
        ASSERT(db.createNewDatabase(pw));
        ASSERT(db.unlock(pw));
        ASSERT(db.addEntry(makeEntry("Alpha", "a@a.com", "aaa",
                                     "note alpha", "https://alpha.com")));
        ASSERT(db.addEntry(makeEntry("Beta",  "b@b.com", "bbb",
                                     "note beta\nmultiline", "https://beta.com")));
        ASSERT(db.save());
    }
    {
        PasswordDatabase db(path);
        ASSERT(db.unlock(pw));
        ASSERT(db.size() == 2);
        ASSERT(db.getEntries()[0].title    == "Alpha");
        ASSERT(db.getEntries()[0].username == "a@a.com");
        ASSERT(db.getEntries()[0].password == "aaa");
        ASSERT(db.getEntries()[0].notes    == "note alpha");
        ASSERT(db.getEntries()[0].url      == "https://alpha.com");
        ASSERT(db.getEntries()[1].title    == "Beta");
        ASSERT(db.getEntries()[1].notes    == "note beta\nmultiline");
    }
    cleanUp(path);
}

TEST("T15 SecureBytes destructor lifecycle") {
    const unsigned char SENTINEL = 0xAB;
    {
        SecureBytes buf(64, SENTINEL);
        for (size_t i = 0; i < 64; ++i) ASSERT(buf[i] == SENTINEL);
    }
}

TEST("T16 lock() clears all state") {
    std::string path = tmpPath("t16");
    cleanUp(path);
    PasswordDatabase db(path);
    ASSERT(db.createNewDatabase("lockTest!"));
    ASSERT(db.unlock("lockTest!"));
    ASSERT(db.addEntry(makeEntry("Secret entry")));
    ASSERT(db.isUnlocked());
    ASSERT(db.size() == 1);

    db.lock();

    ASSERT(!db.isUnlocked());
    ASSERT(db.size() == 0);
    ASSERT(!db.save());
    ASSERT(!db.addEntry(makeEntry("Ghost")));
    cleanUp(path);
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    if (sodium_init() < 0) {
        std::fprintf(stderr, "libsodium init failed\n");
        return 1;
    }

    std::printf("\nPassMan Unit Tests\n");
    std::printf("==================\n\n");

    int passed = 0, failed = 0;
    int idx = 0;
    for (const auto& tc : registry()) {
        ++idx;
        try {
            tc.fn();
            std::printf("[PASS] T%02d %s\n", idx, tc.name.c_str());
            ++passed;
        } catch (const std::exception& ex) {
            std::printf("[FAIL] T%02d %s\n       %s\n",
                        idx, tc.name.c_str(), ex.what());
            ++failed;
        } catch (...) {
            std::printf("[FAIL] T%02d %s\n       unknown exception\n",
                        idx, tc.name.c_str());
            ++failed;
        }
    }

    std::printf("\n%s\n", std::string(40, '-').c_str());
    std::printf("Results: %d passed, %d failed\n\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
