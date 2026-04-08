// core.cpp - Core service program
// Central registry service that manages all Ext connections
// Updated for Cap'n Proto 2.0

#include <capnp/rpc-twoparty.h>
#include <capnp/message.h>
#include <kj/async-io.h>
#include <kj/async-unix.h>
#include <kj/debug.h>

#include <iostream>
#include <sstream>
#include <csignal>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <unistd.h>

#include "messages.capnp.h"

// Store registered Ext information
struct RegisteredExt {
    std::string id;
    std::string address;
    std::string description;
    std::chrono::steady_clock::time_point lastHeartbeat;
    ExtService::Client serviceClient;
    kj::Own<capnp::TwoPartyClient> rpcClient;

    RegisteredExt() : serviceClient(nullptr) {}
    RegisteredExt(RegisteredExt&&) = default;
    RegisteredExt& operator=(RegisteredExt&&) = default;
};

// Forward declaration
class CoreServiceImpl;

// Global pointers for connection management
static kj::AsyncIoContext* g_ioContext = nullptr;
static std::map<std::string, RegisteredExt>* g_registeredExts = nullptr;
static kj::Vector<kj::Own<kj::AsyncIoStream>>* g_connections = nullptr;

// Core service implementation
class CoreServiceImpl final : public CoreService::Server {
public:
    CoreServiceImpl() = default;

    // Ext registration
    kj::Promise<void> register_(RegisterContext context) override {
        auto info = context.getParams().getInfo();
        std::string extId = info.getId().cStr();
        std::string address = info.getAddress().cStr();
        std::string description = info.getDescription().cStr();

        std::cout << "[Core] Ext registration request: id=" << extId
                  << ", address=" << address << std::endl;

        // Check if already registered
        if (g_registeredExts->find(extId) != g_registeredExts->end()) {
            context.getResults().setSuccess(false);
            context.getResults().setMessage("Ext ID already registered");
            std::cout << "[Core] Registration failed: ID already exists" << std::endl;
            return kj::READY_NOW;
        }

        // Parse address
        size_t colonPos = address.find(':');
        if (colonPos == std::string::npos) {
            context.getResults().setSuccess(false);
            context.getResults().setMessage("Invalid address format");
            return kj::READY_NOW;
        }

        std::string host = address.substr(0, colonPos);
        uint16_t port = std::stoi(address.substr(colonPos + 1));

        // Connect to Ext service asynchronously
        auto& network = g_ioContext->provider->getNetwork();
        return network.parseAddress(host.c_str(), port)
            .then([extId, address, description, context](
                      kj::Own<kj::NetworkAddress> addr) mutable {
                return addr->connect().attach(kj::mv(addr))
                    .then([extId, address, description, context](
                              kj::Own<kj::AsyncIoStream> stream) mutable {
                        // Create TwoPartyClient
                        auto client = kj::heap<capnp::TwoPartyClient>(*stream);
                        auto bootstrap = client->bootstrap().castAs<ExtService>();

                        // Store connection and client
                        RegisteredExt ext;
                        ext.id = extId;
                        ext.address = address;
                        ext.description = description;
                        ext.lastHeartbeat = std::chrono::steady_clock::now();
                        ext.serviceClient = bootstrap;
                        ext.rpcClient = kj::mv(client);

                        g_connections->add(kj::mv(stream));
                        (*g_registeredExts)[extId] = kj::mv(ext);

                        context.getResults().setSuccess(true);
                        context.getResults().setMessage("Registration successful");
                        std::cout << "[Core] Ext " << extId << " registered successfully" << std::endl;
                    });
            }).catch_([context, extId](kj::Exception&& e) mutable {
                context.getResults().setSuccess(false);
                context.getResults().setMessage(
                    kj::str("Failed to connect: ", e.getDescription()).cStr());
                std::cout << "[Core] Failed to connect to Ext " << extId << ": "
                          << e.getDescription().cStr() << std::endl;
            });
    }

