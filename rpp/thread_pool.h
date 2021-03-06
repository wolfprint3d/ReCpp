#pragma once
/**
 * Fast cross-platform thread-pool, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  pragma warning(disable: 4251) // class 'std::*' needs to have dll-interface to be used by clients of struct 'rpp::*'
#endif
#include <vector>
#include <thread>
#include <string>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "delegate.h"

namespace rpp
{
    using std::mutex;
    using std::condition_variable;
    using std::lock_guard;
    using std::unique_lock;
    using std::atomic_bool;
    using std::string;
    using std::thread;
    using std::vector;
    using std::unique_ptr;
    using std::move;
    using std::nothrow_t;
    using std::cv_status;
    using std::exception;
    using std::exception_ptr;
    using seconds_t  = std::chrono::seconds;
    using fseconds_t = std::chrono::duration<float>;
    using dseconds_t = std::chrono::duration<double>;
    using milliseconds_t = std::chrono::milliseconds;
    template<class T> using duration_t = std::chrono::duration<T>;

    //////////////////////////////////////////////////////////////////////////////////////////

    // Optimized Action delegate, using the 'Fastest Possible C++ Delegates' method
    // This supports lambdas by grabbing a pointer to the lambda instance,
    // so its main intent is to be used during blocking calls like parallel_for
    // It's completely useless for async callbacks that expect lambdas to be stored
    template<class... TArgs> class action
    {
        using Func = void(*)(void* callee, TArgs...);
        void* Callee;
        Func Function;

        template<class T, void (T::*TMethod)(TArgs...)>
        static void proxy(void* callee, TArgs... args)
        {
            T* p = static_cast<T*>(callee);
            return (p->*TMethod)(std::forward<TArgs>(args)...);
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static void proxy(void* callee, TArgs... args)
        {
            const T* p = static_cast<T*>(callee);
            return (p->*TMethod)(std::forward<TArgs>(args)...);
        }

    public:
        action() : Callee(nullptr), Function(nullptr) {}
        action(void* callee, Func function) : Callee(callee), Function(function) {}

        template<class T, void (T::*TMethod)(TArgs...)>
        static action from_function(T* callee)
        {
            return { (void*)callee, &proxy<T, TMethod> };
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static action from_function(const T* callee)
        {
            return { (void*)callee, &proxy<T, TMethod> };
        }

        inline void operator()(TArgs... args) const
        {
            (*Function)(Callee, args...);
        }

        explicit operator bool() const { return Callee != nullptr; }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Simple semaphore for notifying and waiting on events
     */
    class semaphore
    {
        mutex m;
        condition_variable cv;
        int value = 0;

    public:
        enum wait_result {
            notified,
            timeout,
        };
    
        semaphore() = default;
        explicit semaphore(int initialCount)
        {
            reset(initialCount);
        }

        int count() const { return value; }

        void reset(int newCount = 0)
        {
            value = newCount;
            if (newCount > 0)
                cv.notify_one();
        }

        void notify()
        {
            { lock_guard<mutex> lock{ m };
                ++value;
            }
            cv.notify_one();
        }

        bool notify_once() // only notify if count <= 0
        {
            bool shouldNotify;
            { lock_guard<mutex> lock{ m };
                shouldNotify = value <= 0;
                if (shouldNotify)
                    ++value;
            }
            if (shouldNotify) cv.notify_one();
            return shouldNotify;
        }

        void wait()
        {
            unique_lock<mutex> lock{ m };
            while (value <= 0) // wait until value is actually set
                cv.wait(lock);
            --value; // consume the value
        }

        /**
         * Waits while @taskIsRunning is TRUE and sets it to TRUE again before returning
         * This works well for atomic barriers, for example:
         * @code
         *   sync.wait_barrier_while(IsRunning);  // waits while IsRunning == true and sets it to true on return
         *   processTask();
         * @endcode
         * @param taskIsRunning Reference to atomic flag to wait on
         */
        void wait_barrier_while(atomic_bool& taskIsRunning)
        {
            if (!taskIsRunning) {
                taskIsRunning = true;
                return;
            }
            unique_lock<mutex> lock{ m };
            while (taskIsRunning)
                cv.wait(lock);
            taskIsRunning = true;
        }
        
        /**
         * Waits while @atomicFlag is FALSE and sets it to FALSE again before returning
         * This works well for atomic barriers, for example:
         * @code
         *   sync.wait_barrier_until(HasFinished);  // waits while HasFinished == false and sets it to false on return
         *   processResults();
         * @endcode
         * @param hasFinished Reference to atomic flag to wait on
         */
        void wait_barrier_until(atomic_bool& hasFinished)
        {
            if (hasFinished) {
                hasFinished = false;
                return;
            }
            unique_lock<mutex> lock{ m };
            while (!hasFinished)
                cv.wait(lock);
            hasFinished = false;
        }
        
        /**
         * @param timeout Maximum time to wait for this semaphore to be notified
         * @return signalled if wait was successful or timeout if timeoutSeconds had elapsed
         */
        template<class Rep, class Period>
        wait_result wait(std::chrono::duration<Rep, Period> timeout)
        {
            unique_lock<mutex> lock{ m };
            while (value <= 0)
            {
                if (cv.wait_for(lock, timeout) == cv_status::timeout)
                    return semaphore::timeout;
            }
            --value;
            return notified;
        }

        bool try_wait()
        {
            unique_lock<mutex> lock{ m };
            if (value > 0) {
                --value;
                return true;
            }
            return false;
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////


    template<class Signature> using task_delegate = rpp::delegate<Signature>;


    /**
     * Handles signals for pool tasks. This is expected to throw an exception derived
     * from std::runtime_error
     */
    using pool_signal_handler = void (*)(const char* signal);


    /**
     * Provides a plain function which traces the current callstack
     */
    using pool_trace_provider = string (*)();


    RPPAPI void set_this_thread_name(const char* name);

    /**
     * A simple thread-pool task. Can run owning generic tasks using standard function<> and
     * also range non-owning tasks which use the impossibly fast delegate callback system.
     */
    class RPPAPI pool_task
    {
        mutex m;
        condition_variable cv;
        thread th;
        task_delegate<void()> genericTask;
        action<int, int> rangeTask;
        int rangeStart  = 0;
        int rangeEnd    = 0;
        float maxIdleTime = 15;
        string trace;
        exception_ptr error;
        volatile bool taskRunning = false; // an active task is being executed
        volatile bool killed      = false; // this pool_task is being destroyed/has been destroyed

    public:
        enum wait_result {
            finished,
            timeout,
        };

        bool running()  const noexcept { return taskRunning; }
        const char* start_trace() const noexcept { return trace.empty() ? nullptr : trace.c_str(); }

        pool_task();
        ~pool_task() noexcept;
        NOCOPY_NOMOVE(pool_task)

        // Sets the maximum idle time before this pool task is abandoned to free up thread handles
        // @param maxIdleSeconds Maximum number of seconds to remain idle. If set to 0, the pool task is kept alive forever
        void max_idle_time(float maxIdleSeconds = 15);

        // assigns a new parallel for task to run
        // @warning This range task does not retain any resources, so you must ensure
        //          it survives until end of the loop
        // undefined behaviour if called when already running
        void run_range(int start, int end, const action<int, int>& newTask) noexcept;

        // assigns a new generic task to run
        // undefined behaviour if called when already running
        void run_generic(task_delegate<void()>&& newTask) noexcept;

        // wait for task to finish
        // @note Throws any unhandled exceptions from background thread
        //       This is similar to std::future behaviour
        wait_result wait(int timeoutMillis = 0/*0=no timeout*/);
        wait_result wait(int timeoutMillis, nothrow_t) noexcept;

        // kill the task and wait for it to finish
        wait_result kill(int timeoutMillis = 0/*0=no timeout*/) noexcept;

    private:
        void unhandled_exception(const char* what) noexcept;
        void run() noexcept;
        bool got_task() const noexcept;
        bool wait_for_task(unique_lock<mutex>& lock) noexcept;
        wait_result join_or_detach(wait_result result = finished) noexcept;
    };


    /**
     * A generic thread pool that can be used to group and control pool lifetimes
     * By default a global thread_pool is also available
     *
     * By design, nesting parallel for loops is detected as a fatal error, because
     * creating nested threads will not bring any performance benefits
     *
     * Accidentally running nested parallel_for can end up in 8*8=64 threads on an 8-core CPU,
     * which is why it's considered an error.
     */
    class RPPAPI thread_pool
    {
        mutex tasksMutex;
        vector<unique_ptr<pool_task>> tasks;
        float taskMaxIdleTime = 15; // new task timeout in seconds
        int coreCount = 0;
        atomic_bool rangeRunning { false }; // whether parallel range is running or not

    public:

        // the default global thread pool
        static thread_pool& global();

        thread_pool();
        ~thread_pool() noexcept;
        NOCOPY_NOMOVE(thread_pool)

        // number of thread pool tasks that are currently running
        int active_tasks() noexcept;

        // number of thread pool tasks that are in idle state
        int idle_tasks() noexcept;

        // number of running + idle tasks in this threadpool
        int total_tasks() const noexcept;

        // if you're completely done with the thread pool, simply call this to clean up the threads
        // returns the number of tasks cleared
        int clear_idle_tasks() noexcept;

        // starts a single range task atomically
        pool_task* start_range_task(size_t& poolIndex, int rangeStart, int rangeEnd, 
                                    const action<int, int>& rangeTask) noexcept;

        // sets a new max idle time for spawned tasks
        // this does not notify already idle tasks
        // @param maxIdleSeconds Maximum idle seconds before tasks are abandoned and thread handle is released
        //                       Setting this to 0 keeps pool tasks alive forever
        void max_task_idle_time(float maxIdleSeconds = 15) noexcept;

        /**
         * Runs a new Parallel For range task. Only ONE parallel for can be running, any kind of
         * parallel nesting is forbidden. This prevents quadratic thread explosion.
         *
         * This function will block until all parallel tasks have finished running
         *
         * @param rangeStart Usually 0
         * @param rangeEnd Usually vec.size()
         * @param rangeTask Non-owning callback action.
         */
        void parallel_for(int rangeStart, int rangeEnd, const action<int, int>& rangeTask) noexcept;

        template<class Func> 
        void parallel_for(int rangeStart, int rangeEnd, const Func& func) noexcept
        {
            parallel_for(rangeStart, rangeEnd, 
                action<int, int>::from_function<Func, &Func::operator()>(&func));
        }

        // runs a generic parallel task
        pool_task* parallel_task(task_delegate<void()>&& genericTask) noexcept;

        // return the number of physical cores
        static int physical_cores();

        /**
         * Enables tracing of parallel task calls. This makes it possible
         * to trace the callstack which triggered the parallel task, otherwise
         * there would be no hints where the it was launched if the task crashes.
         * @note This will slow down parallel task startup since the call stack is unwound for debugging
         */
        void set_task_tracer(pool_trace_provider traceProvider);
    };


    /**
     * @brief Runs parallel_for on the default global thread pool
     *
     * Runs a new Parallel For range task. Only ONE parallel for can be running, any kind of
     * parallel nesting is forbidden. This prevents quadratic thread explosion.
     *
     * This function will block until all parallel tasks have finished running
     * 
     * The callback function parameters [start, end) provide a range to iterate over,
     * which yields better loop performance. If your callback tasks are heavy, then
     * consider `rpp::parallel_foreach`
     * 
     * @code
     * rpp::parallel_for(0, images.size(), [&](int start, int end) {
     *     for (int i = start; i < end; ++i) {
     *         ProcessImage(images[i]);
     *     }
     * });
     * @endcode
     * @param rangeStart Usually 0
     * @param rangeEnd Usually vec.size()
     * @param func Non-owning callback action:  void(int start, int end)
     */
    template<class Func>
    inline void parallel_for(int rangeStart, int rangeEnd, const Func& func) noexcept
    {
        thread_pool::global().parallel_for(rangeStart, rangeEnd,
            action<int, int>::from_function<Func, &Func::operator()>(&func));
    }


    /**
     * @brief Runs parallel_foreach on the default global thread pool
     * 
     * Runs a new Parallel For range task. Only ONE parallel for can be running, any kind of
     * parallel nesting is forbidden. This prevents quadratic thread explosion.
     * 
     * This function will block until all parallel tasks have finished running
     * 
     * @code
     * std::vector<string> images = ... ;
     * rpp::parallel_foreach(images, [](string& image) {
     *     ProcessImage(image);
     * });
     * @endcode
     * @param items A random access container with an operator[](int index) and size()
     * @param foreach Non-owning foreach callback action:  void(auto item)
     */
    template<class Container, class ForeachFunc>
    inline void parallel_foreach(Container& items, const ForeachFunc& foreach) noexcept
    {
        thread_pool::global().parallel_for(0, (int)items.size(), [&](int start, int end) {
            for (int i = start; i < end; ++i) {
                foreach(items[i]);
            }
        });
    }


    /**
     * Runs a generic parallel task with no arguments on the default global thread pool
     * @note Returns immediately
     * @code
     * rpp::parallel_task([s] {
     *     run_slow_work(s);
     * });
     * @endcode
     */
    inline pool_task* parallel_task(task_delegate<void()>&& genericTask) noexcept
    {
        return thread_pool::global().parallel_task(std::move(genericTask));
    }

#undef move_args
#define __get_nth_move_arg(_unused, _8, _7, _6, _5, _4, _3, _2, _1, N_0, ...) N_0
#define __move_args0(...)

#if _MSC_VER
#define __move_exp(x) x
#define __move_args1(x)      x=std::move(x)
#define __move_args2(x, ...) x=std::move(x),  __move_exp(__move_args1(__VA_ARGS__))
#define __move_args3(x, ...) x=std::move(x),  __move_exp(__move_args2(__VA_ARGS__))
#define __move_args4(x, ...) x=std::move(x),  __move_exp(__move_args3(__VA_ARGS__))
#define __move_args5(x, ...) x=std::move(x),  __move_exp(__move_args4(__VA_ARGS__))
#define __move_args6(x, ...) x=std::move(x),  __move_exp(__move_args5(__VA_ARGS__))
#define __move_args7(x, ...) x=std::move(x),  __move_exp(__move_args6(__VA_ARGS__))
#define __move_args8(x, ...) x=std::move(x),  __move_exp(__move_args7(__VA_ARGS__))

#define move_args(...) __move_exp(__get_nth_move_arg(0, ##__VA_ARGS__, \
        __move_args8, __move_args7, __move_args6, __move_args5, \
        __move_args4, __move_args3, __move_args2, __move_args1, __move_args0)(__VA_ARGS__))
