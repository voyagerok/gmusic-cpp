#ifndef DB_ENGINE_HPP
#define DB_ENGINE_HPP

#include <memory>

struct sqlite3;
struct sqlite3_stmt;

namespace gmusic
{
namespace db
{

struct DatabaseException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Connection
{
  public:
    Connection(const std::string &dbPath);
    ~Connection();

    Connection(const Connection &other) = delete;
    Connection &operator=(const Connection &other) = delete;

    Connection(Connection &&other);
    Connection &operator=(Connection &&other);

    sqlite3 *getHandle();

  private:
    sqlite3 *handle = nullptr;
};

class Statement
{
  public:
    Statement(Connection *con, const std::string &query);
    ~Statement();

    Statement(const Statement &other) = delete;
    Statement &operator=(const Statement &other) = delete;

    Statement(Statement &&other);
    Statement &operator=(Statement &&other);

    template <class T, class... Args> void bind(T &&arg, Args &&... args);

    template <class T, class... Args> void bind(T *arg, Args &&... args);

    template <class... Args> void bind(Args &&... args);

    bool executeStep();
    void execute();

    template <class T> T get(int columnNum);

    template <class... Args>
    static void
    executeQuery(Connection *con, const std::string &query, Args &&... args)
    {
        Statement st(con, query);
        st.bind(std::forward<Args>(args)...);
        st.execute();
    }

    static void executeQuery(Connection *con, const std::string &query)
    {
        Statement st(con, query);
        st.execute();
    }

    void reset();

  private:
    void bindNull();
    void bindOne(int);
    void bindOne(const std::string &);
    sqlite3_stmt *statement = nullptr;
    Connection *con;
    int n_of_params;
    int binded_count = 1;
};

template <> std::string Statement::get<std::string>(int columnNum);
template <> int Statement::get<int>(int columnNum);

template <class T, class... Args> void Statement::bind(T &&arg, Args &&... args)
{
    bindOne(std::forward<T>(arg));
    bind(std::forward<Args>(args)...);
}

template <class T, class... Args> void Statement::bind(T *arg, Args &&... args)
{
    if (arg == nullptr) {
        bindNull();
    } else {
        bindOne(std::forward<T>(arg));
    }
    bind(std::forward<Args>(args)...);
}

template <> void Statement::bind<>();
}
}

#endif // DB_ENGINE_HPP
