// ext.cpp - Extension service program
// As an extension node, register to Core and provide services
// Updated for Cap'n Proto 2.0

#include <capnp/rpc-twoparty.h>
#include <capnp/message.h>
#include <kj/async-io.h>
#include <kj/async-unix.h>
#include <kj/debug.h>

#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <csignal>
#include <array>
#include <memory>
#include <unistd.h>

#include "messages.capnp.h"

// Global variables
static std::atomic<bool> g_running{true};
static std::chrono::steady_clock::time_point g_startTime;

// Get current timestamp (milliseconds)
uint64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Get uptime (seconds)
uint64_t getUptime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - g_startTime
    ).count();
}

// Simulate getting CPU usage
float getCpuUsage() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(5.0f, 50.0f);
    return dis(gen);
}

// Simulate getting memory usage
float getMemoryUsage() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(20.0f, 60.0f);
    return dis(gen);
}

// Ext service implementation
class ExtServiceImpl final : public ExtService::Server {
public:
    ExtServiceImpl(const std::string& id, const std::string& address,
                   const std::string& description)
        : extId_(id), address_(address), description_(description) {
        std::cout << "[" << extId_ << "] Service instance created" << std::endl;
    }

    // Get system status
    kj::Promise<void> getStatus(GetStatusContext context) override {
        std::cout << "[" << extId_ << "] Received status query request" << std::endl;

        auto status = context.getResults().initStatus();
        status.setCpuUsage(getCpuUsage());
        status.setMemoryUsage(getMemoryUsage());
        status.setUptime(getUptime());
        status.setIsHealthy(g_running.load());

        return kj::READY_NOW;
    }

    // Execute task
    kj::Promise<void> executeTask(ExecuteTaskContext context) override {
        auto request = context.getParams().getRequest();
        std::string taskId = request.getTaskId().cStr();
        std::string command = request.getCommand().cStr();

        std::cout << "[" << extId_ << "] Received task execution request: "
                  << taskId << " - " << command << std::endl;

        auto result = context.getResults().initResult();
        result.setTaskId(taskId.c_str());

        // Simulate command execution
        if (command == "echo") {
            // Simple echo command
            std::ostringstream oss;
            auto args = request.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) oss << " ";
                oss << args[i].cStr();
            }
            result.setSuccess(true);
            result.setOutput(oss.str().c_str());
            result.setExitCode(0);
        } else if (command == "status") {
            // Return status info
            std::ostringstream oss;
            oss << "ExtId: " << extId_ << "\n"
                << "Uptime: " << getUptime() << "s\n"
                << "CPU: " << getCpuUsage() << "%\n"
                << "Memory: " << getMemoryUsage() << "%";
            result.setSuccess(true);
            result.setOutput(oss.str().c_str());
            result.setExitCode(0);
        } else if (command == "sleep") {
            // Simulate time-consuming operation
            auto args = request.getArgs();
            int seconds = 1;
            if (args.size() > 0) {
                seconds = std::atoi(args[0].cStr());
            }
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
            result.setSuccess(true);
            result.setOutput(kj::str("Slept for ", seconds, " seconds").cStr());
            result.setExitCode(0);
        } else {
            result.setSuccess(false);
            result.setErrorMessage(kj::str("Unknown command: ", command.c_str()).cStr());
            result.setExitCode(1);
        }

        std::cout << "[" << extId_ << "] Task " << taskId << " completed" << std::endl;
        return kj::READY_NOW;
    }

    // Shutdown request
    kj::Promise<void> shutdown(ShutdownContext context) override {
        auto reason = context.getParams().getReason().cStr();
        std::cout << "[" << extId_ << "] Received shutdown request, reason: " << reason << std::endl;

        g_running.store(false);
        context.getResults().setAccepted(true);

        std::cout << "[" << extId_ << "] Preparing to shutdown..." << std::endl;
        return kj::READY_NOW;
    }

    // Health check
    kj::Promise<void> ping(PingContext context) override {
        std::cout << "[" << extId_ << "] Received Ping request" << std::endl;

        context.getResults().setPong(kj::str("pong from ", extId_.c_str()).cStr());
        context.getResults().setTimestamp(getCurrentTimestamp());

        return kj::READY_NOW;
    }

    // Handle data message
    kj::Promise<void> sendData(SendDataContext context) override {
        auto message = context.getParams().getMessage();
        std::string sender = message.getSender().cStr();
        std::string messageId = message.getMessageId().cStr();

        std::cout << "[" << extId_ << "] Received data message from " << sender
                  << ": " << messageId << std::endl;

        // Process data (just print here)
        auto payload = message.getPayload();
        std::cout << "[" << extId_ << "] Data size: " << payload.size() << " bytes" << std::endl;

        context.getResults().setReceived(true);
        return kj::READY_NOW;
    }

    // Query handling
    kj::Promise<void> query(QueryContext context) override {
        auto request = context.getParams().getRequest();
        std::string queryId = request.getQueryId().cStr();
        std::string queryType = request.getQueryType().cStr();
        std::string parameters = request.getParameters().cStr();

        std::cout << "[" << extId_ << "] Received query request: "
                  << queryType << " (" << queryId << ")" << std::endl;

        auto response = context.getResults().initResponse();
        response.setQueryId(queryId.c_str());

        if (queryType == "metrics") {
            // Return system metrics
            std::ostringstream oss;
            oss << "{"
                << "\"cpu\":" << getCpuUsage() << ","
                << "\"memory\":" << getMemoryUsage() << ","
                << "\"uptime\":" << getUptime()
                << "}";
            response.setSuccess(true);
            response.setData(oss.str().c_str());
        } else if (queryType == "info") {
            // Return node info
            std::ostringstream oss;
            oss << "{"
                << "\"id\":\"" << extId_ << "\","
                << "\"address\":\"" << address_ << "\","
                << "\"description\":\"" << description_ << "\""
                << "}";
            response.setSuccess(true);
            response.setData(oss.str().c_str());
        } else {
            response.setSuccess(false);
            response.setErrorMessage(kj::str("Unknown query type: ", queryType.c_str()).cStr());
        }

        return kj::READY_NOW;
    }

    const std::string& getExtId() const { return extId_; }

