#pragma once
//#define INT64_MAX    _I64_MAX
//#define INTMAX_MAX   INT64_MAX

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <iostream>

template<typename T>
class threadsafe_queue
{
private:
	mutable std::mutex mut;
	std::queue<std::unique_ptr<T>> unique_ptr_q;
	std::condition_variable cv;
	unsigned int m_unread;
	unsigned int m_maxsize;
	
	std::vector<std::unique_ptr<T>> unique_prt_v;
public:
	threadsafe_queue() : m_unread(0)
	{
		m_maxsize = 300; 
	}
	threadsafe_queue(unsigned int maxsize) : m_unread(0) 
	{
		m_maxsize = maxsize; 
	}

	threadsafe_queue(const threadsafe_queue&) = delete;
	threadsafe_queue& operator=(const threadsafe_queue&) = delete;

	void push_unique(std::unique_ptr<T>&& ptr)
	{
		std::lock_guard<std::mutex> lk(mut);
		unique_ptr_q.push(move(ptr));
		++m_unread;
		cv.notify_one();
	}

	std::unique_ptr<T> pop_unique()
	{
		std::unique_lock<std::mutex> lk(mut);
		cv.wait(lk, [this] {return !unique_ptr_q.empty(); });
		auto res = std::move(unique_ptr_q.front());
		unique_ptr_q.pop();
		--m_unread;
		return res;
	}

	bool empty() const
	{
		std::lock_guard<std::mutex> lk(mut);
		return unique_ptr_q.empty();
	}

	unsigned int get_size() const
	{
		return m_unread;
	}

	bool get_available() const
	{
		return m_unread < m_maxsize;
	}

	void q_clear()
	{
		std::queue<std::unique_ptr<T>> empty;
		std::swap(unique_ptr_q, empty);
	}
};