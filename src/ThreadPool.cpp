#include "../include/ThreadPool.h"

/**
 * @brief Constructs a ThreadPool with a specified number of worker threads.
 *
 * This constructor initializes the thread pool by creating a specified number of
 * worker threads. Each worker thread runs a loop where it waits for tasks to be
 * available in the task queue or for a stop signal to terminate.
 *
 * @param numThreads The number of worker threads to create in the pool.
 *
 * @note If \p numThreads is zero, no worker threads will be created, and tasks
 *       submitted to the pool will not be executed.
 */
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(
            [this] {
                for (;;) {
                    std::function<void()> task;

                    {
                        // Acquire lock on the task queue mutex
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        // Wait until there is a task or the pool is stopped
                        this->condition.wait(lock,
                            [this] { return this->stop || !this->tasks.empty(); });

                        // Exit if stop signal is received and no tasks are left
                        if (this->stop && this->tasks.empty())
                            return;

                        // Retrieve the next task from the queue
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    // Execute the retrieved task
                    task();
                }
            }
        );
    }
}

/**
 * @brief Destructs the ThreadPool and joins all worker threads.
 *
 * This destructor signals all worker threads to stop processing tasks
 * and waits for them to finish execution. It ensures that all tasks
 * have been completed before the ThreadPool object is destroyed.
 *
 * @note The destructor sets the stop flag, notifies all threads, and
 *       joins them to ensure clean shutdown.
 * @note If there are tasks remaining in the queue, they will not be executed.
 */
ThreadPool::~ThreadPool() {
    {
        // Acquire lock on the task queue mutex
        std::unique_lock<std::mutex> lock(queueMutex);
        // Set the stop flag to true to signal threads to stop
        stop = true;
    }

    // Notify all worker threads that they should check the stop condition
    condition.notify_all();

    // Wait for all worker threads to complete execution
    for (std::thread& worker : workers)
        worker.join();
}

/**
 * @brief Enqueues a new task into the ThreadPool for execution.
 *
 * This method adds a new task to the task queue and notifies one of the
 * worker threads that a new task is available. The task will be executed
 * by one of the worker threads when it retrieves it from the queue.
 *
 * @param task A callable object (function, lambda expression, etc.) that takes
 *             no arguments and returns void. This represents the work to be done.
 *
 * @note The task is moved into the queue to avoid unnecessary copying.
 * @note If the ThreadPool has been stopped, the task will not be executed.
 * @note It is assumed that the callable is exception-safe. Exceptions should be
 *       handled within the callable to prevent undefined behavior.
 */
void ThreadPool::enqueue(std::function<void()> task) {
    {
        // Acquire lock on the task queue mutex
        std::unique_lock<std::mutex> lock(queueMutex);
        // Add the new task to the queue
        tasks.emplace(std::move(task));
    }
    // Notify one worker thread that a new task is available
    condition.notify_one();
}
