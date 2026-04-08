// core.cpp - Core service program
// Central registry service that manages all Ext connections

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <kj/async-io.h>
#include <kj/debug.h>

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>

#include "messages.capnp.h"

// Store registered Ext information
struct RegisteredExt {
    std::string id;
    std::string address;
    std::string description;
    std::chrono::steady_clock::time_point lastHeartbeat;
    std::unique_ptr<ExtService::Client> serviceClient;  // RPC client to Ext

    RegisteredExt() = default;
    RegisteredExt(RegisteredExt&&) = default;
    RegisteredExt& operator=(RegisteredExt&&) = default;
};

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
        if (registeredExts_.find(extId) != registeredExts_.end()) {
            context.getResults().setSuccess(false);
            context.getResults().setMessage("Ext ID already registered");
            std::cout << "[Core] Registration failed: ID already exists" << std::endl;
            return kj::READY_NOW;
        }

        // Connect to Ext service
        try {
            RegisteredExt ext;
            ext.id = extId;
            ext.address = address;
            ext.description = description;
            ext.lastHeartbeat = std::chrono::steady_clock::now();

            // Create RPC client to Ext
            auto client = kj::heap<capnp::EzRpcClient>(address);
            ext.serviceClient = std::make_unique<ExtService::Client>(client->getMain<ExtService>());
            clients_[extId] = kj::mv(client);

            registeredExts_[extId] = std::move(ext);

            context.getResults().setSuccess(true);
            context.getResults().setMessage("Registration successful");
            std::cout << "[Core] Ext " << extId << " registered successfully" << std::endl;
        } catch (const std::exception& e) {
            context.getResults().setSuccess(false);
            context.getResults().setMessage(kj::str("Failed to connect: ", e.what()).cStr());
            std::cout << "[Core] Failed to connect to Ext: " << e.what() << std::endl;
        }

        return kj::READY_NOW;
    }

    // Ext unregister
    kj::Promise<void> unregister(UnregisterContext context) override {
        auto extId = context.getParams().getExtId().cStr();

        auto it = registeredExts_.find(extId);
        if (it != registeredExts_.end()) {
            registeredExts_.erase(it);
            clients_.erase(extId);
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
        auto extId = context.getParams().getExtId().cStr();

        auto it = registeredExts_.find(extId);
        if (it != registeredExts_.end()) {
            const auto& ext = it->second;
            context.getResults().setFound(true);
            auto info = context.getResults().initInfo();
            info.setId(ext.id);
            info.setAddress(ext.address);
            info.setDescription(ext.description);
        } else {
            context.getResults().setFound(false);
        }

        return kj::READY_NOW;
    }

    // List all registered Exts
    kj::Promise<void> listExts(ListExtsContext context) override {
        auto exts = context.getResults().initExts(registeredExts_.size());

        size_t i = 0;
        for (const auto& [id, ext] : registeredExts_) {
            exts[i].setId(ext.id);
            exts[i].setAddress(ext.address);
            exts[i].setDescription(ext.description);
            ++i;
        }

        std::cout << "[Core] Returning " << registeredExts_.size() << " registered Ext(s)" << std::endl;
        return kj::READY_NOW;
    }

    // Heartbeat handling
    kj::Promise<void> heartbeat(HeartbeatContext context) override {
        auto extId = context.getParams().getExtId().cStr();

        auto it = registeredExts_.find(extId);
        if (it != registeredExts_.end()) {
            it->second.lastHeartbeat = std::chrono::steady_clock::now();
            context.getResults().setAcknowledged(true);
        } else {
            context.getResults().setAcknowledged(false);
        }

        return kj::READY_NOW;
    }

    // Connect to another Ext (return that Ext's service interface)
    kj::Promise<void> connectToExt(ConnectToExtContext context) override {
        auto targetExtId = context.getParams().getTargetExtId().cStr();

        auto it = registeredExts_.find(targetExtId);
        if (it != registeredExts_.end() && it->second.serviceClient) {
            context.getResults().setService(*it->second.serviceClient);
            std::cout << "[Core] Providing connection to " << targetExtId << std::endl;
        } else {
            std::cout << "[Core] Target Ext '" << targetExtId << "' not found" << std::endl;
        }

        return kj::READY_NOW;
    }

    // Get registered Ext count
    size_t getExtCount() const {
        return registeredExts_.size();
    }

    // Send command to specified Ext
    kj::Promise<void> sendCommandToExt(const std::string& extId, const std::string& command) {
        auto it = registeredExts_.find(extId);
        if (it == registeredExts_.end() || !it->second.serviceClient) {
            std::cout << "[Core] Ext " << extId << " not found" << std::endl;
            return kj::READY_NOW;
        }

        auto& client = *it->second.serviceClient;

        // Execute task
        auto request = client.executeTaskRequest();
        auto taskReq = request.initRequest();
        taskReq.setTaskId("cmd-001");
        taskReq.setCommand(command);
        taskReq.setTimeout(30);

        return request.send().then([extId](auto response) {
            auto result = response.getResult();
            std::cout << "[Core] Ext " << extId << " execution result: "
                      << (result.getSuccess() ? "success" : "failed")
                      << ", output: " << result.getOutput().cStr() << std::endl;
        }).eagerlyEvaluate(nullptr);
    }

    // Get specified Ext status
    kj::Promise<void> getExtStatus(const std::string& extId) {
        auto it = registeredExts_.find(extId);
        if (it == registeredExts_.end() || !it->second.serviceClient) {
            std::cout << "[Core] Ext " << extId << " not found" << std::endl;
            return kj::READY_NOW;
        }

        auto& client = *it->second.serviceClient;

        return client.getStatusRequest().send().then([extId](auto response) {
            auto status = response.getStatus();
            std::cout << "[Core] Ext " << extId << " status:" << std::endl
                      << "  CPU: " << status.getCpuUsage() << "%" << std::endl
                      << "  Memory: " << status.getMemoryUsage() << "%" << std::endl
                      << "  Uptime: " << status.getUptime() << "s" << std::endl
                      << "  Healthy: " << (status.getIsHealthy() ? "yes" : "no") << std::endl;
        }).eagerlyEvaluate(nullptr);
    }

    // Request Ext shutdown
    kj::Promise<void> shutdownExt(const std::string& extId, const std::string& reason) {
        auto it = registeredExts_.find(extId);
        if (it == registeredExts_.end() || !it->second.serviceClient) {
            std::cout << "[Core] Ext " << extId << " not found" << std::endl;
            return kj::READY_NOW;
        }

        auto& client = *it->second.serviceClient;

        auto request = client.shutdownRequest();
        request.setReason(reason);

        return request.send().then([this, extId](auto response) {
            if (response.getAccepted()) {
                std::cout << "[Core] Ext " << extId << " accepted shutdown request" << std::endl;
                registeredExts_.erase(extId);
                clients_.erase(extId);
            } else {
                std::cout << "[Core] Ext " << extId << " rejected shutdown request" << std::endl;
            }
        }).eagerlyEvaluate(nullptr);
    }

private:
    std::map<std::string, RegisteredExt> registeredExts_;
    std::map<std::string, kj::Own<capnp::EzRpcClient>> clients_;
};

