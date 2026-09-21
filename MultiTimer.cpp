#include <iostream>
#include "MultiTimer.h"

void MultiTimer::addTimer(const Clock::duration& duration)
{
	std::lock_guard<std::mutex> lock(mutex);
	const TimeReference expiration = Clock::now() + duration;
	timers.emplace_back(expiration, false);
}

MultiTimer::~MultiTimer()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		active = false;
	}
	condition.notify_all();

	if (thread != nullptr)
	{
		thread->join();
		delete thread;
		thread = nullptr;
	}
}

void MultiTimer::init()
{
	if (thread != nullptr)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			active = false;
		}
		condition.notify_all();
		thread->join();
		delete thread;
		thread = nullptr;
	}

	{
		std::lock_guard<std::mutex> lock(mutex);
	}

	thread = new std::thread([this]()
		{
		std::unique_lock<std::mutex> lock(mutex);
		active = true;

		while (active)
		{
			condition.wait_for(lock, std::chrono::seconds(1));

			if (!active)
			{
				break;
			}
		}
			std::cout << "MultiTimer thread exiting." << std::endl;
	});
}