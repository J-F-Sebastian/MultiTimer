#include <iostream>
#include "MultiTimer.h"

void MultiTimer::markExpired()
{
	const TimeReference now = Clock::now();
	auto iterator = timers.begin();
	while (iterator != timers.end() && std::get<0>(**iterator) <= now)
	{
		TimerEntry* timer = *iterator;
		iterator = timers.erase(iterator);
		std::get<0>(*timer) = TimeReference::max();
		std::get<1>(*timer) = true;
	}
}

void MultiTimer::refreshDelay()
{
	auto iterator = timers.begin();
	while (iterator != timers.end() && std::get<1>(**iterator))
	{
		++iterator;
	}

	timepoint = (iterator == timers.end())
		? TimeReference::max()
		: std::get<0>(**iterator);
}

MultiTimer::TimerEntry* MultiTimer::createTimerEntry(const Clock::duration&)
{
	return new TimerEntry(TimeReference::max(), true);
}

bool MultiTimer::removeTimerEntry(TimerEntry* timer)
{
	if (timer == nullptr)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(mutex);

	auto iterator = timers.find(timer);
	if (iterator != timers.end())
	{
		timers.erase(iterator);
		std::get<0>(*timer) = TimeReference::max();
		std::get<1>(*timer) = true;
		refreshDelay();
		condition.notify_all();
		return true;
	}

	return false;
}

bool MultiTimer::updateTimer(TimerEntry* timer, const Clock::duration &duration)
{
	if (timer == nullptr)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(mutex);
	const bool isDetached = std::get<1>(*timer);
	const TimeReference expiration = Clock::now() + duration;

	if (!isDetached)
	{
		auto iterator = timers.find(timer);

		if (iterator == timers.end())
		{
			return false;
		}

		timers.erase(iterator);
	}

	std::get<0>(*timer) = expiration;
	std::get<1>(*timer) = false;
	timers.insert(timer);

	refreshDelay();
	condition.notify_all();
	return true;
}

bool MultiTimer::isTimerExpired(TimerEntry* timer)
{
	if (timer == nullptr)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(mutex);
	return std::get<1>(*timer);
}

MultiTimer::~MultiTimer()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		timers.clear();
		condition.notify_all();
	}

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
		condition.notify_all();
		thread->join();
		delete thread;
		thread = nullptr;
	}

	thread = new std::thread([this]()
				 {
		std::unique_lock<std::mutex> lock(mutex);
		do
		{
			//std::cout << "MultiTimer thread : " << timepoint.time_since_epoch().count() << std::endl;
			condition.wait_until(lock, timepoint);
			markExpired();
			refreshDelay();
		} while (!timers.empty());

		std::cout << "MultiTimer thread exiting." << std::endl; });
}