#pragma once
#ifndef TIMED_GUARD
#define TIMED_GUARD
	#include "./TinyVulkan.hpp"
	#include <mutex>

	namespace TINYVULKAN_NAMESPACE {
		template<bool wait = true, size_t timeout = 100>
		class _NODISCARD_LOCK timed_guard {
		private:
			bool signal;
			std::timed_mutex& lock;

		public:
			bool Acquired() { return signal; }

			void Unlock() { if (signal) lock.unlock(); }

			~timed_guard() noexcept { Unlock(); }

			/// @brief Creates a timed lock_guard() which accepts a mutex.
			explicit timed_guard(std::timed_mutex& lock) : lock(lock) {
				if (wait) {
					signal = lock.try_lock_for(std::chrono::milliseconds(timeout));
				} else { lock.lock(); signal = true; }
			}

			timed_guard(const timed_guard&) = delete;

			timed_guard& operator=(const timed_guard&) = delete;
		};
	}

#endif