private:
    std::string extId_;
    std::string address_;
    std::string description_;
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <ext_id> <ext_port> <core_address> [description]" << std::endl;
    std::cout << "Example: " << programName << " ext1 9001 127.0.0.1:9000 \"Extension 1\"" << std::endl;
}

// Command line help
void printHelp() {
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  status              - Show local status" << std::endl;
    std::cout << "  info                - Show node info" << std::endl;
    std::cout << "  query <ext_id>      - Query another Ext's info via Core" << std::endl;
    std::cout << "  ping <ext_id>       - Ping another Ext via Core" << std::endl;
    std::cout << "  help                - Show this help" << std::endl;
    std::cout << "  quit                - Exit Ext" << std::endl;
    std::cout << std::endl;
}

// Store ext info for command processing
static std::string g_extId;
static std::string g_extAddress;
static std::string g_description;

// Store Core service client for command use
static kj::Own<capnp::TwoPartyClient> g_rpcClient;
static kj::Maybe<CoreService::Client> g_coreService;

// Process command - returns promise of: true to continue, false to quit
kj::Promise<bool> processCommandAsync(const std::string& line) {
    if (line.empty()) return kj::Promise<bool>(true);

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "status") {
        std::cout << "[" << g_extId << "] Local status:" << std::endl
                  << "  CPU: " << getCpuUsage() << "%" << std::endl
                  << "  Memory: " << getMemoryUsage() << "%" << std::endl
                  << "  Uptime: " << getUptime() << "s" << std::endl
                  << "  Running: " << (g_running.load() ? "yes" : "no") << std::endl;
        return kj::Promise<bool>(true);
    } else if (cmd == "info") {
        std::cout << "[" << g_extId << "] Node info:" << std::endl
                  << "  ID: " << g_extId << std::endl
                  << "  Address: " << g_extAddress << std::endl
                  << "  Description: " << g_description << std::endl;
        return kj::Promise<bool>(true);
    } else if (cmd == "query") {
        std::string targetId;
        iss >> targetId;
        if (targetId.empty()) {
            std::cout << "Usage: query <ext_id>" << std::endl;
            return kj::Promise<bool>(true);
        }
        KJ_IF_SOME(coreService, g_coreService) {
            auto request = coreService.getExtInfoRequest();
            request.setExtId(targetId.c_str());
            return request.send()
                .then([targetId](auto response) {
                    if (response.getFound()) {
                        auto info = response.getInfo();
                        std::cout << "[" << g_extId << "] Ext " << targetId << " info:" << std::endl
                                  << "  ID: " << info.getId().cStr() << std::endl
                                  << "  Address: " << info.getAddress().cStr() << std::endl
                                  << "  Description: " << info.getDescription().cStr() << std::endl;
                    } else {
                        std::cout << "[" << g_extId << "] Ext '" << targetId << "' not found" << std::endl;
                    }
                    return true;
                }, [](kj::Exception&& e) {
                    std::cout << "[" << g_extId << "] Error: " << e.getDescription().cStr() << std::endl;
                    return true;
                });
        } else {
            std::cout << "[" << g_extId << "] Not connected to Core" << std::endl;
            return kj::Promise<bool>(true);
        }
    } else if (cmd == "ping") {
        std::string targetId;
        iss >> targetId;
        if (targetId.empty()) {
            std::cout << "Usage: ping <ext_id>" << std::endl;
            return kj::Promise<bool>(true);
        }
        KJ_IF_SOME(coreService, g_coreService) {
            // Get target Ext's service via Core
            auto request = coreService.connectToExtRequest();
            request.setTargetExtId(targetId.c_str());
            return request.send()
                .then([targetId](auto response) {
                    if (!response.hasService()) {
                        std::cout << "[" << g_extId << "] Ext '" << targetId << "' not found" << std::endl;
                        return kj::Promise<bool>(true);
                    }
                    auto extService = response.getService();
                    return extService.pingRequest().send()
                        .then([targetId](auto pingResponse) {
                            std::cout << "[" << g_extId << "] " << pingResponse.getPong().cStr() << std::endl;
                            return true;
                        });
                }, [](kj::Exception&& e) {
                    std::cout << "[" << g_extId << "] Error: " << e.getDescription().cStr() << std::endl;
                    return true;
                });
        } else {
            std::cout << "[" << g_extId << "] Not connected to Core" << std::endl;
            return kj::Promise<bool>(true);
        }
    } else if (cmd == "help") {
        printHelp();
        return kj::Promise<bool>(true);
    } else if (cmd == "quit" || cmd == "exit") {
        std::cout << "[" << g_extId << "] Exiting..." << std::endl;
        g_running.store(false);
        return kj::Promise<bool>(false);
    } else {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands." << std::endl;
        return kj::Promise<bool>(true);
    }
}

