#include "sockets.h"
#include <stdlib.h>    // malloc
#include <stdio.h>     // printf
#include <string.h>    // memcpy,memset,strlen
#include <assert.h>

#if DEBUG || _DEBUG
#  ifdef __APPLE__ // clang iOS
#    define  __assertion_failure() __assert_rtn(__FUNCTION__, __FILE__, __LINE__, "")
#  elif defined __ANDROID__
#    define  __assertion_failure() __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "")
#  elif defined __GNUC__ // other clang, mingw or linux gcc
#    define  __assertion_failure() __assert_fail("", __FILE__, __LINE__, __FUNCTION__)
#  elif _MSC_VER // windows VC++
#    define __assertion_failure() __debugbreak()
#  else
#    error Debugging Assert not defined for this compiler toolkit!
#  endif

#  define Assert(expr, format, ...) /*Assert with message*/ do { if (!(expr)) { \
        fprintf(stderr, ("%s:%d %s $ " format), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        __assertion_failure(); \
    } } while(0)
#else
#  define __assertion_failure(msg) /*nothing in release builds*/
#  define Assert(expression, format, ...) /*do nothing in release builds*/
#endif

/**
 * Cross-platform compatibility definitions and helper functions
 */
#if (_WIN32 || _WIN64) // MSVC and MinGW
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <WinSock2.h>
    #include <ws2tcpip.h>           // winsock2 and TCP/IP functions
    #include <process.h>            // _beginthreadex
    #include <Iphlpapi.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "Ws2_32.lib") // link against winsock libraries
        #pragma comment(lib, "Iphlpapi.lib")
        #include <codecvt>              // std::codecvt_utf8
    #endif
    #define sleep(milliSeconds) Sleep((milliSeconds))
    #define inwin32(...) __VA_ARGS__
    #ifdef __MINGW32__
        extern "C" __declspec(dllimport) int WSAAPI inet_pton(int af, const char* src, void* dst);
        extern "C" const char* WSAAPI inet_ntop(int af, void* pAddr, char* pStringBuf, size_t StringBufSize);
    #endif
#else // UNIX
    #include <time.h>               // clock_gettime
    #include <unistd.h>             // close()
    #include <pthread.h>            // POSIX threads
    #include <sys/types.h>          // required type definitions
    #include <sys/socket.h>         // LINUX sockets
    #include <netdb.h>              // addrinfo, freeaddrinfo...
    #include <netinet/in.h>         // sockaddr_in
    #include <netinet/tcp.h>        // TCP_NODELAY
    #include <errno.h>              // last error number
    #include <sys/ioctl.h>          // ioctl()
    #include <sys/fcntl.h>          // fcntl()
    #include <arpa/inet.h>          // inet_addr, inet_ntoa
    #include <ifaddrs.h>            // getifaddrs / freeifaddrs
    #define sleep(milliSeconds) ::usleep((milliSeconds) * 1000)
    #define inwin32(...) /*nothing*/ 
     // map linux socket calls to winsock calls via macros
    #define closesocket(fd) ::close(fd)
    #define ioctlsocket(fd, request, arg) ::ioctl(fd, request, arg)
#endif
#if DEBUG || _DEBUG
    #define indebug(...) __VA_ARGS__
#else
    #define indebug(...) /*NOP in release*/
#endif

/////////////////////////////////////////////////////////////////////////////

namespace rpp
{
    /////////////////////////////////////////////////////////////////////////////
    // sleeps for specified milliseconds duration
    void thread_sleep(int milliseconds) noexcept
    {
        sleep(milliseconds);
    }
    // spawns a new thread, thread handles are automatically closed 
    void spawn_thread(void(*thread_func)(void* arg), void* arg) noexcept
    {
    #if _WIN32
        _beginthreadex(nullptr, 0, (unsigned(_stdcall*)(void*))thread_func, arg, 0, nullptr);
    #else // Linux
        pthread_t threadHandle;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&threadHandle, &attr, (void*(*)(void*))thread_func, arg);
        pthread_attr_destroy(&attr);
    #endif
    }
    // measure highest accuracy time in seconds for both Windows and Linux
    double timer_time() noexcept
    {
    #if _WIN32
        static double timer_freq = 0.0;
        LARGE_INTEGER t;
        if (timer_freq == 0.0) // initialize high perf timer frequency
        {
            QueryPerformanceFrequency(&t);
            timer_freq = (double)t.QuadPart;
        }
        QueryPerformanceCounter(&t);
        return (double)t.QuadPart / timer_freq;
    #else
        struct timespec tm;
        clock_gettime(CLOCK_REALTIME, &tm);
        return tm.tv_sec + (double)tm.tv_nsec / 1000000000.0;
    #endif
    }
    ////////////////////////////////////////////////////////////////////


