// ext.cpp - Extension service program
// As an extension node, register to Core and provide services

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <kj/async-io.h>
#include <kj/debug.h>

#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <array>
#include <memory>

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
        result.setTaskId(taskId);

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
            result.setOutput(oss.str());
            result.setExitCode(0);
        } else if (command == "status") {
            // Return status info
            std::ostringstream oss;
            oss << "ExtId: " << extId_ << "\n"
                << "Uptime: " << getUptime() << "s\n"
                << "CPU: " << getCpuUsage() << "%\n"
                << "Memory: " << getMemoryUsage() << "%";
            result.setSuccess(true);
            result.setOutput(oss.str());
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
            result.setErrorMessage(kj::str("Unknown command: ", command).cStr());
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

        context.getResults().setPong(kj::str("pong from ", extId_).cStr());
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
        response.setQueryId(queryId);

        if (queryType == "metrics") {
            // Return system metrics
            std::ostringstream oss;
            oss << "{"
                << "\"cpu\":" << getCpuUsage() << ","
                << "\"memory\":" << getMemoryUsage() << ","
                << "\"uptime\":" << getUptime()
                << "}";
            response.setSuccess(true);
            response.setData(oss.str());
        } else if (queryType == "info") {
            // Return node info
            std::ostringstream oss;
            oss << "{"
                << "\"id\":\"" << extId_ << "\","
                << "\"address\":\"" << address_ << "\","
                << "\"description\":\"" << description_ << "\""
                << "}";
            response.setSuccess(true);
            response.setData(oss.str());
        } else {
            response.setSuccess(false);
            response.setErrorMessage(kj::str("Unknown query type: ", queryType).cStr());
        }

        return kj::READY_NOW;
    }

    // Get node info
    kj::Promise<void> getInfo(GetInfoContext context) override {
        std::cout << "[" << extId_ << "] Received info query request" << std::endl;

        auto info = context.getResults().initInfo();
        info.setId(extId_);
        info.setAddress(address_);
        info.setDescription(description_);

        return kj::READY_NOW;
    }

    const std::string& getExtId() const { return extId_; }

private:
    std::string extId_;
    std::string address_;
    std::string description_;
};

// Register to Core
bool registerToCore(capnp::EzRpcClient& coreClient,
                    const std::string& extId,
                    const std::string& extAddress,
                    const std::string& description,
                    kj::WaitScope& waitScope) {
    auto coreService = coreClient.getMain<CoreService>();

    auto request = coreService.registerRequest();
    auto info = request.initInfo();
    info.setId(extId);
    info.setAddress(extAddress);
    info.setDescription(description);

    std::cout << "[" << extId << "] Registering to Core..." << std::endl;

    auto response = request.send().wait(waitScope);

    if (response.getSuccess()) {
        std::cout << "[" << extId << "] Registration successful: "
                  << response.getMessage().cStr() << std::endl;
        return true;
    } else {
        std::cout << "[" << extId << "] Registration failed: "
                  << response.getMessage().cStr() << std::endl;
        return false;
    }
}

// Query other Ext and send message
void queryOtherExt(capnp::EzRpcClient& coreClient,
                   const std::string& myExtId,
                   const std::string& targetExtId,
                   kj::WaitScope& waitScope) {
    auto coreService = coreClient.getMain<CoreService>();

    // Get target Ext service through Core
    auto connectRequest = coreService.connectToExtRequest();
    connectRequest.setTargetExtId(targetExtId);

    std::cout << "[" << myExtId << "] Trying to connect to " << targetExtId << "..." << std::endl;

    auto connectResponse = connectRequest.send().wait(waitScope);
    auto targetService = connectResponse.getService();

    // Send ping
    auto pingResponse = targetService.pingRequest().send().wait(waitScope);
    std::cout << "[" << myExtId << "] Received response: "
              << pingResponse.getPong().cStr() << std::endl;

    // Query status
    auto statusResponse = targetService.getStatusRequest().send().wait(waitScope);
    auto status = statusResponse.getStatus();
    std::cout << "[" << myExtId << "] " << targetExtId << " status:" << std::endl
              << "  CPU: " << status.getCpuUsage() << "%" << std::endl
              << "  Memory: " << status.getMemoryUsage() << "%" << std::endl;

    // Send query
    auto queryRequest = targetService.queryRequest();
    auto queryReq = queryRequest.initRequest();
    queryReq.setQueryId("q-001");
    queryReq.setQueryType("metrics");
    queryReq.setParameters("{}");

    auto queryResponse = queryRequest.send().wait(waitScope);
    auto queryResp = queryResponse.getResponse();
    std::cout << "[" << myExtId << "] Query result: "
              << queryResp.getData().cStr() << std::endl;
}

