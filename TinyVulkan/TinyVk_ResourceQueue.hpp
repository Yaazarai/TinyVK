#ifndef TINYVK_TINYVKRESOURCEQUEUE
#define TINYVK_TINYVKRESOURCEQUEUE
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/*
			The TinyVkResourceQueue is for creating a ring-buffer of resources
			for use with concepts like the Vulkan Swapchain, where you may have
			per-frame specific resources for rendering synchronization.

			Constructor Parameters:
				std::array<T, S> : array of resource objects (passed by value).
				callback<size_t&> : callback with size_t out argument which outs the frame index.
				callback<T&> : destructor callback for disposing of resource objects.
		*/
		
		/// @brief A ring-queue of resources which accepts an index/destructor function.
		template<class T, size_t S>
		class TinyVkResourceQueue : public TinyVkDisposable {
		public:
			std::array<T, S> resourceQueue;
			TinyVkCallback<size_t&> indexCallback;
			TinyVkCallback<T&> destructorCallback;
			~TinyVkResourceQueue() { this->Dispose(); }

			/// @brief Creates a resource queue which returns an instance of type T at an frame index I for swapchain rendering.
			TinyVkResourceQueue(std::array<T, S> resources, TinyVkCallback<size_t&> indexCallback, TinyVkCallback<T&> destructor)
			: resourceQueue(resources), indexCallback(indexCallback), destructorCallback(destructor) {}

			/// @brief Get a resource by manual index lookup.
			T& GetResourceByIndex(size_t index) { return resourceQueue[index]; }

			/// @brief Get a resource by frame-index using the indexer callback.
			T& GetFrameResource() {
				size_t index = 0;
				indexCallback.invoke(index);
				return resourceQueue[index];
			}
		};
	}

#endif