#if _WIN32
    static void InitWinSock() noexcept
    {
        static struct wsa { // static var init on first entry to func
            wsa() { WSADATA w; (void)WSAStartup(MAKEWORD(2, 2), &w); }
            ~wsa() { WSACleanup(); }
        } wsa_raii_handle; // this RAII handle will be automatically unloaded
    }
#endif


    //////////////////// ENUM conversions /////////////////////

    address_family to_addrfamily(int af) noexcept
    {
        switch (af) { // AF is nonlinear
        default:
        case AF_UNSPEC:   return AF_DontCare;
        case AF_INET:     return AF_IPv4;
        case AF_INET6:    return AF_IPv6;
        case 32/*AF_BTH*/:return AF_Bth;
        }
    }
    socket_type to_socktype(int sock) noexcept
    {
        return socket_type(sock < 0 || sock > SOCK_SEQPACKET ? 0 : sock);
    }
    socket_type to_socktype(ip_protocol ipp) noexcept
    {
        static socket_type defaults[] = { 
            ST_Unspecified, ST_Raw, ST_Raw, ST_Stream, ST_Stream, ST_Datagram, ST_Raw, ST_RDM
        };
        return defaults[ipp];
    }
    ip_protocol to_ipproto(int ipproto) noexcept
    {
        switch (ipproto) { // IPPROTO is nonlinear
        default:                return IPP_DontCare;
        case IPPROTO_ICMP:      return IPP_ICMP;
        case IPPROTO_IGMP:      return IPP_IGMP;
        case 3/*IPPROTO_GGP*/:  return IPP_BTH;
        case IPPROTO_TCP:       return IPP_TCP;
        case IPPROTO_UDP:       return IPP_UDP;
        case IPPROTO_ICMPV6:    return IPP_ICMPV6;
        case 113/*IPPROTO_PGM*/:return IPP_PGM;
        }
    }
    int addrfamily_int(address_family addressFamily) noexcept
    {
        static int families[] = { AF_UNSPEC, AF_INET, AF_INET6, 32/*AF_BTH*/ };
        return families[addressFamily];
    }
    int socktype_int(socket_type sockType) noexcept
    {
        //SOCK_STREAM,SOCK_DGRAM,SOCK_RAW
        return (int)sockType; // SOCK is linear
    }
    int ipproto_int(ip_protocol ipProtocol) noexcept
    {
        static int protos[] = { 0, IPPROTO_ICMP, IPPROTO_IGMP, 3/*IPPROTO_GGP*/, IPPROTO_TCP, IPPROTO_UDP, IPPROTO_ICMPV6, 113/*IPPROTO_PGM*/ };
        return protos[ipProtocol];
    }


    ///////////////////////////////////////////////////////////////////////////
    ////////        IP Address
    ///////////////////////////////////////////////////////////////////////////

    union saddr {
        sockaddr     sa;
        sockaddr_in  sa4;
        sockaddr_in6 sa6;
        sockaddr_storage sas;
        operator const sockaddr*() const { return &sa; }
        int size() const {
            switch (sa.sa_family) {
                default:       return sizeof sa;
                case AF_INET:  return sizeof sa4;
                case AF_INET6: return sizeof sa6;
            }
        }
    };

    ipaddress::ipaddress() noexcept
    {
        memset(this, 0, sizeof(*this));
    }
    ipaddress::ipaddress(address_family af) noexcept
        : Family(af), Port(0), FlowInfo(0), ScopeId(0)
    {
        if (af == AF_IPv4) Addr4 = INADDR_ANY;
        else memset(Addr6, 0, sizeof Addr6);
    }
    ipaddress::ipaddress(address_family af, int port) noexcept
        : Family(af), Port((uint16_t)port), FlowInfo(0), ScopeId(0)
    {
        if (af == AF_IPv4) Addr4 = INADDR_ANY;
        else memset(Addr6, 0, sizeof Addr6);
    }
    ipaddress::ipaddress(address_family af, const char* hostname, int port) noexcept
        : Family(af), Port((uint16_t)port), FlowInfo(0), ScopeId(0)
    {
        resolve_addr(hostname);
    }
    ipaddress::ipaddress(address_family af, const string& ipAddressAndPort) noexcept : ipaddress(af, 0)
    {
        if (ipAddressAndPort.empty()) {
            return;
        }

        auto pos = ipAddressAndPort.rfind(':');
        if (pos != string::npos)
        {
            Port = (uint16_t)atoi(&ipAddressAndPort[pos + 1]);
            string ip = ipAddressAndPort.substr(0, pos);
            resolve_addr(ip.c_str());
        }
        else
        {
            resolve_addr(ipAddressAndPort.c_str());
        }
    }
    bool ipaddress::resolve_addr(const char* hostname) noexcept
    {
        void* addr  = Family == AF_IPv4 ? (void*)&Addr4 : (void*)&Addr6;
        //int addrLen = Family == AF_IPv4 ? sizeof Addr4 : sizeof Addr6;
        int family  = Family == AF_IPv4 ? AF_INET : AF_INET6;

        memset(addr, 0, sizeof Addr6);

        //// 192.168.0.1
        //if (isdigit(hostname[0])) {
        //    if (inet_pton(family, hostname, addr) == 1)
        //        return true;
        //}
        //// www.google.com
        //else
        //{
            addrinfo hint = { 0 }; // must be nulled
            hint.ai_family = family; // only filter by family
            addrinfo* infos = nullptr;
            string strPort = std::to_string(Port);

            if (isdigit(hostname[0]))
                hint.ai_flags = AI_NUMERICHOST;

            inwin32(InitWinSock());

            if (!getaddrinfo(hostname, strPort.empty() ? 0 : strPort.c_str(), &hint, &infos))
            {
                for (addrinfo* info = infos; info != nullptr; info = info->ai_next)
                {
                    if (info->ai_family == family)
                    {
                        sockaddr_in* sin = (sockaddr_in*)info->ai_addr;
                        //int port = ntohs(sin->sin_port);
                        if (family == AF_INET)
                        {
                            Addr4 = sin->sin_addr.s_addr;
                        }
                        else
                        {
                            sockaddr_in6* sin6 = (sockaddr_in6*)sin;
                            memcpy(Addr6, &sin6->sin6_addr, sizeof(Addr6));
                            FlowInfo = sin6->sin6_flowinfo;
                            ScopeId  = sin6->sin6_scope_id;
                        }
                        //printf("Address: %s\n", cname());
                        freeaddrinfo(infos);
                        return true;
                    }
                }
                freeaddrinfo(infos);
            }
            else {
            #if DEBUG || _DEBUG
                string errmsg = socket::last_err();
                fprintf(stderr, "getaddrinfo failed: %s\n", errmsg.c_str());
            #endif
            }
        //}
        //memset(addr, 0, addrLen);
        return false;
    }
    bool ipaddress::is_resolved() const noexcept
    {
        return Addr4 != 0; // also handles IPv6 due to union magic
    }
    ipaddress::ipaddress(int socket) noexcept
    {
        inwin32(InitWinSock());

        saddr a;
        socklen_t len = sizeof(a);
        if (getsockname(socket, (sockaddr*)&a, &len)) {
            Family=AF_IPv4, Port=0, FlowInfo=0, ScopeId=0;
            return; // quiet fail on error/invalid socket
        }

        Family = to_addrfamily(a.sa4.sin_family);
        Port   = ntohs(a.sa4.sin_port);
        if (Family == AF_IPv4) {
            Addr4 = a.sa4.sin_addr.s_addr;
            FlowInfo = 0, ScopeId = 0;
        }
        else if (Family == AF_IPv6) { // AF_IPv6
            memcpy(Addr6, &a.sa6.sin6_addr, sizeof Addr6);
            FlowInfo = a.sa6.sin6_flowinfo;
            ScopeId  = a.sa6.sin6_scope_id;
        }
    }
    int ipaddress::name(char* dst, int maxCount) const noexcept
    {
        inwin32(InitWinSock());

        if (inet_ntop(addrfamily_int(Family), (void*)&Addr4, dst, maxCount)) {
            return Port ? snprintf(dst, maxCount, "%s:%d", dst, Port) : (int)strlen(dst);
        }
        return 0;
    }
    string ipaddress::name() const noexcept
    {
        char buf[128];
        return string(buf, name(buf, 128));
    }
    const char* ipaddress::cname() const noexcept
    {
        static char buf[128];
        (void)name(buf, 128);
        return buf;
    }
    void ipaddress::clear() noexcept
    {
        memset(this, 0, sizeof(*this));
    }
    int ipaddress::port() const noexcept
    {
        return Port;
    }

    static saddr to_saddr(const ipaddress& ipa) noexcept
    {
        saddr a;
        a.sa4.sin_family = (uint16_t)addrfamily_int(ipa.Family);
        a.sa4.sin_port   = htons(ipa.Port);
        if (ipa.Family == AF_IPv4) {
            a.sa4.sin_addr.s_addr = (unsigned)ipa.Addr4;
            memset(a.sa4.sin_zero, 0, sizeof(a.sa4.sin_zero));
        }
        else { // AF_IPv6
            memcpy(&a.sa6.sin6_addr, ipa.Addr6, sizeof(ipa.Addr6));
            a.sa6.sin6_flowinfo = (unsigned)ipa.FlowInfo;
            a.sa6.sin6_scope_id = (unsigned)ipa.ScopeId;
        }
        return a;
    }

    static ipaddress to_ipaddress(const saddr& a) noexcept
    {
        ipaddress ipa = { to_addrfamily(a.sa4.sin_family), (int)ntohs(a.sa4.sin_port) };
        if (ipa.Family == AF_IPv4) {
            ipa.Addr4 = a.sa4.sin_addr.s_addr;
        }
        else { // AF_IPv6
            memcpy(ipa.Addr6, &a.sa6.sin6_addr, sizeof(ipa.Addr6));
            ipa.FlowInfo = a.sa6.sin6_flowinfo;
            ipa.ScopeId  = a.sa6.sin6_scope_id;
        }
        return ipa;
    }


    ///////////////////////////////////////////////////////////////////////////
    ////////        IP Interfaces
    ///////////////////////////////////////////////////////////////////////////

