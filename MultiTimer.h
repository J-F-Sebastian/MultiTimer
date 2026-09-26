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
	using TimerList = std::list<TimerEntry*>;

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
	 * Creates a detached timer entry.
	 * @param duration The duration parameter (not used to initialize the entry).
	 * @return A pointer to a heap-allocated entry initialized as expired with no expiration time.
	 * The caller owns the returned entry and must delete it.
	 */
	TimerEntry* createTimerEntry(const Clock::duration& duration);

	/**
	 * Removes a timer entry from the schedule.
	 * @param timer A pointer to the timer entry to remove.
	 * @return True if the timer was scheduled and removed, otherwise false.
	 */
	bool removeTimerEntry(TimerEntry* timer);

	/**
	 * Updates a timer entry, inserting it if it is detached and expired.
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

	/** Marks expired entries and removes them from the scheduled timer list. */
	void markExpired();

	/** Updates the next expiration time used by the worker thread. */
	void refreshDelay();
};

#endif // MULTI_TIMER_H
