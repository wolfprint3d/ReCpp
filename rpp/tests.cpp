#include "tests.h"
#include <cstdlib>
#include <chrono>
#include <memory>
#include <algorithm>
#include <unordered_set>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <conio.h> // _kbhit
#elif __ANDROID__
    #include <unistd.h> // usleep
    #include <android/log.h>
    #include <stdarg.h>
#else
    #include <unistd.h>
    #include <termios.h>
#endif


namespace rpp
{
    int test::asserts_failed;

    // there are initialization order issues with this global variable, so wrap it to guarantee initialization order
    static vector<test*>& all_tests() noexcept
    {
        static vector<test*> tests;
        return tests;
    }

    test::test(strview name, bool autorun) : name(name), auto_run(autorun)
    {
        all_tests().push_back(this);
    }

    test::~test()
    {
        if (!all_tests().empty())
        {
            auto it = find(all_tests().begin(), all_tests().end(), this);
            if (it != all_tests().end())
                all_tests().erase(it);
        }
    }

    void test::consolef(ConsoleColor color, const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
    #if _WIN32
        static HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        static const int colormap[] = {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, // Default
            FOREGROUND_GREEN, // dark green
            FOREGROUND_RED | FOREGROUND_GREEN, // dark yellow
            FOREGROUND_RED, // dark red
        };
        if (color != Default) SetConsoleTextAttribute(console, colormap[color]);
        if (color == Red) vfprintf(stderr, fmt, ap);
        else              vfprintf(stdout, fmt, ap);
        if (color != Default) SetConsoleTextAttribute(console, colormap[Default]);
    #elif __ANDROID__
        int priority = 0;
        switch (color) {
            case Default: priority = ANDROID_LOG_DEBUG; break;
            case Green:   priority = ANDROID_LOG_INFO;  break;
            case Yellow:  priority = ANDROID_LOG_WARN;  break;
            case Red:     priority = ANDROID_LOG_ERROR; break;
        }
        __android_log_vprint(priority, "rpp", fmt, ap);
    #else // @todo Proper Linux & OSX implementations
        if (color == Red) vfprintf(stderr, fmt, ap);
        else              vfprintf(stdout, fmt, ap);
    #endif
    }

    void test::assert_failed(const char* file, int line, const char* fmt, ...)
    {
        const char* filename = file + int(strview{ file }.rfindany("\\/") - file) + 1;

        char message[8192];
        va_list ap; va_start(ap, fmt);
        vsnprintf(message, 8192, fmt, ap);

        ++asserts_failed;
        consolef(Red, "FAILURE %12s:%d    %s\n", filename, line, message);
    }

    void test::run_test(strview methodFilter)
    {
        char title[256];
        int len = methodFilter
            ? snprintf(title, sizeof(title), "--------  running '%s.%.*s'  --------", name.str, methodFilter.len, methodFilter.str)
            : snprintf(title, sizeof(title), "--------  running '%s'  --------", name.str);

        consolef(Yellow, "%s\n", title);
        run_init();

        if (methodFilter)
        {
            for (size_t i = 0u; i < test_count; ++i)
                if (test_funcs[i].name.find(methodFilter))
                    run_test(test_funcs[i]);
        }
        else
        {
            for (size_t i = 0u; i < test_count; ++i) {
                consolef(Yellow, "%s::%s\n", name.str, test_funcs[i].name.str);
                run_test(test_funcs[i]);
            }
        }

        run_cleanup();
        consolef(Yellow, "%s\n\n", (char*)memset(title, '-', (size_t)len)); // "-------------"
    }

    bool test::run_init()
    {
        try {
            init_test();
            return true;
        } catch (const std::exception& e) {
            consolef(Red, "Unhandled Exception in [%s]::TestInit(): %s\n", name.str, e.what());
            ++asserts_failed;
            return false;
        }
    }

    void test::run_cleanup()
    {
        try {
            cleanup_test();
        } catch (const std::exception& e) {
            consolef(Red, "Unhandled Exception in [%s]::TestCleanup(): %s\n", name.str, e.what());
            ++asserts_failed;
        }
    }

    void test::run_test(test_func& test)
    {
        try {
            (test.lambda.*test.func)();
        } catch (const std::exception& e) {
            consolef(Red, "Unhandled Exception in %s::%s: %s\n", name.str, test.name.str, e.what());
            ++asserts_failed;
        }
    }

    void test::sleep(int millis)
    {
        #if _WIN32
            Sleep(millis);
        #elif __ANDROID__
            usleep(useconds_t(millis) * 1000);
        #else
            usleep(millis * 1000);
        #endif
    }

#if _MSC_VER
    static void pause(int millis = -1/*forever*/)
    {
        printf("\nPress any key to continue...\n");

        using namespace chrono;
        auto start = system_clock::now();
        while (!_kbhit())
        {
            if (millis != -1)
            {
                auto elapsed = duration_cast<milliseconds>(system_clock::now() - start);
                if (elapsed.count() >= millis)
                    break;
            }
            test::sleep(50);
        }
    }
#endif

    int test::run_tests(const char* testNamePattern)
    {
        return run_tests(&testNamePattern, 1);
    }

    int test::run_tests(const vector<string>& testNamePatterns)
    {
        vector<const char*> names;
        names.push_back("");
        for (const string& name : testNamePatterns) names.push_back(name.c_str());
        return run_tests((int)names.size(), (char**)names.data());
    }

    int test::run_tests(const char** testNamePatterns, int numPatterns)
    {
        vector<const char*> names;
        names.push_back("");
        names.insert(names.end(), testNamePatterns, testNamePatterns+numPatterns);
        return run_tests((int)names.size(), (char**)names.data());
    }



