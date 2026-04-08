#ifndef DB_SERVICE_BASE_HPP
#define DB_SERVICE_BASE_HPP


#include <cstdint>
#include <string>
#include <thread>
#include <atomic>
#include <memory>

struct DatabaseInitializeParams {

};

struct DatabaseUpgradeParams {

};

struct DatabaseCleanupParams {

};

struct DatabaseFinalizeParams {

};

// CRTP base class for database services.
// Derived classes should:
// 1. Implement their own operator() overloads for custom parameter types
// 2. Must add "using DBServiceBase::operator();" in the PUBLIC section of derived class
//    This exposes base class operator() overloads and avoids C++ name hiding.
//    If placed in private/protected, std::visit will fail to access these overloads.
template<typename Derived, typename QueueType>
class DBServiceBase {
   public:
    DBServiceBase() : running_(false) {

    }

    virtual ~DBServiceBase() {
        if (running_) {
            if (workerThread_.joinable()) {
                asyncDatabaseFinalize();
                workerThread_.join();
            }
        }
    }

    DBServiceBase(const DBServiceBase&) = delete;
    DBServiceBase& operator=(const DBServiceBase&) = delete;
    DBServiceBase(DBServiceBase&&) = delete;
    DBServiceBase& operator=(DBServiceBase&&) = delete;

   public:
    bool running() const {
        return running_;
    }

    bool asyncDatabaseInitialize() {
        if (running_) {
            return false;
        }

        auto ptr = std::make_unique<DatabaseInitializeParams>();
        if (!ptr) {
            return false;
        }

        running_ = true;
        workerThread_ = std::thread(&DBServiceBase::doWork, this);
        if (!workerThread_.joinable()) {
            running_ = false;
            return false;
        }

        dbQueue_.produce(std::move(ptr));

        return true;
    }

    void asyncDatabaseUpgrade() {
        if (running_) {
            dbQueue_.produce(std::make_unique<DatabaseUpgradeParams>());
        }
    }

    void asyncDatabaseCleanup() {
        if (running_) {
            dbQueue_.produce(std::make_unique<DatabaseCleanupParams>());
        }
    }

    void asyncDatabaseFinalize() {
        if (running_) {
            auto ptr = std::make_unique<DatabaseFinalizeParams>();
            if (!ptr) {
                running_ = false;
                return;
            }
            dbQueue_.produce(std::move(ptr));
            if (workerThread_.joinable()) {
                workerThread_.join();
            }
        }
    }

   public: // visitor handlers
    void operator()(std::unique_ptr<DatabaseInitializeParams> ptr) {
        if (ptr) {
            databaseInitialize();
        }
    }

    void operator()(std::unique_ptr<DatabaseUpgradeParams> ptr) {
        if (ptr) {
            databaseUpgrade();
        }
    }

    void operator()(std::unique_ptr<DatabaseCleanupParams> ptr) {
        if (ptr) {
            databaseCleanup();
        }
    }

    void operator()(std::unique_ptr<DatabaseFinalizeParams> ptr) {
        if (ptr) {
            databaseFinalize();
            running_ = false;
        }
    }

   private:
    void doWork() {
        auto& derived = static_cast<Derived&>(*this);
        while (running_ || !dbQueue_.empty()) {
            dbQueue_.visit(derived);
        }
    }

   private:
    virtual void databaseInitialize() = 0;
    virtual void databaseUpgrade() = 0;
    virtual void databaseCleanup() = 0;
    virtual void databaseFinalize() = 0;

   protected:
    QueueType         dbQueue_;
    std::thread       workerThread_;
    std::atomic<bool> running_;
};


#endif // DB_SERVICE_BASE_HPP
