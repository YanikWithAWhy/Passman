
#ifndef PASSMAN_AUDIT_LOG_H
#define PASSMAN_AUDIT_LOG_H

#include <string>
#include <ctime>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/stat.h>
#endif

class AuditLog {
public:
    explicit AuditLog(const std::string& dbPath)
        : logPath(dbPath + ".audit.log") {}

    void log(const std::string& event) const {
        std::string line = utcNow() + "\t" + event + "\n";
        appendToFile(line);
    }

    void logCreate()      const { log("CREATE"); }
    void logUnlockOk()    const { log("UNLOCK_OK"); }
    void logUnlockFail()  const { log("UNLOCK_FAIL"); }
    void logLock()        const { log("LOCK"); }
    void logSave()        const { log("SAVE"); }
    void logAdd(const std::string& title) const {
        log("ADD_ENTRY\ttitle=" + sanitizeForLog(title));
    }
    void logDelete(size_t index) const {
        log("DELETE_ENTRY\tindex=" + std::to_string(index));
    }
    void logUpdate(size_t index) const {
        log("UPDATE_ENTRY\tindex=" + std::to_string(index));
    }

private:
    std::string logPath;

    static std::string utcNow() {
        time_t t = time(nullptr);
        struct tm tm_buf;
#ifdef _WIN32
        gmtime_s(&tm_buf, &t);
#else
        gmtime_r(&t, &tm_buf);
#endif
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
        return buf;
    }

    static std::string sanitizeForLog(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (unsigned char c : s) {
            if (c == '\t' || c < 0x20 || c == 0x7F) r += '?';
            else r += static_cast<char>(c);
        }
        return r;
    }

    void appendToFile(const std::string& line) const {
#ifdef _WIN32
        auto toWide = [](const std::string& s) -> std::wstring {
            if (s.empty()) return {};
            int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            std::wstring w(n - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
            return w;
        };
        std::wstring wpath = toWide(logPath);
        HANDLE h = CreateFileW(
            wpath.c_str(),
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(h, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        CloseHandle(h);
#else
        // O_APPEND makes write() atomic up to PIPE_BUF on POSIX
        int fd = open(logPath.c_str(),
                      O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                      S_IRUSR | S_IWUSR);   // 0600
        if (fd < 0) return;
        const char* p = line.data();
        size_t rem = line.size();
        while (rem > 0) {
            ssize_t n = write(fd, p, rem);
            if (n <= 0) break;
            p += n; rem -= static_cast<size_t>(n);
        }
        close(fd);
#endif
    }
};

#endif // PASSMAN_AUDIT_LOG_H
