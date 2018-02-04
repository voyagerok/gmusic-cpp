#ifndef OPERATION_QUEUE_HPP
#define OPERATION_QUEUE_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <type_traits>

namespace gmusic
{

class OperationQueue
{
  public:
    using TaskRoutine = std::function<void(void)>;
    using TaskPackage = std::pair<TaskRoutine, void *>;

    OperationQueue();
    ~OperationQueue();

    OperationQueue(const OperationQueue &) = delete;
    OperationQueue &operator=(const OperationQueue &) = delete;

    void scheduleTask(const TaskRoutine &routine, void *token);
    void unregister(void *token);
    void shutdown();
    void wait();

  private:
    void workerRoutine();

    std::queue<TaskPackage> queue;
    std::set<void *> tokens;
    std::recursive_mutex mutex;
    std::condition_variable_any condvar;
    std::atomic<bool> cancellationFlag{false};
    std::thread workerThread;
};

struct BaseTask {
    virtual ~BaseTask();
};

using cancel_t = std::atomic_bool;

template <class> class Task;

template <class Ret, class... Args> class Task<Ret(Args...)> : public BaseTask
{
  public:
    template <class T>
    using JobType = std::function<T(std::atomic_bool *, Args...)>;

    using CompletionHandlerType = std::function<void(void)>;

    Task &run(Args... args);
    Task &setCompletionHandler(const CompletionHandlerType &onCompletion);
    Task &setJob(const JobType<Ret> &job);
    Ret get();
    Task(OperationQueue *operationQueue) : operationQueue(operationQueue) {}
    ~Task();

  private:
    void setPromise(std::promise<void> *promise,
                    JobType<Ret> &fun,
                    cancel_t *flag,
                    Args &... args)
    {
        fun(flag, args...);
        promise->set_value();
    }

    template <class T = Ret,
              class   = std::enable_if_t<!std::is_same<T, void>::value>>
    void setPromise(std::promise<T> *promise,
                    JobType<T> &fun,
                    cancel_t *flag,
                    Args &... args)
    {
        promise->set_value(fun(flag, args...));
    }

    std::queue<std::future<Ret>> taskQueue;
    cancel_t cancelFlag{false};
    OperationQueue *operationQueue;
    JobType<Ret> job;
    CompletionHandlerType completionHandler;
};

template <class Ret, class... Args> Ret Task<Ret(Args...)>::get()
{
    if (taskQueue.empty()) {
        return Ret();
    }
    auto result = std::move(taskQueue.front());
    taskQueue.pop();
    return result.get();
}

template <class Ret, class... Args> Task<Ret(Args...)>::~Task()
{
    cancelFlag = true;
    operationQueue->unregister(this);
}

template <class Ret, class... Args>
Task<Ret(Args...)> &Task<Ret(Args...)>::setCompletionHandler(
    const CompletionHandlerType &onCompletion)
{
    completionHandler = onCompletion;
    return *this;
}

template <class Ret, class... Args>
Task<Ret(Args...)> &Task<Ret(Args...)>::setJob(const JobType<Ret> &job)
{
    this->job = job;
    return *this;
}

template <class Ret, class... Args>
Task<Ret(Args...)> &Task<Ret(Args...)>::run(Args... args)
{
    if (!job) {
        return *this;
    }

    auto resultPromise = std::make_shared<std::promise<Ret>>();
    taskQueue.push(resultPromise->get_future());

    operationQueue->scheduleTask(
        [ this, promise = std::move(resultPromise), args... ]() mutable {
            try {
                setPromise(promise.get(), job, &cancelFlag, args...);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
            if (completionHandler) {
                completionHandler();
            }
        },
        this);

    return *this;
}

template <class T, class... Args> struct Hash {
    static const size_t hash_value;
};

template <class T, class... Args>
const size_t Hash<T, Args...>::hash_value = typeid(T(Args...)).hash_code();

template <class T>
using light_decay_t = std::remove_reference_t<std::remove_cv_t<T>>;

class TaskBuilder
{
  public:
    template <class Ret, class... Args>
    Task<light_decay_t<Ret>(light_decay_t<Args>...)> &task()
    {
        using ResultType = Task<light_decay_t<Ret>(light_decay_t<Args>...)>;

        auto hash_value =
            Hash<light_decay_t<Ret>, light_decay_t<Args>...>::hash_value;
        auto taskIter = tasks.find(hash_value);

        if (taskIter == tasks.end()) {
            auto task       = std::make_unique<ResultType>(&operationQueue);
            auto rawTaskPtr = task.get();
            tasks.insert(std::make_pair(hash_value, std::move(task)));
            return *rawTaskPtr;
        }
        return *(static_cast<ResultType *>(taskIter->second.get()));
    }

  private:
    OperationQueue operationQueue;
    std::map<std::size_t, std::unique_ptr<BaseTask>> tasks;
};

template <class T> struct ThreadSafeQueue {
  public:
    T pop();
    void push(const T &elem);
    bool empty() const;
    void clear();

  private:
    std::queue<T> queue;
    mutable std::recursive_mutex mutex;
};

template <class T> T ThreadSafeQueue<T>::pop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto elem = queue.front();
    queue.pop();
    return elem;
}

template <class T> void ThreadSafeQueue<T>::push(const T &elem)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    queue.push(elem);
}

template <class T> bool ThreadSafeQueue<T>::empty() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return queue.empty();
}

template <class T> void ThreadSafeQueue<T>::clear()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    while (!queue.empty()) {
        queue.pop();
    }
}

class RWLockHandle
{
  public:
    RWLockHandle();
    RWLockHandle(const RWLockHandle &other) = delete;
    RWLockHandle &operator=(const RWLockHandle &other) = delete;
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();

  private:
    std::mutex mut;
    std::condition_variable readersQueue;
    std::condition_variable writersQueue;
    int activeReaders;
    int activeWriters;
};

class ReadLock
{
  public:
    ReadLock(RWLockHandle *handle);
    ReadLock(const ReadLock &other) = delete;
    ReadLock &operator=(const ReadLock &other) = delete;
    ~ReadLock();

  private:
    RWLockHandle *handle;
};

class WriteLock
{
  public:
    WriteLock(RWLockHandle *handle);
    WriteLock(const WriteLock &other) = delete;
    WriteLock &operator=(const WriteLock &other) = delete;
    ~WriteLock();

  private:
    RWLockHandle *handle;
};

} // namespace gmapi

#endif // OPERATION_QUEUE_HPP
