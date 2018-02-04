#include <operation-queue.hpp>
//#include <iostream>
#include "utilities.hpp"

namespace gmusic
{

BaseTask::~BaseTask() = default;

OperationQueue::OperationQueue()
    : workerThread(&OperationQueue::workerRoutine, this)
{
    STDLOG << "OperationQueue ctor" << std::endl;
}

OperationQueue::~OperationQueue()
{
    STDLOG << "OperationQueue dtor" << std::endl;
    shutdown();
}

void OperationQueue::wait()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
}

void OperationQueue::workerRoutine()
{
    while (true) {
        std::unique_lock<std::recursive_mutex> lock{mutex};
        while (queue.empty() && !cancellationFlag) {
            condvar.wait(lock);
        }
        if (cancellationFlag) {
            return;
        }
        while (!queue.empty()) {
            auto &taskPackage = queue.front();
            if (tokens.find(taskPackage.second) != tokens.end()) {
                taskPackage.first();
            }
            queue.pop();
        }
    }
}

void OperationQueue::scheduleTask(const TaskRoutine &routine, void *token)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    tokens.insert(token);
    auto pair = std::make_pair(std::move(routine), token);
    queue.push(std::move(pair));
    lock.unlock();
    condvar.notify_one();
}

void OperationQueue::unregister(void *token)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto tokenIter = tokens.find(token);
    if (tokenIter != tokens.end()) {
        tokens.erase(tokenIter);
    }
}

void OperationQueue::shutdown()
{
    if (workerThread.joinable()) {
        cancellationFlag = true;
        condvar.notify_one();
        workerThread.join();
    }
}

RWLockHandle::RWLockHandle() : activeReaders(0), activeWriters(0)
{
    STDLOG << "RWLockHandle init" << std::endl;
}

void RWLockHandle::readLock()
{
    std::unique_lock<std::mutex> lock(mut);
    readersQueue.wait(lock, [this] { return activeWriters == 0; });
    ++activeReaders;
    lock.unlock();
}

void RWLockHandle::readUnlock()
{
    std::unique_lock<std::mutex> lock(mut);
    --activeReaders;
    lock.unlock();
    writersQueue.notify_all();
}

void RWLockHandle::writeLock()
{
    std::unique_lock<std::mutex> lock(mut);
    writersQueue.wait(
        lock, [this] { return activeReaders == 0 && activeWriters == 0; });
    ++activeWriters;
    lock.unlock();
}

void RWLockHandle::writeUnlock()
{
    std::unique_lock<std::mutex> lock(mut);
    --activeWriters;
    lock.unlock();
    writersQueue.notify_all();
    readersQueue.notify_all();
}

ReadLock::ReadLock(RWLockHandle *handle) : handle(handle)
{
    handle->readLock();
}

ReadLock::~ReadLock() { handle->readUnlock(); }

WriteLock::WriteLock(RWLockHandle *handle) : handle(handle)
{
    handle->writeLock();
}

WriteLock::~WriteLock() { handle->writeUnlock(); }

} // namespace gmapi
