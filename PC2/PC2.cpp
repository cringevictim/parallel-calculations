#include <queue>
#include <thread>
#include <shared_mutex>
#include <functional>
#include <utility>
#include <random>
#include <iostream>
#include <memory>
#include <cassert>

using read_write_lock = std::shared_mutex;
using read_lock = std::shared_lock<read_write_lock>;
using write_lock = std::unique_lock<read_write_lock>;

std::mutex g_console_lock;

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
	void initialize(const size_t workers_per_queue, const size_t queues_count, const size_t queue_size);
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
	std::vector<std::unique_ptr<task_queue<std::function<void()>>>> m_tasks;
	//task_queue<std::function<void()>> m_tasks;
	bool m_initialized = false;
	bool m_terminated = false;

	size_t queue_size;
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

void thread_pool::initialize(const size_t workers_per_queue, const size_t queues_count, const size_t queue_size)
{
	assert(workers_per_queue <= 0 || queues_count <= 0);


	write_lock _(m_rw_lock);

	this->queue_size = queue_size;

	if (m_initialized || m_terminated)
	{
		return;
	}
	m_workers.reserve(workers_per_queue * queues_count);

	for (int i = 0; i < queues_count; i++) {
		m_tasks.push_back(std::make_unique<task_queue<std::function<void()>>>());
	}

	for (size_t i = 0, queue_id = 0; i < queues_count; i++) // TODO: make sure queue_id expression is correct
	{
		for (size_t j = 0; j < workers_per_queue; j++) {
			m_workers.emplace_back([this, i]() {this->routine(i); });
			g_console_lock.lock();
			std::cout << "Attaching thread to queue [" << i << "]" << std::endl;
			g_console_lock.unlock();
		}
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
			auto wait_condition = [this, &task_accquiered, &task, queue_id] {

				

				task_accquiered = m_tasks[queue_id]->pop(task);
				//task_accquiered = m_tasks.pop(task);
				return m_terminated || task_accquiered;
				};
			m_task_waiter.wait(_, wait_condition);
		}
		if (m_terminated && !task_accquiered)
		{
			return;
		}

		if (task_accquiered) {
			g_console_lock.lock();
			std::cout << "Starting to execute task from queue [" << queue_id << "]" << std::endl;
			g_console_lock.unlock();
		}
		
		task();
		
	}
	g_console_lock.lock();
	std::cout << "Thread terminated" << std::endl;
	g_console_lock.unlock();
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

	std::vector<int> numbers(m_tasks.size());
	for (int i = 0; i < m_tasks.size(); ++i) {
		/*g_console_lock.lock();
		std::cout << i << std::endl;
		g_console_lock.unlock();*/
		numbers[i] = i;
	}

	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

	std::shuffle(numbers.begin(), numbers.end(), std::default_random_engine(seed));
	bool was_added = false;
	int added_to;
	for (auto num : numbers) {
		if (m_tasks[num]->size() < queue_size && !was_added) {
			m_tasks[num]->emplace(bind);
			was_added = true;
			added_to = num;
		}
	}

	if (was_added) {
		g_console_lock.lock();
		std::cout << "Task added to queue [" << added_to << "]" << std::endl;
		g_console_lock.unlock();
	}
	else {
		g_console_lock.lock();
		std::cout << "Task wasn't added" << std::endl;
		g_console_lock.unlock();
	}
	
	m_task_waiter.notify_one(); // TODO: read about
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





void sampleTask(int id, int sleeping_time) {
	std::this_thread::sleep_for(std::chrono::seconds(sleeping_time)); // Simulating work

	g_console_lock.lock();
	std::cout << "Task " << id << " executed on thread " << std::this_thread::get_id() << " and lasted " << sleeping_time << " seconds." << std::endl;
	g_console_lock.unlock();
}

int main() {
	thread_pool pool;

	// workers_per_queue, queues_count, queue_size
	pool.initialize(2, 3, 10);

	for (int i = 0; i < 40; ++i) {
		int sleeping_time = rand() % 11;
		pool.add_task(sampleTask, i, sleeping_time);
	}
	
	std::this_thread::sleep_for(std::chrono::seconds(5));

	pool.terminate();

	return 0;
}