#if _WIN32 // msvc or mingw
    static string to_string(const wchar_t* wstr) noexcept
    {
    #if _MSC_VER
        std::wstring_convert<std::codecvt<wchar_t, char, mbstate_t>, wchar_t> cvt;
        return cvt.to_bytes(wstr, wstr + wcslen(wstr));
    #else
        return string{ wstr, wstr + wcslen(wstr) };
    #endif
    }
#endif

    vector<ipinterface> ipinterface::get_interfaces(address_family af)
    {
        int family = addrfamily_int(af);
        vector<ipinterface> out;
        
    #if _WIN32
        InitWinSock();

        ULONG bufLen = 0;
        GetAdaptersAddresses(family, 0, nullptr, nullptr, &bufLen);
        IP_ADAPTER_ADDRESSES* ipa_addrs = (IP_ADAPTER_ADDRESSES*)alloca(bufLen);

        if (!GetAdaptersAddresses(family, 0, nullptr, ipa_addrs, &bufLen))
        {
            int count = 0;
            for (auto ipaa = ipa_addrs; ipaa != nullptr; ipaa = ipaa->Next) {
                ++count;
            }
            out.reserve(count);

            for (auto ipaa = ipa_addrs; ipaa != nullptr; ipaa = ipaa->Next)
            {
                out.emplace_back();
                ipinterface& in = out.back();
                in.name = to_string(ipaa->Description);

                if (auto unicast = ipaa->FirstUnicastAddress) do
                {
                    in.addr = to_ipaddress(*(saddr*)unicast->Address.lpSockaddr);
                    in.addrname = in.addr.name();
                }
                while ((unicast = unicast->Next) != nullptr);
            }
        }
    #else
        ifaddrs* if_addrs;
        if (!getifaddrs(&if_addrs))
        {
            int count = 0;
            for (auto ifa = if_addrs; (ifa && ifa->ifa_addr); ifa = ifa->ifa_next) {
                if (!family || ifa->ifa_addr->sa_family == family) ++count;
            }
            out.reserve(count);

            for (auto ifa = if_addrs; (ifa && ifa->ifa_addr); ifa = ifa->ifa_next) 
            {
                if (!family || ifa->ifa_addr->sa_family == family) 
                {
                    auto ipaddr = to_ipaddress(*(saddr*)ifa->ifa_addr);
                    out.emplace_back(string{ifa->ifa_name}, ipaddr, ipaddr.name());
                }
            }
            freeifaddrs(if_addrs);
        }
    #endif
        return out;
    }


    ///////////////////////////////////////////////////////////////////////////
    /////////        socket
    ///////////////////////////////////////////////////////////////////////////

    socket::socket(int handle, const ipaddress& addr, bool shared) noexcept
        : Sock(handle), Addr(addr), Shared(shared), Category(SC_Unknown)
    {
    }
    socket::socket() noexcept : Sock(-1), Shared(false), Category(SC_Unknown)
    {
    }
    socket::socket(int port, address_family af, ip_protocol ipp, socket_option opt) noexcept
        : Sock(-1), Addr(af, port), Shared(false), Category(SC_Unknown)
    {
        listen(Addr, ipp, opt);
    }
    socket::socket(const ipaddress& address, socket_option opt) noexcept
        : Sock(-1), Addr(address), Shared(false), Category(SC_Unknown)
    {
        connect(Addr, opt);
    }
    socket::socket(const ipaddress& address, int timeoutMillis, socket_option opt) noexcept
        : Sock(-1), Addr(address), Shared(false), Category(SC_Unknown)
    {
        connect(Addr, timeoutMillis, opt);
    }
    socket::socket(const char* hostname, int port, address_family af, socket_option opt) noexcept
        : Sock(-1), Addr(af, hostname, port), Shared(false), Category(SC_Unknown)
    {
        connect(Addr, opt);
    }
    socket::socket(const char* hostname, int port, int timeoutMillis, address_family af, socket_option opt) noexcept
        : Sock(-1), Addr(af, hostname, port), Shared(false), Category(SC_Unknown)
    {
        connect(Addr, timeoutMillis, opt);
    }
    socket::socket(socket&& s) noexcept : Sock(s.Sock), Addr(s.Addr), Shared(s.Shared), Category(s.Category) {
        s.Sock = -1;
        s.Addr.clear();
        s.Shared = false;
        s.Category = SC_Unknown;
    }
    socket& socket::operator=(socket&& s) noexcept {
        close();
        Sock   = s.Sock;
        Addr   = s.Addr;
        Shared = s.Shared;
        Category = s.Category;
        s.Sock = -1;
        s.Addr.clear();
        s.Shared   = false;
        s.Category = SC_Unknown;
        return *this;
    }
    socket::~socket() noexcept {
        close();
    }


    void socket::close() noexcept
    {
        if (Sock != -1) {
            if (!Shared) closesocket(Sock);
            Sock = -1;
        }
        //Addr.clear(); // dont clear the address, so we have info on what we just closed
    }

    int socket::release_noclose() noexcept
    {
        int sock = Sock;
        Sock = -1;
        return sock;
    }

    static int os_getsockerr() noexcept {
        #if _WIN32
            return WSAGetLastError();
        #else
            return errno;
        #endif
    }
    string socket::last_err(int err) noexcept
    {
        char buf[2048];
        int  errcode = err ? err : os_getsockerr();
        #if _WIN32
            char* msg = buf + 1024;
            int len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errcode, 0, msg, 1024, nullptr);
            if (msg[len - 2] == '\r') msg[len -= 2] = '\0';
        #else
            char* msg = strerror(errcode);
        #endif
        int errlen = snprintf(buf, sizeof(buf), "error %d: %s", errcode, msg);
        if (errlen < 0) errlen = (int)strlen(buf);
        return string{ buf, buf + errlen };
    }


    int socket::send(const void* buffer, int numBytes) noexcept
    {
        if (numBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;
        return handle_txres(::send(Sock, (const char*)buffer, numBytes, 0));
    }
    int socket::send(const char* str)    noexcept { return send(str, (int)strlen(str)); }
    int socket::send(const wchar_t* str) noexcept { return send(str, int(sizeof(wchar_t) * wcslen(str))); }


    int socket::sendto(const ipaddress& to, const void* buffer, int numBytes) noexcept
    {
        Assert(type() == ST_Datagram, "sendto only works on UDP sockets");

        if (numBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;

        saddr a = to_saddr(to);
        socklen_t len = sizeof(a);
        return handle_txres(::sendto(Sock, (const char*)buffer, numBytes, 0, &a.sa, len));
    }
    int socket::sendto(const ipaddress& to, const char* str)    noexcept { return sendto(to, str, (int)strlen(str)); }
    int socket::sendto(const ipaddress& to, const wchar_t* str) noexcept { return sendto(to, str, int(sizeof(wchar_t) * wcslen(str))); }


    void socket::flush() noexcept
    {
        // flush write buffer:
        bool nodelay = is_nodelay();
        if (!nodelay) set_nagle(true); // force only if needed
        set_nagle(nodelay); // this must be called at least once

        skip(available()); // flush read buffer
    }

    int socket::available() const noexcept
    {
        int bytesAvail;
        return get_ioctl(FIONREAD, bytesAvail) ? -1 : bytesAvail;
    }

    int socket::recv(void* buffer, int maxBytes) noexcept
    {
        if (maxBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;
        return handle_txres(::recv(Sock, (char*)buffer, maxBytes, 0));
    }

    int socket::peek(void* buffer, int numBytes) noexcept
    {
        if (numBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;
        if (type() == ST_Stream)
            return handle_txres(::recv(Sock, (char*)buffer, numBytes, MSG_PEEK));
        saddr a;
        socklen_t len = sizeof(a);
        return handle_txres(::recvfrom(Sock, (char*)buffer, numBytes, MSG_PEEK, &a.sa, &len));
    }

    void socket::skip(int count) noexcept
    {
        Assert(type() != ST_Datagram, "UDP datagrams cannot be skipped like a TCP stream");

        char dump[128];
        for (int i = 0; i < count;) {
            int n = recv(dump, 128);
            if (n <= 0) break;
            i += n;
        }
    }

    NOINLINE int socket::recvfrom(ipaddress& from, void* buffer, int maxBytes) noexcept
    {
        Assert(type() == ST_Datagram, "recvfrom only works on UDP sockets");

        if (maxBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;

        saddr a;
        socklen_t len = sizeof(a);
        int res = handle_txres(::recvfrom(Sock, (char*)buffer, maxBytes, 0, &a.sa, &len));
        if (res > 0) from = to_ipaddress(a);
        return res;
    }

    // properly handles the crazy responses given by the recv() and send() functions
    // returns -1 on critical failure, otherwise it returns bytesAvailable (0...N)
    int socket::handle_txres(long ret) noexcept
    {
        if (ret == 0) { // socket closed gracefully
            close();
            return -1;
        }
        else if (ret == -1) { // socket error?
            return handle_errno();
        }
        return (int)ret; // return as bytesAvailable
    }

    #if _WIN32
        #define ESOCK(errmacro) WSA ## errmacro
    #else
        #define ESOCK(errmacro) errmacro
    #endif

    int socket::handle_errno(int err) noexcept
    {
        int errcode = err ? err : os_getsockerr();
        switch (errcode) {
            default: {
                indebug(auto errmsg = socket::last_err(errcode));
                indebug(fprintf(stderr, "socket %s\r\n", errmsg.c_str()));
                close();
                Assert(false, "socket operation - unexpected failure");
                return -1;
            }
            case ESOCK(EINPROGRESS): return 0; // request is in progress, you should call wait
            case ESOCK(EWOULDBLOCK): return 0; // no data available right now
            case ESOCK(ENOTCONN):    return 0; // this Socket is not Connection oriented! (aka LISTEN SOCKET)
            case ESOCK(ECONNRESET):    // connection lost
            case ESOCK(ECONNREFUSED):  // connect failed
            case ESOCK(EADDRNOTAVAIL): // address doesn't exist - connect failed
            case ESOCK(ETIMEDOUT):     // remote end did not respond
            case ESOCK(ECONNABORTED):  // connection closed
                close();
                return -1;
            case ESOCK(EADDRINUSE): {
                indebug(auto errmsg = socket::last_err(errcode));
                indebug(fprintf(stderr, "%s\r\n", errmsg.c_str()));
                close();
                return -1; // you should call
            }
        }
    }
    
    bool socket::wait_available(int millis) noexcept
    {
        if (!connected()) return false;
        try_for_period(millis, [this]() { return available() != 0; });
        return available() > 0;
    }

    int socket::get_opt(int optlevel, int socketopt) const noexcept {
        int value; socklen_t len = sizeof(int);
        return getsockopt(Sock, optlevel, socketopt, (char*)&value, &len) ? os_getsockerr() : value;
    }
    int socket::set_opt(int optlevel, int socketopt, int value) noexcept {
        return setsockopt(Sock, optlevel, socketopt, (char*)&value, 4) ? os_getsockerr() : 0;
    }
    int socket::get_ioctl(int iocmd, int& outValue) const noexcept {
        return ioctlsocket(Sock, iocmd, (u_long*)&outValue) ? os_getsockerr() : 0;
    }
    int socket::set_ioctl(int iocmd, int value) noexcept {
        return ioctlsocket(Sock, iocmd, (u_long*)&value) ? os_getsockerr() : 0;
    }


    socket_type socket::type() const noexcept {
        return to_socktype(get_opt(SOL_SOCKET, SO_TYPE));
    }
    address_family socket::family() const noexcept {
        return Addr.Family;
    }
    ip_protocol socket::ipproto() const noexcept 
    {
        #ifdef _WIN32
            WSAPROTOCOL_INFO winf = { 0 };
            int len = sizeof(winf);
            getsockopt(Sock, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&winf, &len);
            return to_ipproto(winf.iProtocol);
        #else // this implementation is incomplete:
            switch (get_opt(SOL_SOCKET, SO_TYPE)) {
            default:          return IPP_DontCare;
            case SOCK_STREAM: return IPP_TCP; // assume TCP... might actually be something else
            case SOCK_DGRAM:  return IPP_UDP;
            }
        #endif
    }


    protocol_info socket::protocol() const noexcept
    {
        #ifdef _WIN32
            WSAPROTOCOL_INFO winf = { 0 };
            int len = sizeof(winf);
            getsockopt(Sock, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&winf, &len);
            return protocol_info {
                winf.iProtocol,
                to_addrfamily(winf.iAddressFamily),
                to_socktype(winf.iSocketType),
                to_ipproto(winf.iProtocol),
            };
        #else // this implementation is incomplete:
            int t = get_opt(SOL_SOCKET, SO_TYPE);
            return protocol_info { t, family(), type(), ipproto() };
        #endif
    }


    bool socket::connected() noexcept 
    {
        if (Sock == -1) 
            return false;
        if (int errcode = get_opt(SOL_SOCKET, SO_ERROR)) {
            handle_errno(errcode);
            return false;
        }
        if (Category == SC_Client || Category == SC_Accept) {
            char c;
            return peek(&c, 1) >= 0;
        }
        return true;
    }


    void socket::set_noblock_nodelay() noexcept {
        set_blocking(false); // blocking: false
        set_nagle(false);    // nagle:    false
    }
    void socket::set_blocking(bool socketsBlock) noexcept {
        set_ioctl(FIONBIO, socketsBlock?0:1); // FIONBIO: 1 nonblock, 0 block
    }
    void socket::set_nagle(bool enableNagle) noexcept {
        set_opt(IPPROTO_TCP, TCP_NODELAY, enableNagle?0:1); // TCP_NODELAY: 1 nodelay, 0 nagle enabled
    }
    bool socket::is_blocking() const noexcept {
        int value; get_ioctl(FIONBIO, value);
        return !!value;
    }
    bool socket::is_nodelay() const noexcept {
        return !!get_opt(IPPROTO_TCP, TCP_NODELAY);
    }


    ////////////////////////////////////////////////////////////////////////////////


    bool socket::create(address_family af, ip_protocol ipp, socket_option opt) noexcept
    {
        inwin32(InitWinSock());
        close();

        // create a generic socket
        int family = addrfamily_int(af);
        int type   = socktype_int(to_socktype(ipp));
        int proto  = ipproto_int(ipp);
        Sock = (int)::socket(family, type, proto);
        if (Sock == -1) {
            handle_errno();
            return false;
        }

        if ((opt & SO_Nagle) == 0) {
            set_nagle(false);
        }
        if ((opt & SO_Blocking) == 0) {
            set_blocking(false);
        }

        if (opt & SO_ReuseAddr) {
            if (set_opt(IPPROTO_IP, SO_REUSEADDR, 1)) {
                handle_errno();
                return false;
            }
        #if !_WIN32
            if (set_opt(IPPROTO_IP, SO_REUSEPORT, 1)) {
                handle_errno();
                return false;
            }
        #endif
        }
        return true;
    }

    bool socket::bind(const ipaddress& addr) noexcept
    {
        auto sa = to_saddr(addr);
        if (!::bind(Sock, sa, sa.size()))
        {
            Addr = addr;
            return true;
        }
        handle_errno();
        return false;
    }

    bool socket::listen() noexcept
    {
        Assert(type() != socket_type::ST_Datagram, "Cannot use socket::listen() on UDP sockets");

        if (!::listen(Sock, SOMAXCONN)) { // start listening for new clients
            Category = SC_Listen;
            return true;
        }
        handle_errno();
        return false;
    }

    bool socket::select(int millis, SelectFlag selectFlags) noexcept
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(Sock, &set);
        timeval timeout;
        timeout.tv_sec  = (millis / 1000);
        timeout.tv_usec = (millis % 1000) * 1000;

        //// select requires a blocking socket, so temporarily force blocking IO
        //bool nonBlockingIO = !is_blocking();
        //if (nonBlockingIO) set_blocking(true);

        fd_set* readfds   = (selectFlags & SF_Read)   ? &set : nullptr;
        fd_set* writefds  = (selectFlags & SF_Write)  ? &set : nullptr;
        fd_set* exceptfds = (selectFlags & SF_Except) ? &set : nullptr;
        int rescode = ::select(Sock + 1, readfds, writefds, exceptfds, &timeout);
        int errcode = os_getsockerr();

        //// restore non-blocking IO
        //if (nonBlockingIO) set_blocking(false);

        if (rescode == -1 || errcode != 0) {
            printf("select res:%2d err:%2d\r\n", rescode, errcode);
            handle_errno(errcode);
            return false;
        }
        return rescode > 0; // success: > 0, timeout == 0
    }

    ////////////////////////////////////////////////////////////////////////////////


    bool socket::listen(const ipaddress& localAddr, ip_protocol ipp, socket_option opt) noexcept
    {
        if (!create(localAddr.Family, ipp, opt) || !bind(localAddr))
            return false;

        if (ipp != IPP_UDP && !listen()) // start listening for new clients
            return false;
        return true;
    }
    bool socket::listen(int localPort, address_family af, ip_protocol ipp, socket_option opt) noexcept
    {
        return listen(ipaddress{ af, localPort }, ipp, opt);
    }
    socket socket::listen_to(const ipaddress& localAddr, ip_protocol ipp, socket_option opt) noexcept
    {
        socket s; 
        s.listen(localAddr, ipp, opt); 
        return s;
    }
    socket socket::listen_to(int localPort, address_family af, ip_protocol ipp, socket_option opt) noexcept
    {
        return listen_to(ipaddress{ af, localPort }, ipp, opt);
    }


    socket socket::accept() const noexcept
    {
        Assert(type() != socket_type::ST_Datagram, "Cannot use socket::accept() on UDP sockets, use recvfrom instead");

        // assume the listener socket is already non-blocking
        socket client = { (int)::accept(Sock, nullptr, nullptr), ipaddress{} };

        if (client.Sock != -1) // do we have a client?
        { 
            new (&client.Addr) ipaddress(client.Sock); // update ipaddress
            // set the client socket as non-blocking, since socket options are not inherited
            client.set_noblock_nodelay();
            client.Category = SC_Accept;
        }
        return client;
    }
    socket socket::accept(int millis) const noexcept
    {
        socket client;
        try_for_period(millis, [this, &client]() -> bool {
            return (client = accept()).good();
        });
        return client;
    }

    bool socket::connect(const ipaddress& remoteAddr, socket_option opt) noexcept
    {
        // a connection only makes sense for TCP.. unless we implement
        // an UDP application layer to handle connections? out of scope for socket..
        // need to use SO_Blocking during connect:
        if (create(remoteAddr.Family, IPP_TCP, socket_option(opt|SO_Blocking)))
        {
            Addr = remoteAddr;
            auto sa = to_saddr(remoteAddr);
            if (::connect(Sock, sa, sa.size())) { // did connect fail?
                int err = os_getsockerr();
                if (err == ESOCK(EWOULDBLOCK)) {
                    close();
                    return false; // You have to call connect again until it works
                }
                else if (handle_errno(err) != 0) {
                    return false;
                }
            }
            Category = SC_Client;

            // restore proper blocking flags
            if (!(opt & SO_Blocking)) set_noblock_nodelay();
            return true;
        }
        return false;
    }	
    bool socket::connect(const ipaddress& remoteAddr, int millis, socket_option opt) noexcept
    {
        // we need a non-blocking socket to do select right after connect
        if (create(remoteAddr.Family, IPP_TCP, socket_option(opt & ~SO_Blocking)))
        {
            Addr = remoteAddr;
            auto sa = to_saddr(remoteAddr);
            if (::connect(Sock, sa, sa.size()) != 0)
            {
                int err = os_getsockerr();
                if (err == ESOCK(EINPROGRESS) || err == ESOCK(EWOULDBLOCK))
                {
                    if (select(millis, SF_Write)) {
                        if (opt & SO_Blocking)
                            set_blocking(true);
                        Category = SC_Client;
                        return true;
                    }
                    // timeout
                }
            }
            handle_errno();
        }
        close();
        return false;
    }
    bool socket::connect(const char* hostname, int port, address_family af, socket_option opt) noexcept
    {
        return connect(ipaddress(af, hostname, port), opt);
    }
    bool socket::connect(const char* hostname, int port, int millis, address_family af, socket_option opt) noexcept
    {
        return connect(ipaddress(af, hostname, port), millis, opt);
    }


    socket socket::connect_to(const ipaddress& addr, socket_option opt) noexcept
    { 
        return socket(addr, opt); 
    }
    socket socket::connect_to(const char* hostname, int port, address_family af, socket_option opt) noexcept
    {
        return connect_to(ipaddress(af, hostname, port), opt);
    }
    socket socket::connect_to(const ipaddress& addr, int millis, socket_option opt) noexcept
    { 
        return socket(addr, millis, opt); 
    }
    socket socket::connect_to(const char* hostname, int port, int millis, address_family af, socket_option opt) noexcept
    {
        return connect_to(ipaddress(af, hostname, port), millis, opt);
    }

    ////////////////////////////////////////////////////////////////////////////////

    socket make_udp_randomport(socket_option opt)
    {
        for (int i = 0; i < 10; ++i) {
            int port = (rand() % (65536 - 8000));
            if (socket s = socket::make_udp(port, AF_IPv4, opt))
                return s;
        }
        return {};
    }

    socket make_tcp_randomport(socket_option opt)
    {
        for (int i = 0; i < 10; ++i) {
            int port = (rand() % (65536 - 8000));
            if (socket s = socket::listen_to(port, AF_IPv4, IPP_TCP, opt))
                return s;
        }
        return {};
    }

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

