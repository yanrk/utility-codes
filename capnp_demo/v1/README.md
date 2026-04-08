# Cap'n Proto RPC Demo

Multi-process RPC communication example based on Cap'n Proto 1.0.2.

## Architecture

```
                    ┌─────────────┐
                    │    Core     │
                    │  (Registry) │
                    │   :9000     │
                    └──────┬──────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
    ┌────▼────┐       ┌────▼────┐       ┌────▼────┐
    │  Ext1   │◄─────►│  Ext2   │◄─────►│  Ext3   │
    │ :9001   │       │ :9002   │       │ :9003   │
    └─────────┘       └─────────┘       └─────────┘
```

- **Core**: Central registry service that manages all Ext connections
- **Ext**: Extension node that registers to Core on startup and can communicate with other Exts

## Features

### Core -> Ext Communication
- Get Ext status (CPU, memory, uptime)
- Execute task commands
- Request Ext shutdown
- Health check (Ping)

### Ext -> Ext Communication
- Get other Ext's service interface through Core relay
- Query other Ext's system metrics
- Send data messages
- Execute query requests

## Requirements

- CMake >= 3.16
- C++17 compiler
- Cap'n Proto 1.0.2

### Install Cap'n Proto (macOS)

```bash
brew install capnp
```

### Install Cap'n Proto (Ubuntu/Debian)

```bash
sudo apt-get install capnproto libcapnp-dev
```

### Install Cap'n Proto 1.0.2 from source

```bash
curl -O https://capnproto.org/capnproto-c++-1.0.2.tar.gz
tar zxf capnproto-c++-1.0.2.tar.gz
cd capnproto-c++-1.0.2
./configure
make -j$(nproc)
sudo make install
```

## Build

```bash
./build.sh
```

Or build manually:

```bash
mkdir build && cd build
cmake ..
make
```

## Run

### Option 1: Use demo script (recommended)

```bash
./demo.sh
```

This will start Core and 3 Ext instances.

### Option 2: Manual startup

**Terminal 1 - Start Core:**
```bash
./run_core.sh
# or
./build/core 127.0.0.1:9000
```

**Terminal 2 - Start Ext1:**
```bash
./run_ext.sh ext1 9001
# or
./build/ext ext1 9001 127.0.0.1:9000 "Extension 1"
```

**Terminal 3 - Start Ext2:**
```bash
./run_ext.sh ext2 9002
# or
./build/ext ext2 9002 127.0.0.1:9000 "Extension 2"
```

## Interactive Commands

Available commands in Ext terminal:

| Command | Description |
|---------|-------------|
| `list` | List all registered Exts |
| `query <ext_id>` | Query status of specified Ext |
| `ping <ext_id>` | Ping specified Ext |
| `quit` | Exit |

### Example Interaction

```
[ext1] > list
[ext1] Registered Ext list:
  - ext1 (127.0.0.1:9001): Extension 1
  - ext2 (127.0.0.1:9002): Extension 2

[ext1] > query ext2
[ext1] Trying to connect to ext2...
[ext1] Received response: pong from ext2
[ext1] ext2 status:
  CPU: 23.5%
  Memory: 45.2%
[ext1] Query result: {"cpu":23.5,"memory":45.2,"uptime":120}
```

## Message Interface Documentation

### CoreService (Service provided by Core)

```capnp
interface CoreService {
    register @0 (info :ExtInfo) -> (success :Bool, message :Text);
    unregister @1 (extId :Text) -> (success :Bool);
    getExtInfo @2 (extId :Text) -> (found :Bool, info :ExtInfo);
    listExts @3 () -> (exts :List(ExtInfo));
    heartbeat @4 (extId :Text) -> (acknowledged :Bool);
    connectToExt @5 (targetExtId :Text) -> (service :ExtService);
}
```

### ExtService (Service provided by Ext)

```capnp
interface ExtService {
    getStatus @0 () -> (status :SystemStatus);
    executeTask @1 (request :TaskRequest) -> (result :TaskResult);
    shutdown @2 (reason :Text) -> (accepted :Bool);
    ping @3 () -> (pong :Text, timestamp :UInt64);
    sendData @4 (message :DataMessage) -> (received :Bool);
    query @5 (request :QueryRequest) -> (response :QueryResponse);
    getInfo @6 () -> (info :ExtInfo);
}
```

## Project Structure

```
capnp_demo/
├── CMakeLists.txt          # CMake build configuration
├── README.md               # This document
├── build.sh                # Build script
├── run_core.sh             # Core startup script
├── run_ext.sh              # Ext startup script
├── demo.sh                 # Demo script
├── proto/
│   └── messages.capnp      # Cap'n Proto interface definition
└── src/
    ├── core.cpp            # Core program implementation
    └── ext.cpp             # Ext program implementation
```

## Threading Model

Cap'n Proto uses a single-threaded async event loop model:
- Each program (Core/Ext) has its own event loop
- RPC calls are asynchronous using Promise/Future pattern
- All RPC handling is done in the same thread, no locks required

## License

MIT