// List all registered Exts
void listAllExts(capnp::EzRpcClient& coreClient,
                 const std::string& myExtId,
                 kj::WaitScope& waitScope) {
    auto coreService = coreClient.getMain<CoreService>();

    auto response = coreService.listExtsRequest().send().wait(waitScope);
    auto exts = response.getExts();

    std::cout << "[" << myExtId << "] Registered Ext list:" << std::endl;
    for (auto ext : exts) {
        std::cout << "  - " << ext.getId().cStr()
                  << " (" << ext.getAddress().cStr() << ")"
                  << ": " << ext.getDescription().cStr() << std::endl;
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <ext_id> <ext_port> <core_address> [description]" << std::endl;
    std::cout << "Example: " << programName << " ext1 9001 127.0.0.1:9000 \"Extension 1\"" << std::endl;
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

    std::cout << "=================================" << std::endl;
    std::cout << "  Cap'n Proto RPC - Ext Service" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Ext ID: " << extId << std::endl;
    std::cout << "Listen address: " << extAddress << std::endl;
    std::cout << "Core address: " << coreAddress << std::endl;
    std::cout << "Description: " << description << std::endl;

    g_startTime = std::chrono::steady_clock::now();

    // Create Ext service implementation
    auto serviceImpl = kj::heap<ExtServiceImpl>(extId, extAddress, description);

    // Create Ext RPC server (EzRpcServer creates its own EventLoop)
    capnp::EzRpcServer server(kj::mv(serviceImpl), extAddress);
    auto& serverWaitScope = server.getWaitScope();

    std::cout << "[" << extId << "] Service started" << std::endl;

    // Run client in separate thread (because one thread can only have one EventLoop)
    std::thread clientThread([&]() {
        try {
            // Client uses its own EventLoop
            capnp::EzRpcClient coreClient(coreAddress);
            auto& clientWaitScope = coreClient.getWaitScope();

            auto coreService = coreClient.getMain<CoreService>();

            // Register to Core
            {
                auto request = coreService.registerRequest();
                auto info = request.initInfo();
                info.setId(extId);
                info.setAddress(extAddress);
                info.setDescription(description);

                std::cout << "[" << extId << "] Registering to Core..." << std::endl;

                auto response = request.send().wait(clientWaitScope);

                if (response.getSuccess()) {
                    std::cout << "[" << extId << "] Registration successful: "
                              << response.getMessage().cStr() << std::endl;
                } else {
                    std::cout << "[" << extId << "] Registration failed: "
                              << response.getMessage().cStr() << std::endl;
                    g_running.store(false);
                    return;
                }
            }

            // Interactive loop
            std::cout << "\nAvailable commands:" << std::endl;
            std::cout << "  list              - List all registered Exts" << std::endl;
            std::cout << "  query <ext_id>    - Query status of specified Ext" << std::endl;
            std::cout << "  quit              - Exit" << std::endl;
            std::cout << std::endl;

            std::string line;
            while (g_running.load()) {
                std::cout << "[" << extId << "] > ";
                std::cout.flush();

                if (!std::getline(std::cin, line)) {
                    break;
                }

                if (line.empty()) continue;

                std::istringstream iss(line);
                std::string cmd;
                iss >> cmd;

                try {
                    if (cmd == "list") {
                        auto response = coreService.listExtsRequest().send().wait(clientWaitScope);
                        auto exts = response.getExts();

                        std::cout << "[" << extId << "] Registered Ext list:" << std::endl;
                        for (auto ext : exts) {
                            std::cout << "  - " << ext.getId().cStr()
                                      << " (" << ext.getAddress().cStr() << ")"
                                      << ": " << ext.getDescription().cStr() << std::endl;
                        }
                    } else if (cmd == "query" || cmd == "ping") {
                        std::string targetId;
                        iss >> targetId;
                        if (targetId.empty()) {
                            std::cout << "Please specify target Ext ID" << std::endl;
                        } else if (targetId == extId) {
                            std::cout << "Cannot query self" << std::endl;
                        } else {
                            auto connectRequest = coreService.connectToExtRequest();
                            connectRequest.setTargetExtId(targetId);

                            std::cout << "[" << extId << "] Trying to connect to " << targetId << "..." << std::endl;

                            auto connectResponse = connectRequest.send().wait(clientWaitScope);
                            auto targetService = connectResponse.getService();

                            auto pingResponse = targetService.pingRequest().send().wait(clientWaitScope);
                            std::cout << "[" << extId << "] Received response: "
                                      << pingResponse.getPong().cStr() << std::endl;

                            auto statusResponse = targetService.getStatusRequest().send().wait(clientWaitScope);
                            auto status = statusResponse.getStatus();
                            std::cout << "[" << extId << "] " << targetId << " status:" << std::endl
                                      << "  CPU: " << status.getCpuUsage() << "%" << std::endl
                                      << "  Memory: " << status.getMemoryUsage() << "%" << std::endl
                                      << "  Uptime: " << status.getUptime() << "s" << std::endl;
                        }
                    } else if (cmd == "quit" || cmd == "exit") {
                        auto req = coreService.unregisterRequest();
                        req.setExtId(extId);
                        req.send().wait(clientWaitScope);
                        std::cout << "[" << extId << "] Unregistered" << std::endl;
                        g_running.store(false);
                    } else if (cmd == "help") {
                        std::cout << "Available commands: list, query <ext_id>, quit" << std::endl;
                    } else {
                        std::cout << "Unknown command: " << cmd << std::endl;
                    }
                } catch (const kj::Exception& e) {
                    std::cerr << "Error: " << e.getDescription().cStr() << std::endl;
                }
            }
        } catch (const kj::Exception& e) {
            std::cerr << "[" << extId << "] Client error: " << e.getDescription().cStr() << std::endl;
            g_running.store(false);
        }
    });

    // Main thread: keep server running
    std::cout << "[" << extId << "] Server running..." << std::endl;
    kj::NEVER_DONE.wait(serverWaitScope);

    if (clientThread.joinable()) {
        clientThread.join();
    }

    std::cout << "[" << extId << "] Program ended" << std::endl;
    return 0;
}
