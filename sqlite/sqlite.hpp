#ifndef SQLITE_HPP
#define SQLITE_HPP


#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <sqlite3.h>

#define RUN_LOG_ERR(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

class SQLiteReader;
class SQLiteWriter;

class SQLiteDB {
   public:
    SQLiteDB() : path_(), sqlite_(nullptr) {

    }

    ~SQLiteDB() {
        exit();
    }

    SQLiteDB(const SQLiteDB& other) = delete;
    SQLiteDB(SQLiteDB&& other) = delete;
    SQLiteDB& operator = (const SQLiteDB& other) = delete;
    SQLiteDB& operator = (SQLiteDB&& other) = delete;

   public:
    bool init(const std::string& path, bool multi = false) {
        exit();

        if (path.empty()) {
            RUN_LOG_ERR("sqlite db init failure while path is invalid");
            return false;
        }

        int result = sqlite3_open(path.c_str(), &sqlite_);
        if (SQLITE_OK != result) {
            RUN_LOG_ERR("sqlite db init failure while open (%s) failed, error (%d: %s)", path.c_str(), result, sqlite3_errstr(result));
            return false;
        }

        path_ = path;

        if (multi) {
            sqlite3_busy_timeout(sqlite_, 3000);
            execute("PRAGMA journal_mode=WAL;");
            execute("PRAGMA synchronous=NORMAL;");
        }

        return true;
    }

    void exit() {
        readers_.clear();
        writers_.clear();
        if (nullptr != sqlite_) {
            int result = sqlite3_close(sqlite_);
            if (SQLITE_OK != result) {
                RUN_LOG_ERR("sqlite db exit failure while close (%s) failed, error (%d: %s)", path_.c_str(), result, sqlite3_errstr(result));
            }
            sqlite_ = nullptr;
            path_.clear();
        }
    }

   public:
    bool is_open() const {
        return nullptr != sqlite_;
    }

    int error() const {
        return nullptr != sqlite_ ? sqlite3_errcode(sqlite_) : 999;
    }

    const char* what() const {
        return nullptr != sqlite_ ? sqlite3_errmsg(sqlite_) : "sqlite db is not open";
    }

   public:
    bool execute(const std::string& sql) {
        if (!is_open() || sql.empty()) {
            return false;
        }

        char* error_message = nullptr;
        int result = sqlite3_exec(sqlite_, sql.c_str(), nullptr, nullptr, &error_message);
        if (SQLITE_OK != result) {
            RUN_LOG_ERR("sqlite db execute failure while exec (%s) failed, error (%d: %s) message (%s)", sql.c_str(), result, sqlite3_errstr(result), nullptr != error_message ? error_message : "unknown");
        }

        if (nullptr != error_message) {
            sqlite3_free(error_message);
            error_message = nullptr;
        }

        return SQLITE_OK == result;
    }

    bool transaction_begin() {
        return execute("BEGIN TRANSACTION;");
    }

    bool transaction_begin_immediate() {
        return execute("BEGIN IMMEDIATE TRANSACTION;");
    }

    bool transaction_begin_exclusive() {
        return execute("BEGIN EXCLUSIVE TRANSACTION;");
    }

    bool transaction_commit() {
        return execute("COMMIT TRANSACTION;");
    }

    bool transaction_rollback() {
        return execute("ROLLBACK TRANSACTION;");
    }

    bool transaction_end(bool success) {
        return success ? transaction_commit() : transaction_rollback();
    }

   public:
    SQLiteReader create_reader(const std::string& sql);
    SQLiteWriter create_writer(const std::string& sql);
    SQLiteReader& get_reader(const std::string& sql);
    SQLiteWriter& get_writer(const std::string& sql);

   private:
    std::string                         path_;
    sqlite3*                            sqlite_;
    std::map<std::string, SQLiteReader> readers_;
    std::map<std::string, SQLiteWriter> writers_;
};

class SQLiteStatement {
   public:
    SQLiteStatement()
        : sql_()
        , sqlite_(nullptr)
        , writer_(false)
        , statement_(nullptr)
        , setIndex_(0)
        , getIndex_(0) {

    }

