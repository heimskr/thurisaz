#include <algorithm>

#include "Kernel.h"
#include "Timer.h"

void Timer::onExpire() {
	for (auto iter = objects.begin(); iter != objects.end();) {
		TimerObject &object = *iter;

		object.timeLeft -= lastDuration;
		if (!object) {
			object.onComplete();
			objects.erase(iter++);
		} else
			++iter;
	}

	if (!objects.empty())
		asm("%%time %0" :: "r"(objects.front().timeLeft));
	
	strprint("-----------\n");
	for (const auto &object: objects)
		printf(": %ld\n", object.timeLeft);
}

void Timer::queue(long duration, const TimerHandler &handler) {
	if (duration <= 0)
		Kernel::panicf("Invalid duration: %ld", duration);
	long time;
	asm("%%time -> %0" : "=r"(time));
	if (time == 0) { // No timer has been set yet
		printf("[%s:%d] duration[%ld]\n", __FILE__, __LINE__, duration);
		asm("%%time %0" :: "r"(duration));
		objects.emplace(std::upper_bound(objects.begin(), objects.end(), duration), handler, duration);
	} else if (duration < time) {
		const long elapsed = lastDuration - time;
		printf("[%s:%d] elapsed[%ld]\n", __FILE__, __LINE__, duration);
		asm("%%time %0" :: "r"(duration));
		lastDuration = duration;
		for (auto &object: objects) {
			printf(":: %ld -> ", object.timeLeft);
			object.timeLeft -= elapsed;
			printf("%ld\n", object.timeLeft);
		}
		objects.emplace(std::upper_bound(objects.begin(), objects.end(), duration), handler, duration);
	} else {
		const long remaining = duration - time;
		printf("[%s:%d] remaining[%ld]\n", __FILE__, __LINE__, duration);
		objects.emplace(std::lower_bound(objects.begin(), objects.end(), remaining), handler, remaining);
	}
}
