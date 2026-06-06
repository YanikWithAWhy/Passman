# PassMan — Threat Model

**Version:** 2.0 (Post-Quantum + Hardened Edition)  
**Date:** 2026  
**File format:** PMDB v4

---

## 1. System Overview

PassMan is an offline, single-user password manager. All data is stored in a single encrypted file (`.pmdb`) on the user's local filesystem. There is no network component, no cloud sync, and no server.

```
┌─────────────────────────────────────────┐
│  User                                   │
│    │ master password (never stored)     │
│    ▼                                    │
│  PassMan (wxWidgets GUI)                │
│    │                                    │
│    ▼                                    │
│  PasswordDatabase                       │
│    │  Argon2id ──► wrap_key             │
│    │  ML-KEM-768 encaps ──► db_key      │
│    │  XChaCha20-Poly1305 ──► ciphertext │
│    ▼                                    │
│  mydb.pmdb  (local filesystem)          │
└─────────────────────────────────────────┘
```

---

## 2. Assets

| Asset | Sensitivity | Location |
|---|---|---|
| Master password | Critical | User's memory only; never stored |
| Database key (`db_key`) | Critical | RAM, mlock'd sodium_malloc; never on disk |
| ML-KEM-768 secret key (`sk`) | Critical | Disk (encrypted); RAM only transiently |
| Password entries (plaintext) | High | RAM while unlocked; disk only encrypted |
| `.pmdb` file | High | Local filesystem |
| Audit log | Medium | Local filesystem (plaintext) |
| ML-KEM-768 public key | Low | Derived from `sk`; not stored separately |

---

## 3. Trust Boundaries

```
┌──────────────────────────────────────────────────────┐
│  TRUSTED                                             │
│  • The user and their session                        │
│  • The OS kernel                                     │
│  • libsodium (audited, pinned ≥ 1.0.18)             │
│  • liboqs    (audited, pinned ≥ 0.10.0)             │
└──────────────────────────────────────────────────────┘
         │                        │
         │ filesystem              │ process memory
         ▼                        ▼
┌────────────────┐   ┌─────────────────────────────────┐
│  UNTRUSTED     │   │  SEMI-TRUSTED                   │
│  • .pmdb file  │   │  • wxWidgets clipboard API      │
│  • Filesystem  │   │  • Win32 clipboard API          │
│  • Other users │   │  • Standard heap (entries use   │
│  • Backup sw.  │   │    wipe() but not sodium_malloc) │
└────────────────┘   └─────────────────────────────────┘
```

---

## 4. Threat Actors

| Actor | Capability | Motivation |
|---|---|---|
| **T1 — Passive disk attacker** | Read-only access to `.pmdb` (stolen laptop, cloud backup, forensic image) | Recover stored passwords |
| **T2 — Active file attacker** | Read + write access to `.pmdb` (malware, shared filesystem) | Corrupt or exfiltrate data |
| **T3 — Memory attacker** | Read process memory (cold-boot, DMA, crash dump, swap) | Recover keys or plaintext passwords |
| **T4 — Quantum adversary** | CRQC (Cryptographically Relevant Quantum Computer) now or in future | Break classical public-key crypto |
| **T5 — Local user** | Other accounts on the same machine | Read the `.pmdb` file |
| **T6 — Clipboard snooper** | Reads clipboard contents (malware, clipboard history tools) | Capture copied passwords |
| **T7 — Shoulder surfer** | Visual access to screen | Read displayed passwords or master password |

---

## 5. Threat Scenarios & Mitigations

### T1 — Passive Disk Attacker

| Threat | Mitigation | Status |
|---|---|---|
| Brute-force master password offline | Argon2id 512 MB / 4 ops (< 0.2 attempts/s on GPU cluster) | ✅ Implemented |
| Classical break of symmetric encryption | XChaCha20-Poly1305 with 256-bit key (128-bit PQ security) | ✅ Implemented |
| Quantum break of key encapsulation | ML-KEM-768 (FIPS 203, Module-LWE) replaces RSA/ECDH | ✅ Implemented |
| Recover old plaintext from backup | Fresh nonce on every save; no nonce reuse | ✅ Implemented |
| Read unencrypted `.pmdb.tmp` during save | atomicWrite: tmp is owner-only, replaced atomically | ✅ Implemented |
| Access file as another local user | File permissions 0600 (POSIX) / user-only DACL (Windows) | ✅ Implemented |

### T2 — Active File Attacker

| Threat | Mitigation | Status |
|---|---|---|
| Flip bits in encrypted ciphertext | Poly1305 MAC; any flip → authentication failure | ✅ Implemented |
| Swap salt to force key derivation to a known value | Salt is covered by AAD in the database AEAD | ✅ Implemented |
| Swap `kemCt` to influence `db_key` | `kemCt` is covered by AAD; tamper → decryption failure | ✅ Implemented |
| Swap `encSk` to inject a malicious secret key | `encSk` is covered by AAD; tamper → decryption failure | ✅ Implemented |
| Corrupt database during save (power loss) | Atomic write (tmp → rename); old file intact on failure | ✅ Implemented |
| Inject malformed entries via tampered file | Input sanitisation in `deserialize()` (length, timestamps) | ✅ Implemented |
| Truncate or extend the file | Minimum size check; AEAD tag verification | ✅ Implemented |

**Residual risk:** An attacker who can write the file before the first unlock can replace the entire `.pmdb` with their own (different master password). PassMan has no TOFU mechanism to detect this because the master password is not stored. **Mitigation:** users should compare file modification timestamps and use filesystem integrity tools (e.g., `aide`, Windows File Integrity Monitoring) for high-security environments.

### T3 — Memory Attacker

