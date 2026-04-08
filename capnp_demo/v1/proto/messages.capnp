@0xb5e6e8c6e5a7c3d1;

# System status information
struct SystemStatus {
    cpuUsage @0 :Float32;      # CPU usage (0-100)
    memoryUsage @1 :Float32;   # Memory usage (0-100)
    uptime @2 :UInt64;         # Uptime (seconds)
    isHealthy @3 :Bool;        # Is healthy
}

# Extension node info (for registration)
struct ExtInfo {
    id @0 :Text;               # Extension ID (e.g., "ext1", "ext2")
    address @1 :Text;          # Listen address (e.g., "127.0.0.1:9001")
    description @2 :Text;      # Description
}

# Task execution request
struct TaskRequest {
    taskId @0 :Text;           # Task ID
    command @1 :Text;          # Command to execute
    args @2 :List(Text);       # Command arguments
    timeout @3 :UInt32;        # Timeout (seconds)
}

# Task execution result
struct TaskResult {
    taskId @0 :Text;
    success @1 :Bool;
    output @2 :Text;
    errorMessage @3 :Text;
    exitCode @4 :Int32;
}

# Generic data message (for inter-extension communication)
struct DataMessage {
    messageId @0 :Text;
    sender @1 :Text;
    payload @2 :Data;
    timestamp @3 :UInt64;
}

# Query request
struct QueryRequest {
    queryId @0 :Text;
    queryType @1 :Text;        # Query type
    parameters @2 :Text;       # JSON formatted parameters
}

# Query response
struct QueryResponse {
    queryId @0 :Text;
    success @1 :Bool;
    data @2 :Text;             # JSON formatted response data
    errorMessage @3 :Text;
}

# ============== RPC Interface Definition ==============

# Service interface provided by Core to Ext
interface CoreService {
    # Ext registers itself to Core
    register @0 (info :ExtInfo) -> (success :Bool, message :Text);

    # Ext unregisters itself
    unregister @1 (extId :Text) -> (success :Bool);

    # Get connection info of other Ext (for direct connection)
    getExtInfo @2 (extId :Text) -> (found :Bool, info :ExtInfo);

    # Get list of all registered Exts
    listExts @3 () -> (exts :List(ExtInfo));

    # Heartbeat
    heartbeat @4 (extId :Text) -> (acknowledged :Bool);

    # Get another Ext's service capability (returns ExtService interface)
    connectToExt @5 (targetExtId :Text) -> (service :ExtService);
}

# Service interface provided by Ext (for Core and other Exts to call)
interface ExtService {
    # Get system status
    getStatus @0 () -> (status :SystemStatus);

    # Execute task
    executeTask @1 (request :TaskRequest) -> (result :TaskResult);

    # Request shutdown
    shutdown @2 (reason :Text) -> (accepted :Bool);

    # Health check
    ping @3 () -> (pong :Text, timestamp :UInt64);

    # Send data message
    sendData @4 (message :DataMessage) -> (received :Bool);

    # Query
    query @5 (request :QueryRequest) -> (response :QueryResponse);

    # Get extension info
    getInfo @6 () -> (info :ExtInfo);
}