    SQLiteStatement(sqlite3* sqlite, const std::string& sql, bool writer)
        : sql_()
        , sqlite_(nullptr)
        , writer_(false)
        , statement_(nullptr)
        , setIndex_(0)
        , getIndex_(0) {
        if (nullptr != sqlite && !sql.empty()) {
            sqlite3_stmt* statement = nullptr;
            int result = sqlite3_prepare_v2(sqlite, sql.c_str(), static_cast<int>(sql.size()), &statement, nullptr);
            if (SQLITE_OK == result) {
                sql_ = sql;
                sqlite_ = sqlite;
                writer_ = writer;
                statement_ = statement;
            } else {
                RUN_LOG_ERR("sqlite %s (%s) create failure while prepare failed, error (%d: %s)", writer ? "writer" : "reader", sql.c_str(), result, sqlite3_errstr(result));
            }
        }
    }

    SQLiteStatement(SQLiteStatement&& other)
        : sql_()
        , sqlite_(nullptr)
        , writer_(false)
        , statement_(nullptr)
        , setIndex_(0)
        , getIndex_(0) {
        std::swap(sql_, other.sql_);
        std::swap(sqlite_, other.sqlite_);
        std::swap(writer_, other.writer_);
        std::swap(statement_, other.statement_);
        std::swap(setIndex_, other.setIndex_);
        std::swap(getIndex_, other.getIndex_);
    }

    SQLiteStatement& operator = (SQLiteStatement&& other) {
        if (&other != this) {
            clear();
            std::swap(sql_, other.sql_);
            std::swap(sqlite_, other.sqlite_);
            std::swap(writer_, other.writer_);
            std::swap(statement_, other.statement_);
            std::swap(setIndex_, other.setIndex_);
            std::swap(getIndex_, other.getIndex_);
        }
        return *this;
    }

    ~SQLiteStatement() {
        clear();
    }

    SQLiteStatement(const SQLiteStatement& other) = delete;
    SQLiteStatement& operator = (const SQLiteStatement& other) = delete;

   public:
    bool good() const {
        return nullptr != statement_;
    }

    void clear() {
        if (nullptr != statement_) {
            int result = sqlite3_finalize(statement_);
            if (SQLITE_OK != result) {
                RUN_LOG_ERR("sqlite %s (%s) clear failure while finalize failed, error (%d: %s)", writer_ ? "writer" : "reader", sql_.c_str(), result, sqlite3_errstr(result));
            }
            sql_.clear();
            sqlite_ = nullptr;
            writer_ = false;
            statement_ = nullptr;
            setIndex_ = 0;
            getIndex_ = 0;
        }
    }

    bool reset() {
        if (nullptr == statement_) {
            return false;
        }

        setIndex_ = 0;

        int result = sqlite3_reset(statement_);
        if (SQLITE_OK != result) {
            RUN_LOG_ERR("sqlite %s (%s) reset failure while reset failed, error (%d: %s)", writer_ ? "writer" : "reader", sql_.c_str(), result, sqlite3_errstr(result));
            return false;
        }

        return true;
    }

   public:
    bool set(bool value) {
        return set(setIndex_++, static_cast<int>(value));
    }

    bool set(int8_t value) {
        return set(setIndex_++, static_cast<int>(value));
    }

    bool set(uint8_t value) {
        return set(setIndex_++, static_cast<int>(value));
    }

    bool set(int16_t value) {
        return set(setIndex_++, static_cast<int>(value));
    }

    bool set(uint16_t value) {
        return set(setIndex_++, static_cast<int>(value));
    }

    bool set(int32_t value) {
        return set(setIndex_++, static_cast<int>(value));
    }

    bool set(uint32_t value) {
        return set(setIndex_++, static_cast<int>(value));
    }

    bool set(int64_t value) {
        return set(setIndex_++, value);
    }

    bool set(uint64_t value) {
        return set(setIndex_++, static_cast<int64_t>(value));
    }

    bool set(float value) {
        return set(setIndex_++, static_cast<double>(value));
    }

    bool set(double value) {
        return set(setIndex_++, value);
    }

    bool set(const std::string& value) {
        return set(setIndex_++, value);
    }

