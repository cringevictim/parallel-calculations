#include <queue>
#include <thread>
#include <shared_mutex>
#include <functional>
#include <utility>
#include <random>
#include <iostream>

using read_write_lock = std::shared_mutex;
using read_lock = std::shared_lock<read_write_lock>;
using write_lock = std::unique_lock<read_write_lock>;


template <typename task_type_t>
class task_queue
{
	using task_queue_implementation = std::queue<task_type_t>;
public:
	inline task_queue() = default;
	inline ~task_queue() { clear(); }
	inline bool empty() const;
	inline size_t size() const;
public:
	inline void clear();
	inline bool pop(task_type_t& task);
	template <typename... arguments>
	inline void emplace(arguments&&... parameters);
public:
	task_queue(const task_queue& other) = delete;
	task_queue(task_queue&& other) = delete;
	task_queue& operator=(const task_queue& rhs) = delete;
	task_queue& operator=(task_queue&& rhs) = delete;
private:
	mutable read_write_lock m_rw_lock;
	task_queue_implementation m_tasks;
};


template <typename task_type_t>
bool task_queue<task_type_t>::empty() const
{
	read_lock _(m_rw_lock);
	return m_tasks.empty();
}
template <typename task_type_t>
size_t task_queue<task_type_t>::size() const
{
	read_lock _(m_rw_lock);
	return m_tasks.size();
}
template <typename task_type_t>
void task_queue<task_type_t>::clear()
{
	write_lock _(m_rw_lock);
	while (!m_tasks.empty())
	{
		m_tasks.pop();
	}
}
template <typename task_type_t>
bool task_queue<task_type_t>::pop(task_type_t& task)
{
	write_lock _(m_rw_lock);
	if (m_tasks.empty())
	{
		return false;
	}
	else
	{
		task = std::move(m_tasks.front());
		m_tasks.pop();                      
		return true;
	}
}

template <typename task_type_t>
template <typename... arguments>
void task_queue<task_type_t>::emplace(arguments&&... parameters)
{
	write_lock _(m_rw_lock);
	m_tasks.emplace(std::forward<arguments>(parameters)...);
}




// ===========================================================================================================================================================================


class thread_pool
{
public:
	inline thread_pool() = default;
	inline ~thread_pool() { terminate(); }
public:
	void initialize(const size_t worker_count, const size_t workers_per_queue, const size_t queues_count, const size_t queue_size);
	void terminate();
	void routine(size_t queue_id);
	bool working() const;
	bool working_unsafe() const;
public:
	template <typename task_t, typename... arguments>
	inline void add_task(task_t&& task, arguments&&... parameters);
public:
	thread_pool(const thread_pool& other) = delete;
	thread_pool(thread_pool&& other) = delete;
	thread_pool& operator=(const thread_pool& rhs) = delete;
	thread_pool& operator=(thread_pool&& rhs) = delete;
private:
	mutable read_write_lock m_rw_lock;
	mutable std::condition_variable_any m_task_waiter;
	std::vector<std::thread> m_workers;
	task_queue<std::function<void()>> m_tasks;
	bool m_initialized = false;
	bool m_terminated = false;
};

bool thread_pool::working() const
{
	read_lock _(m_rw_lock);
	return working_unsafe();
}

bool thread_pool::working_unsafe() const
{
	return m_initialized && !m_terminated;
}

void thread_pool::initialize(const size_t worker_count, const size_t workers_per_queue, const size_t queues_count, const size_t queue_size)
{
	write_lock _(m_rw_lock);
	if (m_initialized || m_terminated)
	{
		return;
	}
	m_workers.reserve(workers_per_queue * queues_count);
	for (size_t id = 0, queue_id = 0; id < workers_per_queue * queues_count; id++, queue_id += id % workers_per_queue)
	{
		m_workers.emplace_back([this, queue_id]() {this->routine(queue_id); });
	}
	m_initialized = !m_workers.empty();
}

void thread_pool::routine(size_t queue_id)
{
	while (true)
	{
		bool task_accquiered = false;
		std::function<void()> task;
		{
			write_lock _(m_rw_lock);
			auto wait_condition = [this, &task_accquiered, &task] {
				task_accquiered = m_tasks.pop(task);
				return m_terminated || task_accquiered;
				};
			m_task_waiter.wait(_, wait_condition);
		}
		if (m_terminated && !task_accquiered)
		{
			return;
		}
		task();
	}
}

template <typename task_t, typename... arguments>
void thread_pool::add_task(task_t&& task, arguments&&... parameters)
{
	{
		read_lock _(m_rw_lock);
		if (!working_unsafe()) {
			return;
		}
	}
	auto bind = std::bind(std::forward<task_t>(task),
		std::forward<arguments>(parameters)...);

	// TODO: add pushing into the random free queue, skip if there is none

	m_tasks.emplace(bind);
	m_task_waiter.notify_one();
}

void thread_pool::terminate()
{
	{
		write_lock _(m_rw_lock);
		if (working_unsafe())
		{
			m_terminated = true;
		}
		else
		{
			m_workers.clear();
			m_terminated = false;
			m_initialized = false;
			return;
		}
	}
	m_task_waiter.notify_all();
	for (std::thread& worker : m_workers)
	{
		worker.join();
	}
	m_workers.clear();
	m_terminated = false;
	m_initialized = false;
}


std::mutex g_console_lock;

void sampleTask(int id, int sleeping_time) {
	std::this_thread::sleep_for(std::chrono::seconds(sleeping_time)); // Simulating work

	g_console_lock.lock();
	std::cout << "Task " << id << " executed on thread " << std::this_thread::get_id() << " and lasted " << sleeping_time << " seconds." << std::endl;
	g_console_lock.unlock();
}

int main() {
	thread_pool pool;

	// Initialize the pool with 4 worker threads
	pool.initialize(6, 2, 3, 10);

	// Adding some tasks
	for (int i = 1; i <= 10; ++i) {
		int sleeping_time = rand() % 11;
		pool.add_task(sampleTask, i, sleeping_time);
	}

	// Give some time for tasks to complete
	std::this_thread::sleep_for(std::chrono::seconds(5));

	// Terminate the pool and wait for all tasks to finish
	pool.terminate();

	return 0;
}