    // Ext unregister
    kj::Promise<void> unregister(UnregisterContext context) override {
        std::string extId = context.getParams().getExtId().cStr();

        auto it = g_registeredExts->find(extId);
        if (it != g_registeredExts->end()) {
            g_registeredExts->erase(it);
            context.getResults().setSuccess(true);
            std::cout << "[Core] Ext " << extId << " unregistered" << std::endl;
        } else {
            context.getResults().setSuccess(false);
            std::cout << "[Core] Unregister failed: Ext " << extId << " not found" << std::endl;
        }

        return kj::READY_NOW;
    }

    // Get specified Ext info
    kj::Promise<void> getExtInfo(GetExtInfoContext context) override {
        std::string extId = context.getParams().getExtId().cStr();

        auto it = g_registeredExts->find(extId);
        if (it != g_registeredExts->end()) {
            const auto& ext = it->second;
            context.getResults().setFound(true);
            auto info = context.getResults().initInfo();
            info.setId(ext.id.c_str());
            info.setAddress(ext.address.c_str());
            info.setDescription(ext.description.c_str());
        } else {
            context.getResults().setFound(false);
        }

        return kj::READY_NOW;
    }

    // List all registered Exts
    kj::Promise<void> listExts(ListExtsContext context) override {
        auto exts = context.getResults().initExts(g_registeredExts->size());

        size_t i = 0;
        for (const auto& [id, ext] : *g_registeredExts) {
            exts[i].setId(ext.id.c_str());
            exts[i].setAddress(ext.address.c_str());
            exts[i].setDescription(ext.description.c_str());
            ++i;
        }

        std::cout << "[Core] Returning " << g_registeredExts->size() << " registered Ext(s)" << std::endl;
        return kj::READY_NOW;
    }

    // Heartbeat handling
    kj::Promise<void> heartbeat(HeartbeatContext context) override {
        std::string extId = context.getParams().getExtId().cStr();

        auto it = g_registeredExts->find(extId);
        if (it != g_registeredExts->end()) {
            it->second.lastHeartbeat = std::chrono::steady_clock::now();
            context.getResults().setAcknowledged(true);
        } else {
            context.getResults().setAcknowledged(false);
        }

        return kj::READY_NOW;
    }

    // Connect to another Ext (return that Ext's service interface)
    kj::Promise<void> connectToExt(ConnectToExtContext context) override {
        std::string targetExtId = context.getParams().getTargetExtId().cStr();

        auto it = g_registeredExts->find(targetExtId);
        if (it != g_registeredExts->end()) {
            context.getResults().setService(it->second.serviceClient);
            std::cout << "[Core] Providing connection to " << targetExtId << std::endl;
        } else {
            std::cout << "[Core] Target Ext '" << targetExtId << "' not found" << std::endl;
        }

        return kj::READY_NOW;
    }
};

// Global running flag
static std::atomic<bool> g_running{true};

// Command line interaction
void printHelp() {
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  list                - List all registered Exts" << std::endl;
    std::cout << "  status <ext_id>     - Get status of specified Ext" << std::endl;
    std::cout << "  ping <ext_id>       - Ping specified Ext" << std::endl;
    std::cout << "  shutdown <ext_id>   - Request specified Ext to shutdown" << std::endl;
    std::cout << "  help                - Show this help" << std::endl;
    std::cout << "  quit                - Exit Core" << std::endl;
    std::cout << std::endl;
}