   protected:
    bool set(int index, int value) {
        if (nullptr == statement_) {
            return false;
        }

        int result = sqlite3_bind_int(statement_, index + 1, value);
        if (SQLITE_OK != result) {
            RUN_LOG_ERR("sqlite %s (%s) set failure while bind failed, error (%d: %s)", writer_ ? "writer" : "reader", sql_.c_str(), result, sqlite3_errstr(result));
            return false;
        }

        return true;
    }

    bool set(int index, int64_t value) {
        if (nullptr == statement_) {
            return false;
        }

        int result = sqlite3_bind_int64(statement_, index + 1, value);
        if (SQLITE_OK != result) {
            RUN_LOG_ERR("sqlite %s (%s) set failure while bind failed, error (%d: %s)", writer_ ? "writer" : "reader", sql_.c_str(), result, sqlite3_errstr(result));
            return false;
        }

        return true;
    }

    bool set(int index, double value) {
        if (nullptr == statement_) {
            return false;
        }

        int result = sqlite3_bind_double(statement_, index + 1, value);
        if (SQLITE_OK != result) {
            RUN_LOG_ERR("sqlite %s (%s) set failure while bind failed, error (%d: %s)", writer_ ? "writer" : "reader", sql_.c_str(), result, sqlite3_errstr(result));
            return false;
        }

        return true;
    }

    bool set(int index, const std::string& value) {
        if (nullptr == statement_) {
            return false;
        }

        int result = sqlite3_bind_text(statement_, index + 1, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        if (SQLITE_OK != result) {
            RUN_LOG_ERR("sqlite %s (%s) set failure while bind failed, error (%d: %s)", writer_ ? "writer" : "reader", sql_.c_str(), result, sqlite3_errstr(result));
            return false;
        }

        return true;
    }

   protected:
    std::string   sql_;
    sqlite3*      sqlite_;
    bool          writer_;
    sqlite3_stmt* statement_;
    int           setIndex_;
    int           getIndex_;
};

class SQLiteReader : public SQLiteStatement {
   public:
    SQLiteReader() : SQLiteStatement() {

    }

    SQLiteReader(sqlite3* sqlite, const std::string& sql) : SQLiteStatement(sqlite, sql, false) {

    }

   public:
    bool read() {
        if (nullptr == statement_) {
            return false;
        }

        getIndex_ = 0;

        int result = sqlite3_step(statement_);
        if (SQLITE_ROW != result && SQLITE_DONE != result) {
            RUN_LOG_ERR("sqlite reader (%s) read failure while step failed, error (%d: %s, %d: %s)", sql_.c_str(), result, sqlite3_errstr(result), sqlite3_errcode(sqlite_), sqlite3_errmsg(sqlite_));
        }

        return SQLITE_ROW == result;
    }

   public:
    bool get(bool& value) {
        int dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<bool>(dummy);
        return result;
    }

    bool get(int8_t& value) {
        int dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<int8_t>(dummy);
        return result;
    }

    bool get(uint8_t& value) {
        int dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<uint8_t>(dummy);
        return result;
    }

    bool get(int16_t& value) {
        int dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<int16_t>(dummy);
        return result;
    }

    bool get(uint16_t& value) {
        int dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<uint16_t>(dummy);
        return result;
    }

    bool get(int32_t& value) {
        int dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<int32_t>(dummy);
        return result;
    }

    bool get(uint32_t& value) {
        int dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<uint32_t>(dummy);
        return result;
    }

    bool get(int64_t& value) {
        return get(getIndex_++, value);
    }

    bool get(uint64_t& value) {
        int64_t dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<uint64_t>(dummy);
        return result;
    }

    bool get(float& value) {
        double dummy = 0;
        bool result = get(getIndex_++, dummy);
        value = static_cast<float>(dummy);
        return result;
    }

    bool get(double& value) {
        return get(getIndex_++, value);
    }

    bool get(std::string& value) {
        return get(getIndex_++, value);
    }

    bool get(void* ignore) {
        if (nullptr == statement_) {
            return false;
        }

        int index = getIndex_++;
        if (index >= sqlite3_column_count(statement_)) {
            RUN_LOG_ERR("sqlite reader (%s) get failure while get index is overflow (%d >= %d)", sql_.c_str(), index, sqlite3_column_count(statement_));
            return false;
        }

        (void)(ignore); // suppress unused variable warning

        return true;
    }

