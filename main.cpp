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
		timers.push_back(timer.addTimer(std::chrono::seconds(distribution(generator))));
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));

	for (MultiTimer::TimerEntry* entry : timers)
	{
		timer.updateTimer(entry, std::chrono::seconds(distribution(generator)));
	}

	std::this_thread::sleep_for(std::chrono::seconds(5));

	for (auto iterator = timers.begin(); iterator != timers.end() - 1; ++iterator)
	{
		if (!timer.removeTimer(*iterator)) 
		{
			std::cout << "Timer not present" << std::endl;
		}
		else
		{
			std::cout << "Timer removed" << std::endl;
		}
	}

	return 0;
}
