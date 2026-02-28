# PassMan

PassMan is a desktop password manager application written in C++. It provides a secure, local-first solution for storing and managing your passwords. The application uses the robust `libsodium` library for cryptographic operations and `wxWidgets` for its graphical user interface.

## Features

*   **Secure Database:** Create encrypted password databases (`.pmdb` files) protected by a master password.
*   **Strong Encryption:** Utilizes `libsodium` for key derivation (`crypto_pwhash`) and authenticated encryption (`crypto_secretbox`).
*   **Full CRUD Operations:** Add, edit, and delete password entries.
*   **Entry Details:** Store a title, username, password, URL, and notes for each entry.
*   **Clipboard Integration:**
    *   Quickly copy usernames (`Ctrl+B`) and passwords (`Ctrl+C`) to the clipboard.
    *   For security, the clipboard is automatically cleared after 20 seconds.
*   **User-Friendly Interface:** A clean and simple GUI built with `wxWidgets` allows for easy viewing and management of entries.
*   **Password Visibility:** Toggle password visibility when adding or editing entries.

## Security

PassMan prioritizes the security of your data. The encryption model is built on the following principles:

*   **Key Derivation:** The master password is not stored directly. Instead, a strong encryption key is derived from the master password and a unique random salt using `crypto_pwhash` (Argon2id algorithm).
*   **Authenticated Encryption:** The entire database of entries is encrypted and authenticated using `crypto_secretbox_easy` (XChaCha20-Poly1305). This ensures both confidentiality and integrity, preventing tampering with the encrypted data.
*   **Database Format:** Each `.pmdb` file stores a header, the salt used for key derivation, a nonce for the encryption, and the final ciphertext.
*   **Secure Memory:** The derived encryption key is locked in memory using `sodium_mlock` to prevent it from being swapped to disk.

## Dependencies

To build PassMan from source, you will need the following dependencies:

*   A C++17 compatible compiler (e.g., MinGW-w64 on Windows)
*   [CMake](https://cmake.org/) (version 3.16 or higher)
*   [wxWidgets](https://www.wxwidgets.org/) (version 3.2)
*   [libsodium](https://libsodium.gitbook.io/doc/)

## Building from Source

The project is configured to be built with CMake. The current `CMakeLists.txt` is configured for a MinGW-w64 environment on Windows with dependencies installed via MSYS2. You may need to adjust the include and link paths to match your system's configuration.

1.  **Clone the repository:**
    ```sh
    git clone https://github.com/YanikWithAWhy/Passman.git
    cd Passman
    ```

2.  **Install dependencies.** On Windows with MSYS2, you can install the required libraries:
    ```sh
    pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-wxwidgets mingw-w64-x86_64-libsodium
    ```

3.  **Configure and build with CMake:**
    ```sh
    mkdir build
    cd build
    cmake .. -G "MinGW Makefiles"
    cmake --build .
    ```
    *Note: If dependencies are not in the default system paths, you may need to update the hardcoded paths in `CMakeLists.txt`.*

4.  **Run the application:**
    The executable `cpp_passman.exe` will be located in the `build` directory.

## Usage

1.  Launch the `cpp_passman` executable.
2.  To start, create a new database via **File > New Database...**. Choose a location to save your `.pmdb` file and set a strong master password.
3.  To open an existing database, go to **File > Open Database...** and enter your master password.
4.  Once the database is unlocked, you can manage your entries:
    *   **File > New Entry** (Ctrl+Shift+N): Add a new password entry.
    *   **File > Edit Entry** (Ctrl+E) or right-click: Modify the selected entry.
    *   **File > Delete Entry** (Del) or right-click: Remove the selected entry.
5.  Select an entry and use the context menu or keyboard shortcuts to copy credentials:
    *   **Copy Username:** `Ctrl+B`
    *   **Copy Password:** `Ctrl+C`
6.  Your changes are saved automatically when you close the application, or you can save manually with **File > Save** (Ctrl+S).

Generated using https://www.gitread.dev/ using commit b5a71b57c06f1bbf38128ad3c37fe6e624c31b17.