#else
#define __move_args1(x)      x=std::move(x)
#define __move_args2(x, ...) x=std::move(x),  __move_args1(__VA_ARGS__)
#define __move_args3(x, ...) x=std::move(x),  __move_args2(__VA_ARGS__)
#define __move_args4(x, ...) x=std::move(x),  __move_args3(__VA_ARGS__)
#define __move_args5(x, ...) x=std::move(x),  __move_args4(__VA_ARGS__)
#define __move_args6(x, ...) x=std::move(x),  __move_args5(__VA_ARGS__)
#define __move_args7(x, ...) x=std::move(x),  __move_args6(__VA_ARGS__)
#define __move_args8(x, ...) x=std::move(x),  __move_args7(__VA_ARGS__)

#define move_args(...) __get_nth_move_arg(0, ##__VA_ARGS__, \
        __move_args8, __move_args7, __move_args6, __move_args5, \
        __move_args4, __move_args3, __move_args2, __move_args1, __move_args0)(__VA_ARGS__)
#endif


#undef forward_args
#define __get_nth_forward_arg(_unused, _8, _7, _6, _5, _4, _3, _2, _1, N_0, ...) N_0
#define __forward_args0(...)

#if _MSC_VER
#define __forward_exp(x) x
#define __forward_args1(x)      std::forward<decltype(x)>(x)
#define __forward_args2(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args1(__VA_ARGS__))
#define __forward_args3(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args2(__VA_ARGS__))
#define __forward_args4(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args3(__VA_ARGS__))
#define __forward_args5(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args4(__VA_ARGS__))
#define __forward_args6(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args5(__VA_ARGS__))
#define __forward_args7(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args6(__VA_ARGS__))
#define __forward_args8(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args7(__VA_ARGS__))