// Async command reader class
class CommandReader {
public:
    CommandReader(kj::AsyncInputStream& input)
        : input_(input), lineBuffer_() {}

    kj::Promise<void> readLoop() {
        if (!g_running.load()) {
            return kj::READY_NOW;
        }
        std::cout << "[" << g_extId << "] > " << std::flush;

        return input_.tryRead(buffer_, 1, sizeof(buffer_))
            .then([this](size_t bytesRead) -> kj::Promise<void> {
                if (!g_running.load()) {
                    return kj::READY_NOW;
                }
                if (bytesRead == 0) {
                    return g_running.load() ? readLoop() : kj::READY_NOW;
                }
                return processBuffer(0, bytesRead);
            }, [this](kj::Exception&& e) -> kj::Promise<void> {
                if (!g_running.load()) {
                    return kj::READY_NOW;
                }
                return readLoop();
            });
    }

private:
    kj::Promise<void> processBuffer(size_t idx, size_t bytesRead) {
        while (idx < bytesRead) {
            char c = buffer_[idx++];
            if (c == '\n') {
                std::string line = lineBuffer_;
                lineBuffer_.clear();
                return processCommandAsync(line)
                    .then([this, idx, bytesRead](bool shouldContinue) -> kj::Promise<void> {
                        if (!shouldContinue || !g_running.load()) {
                            return kj::READY_NOW;
                        }
                        if (idx < bytesRead) {
                            return processBuffer(idx, bytesRead);
                        }
                        return readLoop();
                    });
            } else if (c != '\r') {
                lineBuffer_ += c;
            }
        }
        return readLoop();
    }

    kj::AsyncInputStream& input_;
    std::string lineBuffer_;
    char buffer_[256];
};

