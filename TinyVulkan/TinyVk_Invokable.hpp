/// Source: https://stackoverflow.com/questions/9568150/what-is-a-c-delegate/9568485#9568485
/// Source: https://en.cppreference.com/w/cpp/utility/functional/invoke
#pragma once
#ifndef TINYVK_TINYVKINVOKABLECALLBACK
#define TINYVK_TINYVKINVOKABLECALLBACK
    #include <functional>
    #include <vector>
    #include <mutex>
    #include <utility>
    #include <type_traits>

    namespace TINYVULKAN_NAMESPACE {
        template<typename... A>
        class TinyVkCallback {
        protected:
            /// Unique identifying hash code.
            size_t hash;
            /// The function bound to this TinyVkCallback.
            std::function<void(A...)> bound;

        public:
            // Create a new TinyVkCallback with the specified arguments.
            TinyVkCallback(std::function<void(A...)> func) : hash(func.target_type().hash_code()), bound(std::move(func)) {}

            /// Compares the underlying hash_code of the TinyVkCallback function(s).
            bool operator == (const TinyVkCallback<A...>& cb) { return hash == cb.hash; }
        
            /// Inequality Compares the underlying hash_code of the TinyVkCallback function(s).
            bool operator != (const TinyVkCallback<A...>& cb) { return hash != cb.hash; }
        
            /// Returns the unique hash code for this TinyVkCallback function.
            constexpr size_t hash_code() const throw() { return hash; }
        
            /// Invoke this TinyVkCallback with required arguments.
            TinyVkCallback<A...>& invoke(A... args) { bound(static_cast<A&&>(args)...); return (*this); }

            /// Operator() invoke this TinyVkCallback with required arguments.
            void operator()(A... args) { bound(static_cast<A&&>(args)...); }
        };

        template<typename... A>
        class TinyVkInvokable {
        protected:
            /// Resource lock for thread-safe accessibility.
            std::timed_mutex safety_lock;
            /// Record of stored TinyVkCallbacks to invoke.
            std::vector<TinyVkCallback<A...>> TinyVkCallbacks;

        public:
            /// Adds a TinyVkCallback to this event, operator +=
            TinyVkInvokable<A...>& hook(const TinyVkCallback<A...> cb) {
                timed_guard<false> g(safety_lock);
                TinyVkCallbacks.push_back(cb);
                return (*this);
            }

            /// Removes a TinyVkCallback from this event, operator -=
            TinyVkInvokable<A...>& unhook(const TinyVkCallback<A...> cb) {
                timed_guard<false> g(safety_lock);
                std::erase_if(TinyVkCallbacks, [cb](TinyVkCallback<A...> c){ return cb.hash_code() == c.hash_code(); });
                return (*this);
            }

            /// Removes all registered TinyVkCallbacks and adds a new TinyVkCallback, operator =
            TinyVkInvokable<A...>& rehook(const TinyVkCallback<A...> cb) {
                timed_guard<false> g(safety_lock);
                TinyVkCallbacks.clear();
                TinyVkCallbacks.push_back(cb);
                return (*this);
            }

            /// Removes all registered TinyVkCallbacks.
            TinyVkInvokable<A...>& empty() {
                timed_guard<false> g(safety_lock);
                TinyVkCallbacks.clear();
                return (*this);
            }

            /// Execute all registered TinyVkCallbacks, operator ()
            TinyVkInvokable<A...>& invoke(A... args) {
                timed_guard<false> g(safety_lock);
                std::vector<TinyVkCallback<A...>> clonecb(TinyVkCallbacks);
                g.Unlock();
                for (TinyVkCallback<A...> cb : clonecb) cb.invoke(static_cast<A&&>(args)...);
                return (*this);
            }

            /// Execute all registered TinyVkCallbacks, operator ()
            TinyVkInvokable<A...>& invoke_blocking(A... args) {
                timed_guard<false> g(safety_lock);
                for (TinyVkCallback<A...> cb : TinyVkCallbacks) cb.invoke(static_cast<A&&>(args)...);
                return (*this);
            }
        };
    }

#endif