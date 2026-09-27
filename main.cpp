#include <chrono>
#include <vector>
#include <random>
#include <thread>
#include <iostream>

#include "MultiTimer.h"

class MultiTimerSingleton : public MultiTimer
{
public:
	static MultiTimerSingleton& instance()
	{
		static MultiTimerSingleton singleton;
		return singleton;
	}

private:
	MultiTimerSingleton() = default;
	~MultiTimerSingleton() = default;

	MultiTimerSingleton(const MultiTimerSingleton&) = delete;
	MultiTimerSingleton& operator=(const MultiTimerSingleton&) = delete;
	MultiTimerSingleton(MultiTimerSingleton&&) = delete;
	MultiTimerSingleton& operator=(MultiTimerSingleton&&) = delete;
};

class MultiTimerTestInstance : public MultiTimer
{
public:
	MultiTimerTestInstance() = default;

	MultiTimerTestInstance(const MultiTimerTestInstance&) = delete;
	MultiTimerTestInstance& operator=(const MultiTimerTestInstance&) = delete;
	MultiTimerTestInstance(MultiTimerTestInstance&&) = delete;
	MultiTimerTestInstance& operator=(MultiTimerTestInstance&&) = delete;
};

bool testPredefinedTimerExpirations()
{
	MultiTimerTestInstance timer;
	timer.init();

	const std::vector<std::chrono::seconds> delays = {
		std::chrono::seconds(1),
		std::chrono::seconds(2),
		std::chrono::seconds(3),
		std::chrono::seconds(4)
	};
	std::vector<MultiTimer::TimerEntry*> timers;
	timers.reserve(delays.size());

	bool passed = true;
	for (const std::chrono::seconds& delay : delays)
	{
		MultiTimer::TimerEntry* entry = timer.createTimerEntry(delay);
		timers.push_back(entry);
		if (!timer.updateTimer(entry, delay))
		{
			std::cout << "Failed to schedule a predefined timer." << std::endl;
			passed = false;
		}
	}

	std::chrono::seconds previousDelay(0);
	for (std::size_t index = 0; index < delays.size(); ++index)
	{
		std::this_thread::sleep_for(delays[index] - previousDelay);
		previousDelay = delays[index];

		const MultiTimer::TimeReference expirationCheckDeadline =
			MultiTimer::Clock::now() + std::chrono::milliseconds(100);
		while (!timer.isTimerExpired(timers[index])
			&& MultiTimer::Clock::now() < expirationCheckDeadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (!timer.isTimerExpired(timers[index]))
		{
			std::cout << "Timer " << index + 1 << " had not expired at its wakeup." << std::endl;
			passed = false;
		}
	}

	for (MultiTimer::TimerEntry* entry : timers)
	{
		delete entry;
	}

	return passed;
}

int main()
{
	MultiTimerSingleton& timer = MultiTimerSingleton::instance();
	timer.init();

	std::random_device randomDevice;
	std::mt19937 generator(randomDevice());
	std::uniform_int_distribution<int> distribution(1, 5);
	std::vector<MultiTimer::TimerEntry*> timers;

	for (int index = 0; index < 5; ++index)
	{
		timers.push_back(timer.createTimerEntry(std::chrono::seconds(distribution(generator))));
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));

	for (MultiTimer::TimerEntry* entry : timers)
	{
		timer.updateTimer(entry, std::chrono::seconds(distribution(generator)));
	}

	std::this_thread::sleep_for(std::chrono::seconds(4));

	for (auto iterator = timers.begin(); iterator != timers.end() - 1; ++iterator)
	{
		if (!timer.removeTimerEntry(*iterator))
		{
			std::cout << "Timer not present" << std::endl;
		}
		else
		{
			std::cout << "Timer removed" << std::endl;
		}
	}

	for (MultiTimer::TimerEntry* entry : timers)
	{
		delete entry;
	}

	if (!testPredefinedTimerExpirations())
	{
		return 1;
	}

	std::cout << "Predefined timer expiration test passed." << std::endl;
	return 0;
}
