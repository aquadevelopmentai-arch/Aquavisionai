#pragma once
// threadsafe_queue - 생산자/소비자 큐 (스레드 안전)
//   push        : 가득 차면 가장 오래된 것을 버리고 넣음 (라인스캔처럼 최신 프레임이 중요할 때)
//   wait_pop    : timeout_ms 동안 기다림 -> 없으면 nullptr (스레드가 종료 플래그를 확인할 수 있게)
//   try_pop     : 기다리지 않음
//   stop        : 기다리는 소비자를 모두 깨움 (종료할 때)
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>

template<typename T>
class threadsafe_queue
{
public:
	explicit threadsafe_queue(size_t maxsize = 300)
		: m_maxsize(maxsize), m_dropped(0), m_stopped(false) {
	}
	threadsafe_queue(const threadsafe_queue&) = delete;
	threadsafe_queue& operator=(const threadsafe_queue&) = delete;

	// false = 가득 차서 가장 오래된 것을 버림 (넣기는 항상 성공)
	bool push(std::unique_ptr<T>&& ptr)
	{
		bool noDrop = true;
		{
			std::lock_guard<std::mutex> lk(m_mut);
			if (m_q.size() >= m_maxsize)
			{
				m_q.pop();                         // 오래된 것 버림 (unique_ptr 이 delete)
				++m_dropped;
				noDrop = false;
			}
			m_q.push(std::move(ptr));
		}
		m_cv.notify_one();
		return noDrop;
	}

	// timeout_ms 동안 기다림. 없거나 stop 되면 nullptr
	std::unique_ptr<T> wait_pop(int timeout_ms)
	{
		std::unique_lock<std::mutex> lk(m_mut);
		m_cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
			[this] { return !m_q.empty() || m_stopped; });
		if (m_q.empty())
			return nullptr;
		std::unique_ptr<T> res = std::move(m_q.front());
		m_q.pop();
		return res;
	}

	// 기다리지 않음. 없으면 nullptr
	std::unique_ptr<T> try_pop()
	{
		std::lock_guard<std::mutex> lk(m_mut);
		if (m_q.empty())
			return nullptr;
		std::unique_ptr<T> res = std::move(m_q.front());
		m_q.pop();
		return res;
	}

	// 종료 : 기다리는 소비자를 모두 깨움 (이후 wait_pop 은 바로 반환)
	void stop()
	{
		{
			std::lock_guard<std::mutex> lk(m_mut);
			m_stopped = true;
		}
		m_cv.notify_all();
	}
	void restart()                                // 다시 쓸 때
	{
		std::lock_guard<std::mutex> lk(m_mut);
		m_stopped = false;
	}

	void clear()
	{
		std::lock_guard<std::mutex> lk(m_mut);
		std::queue<std::unique_ptr<T>> empty;
		std::swap(m_q, empty);                    // 남은 것 전부 delete
	}

	size_t size() const
	{
		std::lock_guard<std::mutex> lk(m_mut);
		return m_q.size();
	}
	bool empty() const
	{
		std::lock_guard<std::mutex> lk(m_mut);
		return m_q.empty();
	}
	bool available() const                         // 여유 있음
	{
		std::lock_guard<std::mutex> lk(m_mut);
		return m_q.size() < m_maxsize;
	}
	size_t dropped() const                         
	{
		std::lock_guard<std::mutex> lk(m_mut);
		return m_dropped;
	}

private:
	mutable std::mutex              m_mut;
	std::condition_variable         m_cv;
	std::queue<std::unique_ptr<T>>  m_q;
	size_t                          m_maxsize;
	size_t                          m_dropped;
	bool                            m_stopped;
};