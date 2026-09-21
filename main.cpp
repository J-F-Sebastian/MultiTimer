#include <chrono>
#include <random>
#include <thread>

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

	for (int index = 0; index < 5; ++index)
	{
		timer.addTimer(std::chrono::seconds(distribution(generator)));
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));
	return 0;
}
