# 📋 XoidCraft
XoidCraft is a lightweight, high-performance C++23 Minecraft 1.8.9 (Protocol 47) custom server built from scratch. Operating without any external libraries or dependencies, it utilizes raw WinSock2 API and multi-threaded architecture to handle low-level packet framing, custom chunk serialization, dynamic world saving, and real-time client interactions.

---

## 🛠 Features  
- **Zero External Dependencies:** Built using native Windows Win32 / WinSock2 sockets and standard C++ libraries.
- **Custom Network & Packet Engine:** Complete implementation of Big-Endian binary reader/writer with variable-length integer (`VarInt` and `VarLong`) encoding/decoding.
- **Robust Packet Framing:** Built-in `PacketFramer` buffer mechanism to handle TCP packet fragmentation and coalescing.
- **Multi-Threaded Architecture:**
  - Asynchronous client socket handling with isolated threads.
  - Background thread for periodic **Keep-Alive** keep-alive ping loop.
  - Thread-safe packet sending and broadcast synchronization (`std::mutex`).
- **Server List Ping & Handshake:** Customized JSON MOTD, color codes, player counts, and ping/pong response handling.
- **Dynamic World & Persistence:**
  - 5x5 Chunk column serialization (`0x21 Chunk Data`) sent upon login.
  - Real-time block digging/breaking (`0x07`) packet handling and block updates broadcasted to all connected clients (`0x23`).
  - Automatic background world saving (**Auto-Save thread every 10s**) to a custom binary file (`world.dat`).
- **Advanced Logging System:** Color-coded Win32 console logs for `INFO`, `DEBUG`, `WARN`, and `ERROR` levels with C++20 `std::format` support and debugger attachment detection.

---

## 🚀 Tech Stack  

- **Language:** ISO C++23 Standard (Visual Studio Community)    
- **Compiler Standards:**  
  - C++23 (`/std:c++23preview`)  
  - C17 (`/std:c17`)  
  - Target: `x64/Release`
- **Toolset:** `v145 for Microsoft C++ Build Tools`
- **Platform:** Windows (WinSock2, Ole32/CoCreateGuid)

---

## 🎮 Roadmap  

### Server & Network Engine
- [x] WinSock2 TCP Socket listener and connection manager
- [x] Custom binary packet serializer & deserializer (`PacketReader` / `PacketWriter`)
- [x] Advanced TCP stream `PacketFramer`
- [x] Colored Win32 Logger with `std::format` and debugger check
- [x] Background Keep-Alive thread loop

### Protocol 47 (Minecraft 1.8.9)
- [x] Handshake state processing
- [x] Server Status Response (Custom JSON MOTD, Online/Max players, Version)
- [x] Status Ping & Pong packets
- [x] Login Start & GUID/UUID generation (`CoCreateGuid`)
- [x] Join Game packet (`0x01`) & Spawn Position (`0x05`)
- [x] Initial Player Position & Look packet (`0x08`)
- [x] 5x5 Chunk Column Data generation (`0x21`)
- [x] Digging / Block Break handling (`0x07 Finished Digging`)
- [x] Real-time Block Change broadcasting (`0x23`)

### World & Player Management
- [x] Binary World Save / Load override system (`world.dat`)
- [x] Automatic background auto-save loop (10-second interval)
- [x] Multi-player movement synchronization (`0x04`, `0x05`, `0x06`)
- [x] Block Placement (`0x08` / `0x1B`)
- [ ] Chat Message processing (`0x01`)
- [ ] Inventory & Window Click handling

---

## 📂 Project Structure

```text
XoidCraft/
└── Source/
    ├── Logger/               # Colored console logging system with std::format
    │   ├── Logger.cpp
    │   └── Logger.hpp
    ├── Main/                 # Entry point and global configuration settings
    │   ├── Application.cpp
    │   └── Config.hpp
    ├── Network/              # WinSock2 TCP server, packet framer, reader & writer
    │   ├── PacketBuilder.cpp
    │   ├── PacketBuilder.hpp
    │   ├── PacketFramer.cpp  # TCP stream packet slicer / buffer manager
    │   ├── PacketFramer.hpp
    │   ├── PacketReader.cpp  # Big-Endian & VarInt binary reader
    │   ├── PacketReader.hpp
    │   ├── PacketWriter.cpp  # Big-Endian & VarInt binary writer
    │   ├── PacketWriter.hpp
    │   ├── TcpServer.cpp     # Multi-threaded server & packet router
    │   └── TcpServer.hpp
    ├── Protocol/             # Minecraft 1.8.9 state & packet handlers
    │   ├── ConnectionState.hpp
    │   ├── PacketHandler.cpp # Handshake, Login & Play state logic
    │   ├── PacketHandler.hpp
    │   └── Position.hpp      # Bit-packed 64-bit block position helper
    └── World/                # World data storage, block overrides & binary persistence
        ├── World.cpp
        └── World.hpp
```

---

## 🚀 Getting Started

### 1. Prerequisites
- **Visual Studio 2026 / Community Edition** (Build Tools v145)
- **Windows 10 / 11 SDK**
- **No external package managers (VCPKG/CMake) needed.**

### 2. Building the Project
1. Clone the repository:
   ```bash
   git clone [https://github.com/batutekoz77/XoidCraft.git](https://github.com/batutekoz77/XoidCraft.git)
   ```
2. Open `XoidCraft.sln` in **Visual Studio**.
3. Set the build configuration to **Release** and platform to **x64**.
4. Build the solution (`Ctrl + Shift + B`).
5. Run the generated `XoidCraft.exe` inside `x64/Release/`.

### 3. Connecting
Launch **Minecraft 1.8.9**, navigate to **Multiplayer -> Direct Connect**, and type:
```text
127.0.0.1:25565
```

---

## 📜 Config Customization (`Source/Main/Config.hpp`)

You can easily adjust the default server parameters directly in the configuration file:

```cpp
namespace SERVER {
    inline const char*     IP          = "127.0.0.1";
    inline unsigned short  PORT        = 25565;

    inline int             MAX_PLAYER  = 20;
    inline int             PROTOCOL    = 47; // 1.8.9

    inline std::string     NAME        = "XoidCraft";
    inline std::string     TAG         = "Network";
    inline std::string     DESCRIPTION = "Development Server";
    inline std::string     VERSION     = "1.8.9";
}
```

---

## 📄 License
This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing
Contributions, issue reports, and feature requests are welcome! Feel free to open a pull request or issue on GitHub.

## 💳 Credits
- **C++23** – Core programming language standard
- **WinSock2** – Native Windows network API
