#pragma once

#include <cstddef>
#include <functional>
#include <list>

using TimerHandler = std::function<void()>;

struct TimerObject {
	long timeLeft = 0;
	TimerHandler onComplete;

	TimerObject(const TimerHandler &on_complete, long time_left):
		timeLeft(time_left), onComplete(on_complete) {}

	bool expired() const { return timeLeft <= 0; }
	operator bool() const { return 0 < timeLeft; }
	bool operator!() const { return timeLeft <= 0; }

	TimerObject & operator-=(long delta) {
		timeLeft -= delta;
		return *this;
	}

	bool operator<(const TimerObject &other) const {
		return timeLeft < other.timeLeft;
	}

	bool operator<(long value) {
		return timeLeft < value;
	}
};

struct Timer {
	long lastDuration = 0;
	std::list<TimerObject> objects;
	void onExpire();
	void queue(long duration, const TimerHandler &handler);
};
