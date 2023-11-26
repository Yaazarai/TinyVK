#pragma once
#ifndef TINYVK_DISPOSABLE
#define DISPOSABLE_OBJECT
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		#ifndef DISPOSABLE_BOOL_DEFAULT
			#define DISPOSABLE_BOOL_DEFAULT true
		#endif

		class TinyVkDisposable {
		protected:
			std::atomic_bool disposed = false;

		public:
			TinyVkInvokable<bool> onDispose;

			void Dispose() {
				if (disposed) return;
				onDispose.invoke(DISPOSABLE_BOOL_DEFAULT);
				disposed = true;
			}

			bool IsDisposed() { return disposed; }
		};
	}

#endif