#define forward_args(...) __forward_exp(__get_nth_forward_arg(0, ##__VA_ARGS__, \
        __forward_args8, __forward_args7, __forward_args6, __forward_args5, \
        __forward_args4, __forward_args3, __forward_args2, __forward_args1, __forward_args0)(__VA_ARGS__))
#else
#define __forward_args1(x)      x=std::move(x)
#define __forward_args2(x, ...) x=std::move(x),  __forward_args1(__VA_ARGS__)
#define __forward_args3(x, ...) x=std::move(x),  __forward_args2(__VA_ARGS__)
#define __forward_args4(x, ...) x=std::move(x),  __forward_args3(__VA_ARGS__)
#define __forward_args5(x, ...) x=std::move(x),  __forward_args4(__VA_ARGS__)
#define __forward_args6(x, ...) x=std::move(x),  __forward_args5(__VA_ARGS__)
#define __forward_args7(x, ...) x=std::move(x),  __forward_args6(__VA_ARGS__)
#define __forward_args8(x, ...) x=std::move(x),  __forward_args7(__VA_ARGS__)

#define forward_args(...) __get_nth_forward_arg(0, ##__VA_ARGS__, \
        __forward_args8, __forward_args7, __forward_args6, __forward_args5, \
        __forward_args4, __forward_args3, __forward_args2, __forward_args1, __forward_args0)(__VA_ARGS__)
