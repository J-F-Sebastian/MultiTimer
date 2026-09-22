#ifndef MULTI_TIMER_H
#define MULTI_TIMER_H

#include <chrono>
#include <condition_variable>
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

	/** Destroys the timer and waits for its worker thread to finish. */
	~MultiTimer();

	/** Copy construction is disabled. */
	MultiTimer(const MultiTimer&) = delete;

	/** Copy assignment is disabled. */
	MultiTimer& operator=(const MultiTimer&) = delete;

	/** Move construction is disabled. */
	MultiTimer(MultiTimer&&) = delete;

	/** Move assignment is disabled. */
	MultiTimer& operator=(MultiTimer&&) = delete;

	/**
	 * Adds a timer with the specified duration.
	 * @param duration The duration until the timer expires.
	 * @return A pointer to the inserted timer entry.
	 */
	TimerEntry* addTimer(const Clock::duration& duration);

	/**
	 * Removes a timer identified by its entry pointer.
	 * @param timer A pointer to the timer entry to remove.
	 * @return True if the timer was found and removed, otherwise false.
	 */
	bool removeTimer(TimerEntry* timer);

	/**
	 * Updates and reorders a timer identified by its entry pointer.
	 * @param timer A pointer to the timer entry to update.
	 * @param duration The duration until the timer expires again.
	 * @return True if the timer was found and updated, otherwise false.
	 */
	bool updateTimer(TimerEntry* timer, const Clock::duration& duration);

	/** Starts the timer worker thread. */
	void init();

protected:
	/** Constructs an empty timer. */
	MultiTimer() = default;

private:
	TimerList timers;
	TimeReference timepoint = TimeReference::max();
	std::condition_variable condition;
	std::mutex mutex;
	std::thread* thread = nullptr;

	/** Marks all timers whose expiration time has passed as expired. */
	void markExpired();

	/** Updates the next expiration time used by the worker thread. */
	void refreshDelay();
};

#endif // MULTI_TIMER_H