// Process a single command line - async version (returns promise of: true to continue, false to quit)
kj::Promise<bool> processCommandAsync(const std::string& line) {
    if (line.empty()) return kj::Promise<bool>(true);

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "list") {
        std::cout << "[Core] Registered Ext list:" << std::endl;
        if (g_registeredExts->empty()) {
            std::cout << "  (none)" << std::endl;
        } else {
            for (const auto& [id, ext] : *g_registeredExts) {
                std::cout << "  - " << id << " (" << ext.address << "): "
                          << ext.description << std::endl;
            }
        }
        return kj::Promise<bool>(true);
    } else if (cmd == "status") {
        std::string extId;
        iss >> extId;
        if (extId.empty()) {
            std::cout << "Usage: status <ext_id>" << std::endl;
            return kj::Promise<bool>(true);
        }
        auto it = g_registeredExts->find(extId);
        if (it == g_registeredExts->end()) {
            std::cout << "[Core] Ext '" << extId << "' not found" << std::endl;
            return kj::Promise<bool>(true);
        }
        return it->second.serviceClient.getStatusRequest().send()
            .then([extId](auto response) {
                auto status = response.getStatus();
                std::cout << "[Core] Ext " << extId << " status:" << std::endl
                          << "  CPU: " << status.getCpuUsage() << "%" << std::endl
                          << "  Memory: " << status.getMemoryUsage() << "%" << std::endl
                          << "  Uptime: " << status.getUptime() << "s" << std::endl
                          << "  Healthy: " << (status.getIsHealthy() ? "yes" : "no") << std::endl;
                return true;
            }, [](kj::Exception&& e) {
                std::cout << "[Core] Error: " << e.getDescription().cStr() << std::endl;
                return true;
            });
    } else if (cmd == "ping") {
        std::string extId;
        iss >> extId;
        if (extId.empty()) {
            std::cout << "Usage: ping <ext_id>" << std::endl;
            return kj::Promise<bool>(true);
        }
        auto it = g_registeredExts->find(extId);
        if (it == g_registeredExts->end()) {
            std::cout << "[Core] Ext '" << extId << "' not found" << std::endl;
            return kj::Promise<bool>(true);
        }
        return it->second.serviceClient.pingRequest().send()
            .then([](auto response) {
                std::cout << "[Core] " << response.getPong().cStr() << std::endl;
                return true;
            }, [](kj::Exception&& e) {
                std::cout << "[Core] Error: " << e.getDescription().cStr() << std::endl;
                return true;
            });
    } else if (cmd == "shutdown") {
        std::string extId;
        iss >> extId;
        if (extId.empty()) {
            std::cout << "Usage: shutdown <ext_id>" << std::endl;
            return kj::Promise<bool>(true);
        }
        auto it = g_registeredExts->find(extId);
        if (it == g_registeredExts->end()) {
            std::cout << "[Core] Ext '" << extId << "' not found" << std::endl;
            return kj::Promise<bool>(true);
        }
        auto request = it->second.serviceClient.shutdownRequest();
        request.setReason("Requested by Core");
        return request.send()
            .then([extId](auto response) {
                if (response.getAccepted()) {
                    std::cout << "[Core] Ext " << extId << " accepted shutdown request" << std::endl;
                    g_registeredExts->erase(extId);
                } else {
                    std::cout << "[Core] Ext " << extId << " rejected shutdown request" << std::endl;
                }
                return true;
            }, [](kj::Exception&& e) {
                std::cout << "[Core] Error: " << e.getDescription().cStr() << std::endl;
                return true;
            });
    } else if (cmd == "help") {
        printHelp();
        return kj::Promise<bool>(true);
    } else if (cmd == "quit" || cmd == "exit") {
        std::cout << "[Core] Exiting..." << std::endl;
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
        std::cout << "[Core] > " << std::flush;

        return input_.tryRead(buffer_, 1, sizeof(buffer_))
            .then([this](size_t bytesRead) -> kj::Promise<void> {
                if (!g_running.load()) {
                    return kj::READY_NOW;
                }
                if (bytesRead == 0) {
                    // EOF or interrupted - continue if still running
                    return g_running.load() ? readLoop() : kj::READY_NOW;
                }

                // Process characters, accumulating into lineBuffer_
                return processBuffer(0, bytesRead);
            }, [this](kj::Exception&& e) -> kj::Promise<void> {
                // Read interrupted (e.g., by signal)
                if (!g_running.load()) {
                    return kj::READY_NOW;
                }
                return readLoop();
            });
    }

