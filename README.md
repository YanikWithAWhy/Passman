# PassMan — Post-Quantum Edition

A lightweight, offline password manager built with **C++17**, **wxWidgets**, **libsodium** and **liboqs (Open Quantum Safe)**.  
All data is stored locally in an encrypted `.pmdb` file — now protected by NIST-standardised post-quantum cryptography.

[![Download](https://img.shields.io/github/v/release/YanikWithAWhy/Passman?label=Download&logo=github)](https://github.com/YanikWithAWhy/Passman/releases/latest)

---

## Cryptographic Architecture

```
masterPassword + salt  ──Argon2id──►  wrap_key (32 B)
wrap_key  ──XChaCha20-Poly1305──►  encrypted ML-KEM-768 secret key  (stored in file)

ML-KEM-768 encaps(pk)  ──►  (kem_ct, shared_secret)
shared_secret  =  db_key  (32 B, never stored)

plaintext  ──XChaCha20-Poly1305[db_key]──►  ciphertext  (stored in file)
```

| Component | Algorithm | Standard | Quantum Security |
|---|---|---|---|
| Key derivation | Argon2id | NIST SP 800-63B | ✅ ~128-bit (Grover) |
| KEM / key encapsulation | **ML-KEM-768** (CRYSTALS-Kyber) | **FIPS 203** | ✅ Module-LWE hardness |
| Symmetric AEAD | XChaCha20-Poly1305 (256-bit key) | RFC 8439 | ✅ ~128-bit (Grover) |
| Secure memory | sodium_malloc / sodium_mlock | libsodium | n/a |

### Why ML-KEM-768?

ML-KEM (Module Lattice-based Key Encapsulation Mechanism) is NIST's primary post-quantum KEM standard (FIPS 203, August 2024).  
Its security is based on the hardness of the **Module Learning With Errors (MLWE)** problem — believed to be intractable for both classical *and* quantum computers.

In this password manager it protects the database key: even if a quantum adversary records the `.pmdb` file today, they cannot recover the database key without also breaking ML-KEM-768.

---

## File Format (`.pmdb`, version 3)

| Offset | Size | Content |
|---|---|---|
| 0 | 6 B | Magic header `PMDB\x03\x00` |
| 6 | 16 B | Argon2id salt |
| 22 | 24 B | XChaCha20 nonce (SK encryption) |
| 46 | 2416 B | Encrypted ML-KEM-768 secret key (2400 B + 16 B Poly1305 tag) |
| 2462 | 1088 B | ML-KEM-768 ciphertext (encapsulated database key) |
| 3550 | 24 B | XChaCha20 nonce (database encryption) |
| 3574 | n+16 B | Encrypted database entries + Poly1305 tag |

> **Note:** The `.pmdb` v3 format is not backwards-compatible with v2 files (original XSalsa20 format).  
> Re-create your database or export/import entries manually.

---

## Features

* **Post-quantum key encapsulation** via ML-KEM-768 (FIPS 203) — liboqs
* **Argon2id key derivation** (libsodium `crypto_pwhash`) with moderate cost parameters
* **XChaCha20-Poly1305** authenticated encryption for the database payload
* Create, edit, delete and view password entries
* Toggle password visibility in entry dialogs
* Built-in secure password generator (length, charset configurable)
* Copy username / password to clipboard (auto-cleared after 20 seconds)
* Double-click or press Enter on an entry to edit it
* Master password is **never stored** — only the derived key in `sodium_malloc`/`sodium_mlock` memory

---

## Requirements

| Dependency | Version | Source |
|---|---|---|
| wxWidgets | 3.2+ | https://www.wxwidgets.org |
| libsodium | latest | https://libsodium.org |
| **liboqs** | **0.10+** | **https://openquantumsafe.org** |
| MinGW-w64 | via MSYS2 | https://www.msys2.org |
| CMake | 3.16+ | https://cmake.org |

Install dependencies via MSYS2 (MINGW64 shell):

```bash
pacman -S mingw-w64-x86_64-wxWidgets3.2 \
          mingw-w64-x86_64-libsodium \
          mingw-w64-x86_64-liboqs
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
Two example entries are added automatically.

### Open an existing database

`File → Open Database…` — select your `.pmdb` file and enter your master password.

### Keyboard shortcuts

| Shortcut | Action |
|---|---|
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

---

## Project Structure

```
passman/
├── main.cpp                        # App entry point, main frame & UI logic
├── PasswordDatabase.h/.cpp         # PQ encryption, serialization, entry management
├── PasswordGeneratorDialog.h/.cpp  # Password generator dialog
├── EntryUI/
│   ├── NewEntryDialog.h/.cpp       # Dialog for creating entries
│   └── EditEntryDialog.h/.cpp      # Dialog for editing entries
└── CMakeLists.txt
```

---

## Security Notes

* The master password is **never stored** — only the derived `wrap_key` (held transiently in a `std::vector`, zeroed immediately after use).
* The database key (`db_key`) lives exclusively in `sodium_malloc` / `sodium_mlock`-protected memory and is never written to disk.
* A fresh **XChaCha20 nonce** is generated on every save, so ciphertexts are never repeated even for identical plaintexts.
* The **ML-KEM-768 secret key** is stored in the file, but encrypted with the Argon2id-derived `wrap_key` — it cannot be recovered without the master password.
* Clipboard contents are **automatically cleared** 20 seconds after copying a username or password.
* All sensitive memory is zeroed with `sodium_memzero` / `sodium_free` when the database is locked or the app exits.

---

## License

[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) — © 2026 YanikWithAWhy
