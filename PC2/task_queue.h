#pragma once
#include <queue>
#include <shared_mutex>

template <typename task_type_t>
class task_queue {
public:
    task_queue();
    ~task_queue();
    bool empty() const;
    size_t size() const;
    void clear();
    bool pop(task_type_t& task);
    template <typename... arguments>
    void emplace(arguments&&... parameters);

private:
    using task_queue_implementation = std::queue<task_type_t>;
    using read_write_lock = std::shared_mutex;
    using read_lock = std::shared_lock<read_write_lock>;
    using write_lock = std::unique_lock<read_write_lock>;

    mutable read_write_lock m_rw_lock;
    task_queue_implementation m_tasks;
};

#include <functional>
#include <utility>

template <typename task_type_t>
task_queue<task_type_t>::task_queue() = default;

template <typename task_type_t>
task_queue<task_type_t>::~task_queue() {
    clear();
}

template <typename task_type_t>
bool task_queue<task_type_t>::empty() const {
    read_lock _(m_rw_lock);
    return m_tasks.empty();
}

template <typename task_type_t>
size_t task_queue<task_type_t>::size() const {
    read_lock _(m_rw_lock);
    return m_tasks.size();
}

template <typename task_type_t>
void task_queue<task_type_t>::clear() {
    write_lock _(m_rw_lock);
    while (!m_tasks.empty()) {
        m_tasks.pop();
    }
}

template <typename task_type_t>
bool task_queue<task_type_t>::pop(task_type_t& task) {
    write_lock _(m_rw_lock);
    if (m_tasks.empty()) {
        return false;
    }
    task = std::move(m_tasks.front());
    m_tasks.pop();
    return true;
}

template <typename task_type_t>
template <typename... arguments>
inline void task_queue<task_type_t>::emplace(arguments&&... parameters) {
    write_lock _(m_rw_lock);
    m_tasks.emplace(std::forward<arguments>(parameters)...);
}