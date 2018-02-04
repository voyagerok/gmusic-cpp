#include "db/database.hpp"

#include <sqlite3.h>

namespace gmusic
{

namespace db
{

// template<int N>
// static constexpr size_t countSymbols(const char (&str)[N], char symbol, int
// pos = 0, int counter = 0) {
//    if (pos == N) {
//        return counter;
//    }
//    if (str[pos] == symbol)
//        return countSymbols(str, symbol, pos + 1, counter + 1);
//    else
//        return countSymbols(str, symbol, pos + 1, counter);
//}

template <class Func, class... Args>
inline static void checkedSqlite3Call(Func func, Args &&... args)
{
    int errCode;
    if ((errCode = func(std::forward<Args>(args)...)) != SQLITE_OK) {
        const char *errMsg = sqlite3_errstr(errCode);
        throw DatabaseException{errMsg};
    }
}

Connection::Connection(const std::string &dbPath)
{
    checkedSqlite3Call(sqlite3_open, dbPath.c_str(), &handle);
}

Connection::~Connection()
{
    if (handle != nullptr)
        sqlite3_close(handle);
}

sqlite3 *Connection::getHandle() { return handle; }

Connection::Connection(Connection &&other) : handle(other.handle)
{
    other.handle = nullptr;
}

Connection &Connection::operator=(Connection &&other)
{
    std::swap(handle, other.handle);
    return *this;
}

Statement::Statement(Connection *con, const std::string &query)
{
    this->con = con;
    checkedSqlite3Call(sqlite3_prepare_v2,
                       con->getHandle(),
                       query.c_str(),
                       -1,
                       &statement,
                       nullptr);
    n_of_params = sqlite3_bind_parameter_count(statement);
}

Statement::~Statement()
{
    if (statement != nullptr) {
        sqlite3_finalize(statement);
    }
}

Statement::Statement(Statement &&other)
    : statement(other.statement), con(other.con),
      n_of_params(other.n_of_params), binded_count(other.binded_count)
{
    other.statement = nullptr;
}

Statement &Statement::operator=(Statement &&other)
{
    statement       = other.statement;
    con             = other.con;
    n_of_params     = other.n_of_params;
    binded_count    = other.binded_count;
    other.statement = nullptr;

    return *this;
}

template <> void Statement::bind<>() {}

void Statement::bindOne(int value)
{
    if (binded_count > n_of_params) {
        throw DatabaseException{"error: too many bind calls"};
    }
    checkedSqlite3Call(sqlite3_bind_int, statement, binded_count++, value);
}

void Statement::bindOne(const std::string &value)
{
    if (binded_count > n_of_params) {
        throw DatabaseException{"error: too many bind calls"};
    }
    checkedSqlite3Call(sqlite3_bind_text,
                       statement,
                       binded_count++,
                       value.c_str(),
                       -1,
                       SQLITE_TRANSIENT);
}

void Statement::bindNull()
{
    if (binded_count > n_of_params) {
        throw DatabaseException{"error: too many bind calls"};
    }
    checkedSqlite3Call(sqlite3_bind_null, statement, binded_count++);
}

template <> int Statement::get<int>(int colNum)
{
    int colType = sqlite3_column_type(statement, colNum);
    if (colType != SQLITE_INTEGER) {
        throw DatabaseException{"error: wrong column type"};
    }
    return sqlite3_column_int(statement, colNum);
}

template <> std::string Statement::get<std::string>(int colNum)
{
    int colType = sqlite3_column_type(statement, colNum);
    if (colType != SQLITE_TEXT) {
        throw DatabaseException{"error: wrong column type"};
    }

    int length                = sqlite3_column_bytes(statement, colNum);
    const unsigned char *text = sqlite3_column_text(statement, colNum);
    return std::string{text, text + length};
}

bool Statement::executeStep()
{
    int errCode = sqlite3_step(statement);
    if (errCode == SQLITE_ROW) {
        return true;
    } else if (errCode == SQLITE_DONE) {
        return false;
    } else {
        throw DatabaseException{sqlite3_errstr(errCode)};
    }
}

void Statement::execute()
{
    int errCode = sqlite3_step(statement);
    if (errCode != SQLITE_DONE && errCode != SQLITE_ROW) {
        throw DatabaseException{sqlite3_errstr(errCode)};
    }
}

void Statement::reset()
{
    checkedSqlite3Call(sqlite3_reset, statement);
    checkedSqlite3Call(sqlite3_clear_bindings, statement);
    binded_count = 1;
}
}
}
