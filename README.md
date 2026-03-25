# PassMan

A lightweight, offline password manager built with **C++17**, **wxWidgets** and **libsodium**.  
All data is stored locally in an encrypted `.pmdb` file — nothing ever leaves your machine.

---

## Features

- **AES-equivalent encryption** via libsodium `crypto_secretbox` (XSalsa20-Poly1305)
- **Argon2id key derivation** (via `crypto_pwhash`) with a random 32-byte salt
- Create, edit, delete and view password entries
- Toggle password visibility in entry dialogs
- Built-in secure password generator (length, charset configurable)
- Copy username / password to clipboard — auto-clears after **20 seconds**
- Double-click or press Enter on an entry to edit it

---

## File Format (`.pmdb`)

| Offset | Size | Content |
|--------|------|---------|
| 0 | 6 B | Magic header `PMDB\x02\x00` |
| 6 | 32 B | Random salt (Argon2id) |
| 38 | 24 B | Random nonce (XSalsa20) |
| 62 | n B | Ciphertext + 16-byte Poly1305 MAC |

Entries are stored as tab-separated lines inside the encrypted payload.

---

## Requirements

| Dependency | Version | Source |
|------------|---------|--------|
| wxWidgets | 3.2+ | https://www.wxwidgets.org |
| libsodium | latest | https://libsodium.org |
| MinGW-w64 | via MSYS2 | https://www.msys2.org |
| CMake | 3.16+ | https://cmake.org |

Install dependencies via MSYS2 (MINGW64 shell):

```bash
pacman -S mingw-w64-x86_64-wxWidgets3.2 mingw-w64-x86_64-libsodium
```

---

## Building

```bash
git clone https://github.com/yourname/passman.git
cd passman

cmake -B build -G "MinGW Makefiles"
cmake --build build
```

The compiled binary will be at `build/cpp_passman.exe`.

---

## Usage

### Create a new database
`File → New Database…` — choose a file path and set a master password.  
Two example entries are added automatically so you can explore the UI right away.

### Open an existing database
`File → Open Database…` — select your `.pmdb` file and enter your master password.

### Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New Database |
| `Ctrl+O` | Open Database |
| `Ctrl+Shift+N` | New Entry |
| `Ctrl+E` | Edit selected Entry |
| `Del` | Delete selected Entry |
| `Ctrl+S` | Save |
| `Ctrl+B` | Copy Username |
| `Ctrl+C` | Copy Password |
| `Ctrl+Q` | Exit |
| `Double-click` / `Enter` | Edit Entry |

### Right-click context menu
Right-clicking an entry opens a context menu with Edit, Delete, Copy Username and Copy Password.

---

## Project Structure

```
passman/
├── main.cpp                        # App entry point, main frame & UI logic
├── PasswordDatabase.h/.cpp         # Encryption, serialization, entry management
├── PasswordGeneratorDialog.h/.cpp  # Password generator dialog
├── EntryUI/
│   ├── NewEntryDialog.h/.cpp       # Dialog for creating entries
│   └── EditEntryDialog.h/.cpp      # Dialog for editing entries
└── CMakeLists.txt
```

---

## Security Notes

- The master password is **never stored** — only the derived key is held in memory, protected via `sodium_mlock`.
- A fresh random **nonce** is generated on every save, so ciphertexts are never repeated.
- Clipboard contents are **automatically cleared** 20 seconds after copying a username or password.
- All sensitive memory is freed with `sodium_free` when the database is locked or the app exits.

---

## License
[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) — © 2026 YanikWithAWhy
