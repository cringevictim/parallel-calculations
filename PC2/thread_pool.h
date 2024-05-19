#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include "task_queue.h"
#include <iostream>
#include "global.h"
#include <random>
#include <numeric>

//#include "tracy/Tracy.hpp"

class thread_pool {
public:
    thread_pool();
    ~thread_pool();
    void initialize(size_t workers_per_queue, size_t queues_count, size_t queue_size);
    void terminate();
    void routine(size_t queue_id);
    bool working() const;
    bool working_unsafe() const;

    template <typename task_t, typename... arguments>
    void add_task(task_t&& task, arguments&&... parameters);

private:
    using read_write_lock = std::shared_mutex;
    using read_lock = std::shared_lock<read_write_lock>;
    using write_lock = std::unique_lock<read_write_lock>;

    mutable read_write_lock m_rw_lock;
    mutable std::condition_variable_any m_task_waiter;
    std::vector<std::thread> m_workers;
    std::vector<std::unique_ptr<task_queue<std::function<void()>>>> m_tasks;
    bool m_initialized = false;
    bool m_terminated = false;

    size_t queue_size = 0;
};

template <typename task_t, typename... arguments>
void thread_pool::add_task(task_t&& task, arguments&&... parameters) {
    //ZoneScoped;
    {
        read_lock _(m_rw_lock);
        if (!working_unsafe()) {
            return;
        }
    }
    auto bind_task = std::bind(std::forward<task_t>(task), std::forward<arguments>(parameters)...);

    std::vector<int> numbers(m_tasks.size());
    std::iota(numbers.begin(), numbers.end(), 0);

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(numbers.begin(), numbers.end(), std::default_random_engine(seed));

    bool was_added = false;
    for (auto num : numbers) {
        if (m_tasks[num]->size() < queue_size) {
            m_tasks[num]->emplace(bind_task);
            was_added = true;
            g_console_lock.lock();
            std::cout << "Task added to queue [" << num << "]" << std::endl;
            g_console_lock.unlock();
            break;
        }
    }
    if (!was_added) {
        g_console_lock.lock();
        std::cout << "Task wasn't added due to full queue" << std::endl;
        g_console_lock.unlock();
    }

    m_task_waiter.notify_one();
}