// Command line interaction
void printHelp() {
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  list                - List all registered Exts" << std::endl;
    std::cout << "  status <ext_id>     - Get status of specified Ext" << std::endl;
    std::cout << "  exec <ext_id> <cmd> - Execute command on specified Ext" << std::endl;
    std::cout << "  shutdown <ext_id>   - Request specified Ext to shutdown" << std::endl;
    std::cout << "  help                - Show this help" << std::endl;
    std::cout << "  quit                - Exit Core" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    std::string bindAddress = "127.0.0.1:9000";

    if (argc >= 2) {
        bindAddress = argv[1];
    }

    std::cout << "=================================" << std::endl;
    std::cout << "  Cap'n Proto RPC - Core Service" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Listen address: " << bindAddress << std::endl;

    // Create RPC server (EzRpcServer creates its own EventLoop internally)
    capnp::EzRpcServer server(kj::heap<CoreServiceImpl>(), bindAddress);
    auto& waitScope = server.getWaitScope();

    std::cout << "Core service started, waiting for Ext connections..." << std::endl;
    printHelp();

    // Get port info
    auto port = server.getPort().wait(waitScope);
    std::cout << "Actual listen port: " << port << std::endl;

    // Simple command line interaction (in real applications, more complex methods can be used)
    // Here we just keep the server running
    std::cout << "\n[Info] Server is running, press Ctrl+C to exit" << std::endl;

    // Infinite loop to keep service running
    kj::NEVER_DONE.wait(waitScope);

    return 0;
}