// Async command reading
kj::Promise<void> readCommands(kj::AsyncInputStream& input) {
    auto reader = kj::heap<CommandReader>(input);
    auto& readerRef = *reader;
    return readerRef.readLoop().attach(kj::mv(reader));
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string extId = argv[1];
    std::string extPort = argv[2];
    std::string coreAddress = argv[3];
    std::string description = (argc >= 5) ? argv[4] : ("Extension " + extId);

    std::string extAddress = "127.0.0.1:" + extPort;
    uint16_t port = std::stoi(extPort);

    // Set global variables for command processing
    g_extId = extId;
    g_extAddress = extAddress;
    g_description = description;

    // Parse core address
    size_t colonPos = coreAddress.find(':');
    std::string coreHost = coreAddress.substr(0, colonPos);
    uint16_t corePort = std::stoi(coreAddress.substr(colonPos + 1));

    std::cout << "=================================" << std::endl;
    std::cout << "  Cap'n Proto RPC - Ext Service" << std::endl;
    std::cout << "        (Cap'n Proto 2.0)" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Ext ID: " << extId << std::endl;
    std::cout << "Listen address: " << extAddress << std::endl;
    std::cout << "Core address: " << coreAddress << std::endl;
    std::cout << "Description: " << description << std::endl;

    g_startTime = std::chrono::steady_clock::now();

    // Create async I/O context
    auto ioContext = kj::setupAsyncIo();
    auto& network = ioContext.provider->getNetwork();

    // Create service implementation
    auto serviceImpl = kj::heap<ExtServiceImpl>(extId, extAddress, description);

    // Create TwoPartyServer
    capnp::TwoPartyServer server(kj::mv(serviceImpl));

    std::cout << "[" << extId << "] Service started" << std::endl;

    // Start server in background
    auto serverPromise = network.parseAddress("127.0.0.1", port)
        .then([&server](kj::Own<kj::NetworkAddress> addr) {
            auto listener = addr->listen();
            return server.listen(*listener).attach(kj::mv(listener), kj::mv(addr));
        });

    // Connect to Core and register
    auto clientPromise = network.parseAddress(coreHost.c_str(), corePort)
        .then([&ioContext, extId, extAddress, description](kj::Own<kj::NetworkAddress> addr) {
            return addr->connect().attach(kj::mv(addr));
        })
        .then([extId, extAddress, description](kj::Own<kj::AsyncIoStream> stream) mutable {
            std::cout << "[" << extId << "] Connected to Core" << std::endl;

            auto client = kj::heap<capnp::TwoPartyClient>(*stream);
            auto coreService = client->bootstrap().castAs<CoreService>();

            // Save Core service for command use
            g_coreService = coreService;

            // Register to Core
            auto request = coreService.registerRequest();
            auto info = request.initInfo();
            info.setId(extId.c_str());
            info.setAddress(extAddress.c_str());
            info.setDescription(description.c_str());

            std::cout << "[" << extId << "] Registering to Core..." << std::endl;

            return request.send()
                .then([extId](auto response) {
                    if (response.getSuccess()) {
                        std::cout << "[" << extId << "] Registration successful: "
                                  << response.getMessage().cStr() << std::endl;
                    } else {
                        std::cout << "[" << extId << "] Registration failed: "
                                  << response.getMessage().cStr() << std::endl;
                        g_running.store(false);
                    }
                })
                .then([client = kj::mv(client), stream = kj::mv(stream)]() mutable {
                    // Keep connection alive - store stream with client
                    return client->onDisconnect().attach(kj::mv(client), kj::mv(stream));
                });
        });

    auto exitPromise = ioContext.unixEventPort.onSignal(SIGINT)
        .then([extId](siginfo_t) {
            std::cout << "\n[" << extId << "] Received Ctrl+C, shutting down..." << std::endl;
            g_running.store(false);
        });

    // Async stdin reading
    auto stdinStream = ioContext.lowLevelProvider->wrapInputFd(STDIN_FILENO);
    auto commandPromise = readCommands(*stdinStream).attach(kj::mv(stdinStream));

    printHelp();
    std::cout << "[Info] Server is running, type 'quit' or press Ctrl+C to exit\n" << std::endl;

    // Wait for any to complete
    serverPromise
        .exclusiveJoin(kj::mv(clientPromise))
        .exclusiveJoin(kj::mv(exitPromise))
        .exclusiveJoin(kj::mv(commandPromise))
        .wait(ioContext.waitScope);

    std::cout << "[" << extId << "] Server stopped." << std::endl;
    return 0;
}
