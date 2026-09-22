#include <iostream>
#include "MultiTimer.h"

void MultiTimer::markExpired()
{
	const TimeReference now = Clock::now();
	auto iterator = timers.begin();
	while (iterator != timers.end() && std::get<0>(*iterator) <= now)
	{
		auto expired = iterator++;
		std::get<0>(*expired) = TimeReference::max();
		std::get<1>(*expired) = true;
		timers.splice(timers.end(), timers, expired);
	}
}

void MultiTimer::refreshDelay()
{
	auto iterator = timers.begin();
	while (iterator != timers.end() && std::get<1>(*iterator))
	{
		++iterator;
	}

	timepoint = (iterator == timers.end())
		? TimeReference::max()
		: std::get<0>(*iterator);
}

MultiTimer::TimerEntry* MultiTimer::addTimer(const Clock::duration &duration)
{
	std::lock_guard<std::mutex> lock(mutex);
	const TimeReference expiration = Clock::now() + duration;
	auto iterator = timers.begin();
	while (iterator != timers.end() && std::get<0>(*iterator) <= expiration)
	{
		++iterator;
	}

	TimerEntry* timer = &*timers.emplace(iterator, expiration, false);
	refreshDelay();
	condition.notify_all();
	return timer;
}

bool MultiTimer::removeTimer(TimerEntry* timer)
{
	std::lock_guard<std::mutex> lock(mutex);

	for (auto iterator = timers.begin(); iterator != timers.end(); ++iterator)
	{
		if (&*iterator == timer)
		{
			timers.erase(iterator);
			refreshDelay();
			condition.notify_all();
			return true;
		}
	}

	return false;
}

bool MultiTimer::updateTimer(TimerEntry* timer, const Clock::duration &duration)
{
	std::lock_guard<std::mutex> lock(mutex);

	for (auto iterator = timers.begin(); iterator != timers.end(); ++iterator)
	{
		if (&*iterator == timer)
		{
			const TimeReference expiration = Clock::now() + duration;
			std::get<0>(*iterator) = expiration;
			std::get<1>(*iterator) = false;

			auto position = timers.begin();
			while (position != timers.end() &&
				(position == iterator || std::get<0>(*position) <= expiration))
			{
				++position;
			}

			timers.splice(position, timers, iterator);
			refreshDelay();
			condition.notify_all();
			return true;
		}
	}

	return false;
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
			std::cout << "MultiTimer thread : " << timepoint.time_since_epoch().count() << std::endl;
			condition.wait_until(lock, timepoint);
			markExpired();
			refreshDelay();
		} while (!timers.empty());

		std::cout << "MultiTimer thread exiting." << std::endl; });
}