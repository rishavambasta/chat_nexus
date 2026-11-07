# Qt P2P Chat and File Exchange

A high-performance, peer-to-peer (P2P) chat and file transfer desktop application built with C++ and Qt 5 (using Qt 5.15 / 5.14.2 at `/run/media/rishav/DS1/qt/qt5.15`).

## Highlights & Features

- **Direct Peer-to-Peer (No Discovery / No Central Server)**:
  - Peers are added manually by IPv4 address (e.g. `192.168.1.100` or `192.168.1.100:42424`).
  - When adding a peer, if the peer is online, their display alias is automatically fetched via a handshake exchange and the peer is saved.
  - If the peer is offline, the user can add them with an address placeholder; when the peer comes online, the alias is automatically updated.

- **Offline Message Queuing**:
  - If a peer is offline when a message is sent, the message is queued in the sender's local SQLite database (marked with `🕒`).
  - As soon as the peer comes online (detected via background polling or incoming connection), all queued messages and queued file transfers are automatically transmitted in chronological order.

- **WhatsApp-Style Delivery & Read Receipts (Two Ticks)**:
  - **Queued / Pending**: Displayed with a clock (`🕒`).
  - **Delivered**: Two grey ticks (`✓✓`, `#8696a0`) as soon as the recipient node receives the message and returns a delivery ACK over TCP. (Single tick is intentionally not used as there is no central intermediary server).
  - **Read**: Two blue ticks (`✓✓`, WhatsApp signature `#34b7f1`) as soon as the recipient opens/views the chat!

- **Automatic Streamed File Transfer with Real-Time Progress Bar**:
  - Automatically accepted on the recipient's side when sent by the sender (streamed directly to `Downloads/P2PChat/`, avoiding filename collisions).
  - High-throughput chunk streaming (64 KB chunks) with a non-blocking throttled TCP pipeline.
  - Interactive UI bubble embedded directly in the chat with a live progress bar (`0% -> 100%`), file size formatting, and clickable "Open File" and "Show in Folder" actions upon completion.

- **SQLite Database**:
  - Single-file SQLite database (`p2p_chat.db`) maintaining user profile, added peers, complete chat message history, and file transfer logs.
  - Single-file SQLite amalgamation source (`sqlite3.c` & `sqlite3.h`) included in `src/3rdparty/sqlite3/`.

- **Modern WhatsApp-Inspired Dark Theme**:
  - Polished dark UI styling (`#111b21` background, `#202c33` panels, `#005c4b` sent message bubbles, `#00a884` accents).
  - Split pane layout: Left pane for peer list with online status indicators, search filter, and unread badges; Right pane for active chat log, status, file attachment, and message composer.

- **First-Time Setup**:
  - On first launch, user is prompted to enter their display alias (full support for spaces and special characters).
  - Profile and settings (alias, listen port, polling interval, downloads folder) can be changed anytime via the Settings dialog (`⚙`).

- **Configurable Periodic Polling**:
  - Background polling timer (default 15 seconds, customizable in Settings).
  - Probes peer connectivity, updates online/offline status, and flushes any pending messages when peers reconnect.

---

## Directory Structure