#endif


    /**
     * Runs a generic lambda with arguments
     * @note Returns immediately
     * @code 
     * rpp::parallel_task([](string s) {
     *     auto r = run_slow_work(s, 42);
     *     send_results(r);
     * }, "somestring"s);
     * @endcode
     */
    template<class Func, class A>
    inline pool_task* parallel_task(Func&& func, A&& a)
    {
        return thread_pool::global().parallel_task([move_args(func, a)]() mutable {
            func(forward_args(a));
        });
    }
    template<class Func, class A, class B>
    inline pool_task* parallel_task(Func&& func, A&& a, B&& b)
    {
        return thread_pool::global().parallel_task([move_args(func, a, b)]() mutable {
            func(forward_args(a, b));
        });
    }
    template<class Func, class A, class B, class C>
    inline pool_task* parallel_task(Func&& func, A&& a, B&& b, C&& c)
    {
        return thread_pool::global().parallel_task([move_args(func, a, b, c)]() mutable {
            func(forward_args(a, b, c));
        });
    }
    template<class Func, class A, class B, class C, class D>
    inline pool_task* parallel_task(Func&& func, A&& a, B&& b, C&& c, D&& d)
    {
        return thread_pool::global().parallel_task([move_args(func, a, b, c, d)]() mutable {
            func(forward_args(a, b, c, d));
        });
    }

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @return TRUE if @flag == @expectedValue and atomically sets @flag to @newValue
     */
    inline bool atomic_test_and_set(atomic_bool& flag, bool expectedValue = true, bool newValue = false)
    {
        return flag.compare_exchange_weak(expectedValue, newValue);
    }

    //////////////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