| Threat | Mitigation | Status |
|---|---|---|
| Read `db_key` from swap / pagefile | `sodium_mlock` on the key; `mlockall`/`SetProcessWorkingSetSize` at startup | ✅ Implemented |
| Read decrypted passwords from heap | `PasswordEntry::wipe()` + `sodium_memzero` before free; `shrink_to_fit()` | ✅ Implemented |
| Read intermediate crypto buffers (wrapKey, sk, ss) | All crypto temporaries use `SecureBytes` / `SecureString` (SodiumAllocator) | ✅ Implemented |
| Read master password from heap | `SecurePasswordDialog` drains to SecureString; wxString wiped immediately | ✅ Implemented |
| Cold-boot attack on RAM | mlock prevents swap; physical DRAM cooling is out of scope | ⚠️ Partial |
| DMA attack (Thunderbolt/FireWire) | OS-level IOMMU; out of scope for application | ❌ Out of scope |
| Core dump contains keys | sodium_malloc guard pages reduce exposure; OS-level dump prevention recommended | ⚠️ Partial |

### T4 — Quantum Adversary

| Threat | Mitigation | Status |
|---|---|---|
| Grover's algorithm on AES/XChaCha20 | 256-bit keys → ~128-bit post-quantum security | ✅ Sufficient |
| Shor's algorithm on key encapsulation | ML-KEM-768 (MLWE, no group structure to exploit) | ✅ Implemented |
| "Harvest now, decrypt later" on stored files | ML-KEM-768 protects `db_key` even against future CRQC | ✅ Implemented |
| Grover on Argon2id brute force | 512 MB / 4 ops cost still applies; PQ doesn't help attacker here | ✅ Sufficient |

### T5 — Local User (Other Accounts)

| Threat | Mitigation | Status |
|---|---|---|
| Read `.pmdb` file | 0600 / user-only DACL | ✅ Implemented |
| Read audit log | 0600 / user-only DACL (same directory) | ✅ Implemented |
| Ptrace / `/proc/mem` process memory | OS-level ptrace restrictions (Yama LSM on Linux) | ⚠️ OS-dependent |

### T6 — Clipboard Snooper

| Threat | Mitigation | Status |
|---|---|---|
| Malware reads clipboard after copy | 20-second auto-clear timer | ✅ Implemented |
| Windows clipboard history records password | `ExcludeClipboardContentFromMonitorProcessing` + related formats | ✅ Implemented |
| Third-party clipboard manager captures password | Same Windows formats signal opt-out; not all managers honour this | ⚠️ Partial |
| Clipboard capture when DB locks | `clearClipboard()` called on every `lockDatabase()` | ✅ Implemented |
| Clipboard survives app exit | `clearClipboard()` on `OnCloseWindow` | ✅ Implemented |

**Residual risk:** A malicious clipboard manager that ignores the opt-out formats cannot be prevented at the application level. Users should disable third-party clipboard managers when using PassMan.

### T7 — Shoulder Surfer

| Threat | Mitigation | Status |
|---|---|---|
| See passwords in list view | Passwords not shown in the list (title/username/URL only) | ✅ Implemented |
| See master password during entry | `wxTE_PASSWORD` masks the field; paste disabled | ✅ Implemented |
| See file path in window title | Title shows only filename, not full path | ✅ Implemented |
| Unattended unlocked database | Auto-lock after 5 minutes idle | ✅ Implemented |
| Screen recording / screenshot | Out of scope (OS-level problem) | ❌ Out of scope |

---

## 6. Explicitly Out-of-Scope Threats

- **Physical access** to the running machine (evil maid, cold-boot with equipment)
- **Malicious OS kernel** or compromised hypervisor
- **Side-channel attacks** (timing, power, EM) against libsodium / liboqs primitives
- **Vulnerabilities in wxWidgets** beyond what SecurePasswordDialog addresses
- **Social engineering** of the user
- **Weak master passwords** (PassMan provides a generator but cannot enforce policy)

---

## 7. Dependency Security Status

| Library | Pinned version | Last audit (public) | Known critical CVEs |
|---|---|---|---|
| libsodium | ≥ 1.0.18 | Ongoing (NaCl lineage) | None in ≥ 1.0.18 |
| liboqs | ≥ 0.10.0 | Trail of Bits (2022 pre-standardisation) | None in ≥ 0.10.0 |
| wxWidgets | 3.2+ | Community review | n/a (UI only, no crypto) |

**Action:** Run `pacman -Syu` regularly to keep MSYS2 packages updated. Subscribe to [libsodium releases](https://github.com/jedisct1/libsodium/releases) and [liboqs releases](https://github.com/open-quantum-safe/liboqs/releases).

---

## 8. Security Assumptions

1. The OS kernel is not compromised.
2. The user's session is not shared with an adversary.
3. The master password has sufficient entropy (≥ 80 bits recommended; the built-in generator with all charsets at length 20 provides ≈ 130 bits).
4. libsodium's and liboqs' cryptographic implementations are correct.
5. ML-KEM-768 remains secure against quantum adversaries (based on MLWE hardness).
6. The filesystem used for the `.pmdb` supports atomic `rename()`.

---

## 9. What a Third-Party Audit Should Focus On

1. Correctness of `saveToFile()` AAD construction (bytes [6..3549] cover all header fields)
2. Absence of timing side-channels in error paths (especially `unlock()` failures)
3. Memory lifecycle of `PasswordEntry` objects — specifically the gap between `std::string` allocation and `wipe()` call
4. Correctness of `SecurePasswordDialog` — whether the wxTextCtrl internal buffer is reliably overwritten
5. The `atomicWrite()` implementation on NTFS (interaction with filesystem journalling)
6. liboqs integration — correct use of `OQS_KEM_new`, `OQS_KEM_free`, error codes
