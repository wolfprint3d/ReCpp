#include "thread_pool.h"
#include <chrono>
#include <cassert>
#include <csignal>
#include <unordered_map>
#if __APPLE__ || __linux__
# include <pthread.h>
#endif
#if __has_include("debugging.h")
# include "debugging.h"
#endif

#define POOL_TASK_DEBUG 0

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    static pool_trace_provider TraceProvider;
    
#if _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #  define WIN32_LEAN_AND_MEAN 1
    #endif
    #include <Windows.h>
    #pragma pack(push,8)
    struct THREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    };
    #pragma pack(pop)
#endif

    void set_this_thread_name(const char* name)
    {
        #if __APPLE__
            pthread_setname_np(name);
        #elif __linux__
            pthread_setname_np(pthread_self(), name);
        #elif _WIN32
            THREADNAME_INFO info { 0x1000, name, DWORD(-1), 0 };
            #pragma warning(push)
            #pragma warning(disable: 6320 6322)
                const DWORD MS_VC_EXCEPTION = 0x406D1388;
                __try {
                    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
                } __except (1){}
            #pragma warning(pop)
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////

#if POOL_TASK_DEBUG
#  ifdef LogWarning
#    define TaskDebug(fmt, ...) LogWarning(fmt, ##__VA_ARGS__)
#  else
#    define TaskDebug(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#  endif
#else
#  define TaskDebug(fmt, ...) // do nothing
#endif

#ifdef LogWarning
#  define UnhandledEx(fmt, ...) LogWarning(fmt, ##__VA_ARGS__)
#else
#  define UnhandledEx(fmt, ...) fprintf(stderr, "pool_task::unhandled_exception $ " fmt "\n", ##__VA_ARGS__)
#endif

    pool_task::pool_task()
    {
        // @note thread must start AFTER mutexes, flags, etc. are initialized, or we'll have a nasty race condition
        th = thread{[this] { run(); }};
    }

    pool_task::~pool_task() noexcept
    {
        kill();
    }

    void pool_task::max_idle_time(float maxIdleSeconds) { maxIdleTime = maxIdleSeconds; }

    void pool_task::run_range(int start, int end, const action<int, int>& newTask) noexcept
    {
        assert(!taskRunning && "rpp::pool_task already running! This can cause deadlocks due to abandoned tasks!");

        { lock_guard<mutex> lock{m};
            trace.clear();
            error = nullptr;
            if (auto tracer = TraceProvider)
                trace = tracer();
            genericTask  = {};
            rangeTask    = newTask;
            rangeStart   = start;
            rangeEnd     = end;

            if (killed) {
                TaskDebug("resurrecting task");
                killed = false;
                (void)join_or_detach(finished);
                th = thread{[this] { run(); }}; // restart thread if needed
            }
            taskRunning = true;
            cv.notify_one();
        }
    }

    void pool_task::run_generic(task_delegate<void()>&& newTask) noexcept
    {
        assert(!taskRunning && "rpp::pool_task already running! This can cause deadlocks due to abandoned tasks!");

        //TaskDebug("queue task");
        { lock_guard<mutex> lock{m};
            trace.clear();
            error = nullptr;
            if (auto tracer = TraceProvider)
                trace = tracer();
            genericTask  = move(newTask);
            rangeTask    = {};
            rangeStart   = 0;
            rangeEnd     = 0;

            if (killed) {
                TaskDebug("resurrecting task");
                killed = false;
                (void)join_or_detach(finished);
                th = thread{[this] { run(); }}; // restart thread if needed
            }
            taskRunning = true;
            cv.notify_one();
        }
    }

    pool_task::wait_result pool_task::wait(int timeoutMillis)
    {
        wait_result result = wait(timeoutMillis, std::nothrow);
        if (error) rethrow_exception(error);
        return result;
    }

    pool_task::wait_result pool_task::wait(int timeoutMillis, nothrow_t) noexcept
    {
        unique_lock<mutex> lock{m};
        while (taskRunning && !killed)
        {
            if (timeoutMillis)
            {
                if (cv.wait_for(lock, milliseconds_t(timeoutMillis)) == std::cv_status::timeout)
                    return timeout;
            }
            else
            {
                cv.wait(lock);
            }
        }
        return finished;
    }

    pool_task::wait_result pool_task::kill(int timeoutMillis) noexcept
    {
        if (killed) {
            return join_or_detach(finished);
        }
        { unique_lock<mutex> lock{m};
            TaskDebug("killing task");
            killed = true;
        }
        cv.notify_all();
        wait_result result = wait(timeoutMillis, std::nothrow);
        return join_or_detach(result);
    }

    pool_task::wait_result pool_task::join_or_detach(wait_result result) noexcept
    {
        if (th.joinable())
        {
            if (result == timeout)
                th.detach();
            // can't join if we're on the same thread as the task itself
            // (can happen during exit())
            else if (std::this_thread::get_id() == th.get_id())
                th.detach();
            else
                th.join();
        }
        return result;
    }

    void pool_task::run() noexcept
    {
        static int pool_task_id;
        char name[32];
        snprintf(name, sizeof(name), "rpp_task_%d", pool_task_id++);
        set_this_thread_name(name);
        //TaskDebug("%s start", name);
        for (;;)
        {
            try
            {
                decltype(rangeTask)   range;
                decltype(genericTask) generic;
                
                // consume the tasks atomically
                { unique_lock<mutex> lock{m};
                    //TaskDebug("%s wait for task", name);
                    if (!wait_for_task(lock)) {
                        TaskDebug("%s stop (%s)", name, killed ? "killed" : "timeout");
                        killed = true;
                        taskRunning = false;
                        cv.notify_all();
                        return;
                    }
                    range   = rangeTask;
                    generic = move(genericTask);
                    rangeTask   = {};
                    genericTask = {};
                    taskRunning = true;
                }
                if (range)
                {
                    //TaskDebug("%s(range_task[%d,%d))", name, rangeStart, rangeEnd);
                    range(rangeStart, rangeEnd);
                }
                else
                {
                    //TaskDebug("%s(generic_task)", name);
                    generic();
                }
            }
            // prevent failures that would terminate the thread
            catch (const exception& e) { unhandled_exception(e.what()); }
            catch (const char* e)      { unhandled_exception(e);        }
            catch (...)                { unhandled_exception("");       }
            { lock_guard<mutex> lock{m};
                taskRunning = false;
                cv.notify_all();
            }
        }
    }

    void pool_task::unhandled_exception(const char* what) noexcept
    {
        if (trace.empty()) UnhandledEx("%s", what);
        else               UnhandledEx("%s\nTask Start Trace:\n%s", what, trace.c_str());
        error = std::current_exception();
    }

    bool pool_task::got_task() const noexcept
    {
        return (bool)rangeTask || (bool)genericTask;
    }

    bool pool_task::wait_for_task(unique_lock<mutex>& lock) noexcept
    {
        for (;;)
        {
            if (killed)
                return false;
            if (got_task())
                return true;
            if (maxIdleTime > 0.000001f)
            {
                if (cv.wait_for(lock, fseconds_t(maxIdleTime)) == cv_status::timeout)
                    return got_task(); // make sure to check for task even if it timeouts
            }
            else
            {
                cv.wait(lock);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////

    thread_pool& thread_pool::global()
    {
        static thread_pool globalPool;
        return globalPool;
    }

#if _WIN32
    static int num_physical_cores()
    {
        DWORD bytes = 0;
        GetLogicalProcessorInformation(nullptr, &bytes);
        vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> coreInfo(bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        GetLogicalProcessorInformation(coreInfo.data(), &bytes);

        int cores = 0;
        for (auto& info : coreInfo)
        {
            if (info.Relationship == RelationProcessorCore)
                ++cores;
        }
        return cores;
    }
#else
    static int num_physical_cores()
    {
        return (int)thread::hardware_concurrency();
    }
#endif

    int thread_pool::physical_cores()
    {
        return global().coreCount;
    }

    void thread_pool::set_task_tracer(pool_trace_provider traceProvider)
    {
        lock_guard<mutex> lock{ tasksMutex };
        TraceProvider = traceProvider;
    }

    thread_pool::thread_pool()
    {
        coreCount = num_physical_cores();
    }

    thread_pool::~thread_pool() noexcept
    {
        // defined destructor to prevent agressive inlining and to manually control task destruction
        lock_guard<mutex> lock{tasksMutex};
        tasks.clear();
    }

    int thread_pool::active_tasks() noexcept
    {
        lock_guard<mutex> lock{tasksMutex};
        int active = 0;
        for (auto& task : tasks) 
            if (task->running()) ++active;
        return active;
    }

    int thread_pool::idle_tasks() noexcept
    {
        lock_guard<mutex> lock{tasksMutex};
        int idle = 0;
        for (auto& task : tasks)
            if (!task->running()) ++idle;
        return idle;
    }

    int thread_pool::total_tasks() const noexcept
    {
        return (int)tasks.size();
    }

    int thread_pool::clear_idle_tasks() noexcept
    {
        lock_guard<mutex> lock{tasksMutex};
        int cleared = 0;
        for (int i = 0; i < (int)tasks.size();)
        {
            if (!tasks[i]->running())
            {
                swap(tasks[i], tasks.back()); // erase_swap_last pattern
                tasks.pop_back();
                ++cleared;
            }
            else ++i;
        }
        return cleared;
    }

    pool_task* thread_pool::start_range_task(size_t& poolIndex, int rangeStart, int rangeEnd, 
                                             const action<int, int>& rangeTask) noexcept
    {
        { lock_guard<mutex> lock{tasksMutex};
            for (; poolIndex < tasks.size(); ++poolIndex)
            {
                pool_task* task = tasks[poolIndex].get();
                if (!task->running())
                {
                    ++poolIndex;
                    task->run_range(rangeStart, rangeEnd, rangeTask);
                    return task;
                }
            }
        }

        auto t  = std::make_unique<pool_task>();
        auto* task = t.get();
        task->max_idle_time(taskMaxIdleTime);
        task->run_range(rangeStart, rangeEnd, rangeTask);

        lock_guard<mutex> lock{tasksMutex};
        tasks.emplace_back(move(t));
        return task;
    }

    void thread_pool::max_task_idle_time(float maxIdleSeconds) noexcept
    {
        lock_guard<mutex> lock{tasksMutex};
        taskMaxIdleTime = maxIdleSeconds;
        for (auto& task : tasks)
            task->max_idle_time(taskMaxIdleTime);
    }

    void thread_pool::parallel_for(int rangeStart, int rangeEnd, 
                                   const action<int, int>& rangeTask) noexcept
    {
        assert(!rangeRunning && "Fatal error: nested parallel loops are forbidden!");
        assert(coreCount > 0 && "There appears to be no hardware concurrency");

        const int range = rangeEnd - rangeStart;
        if (range <= 0)
            return;

        const int cores = range < coreCount ? range : coreCount;
        const int len = range / cores;

        // only one physical core or only one task to run. don't run in a thread
        if (cores <= 1)
        {
            rangeTask(0, len);
            return;
        }

        auto active = (pool_task**)alloca(sizeof(pool_task*) * cores);
        //vector<pool_task*> active(cores, nullptr);
        {
            rangeRunning = true;

            size_t poolIndex = 0;
            for (int i = 0; i < cores; ++i)
            {
                const int start = i * len;
                const int end   = i == cores - 1 ? rangeEnd : start + len;
                active[i] = start_range_task(poolIndex, start, end, rangeTask);
            }
        }

        for (int i = 0; i < cores; ++i)
            active[i]->wait();

        rangeRunning = false;
    }

    pool_task* thread_pool::parallel_task(task_delegate<void()>&& genericTask) noexcept
    {
        { lock_guard<mutex> lock{tasksMutex};
            for (unique_ptr<pool_task>& t : tasks)
            {
                pool_task* task = t.get();
                if (!task->running())
                {
                    task->run_generic(move(genericTask));
                    return task;
                }
            }
        }
        
        // create and run a new task atomically
        auto t = std::make_unique<pool_task>();
        auto* task = t.get();
        task->max_idle_time(taskMaxIdleTime);
        task->run_generic(move(genericTask));

        lock_guard<mutex> lock{tasksMutex};
        tasks.emplace_back(move(t));
        return task;
    }
}
