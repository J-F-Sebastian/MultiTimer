#ifndef MULTI_TIMER_H
#define MULTI_TIMER_H

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <thread>
#include <tuple>

class MultiTimer
{
public:
	using Clock = std::chrono::steady_clock;
	using TimeReference = Clock::time_point;
	using TimerEntry = std::tuple<TimeReference, bool>;
	using TimerList = std::list<TimerEntry>;

	~MultiTimer();

	MultiTimer(const MultiTimer&) = delete;
	MultiTimer& operator=(const MultiTimer&) = delete;
	MultiTimer(MultiTimer&&) = delete;
	MultiTimer& operator=(MultiTimer&&) = delete;

	void addTimer(const Clock::duration& duration);
	void init();

protected:
	MultiTimer() = default;

private:
	bool active;
	std::uint8_t activePadding[7];
	TimerList timers;
	std::condition_variable condition;
	std::mutex mutex;
	std::thread* thread = nullptr;
};

#endif // MULTI_TIMER_H