   protected:
    bool get(int index, int& value) {
        if (nullptr == statement_) {
            return false;
        }

        if (index >= sqlite3_column_count(statement_)) {
            RUN_LOG_ERR("sqlite reader (%s) get failure while get index is overflow (%d >= %d)", sql_.c_str(), index, sqlite3_column_count(statement_));
            return false;
        }

        value = sqlite3_column_int(statement_, index);

        return true;
    }

    bool get(int index, int64_t& value) {
        if (nullptr == statement_) {
            return false;
        }

        if (index >= sqlite3_column_count(statement_)) {
            RUN_LOG_ERR("sqlite reader (%s) get failure while get index is overflow (%d >= %d)", sql_.c_str(), index, sqlite3_column_count(statement_));
            return false;
        }

        value = sqlite3_column_int64(statement_, index);

        return true;
    }

    bool get(int index, double& value) {
        if (nullptr == statement_) {
            return false;
        }

        if (index >= sqlite3_column_count(statement_)) {
            RUN_LOG_ERR("sqlite reader (%s) get failure while get index is overflow (%d >= %d)", sql_.c_str(), index, sqlite3_column_count(statement_));
            return false;
        }

        value = sqlite3_column_double(statement_, index);

        return true;
    }

    bool get(int index, std::string& value) {
        if (nullptr == statement_) {
            return false;
        }

        if (index >= sqlite3_column_count(statement_)) {
            RUN_LOG_ERR("sqlite reader (%s) get failure while get index is overflow (%d >= %d)", sql_.c_str(), index, sqlite3_column_count(statement_));
            return false;
        }

        const char* column_text = reinterpret_cast<const char*>(sqlite3_column_text(statement_, index));
        int column_size = sqlite3_column_bytes(statement_, index);
        value.assign(column_text, column_size);

        return true;
    }
};

class SQLiteWriter : public SQLiteStatement {
   public:
    SQLiteWriter() : SQLiteStatement() {

    }

    SQLiteWriter(sqlite3* sqlite, const std::string& sql) : SQLiteStatement(sqlite, sql, true) {

    }

   public:
    bool write() {
        if (nullptr == statement_) {
            return false;
        }

        int result = sqlite3_step(statement_);
        if (SQLITE_ERROR == result || SQLITE_MISUSE == result) {
            RUN_LOG_ERR("sqlite writer (%s) write failure while step failed, error (%d: %s, %d: %s)", sql_.c_str(), result, sqlite3_errstr(result), sqlite3_errcode(sqlite_), sqlite3_errmsg(sqlite_));
            return false;
        }

        return true;
    }

    uint64_t rowid() const {
        if (nullptr == sqlite_) {
            return 0;
        }

        return static_cast<uint64_t>(sqlite3_last_insert_rowid(sqlite_));
    }

    uint64_t changes() const {
        if (nullptr == sqlite_) {
            return 0;
        }

        return static_cast<uint64_t>(sqlite3_changes(sqlite_));
    }

    bool changed() const {
        return changes() > 0;
    }
};

inline SQLiteReader SQLiteDB::create_reader(const std::string& sql) {
    return SQLiteReader(sqlite_, sql);
}

inline SQLiteWriter SQLiteDB::create_writer(const std::string& sql) {
    return SQLiteWriter(sqlite_, sql);
}

SQLiteReader& SQLiteDB::get_reader(const std::string& sql) {
    auto it = readers_.find(sql);
    if (it != readers_.end()) {
        auto& reader = it->second;
        reader.reset();
        return reader;
    } else {
        auto result = readers_.emplace(sql, SQLiteReader(sqlite_, sql));
        auto& reader = result.first->second;
        reader.reset();
        return reader;
    }
}

SQLiteWriter& SQLiteDB::get_writer(const std::string& sql) {
    auto it = writers_.find(sql);
    if (it != writers_.end()) {
        auto& writer = it->second;
        writer.reset();
        return writer;
    } else {
        auto result = writers_.emplace(sql, SQLiteWriter(sqlite_, sql));
        auto& writer = result.first->second;
        writer.reset();
        return writer;
    }
}


#endif // SQLITE_HPP
