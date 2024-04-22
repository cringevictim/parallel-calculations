#include "thread_pool.h"
#include "global.h" 
#include <iostream>
#include <mutex>


#include <cassert>

thread_pool::thread_pool() = default;

thread_pool::~thread_pool() {
    terminate();
}

void thread_pool::initialize(size_t workers_per_queue, size_t queues_count, size_t queue_size) {
    assert(workers_per_queue > 0 && queues_count > 0);
    write_lock _(m_rw_lock);

    this->queue_size = queue_size;

    if (m_initialized || m_terminated) {
        return;
    }

    m_workers.reserve(workers_per_queue * queues_count);
    for (size_t i = 0; i < queues_count; i++) {
        m_tasks.push_back(std::make_unique<task_queue<std::function<void()>>>());
    }

    for (size_t i = 0; i < queues_count; i++) {
        for (size_t j = 0; j < workers_per_queue; j++) {
            m_workers.emplace_back([this, i]() { this->routine(i); });
            g_console_lock.lock();
            std::cout << "Attaching thread to queue [" << i << "]" << std::endl;
            g_console_lock.unlock();
        }
    }
    m_initialized = !m_workers.empty();
}

void thread_pool::terminate() {
    {
        write_lock _(m_rw_lock);
        if (!m_initialized) return;

        m_terminated = true;
    }

    m_task_waiter.notify_all();
    for (std::thread& worker : m_workers) {
        worker.join();
    }
    m_workers.clear();
    m_terminated = false;
    m_initialized = false;
}

void thread_pool::routine(size_t queue_id) {
    while (true) {
        bool task_acquired = false;
        std::function<void()> task;
        {
            write_lock _(m_rw_lock);
            auto wait_condition = [this, &task_acquired, &task, queue_id]() {
                task_acquired = m_tasks[queue_id]->pop(task);
                return m_terminated || task_acquired;
                };
            m_task_waiter.wait(_, wait_condition);
        }
        if (m_terminated && !task_acquired) {
            return;
        }
        if (task_acquired) {
            task();
        }
    }
}



bool thread_pool::working() const {
    read_lock _(m_rw_lock);
    return working_unsafe();
}

bool thread_pool::working_unsafe() const {
    return m_initialized && !m_terminated;
}