private:
    kj::Promise<void> processBuffer(size_t idx, size_t bytesRead) {
        // Process remaining characters in the buffer
        while (idx < bytesRead) {
            char c = buffer_[idx++];
            if (c == '\n') {
                // Found a complete line - process it asynchronously
                std::string line = lineBuffer_;
                lineBuffer_.clear();
                return processCommandAsync(line)
                    .then([this, idx, bytesRead](bool shouldContinue) -> kj::Promise<void> {
                        if (!shouldContinue || !g_running.load()) {
                            return kj::READY_NOW;
                        }
                        // Continue processing remaining buffer
                        if (idx < bytesRead) {
                            return processBuffer(idx, bytesRead);
                        }
                        // Buffer exhausted, read more
                        return readLoop();
                    });
            } else if (c != '\r') {
                lineBuffer_ += c;
            }
        }
        // Buffer exhausted without newline, read more
        return readLoop();
    }

    kj::AsyncInputStream& input_;
    std::string lineBuffer_;
    char buffer_[256];
};

// Async version of command reading
kj::Promise<void> readCommands(kj::AsyncInputStream& input) {
    auto reader = kj::heap<CommandReader>(input);
    auto& readerRef = *reader;
    return readerRef.readLoop().attach(kj::mv(reader));
}

int main(int argc, char* argv[]) {
    std::string bindAddress = "127.0.0.1";
    uint16_t bindPort = 9000;

    if (argc >= 2) {
        std::string addr = argv[1];
        size_t colonPos = addr.find(':');
        if (colonPos != std::string::npos) {
            bindAddress = addr.substr(0, colonPos);
            bindPort = std::stoi(addr.substr(colonPos + 1));
        }
    }

    std::cout << "=================================" << std::endl;
    std::cout << "  Cap'n Proto RPC - Core Service" << std::endl;
    std::cout << "        (Cap'n Proto 2.0)" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Listen address: " << bindAddress << ":" << bindPort << std::endl;

    // Create async I/O context
    auto ioContext = kj::setupAsyncIo();
    g_ioContext = &ioContext;

    // Storage for registered extensions and connections
    std::map<std::string, RegisteredExt> registeredExts;
    kj::Vector<kj::Own<kj::AsyncIoStream>> connections;
    g_registeredExts = &registeredExts;
    g_connections = &connections;

    // Create service implementation
    auto serviceImpl = kj::heap<CoreServiceImpl>();

    // Create TwoPartyServer
    capnp::TwoPartyServer server(kj::mv(serviceImpl));

    // Start listening
    auto& network = ioContext.provider->getNetwork();

    std::cout << "Core service started, waiting for Ext connections..." << std::endl;
    printHelp();
    std::cout << "[Info] Server is running, type 'quit' or press Ctrl+C to exit\n" << std::endl;

    // Setup Ctrl+C (SIGINT) handler
    auto& unixEventPort = ioContext.unixEventPort;
    auto signalPromise = unixEventPort.onSignal(SIGINT)
        .then([](siginfo_t) {
            std::cout << "\n[Core] Received Ctrl+C, shutting down gracefully..." << std::endl;
            g_running.store(false);
        });

    // Server listen promise
    auto serverPromise = network.parseAddress(bindAddress.c_str(), bindPort)
        .then([&server](kj::Own<kj::NetworkAddress> addr) {
            auto listener = addr->listen();
            return server.listen(*listener).attach(kj::mv(listener), kj::mv(addr));
        });

    // Wrap stdin for async reading
    auto stdinStream = ioContext.lowLevelProvider->wrapInputFd(STDIN_FILENO);
    auto commandPromise = readCommands(*stdinStream)
        .attach(kj::mv(stdinStream));

    // Wait for any of: server error, signal, or quit command
    serverPromise
        .exclusiveJoin(kj::mv(signalPromise))
        .exclusiveJoin(kj::mv(commandPromise))
        .wait(ioContext.waitScope);

    std::cout << "[Core] Server stopped." << std::endl;
    return 0;
}