```
chat/
├── CMakeLists.txt              # CMake build configuration
├── P2PChat.pro                 # qmake project configuration
├── build.sh                    # Automated qmake build script
├── run.sh                      # Convenience runner setting LD_LIBRARY_PATH
├── .gitignore                  # Git ignore rules
├── README.md                   # Complete documentation
├── resources/
│   ├── p2pchat.qrc             # Qt resource file
│   └── styles/
│       └── dark_theme.qss      # Dark theme stylesheet
├── src/
│   ├── main.cpp                # App entry point & CLI parser
│   ├── 3rdparty/
│   │   └── sqlite3/            # Single-file SQLite source amalgamation
│   │       ├── sqlite3.c
│   │       └── sqlite3.h
│   ├── models/
│   │   ├── Peer.h              # Peer data model
│   │   ├── Message.h           # Chat message model
│   │   └── FileTransfer.h      # File transfer model
│   ├── db/
│   │   ├── DatabaseManager.h   # SQLite manager with single-file DB
│   │   └── DatabaseManager.cpp
│   ├── network/
│   │   ├── Protocol.h          # TCP packet framing & types
│   │   ├── PeerConnection.h    # Per-socket packet streaming & file chunking
│   │   ├── PeerConnection.cpp
│   │   ├── NetworkManager.h    # P2P TCP server, connection pool, polling
│   │   └── NetworkManager.cpp
│   └── ui/
│       ├── MainWindow.h        # Split-pane main window
│       ├── MainWindow.cpp
│       ├── PeerListWidget.h    # Left pane peer list with search & unread badges
│       ├── PeerListWidget.cpp
│       ├── ChatAreaWidget.h    # Right pane chat view & composer
│       ├── ChatAreaWidget.cpp
│       ├── MessageBubbleWidget.h # Message bubble with WhatsApp tick states
│       ├── MessageBubbleWidget.cpp
│       ├── FileTransferBubbleWidget.h # File bubble with live progress bar
│       ├── FileTransferBubbleWidget.cpp
│       ├── AddPeerDialog.h     # IPv4 peer entry & automatic alias fetching
│       ├── AddPeerDialog.cpp
│       ├── FirstLaunchDialog.h # First-time alias setup
│       ├── FirstLaunchDialog.cpp
│       ├── SettingsDialog.h    # Application settings dialog
│       └── SettingsDialog.cpp
└── tests/
    └── test_p2p.cpp            # Comprehensive unit & integration test suite
```

---

## Building the Application

Qt 5 is located at `/run/media/rishav/DS1/qt/qt5.15/5.14.2/gcc_64`.

### Option A: Using the `build.sh` script (qmake)

```bash
./build.sh
```

### Option B: Using CMake

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/run/media/rishav/DS1/qt/qt5.15/5.14.2/gcc_64"
make -j$(nproc)
```

---

## Running the Application

### Single Instance

```bash
./run.sh
```

### Multi-Instance Local Testing (Same Machine)

You can launch multiple independent instances with custom ports, databases, and aliases:

**Instance 1 (Alice):**
```bash
./run.sh --port 42424 --db alice.db --alias "Alice in Wonderland"
```

**Instance 2 (Bob):**
```bash
./run.sh --port 42425 --db bob.db --alias "Bob Dylan"
```

1. In Alice's window, click **+ Add Peer**, enter `127.0.0.1:42425`, and click **Connect & Add**.
2. Alice automatically fetches Bob's alias (`"Bob Dylan"`), adds him to the peer list, and marks him online with a green indicator (`●`).
3. Send text messages back and forth:
   - Notice delivery shows two grey ticks (`✓✓`).
   - When Bob views the chat, Alice's ticks turn blue (`✓✓`).
4. Click **📎 Attach File** to send any file:
   - Progress bar streams automatically from 0% to 100%.
   - Recipient automatically receives the file in `Downloads/P2PChat/`.
   - Click "Open File" or "Show in Folder" once completed.
5. Close Bob's window to test offline queuing:
   - Alice sends a message: it is marked `🕒` (queued).
   - Reopen Bob's window: background polling (or reconnection) automatically flushes the message, Bob receives it, and Alice's ticks change to `✓✓`!

---

## Running the Test Suite

Run the automated Qt unit and integration test suite:

```bash
cd build
make test_p2p
./test_p2p
```

The test suite covers:
- User profile alias support with spaces
- Database peer management and online status updates
- Offline message queuing and chronological retrieval
- WhatsApp tick status transitions (`queued` -> `delivered` -> `read`)
- Network handshake & message delivery over TCP
- File transfer model & progress percentage calculations
- End-to-end multi-node live handshake, messaging, read receipts, and streamed file transfer verification via SHA256 checksums.