    int test::run_tests()
    {
        char empty[1] = "";
        char* argv[1] = { empty };
        return run_tests(1, argv);
    }

    int test::add_test_func(test_func func) // @note Because we can't dllexport std::vector
    {
        if (test_count == test_cap)
        {
            test_cap = test_funcs ? test_count * 2 : 8;
            auto* funcs = new test_func[test_cap];
            for (int i = 0; i < test_count; ++i) funcs[i] = move(test_funcs[i]);
            delete[] test_funcs;
            test_funcs = funcs;
        }
        test_funcs[test_count++] = move(func);
        return test_count - 1;
    }

    static void move_console_window()
    {
        // move console window to the other monitor to make test debugging more seamless
        // if debugger is attached with Visual Studio
    #if _WIN32 && _MSC_VER
        int numMonitors = 0;
        if (IsDebuggerPresent() && (numMonitors = GetSystemMetrics(SM_CMONITORS)) > 1)
        {
            vector<HMONITOR> mon;
            EnumDisplayMonitors(0, 0, [](HMONITOR monitor, HDC, RECT*, LPARAM data) {
                ((vector<HMONITOR>*)data)->push_back(monitor); return 1; }, (LPARAM)&mon);

            RECT consoleRect; GetWindowRect(GetConsoleWindow(), &consoleRect);
            HMONITOR consoleMon = MonitorFromRect(&consoleRect, MONITOR_DEFAULTTONEAREST);
            HMONITOR otherMon   = consoleMon != mon[0] ? mon[0] : mon[1];

            MONITORINFO consoleMI = { sizeof(MONITORINFO) };
            MONITORINFO otherMI   = { sizeof(MONITORINFO) };
            GetMonitorInfo(consoleMon, &consoleMI);
            GetMonitorInfo(otherMon, &otherMI);

            int x = consoleMI.rcMonitor.left > otherMI.rcMonitor.left // moveLeft ?
                ? otherMI.rcMonitor.right - (consoleRect.left - consoleMI.rcMonitor.left) - (consoleRect.right-consoleRect.left)
                : otherMI.rcMonitor.left  + (consoleRect.left - consoleMI.rcMonitor.left);
            int y = otherMI.rcMonitor.top + (consoleRect.top - consoleMI.rcMonitor.top);
            SetWindowPos(GetConsoleWindow(), 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    #endif
    }

    int test::run_tests(int argc, char* argv[])
    {
        move_console_window();
        
        for (test* t : all_tests()) { // set the defaults
            if (!t->auto_run) t->test_enabled = false;
        }

        int numTest = 0;
        if (argc > 1)
        {
            // if arg is provided, we assume they are:
            // test_testname or testname or -test_testname or -testname
            // OR to run a specific test:  testname.specifictest
            unordered_set<strview> enabled, disabled;

            for (int iarg = 1; iarg < argc; ++iarg)
            {
                rpp::strview arg = argv[iarg];
                rpp::strview testName = arg.next('.');
                rpp::strview specific = arg.next('.');

                const bool enableTest = testName[0] != '-';
                if (!enableTest) testName.chomp_first();

                const bool exactMatch = testName.starts_with("test_");
                if (exactMatch) consolef(Yellow, "Filtering exact tests '%s'\n\n", argv[iarg]);
                else            consolef(Yellow, "Filtering substr tests '%s'\n\n", argv[iarg]);
                
                for (test* t : all_tests())
                {
                    if (( exactMatch && t->name == testName) ||
                        (!exactMatch && t->name.find(testName)))
                    {
                        t->test_specific = specific;
                        if (enableTest) enabled.insert(t->name);
                        else            disabled.insert(t->name);
                        break;
                    }
                }
            }

            if (disabled.size())
            {
                for (test* t : all_tests()) {
                    if (t->auto_run) { // only consider disabling auto_run tests
                        t->test_enabled = disabled.find(t->name) == disabled.end();
                        if (!t->test_enabled)
                            consolef(Red, "  Disabled %s\n", t->name.to_cstr());
                    }
                }
            }
            else if (enabled.size())
            {
                for (test* t : all_tests()) { // enable whatever was requested
                    t->test_enabled = enabled.find(t->name) != enabled.end();
                    if (t->test_enabled)
                        consolef(Green, "  Enabled %s\n", t->name.to_cstr());
                }
            }
        }
        else
        {
            consolef(Green, "Running all auto-run tests\n");
            for (test* t : all_tests())
                if (!t->auto_run && !t->test_enabled)
                    consolef(Yellow, "  Disabled NoAutoRun %s\n", t->name.to_cstr());
        }

        // run all the marked tests
        for (test* t : all_tests()) {
            if (t->test_enabled) {
                t->run_test(t->test_specific);
                ++numTest;
            }
        }

        if (test::asserts_failed)
        {
            consolef(Red, "\nWARNING: %d assertions failed!\n", test::asserts_failed);
            #if _MSC_VER
                pause();
            #endif
            return -1;
        }

        if (numTest > 0)
            consolef(Green, "\nSUCCESS: All test runs passed!\n");
        else
            consolef(Yellow, "\nNOTE: No tests were run! (out of %d)\n", (int)all_tests().size());

        #if _MSC_VER
            if (IsDebuggerPresent()) // only pause if we launched from Visual Studio
                pause();
        #endif
        return 0;
    }

} // namespace rpp

#if RPP_TESTS_DEFINE_MAIN
int main(int argc, char* argv[])
{
    return rpp::test::run_tests(argc, argv);
}
#endif
