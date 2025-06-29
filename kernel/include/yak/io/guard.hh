#pragma once

#include <yak/sched.h>
#include <yak/mutex.h>

class LockGuard {
    public:
	explicit LockGuard(kmutex &mtx)
		: mutex_(mtx)
		, locked_(true)
	{
		kmutex_acquire(&mutex_, TIMEOUT_INFINITE);
	}

	LockGuard(const LockGuard &) = delete;
	LockGuard &operator=(const LockGuard &) = delete;
	LockGuard(LockGuard &&other) = delete;
	LockGuard &operator=(const LockGuard &&) = delete;

	~LockGuard()
	{
		if (locked_)
			kmutex_release(&mutex_);
	}

	void unlock()
	{
		if (locked_)
			kmutex_release(&mutex_);
	}

	void lock()
	{
		if (!locked_)
			kmutex_acquire(&mutex_, TIMEOUT_INFINITE);
	}

    private:
	kmutex &mutex_;
	bool locked_;
};
