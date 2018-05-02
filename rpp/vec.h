#pragma once
/**
 * Basic Vector Math, Copyright (c) 2015 - Jorma Rebane
 * Distributed under MIT Software License
 */
#include "strview.h"
#include <cmath>  // fabsf, fabs
#include <vector> // for vector<Vector3> and vector<int>

// You can disable RPP SSE intrinsics by declaring #define RPP_SSE_INTRINSICS 0 before including <rpp/vec.h>
#ifndef RPP_SSE_INTRINSICS
#  if _M_IX86_FP || _M_AMD64 || _M_X64 || __SSE2__
#    define RPP_SSE_INTRINSICS 1
#  endif
#endif

#if RPP_SSE_INTRINSICS
#  include <emmintrin.h>
#endif

#undef min
#undef max

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union warning
#endif

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI __declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    constexpr double PI     = 3.14159265358979323846264338327950288;
    constexpr float  PIf    = 3.14159265358979323846264338327950288f;
    constexpr double SQRT2  = 1.41421356237309504880;  // sqrt(2)
    constexpr float  SQRT2f = 1.41421356237309504880f; // sqrt(2)

    /** @return Radians from degrees */
    static constexpr float radf(float degrees)
    {
        return (degrees * rpp::PIf) / 180.0f; // rads=(degs*PI)/180
    }
    /** @return Radians from degrees */
    static constexpr double radf(double degrees)
    {
        return (degrees * rpp::PI) / 180.0; // rads=(degs*PI)/180
    }
    /** @return Degrees from radians */
    static constexpr float degf(float radians)
    {
        return radians * (180.0f / rpp::PIf); // degs=rads*(180/PI)
    }
    /** @return Degrees from radians */
    static constexpr double degf(double radians)
    {
        return radians * (180.0 / rpp::PI); // degs=rads*(180/PI)
    }

    /** @brief Clamps a value between:  min <= value <= max */
    template<class T> static constexpr T clamp(const T value, const T min, const T max) 
    {
        return value < min ? min : (value < max ? value : max);
    }

    /**
     * @brief Math utility, Linear Interpolation
     *        ex: lerp(0.5, 30.0, 60.0); ==> 45.0, because 0.5 is halfway between [30, 60]
     * @param position Multiplier value such as 0.5, 1.0 or 1.5
     * @param start Starting bound of the linear range
     * @param end Ending bound of the linear range
     */
    template<class T> static constexpr T lerp(const T position, const T start, const T end)
    {
        return start + (end-start)*position;
    }

    /**
     * @brief Math utility, inverse operation of Linear Interpolation
     *        ex:  lerpInverse(45.0, 30.0, 60.0); ==> 0.5, because 45 is halfway between [30, 60]
     * @param value Value somewhere between [start, end].
     *              !!Out of bounds values are not checked!! Use clamp(lerpInverse(min, max, value), 0, 1); instead
     * @param start Starting bound of the linear range
     * @param end Ending bound of the linear range
     * @return Ratio between [0..1] or < 0 / > 1 if value is out of bounds
     */
    template<class T> static constexpr T lerpInverse(const T value, const T start, const T end)
    {
        return (value - start) / (end - start);
    }

    ///////////////////////////////////////////////////////////////////////////////

#if RPP_SSE_INTRINSICS && !__clang__ // disabled for now due to __clang__ always picking std::sqrt etc.
    static FINLINE double sqrt(const double& d) noexcept
    { auto m = _mm_set_sd(d); return _mm_cvtsd_f64(_mm_sqrt_sd(m, m)); }
    static FINLINE float sqrt(const float& f) noexcept
    { return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(f))); }

    static FINLINE float min(const float& a, const float& b) noexcept
    { return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b))); }
    static FINLINE double min(const double& a, const double& b) noexcept
    { return _mm_cvtsd_f64(_mm_min_sd(_mm_set_sd(a), _mm_set_sd(b))); }

    static FINLINE float max(const float& a, const float& b) noexcept
    { return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b))); }
    static FINLINE double max(const double& a, const double& b) noexcept
    { return _mm_cvtsd_f64(_mm_max_sd(_mm_set_sd(a), _mm_set_sd(b))); }

    static FINLINE float abs(const float& a) noexcept
    { return _mm_cvtss_f32(_mm_andnot_ps(_mm_castsi128_ps(_mm_set1_epi32(0x80000000)), _mm_set_ss(a))); }
    static FINLINE double abs(const double& a) noexcept
    { return _mm_cvtsd_f64(_mm_andnot_pd(_mm_castsi128_pd(_mm_set1_epi64x(0x8000000000000000UL)), _mm_set_sd(a))); }
#endif

#ifndef RPP_MINMAX_DEFINED
#define RPP_MINMAX_DEFINED
    template<class T> static constexpr const T& min(const T& a, const T& b) { return a < b ? a : b; }
    template<class T> static constexpr const T& max(const T& a, const T& b) { return a > b ? a : b; }
    template<class T> static constexpr const T& min3(const T& a, const T& b, const T& c)
    { return a < b ? (a<c?a:c) : (b<c?b:c); }
    template<class T> static constexpr const T& max3(const T& a, const T& b, const T& c)
    { return a > b ? (a>c?a:c) : (b>c?b:c); }
#endif

    ///////////////////////////////////////////////////////////////////////////////

    /** @return TRUE if abs(value) is very close to 0.0, epsilon controls the threshold */
    template<class T> static constexpr bool nearlyZero(const T& value, const T epsilon = (T)0.001)
    {
        return abs(value) <= epsilon;
    }

    /** @return TRUE if a and b are very close to being equal, epsilon controls the threshold */
    template<class T> static constexpr bool almostEqual(const T& a, const T& b, const T epsilon = (T)0.001)
    {
        return abs(a - b) <= epsilon;
    }

    ///////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Vector2 */
    struct RPPAPI float2
    {
        float x, y;
        constexpr float2(float x, float y) : x(x), y(y) {}
    };

    /** @brief 2D Vector for UI calculations */
    struct RPPAPI Vector2
    {
        float x, y;
        static constexpr float2 ZERO  = { 0.0f, 0.0f }; // 0 0
        static constexpr float2 ONE   = { 1.0f, 1.0f }; // 1 1
        static constexpr float2 RIGHT = { 1.0f, 0.0f }; // X-axis
        static constexpr float2 UP    = { 0.0f, 1.0f }; // Y-axis, OpenGL UP
    
        Vector2() {}
        explicit constexpr Vector2(float xy) : x(xy),  y(xy)  {}
        constexpr Vector2(float x, float y)  : x(x),   y(y)   {}
        constexpr Vector2(const float2& v)   : x(v.x), y(v.y) {}
    
        /** Print the Vector2 */
        void print() const;
    
        /** @return Temporary static string from this Vector */
        const char* toString() const;
    
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) {
            return toString(buffer, SIZE);
        }

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized. 
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0f && y == 0.0f; }
        bool notZero() const { return x != 0.0f || y != 0.0f; }
        bool hasNaN() const { return isnan(x) || isnan(y); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector2& b) const;

        /** @brief Set new XY values */
        void set(float x, float y);
    
        /** @return Length of the vector */
        float length() const;
    
        /** @return Squared length of the vector */
        float sqlength() const;
    
        /** @brief Normalize this vector */
        void normalize();

        /** @brief Normalize this vector to the given magnitude */
        void normalize(const float magnitude);
    
        /** @return A normalized copy of this vector */
        Vector2 normalized() const;
        Vector2 normalized(const float magnitude) const;

        /** @return Dot product of two vectors */
        float dot(const Vector2& v) const;

        /** @return Normalized direction of this vector */
        Vector2 direction() const { return normalized(); }

        /**
         * Treating this as point A, gives the RIGHT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 right(const Vector2& b, float magnitude = 1.0f) const;

        /**
         * Treating this as point A, gives the LEFT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 left(const Vector2& b, float magnitude = 1.0f) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular RIGHT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 right(float magnitude = 1.0f) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular LEFT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 left(float magnitude = 1.0f) const;
    
        Vector2& operator+=(float f) { x+=f; y+=f; return *this; }
        Vector2& operator-=(float f) { x-=f; y-=f; return *this; }
        Vector2& operator*=(float f) { x*=f; y*=f; return *this; }
        Vector2& operator/=(float f) { x/=f; y/=f; return *this; }
        Vector2& operator+=(const Vector2& b) { x+=b.x; y+=b.y; return *this; }
        Vector2& operator-=(const Vector2& b) { x-=b.x; y-=b.y; return *this; }
        Vector2& operator*=(const Vector2& b) { x*=b.x; y*=b.y; return *this; }
        Vector2& operator/=(const Vector2& b) { x/=b.x; y/=b.y; return *this; }
        Vector2  operator+ (const Vector2& b) const { return { x+b.x, y+b.y }; }
        Vector2  operator- (const Vector2& b) const { return { x-b.x, y-b.y }; }
        Vector2  operator* (const Vector2& b) const { return { x*b.x, y*b.y }; }
        Vector2  operator/ (const Vector2& b) const { return { x/b.x, y/b.y }; }
        Vector2  operator- () const { return {-x, -y}; }
    
        bool operator==(const Vector2& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Vector2& b) const { return x != b.x || y != b.y; }
    };

    inline Vector2 operator+(const Vector2& a, float f) { return { a.x+f, a.y+f }; }
    inline Vector2 operator-(const Vector2& a, float f) { return { a.x-f, a.y-f }; }
    inline Vector2 operator*(const Vector2& a, float f) { return { a.x*f, a.y*f }; }
    inline Vector2 operator/(const Vector2& a, float f) { return { a.x/f, a.y/f }; }
    inline Vector2 operator+(float f, const Vector2& a) { return { f+a.x, f+a.y }; }
    inline Vector2 operator-(float f, const Vector2& a) { return { f-a.x, f-a.y }; }
    inline Vector2 operator*(float f, const Vector2& a) { return { f*a.x, f*a.y }; }
    inline Vector2 operator/(float f, const Vector2& a) { return { f/a.x, f/a.y }; }

    inline constexpr Vector2 clamp(const Vector2& value, const Vector2& min, const Vector2& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y) };
    }

    inline constexpr Vector2 lerp(float position, const Vector2& start, const Vector2& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position };
    }
    
    ///////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Vector2d */
    struct RPPAPI double2
    {
        double x, y;
        constexpr double2(double x, double y) : x(x), y(y) {}
    };

    /** @brief 2D Vector for UI calculations */
    struct RPPAPI Vector2d
    {
        double x, y;
        static constexpr double2 ZERO  = { 0.0, 0.0 }; // 0 0
        static constexpr double2 ONE   = { 1.0, 1.0 }; // 1 1
        static constexpr double2 RIGHT = { 1.0, 0.0 }; // X-axis
        static constexpr double2 UP    = { 0.0, 1.0 }; // Y-axis, OpenGL UP
    
        Vector2d() {}
        explicit constexpr Vector2d(double xy) : x(xy),  y(xy)  {}
        constexpr Vector2d(double x, double y) : x(x),   y(y)   {}
        constexpr Vector2d(const double2& v)   : x(v.x), y(v.y) {}
    
        /** Print the Vector2 */
        void print() const;
    
        /** @return Temporary static string from this Vector */
        const char* toString() const;
    
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) {
            return toString(buffer, SIZE);
        }

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized. 
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0 && y == 0.0; }
        bool notZero() const { return x != 0.0 || y != 0.0; }
        bool hasNaN() const { return isnan(x) || isnan(y); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector2d& b) const;

        /** @brief Set new XY values */
        void set(double x, double y);
    
        /** @return Length of the vector */
        double length() const;
    
        /** @return Squared length of the vector */
        double sqlength() const;
    
        /** @brief Normalize this vector */
        void normalize();

        /** @brief Normalize this vector to the given magnitude */
        void normalize(const double magnitude);
    
        /** @return A normalized copy of this vector */
        Vector2d normalized() const;
        Vector2d normalized(const double magnitude) const;

        /** @return Dot product of two vectors */
        double dot(const Vector2d& v) const;

        /** @return Normalized direction of this vector */
        Vector2d direction() const { return normalized(); }

        /**
         * Treating this as point A, gives the RIGHT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d right(const Vector2d& b, double magnitude = 1.0) const;

        /**
         * Treating this as point A, gives the LEFT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d left(const Vector2d& b, double magnitude = 1.0) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular RIGHT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d right(double magnitude = 1.0) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular LEFT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d left(double magnitude = 1.0) const;
        
        Vector2d& operator+=(double f) { x+=f; y+=f; return *this; }
        Vector2d& operator-=(double f) { x-=f; y-=f; return *this; }
        Vector2d& operator*=(double f) { x*=f; y*=f; return *this; }
        Vector2d& operator/=(double f) { x/=f; y/=f; return *this; }
        Vector2d& operator+=(const Vector2d& b) { x+=b.x; y+=b.y; return *this; }
        Vector2d& operator-=(const Vector2d& b) { x-=b.x; y-=b.y; return *this; }
        Vector2d& operator*=(const Vector2d& b) { x*=b.x; y*=b.y; return *this; }
        Vector2d& operator/=(const Vector2d& b) { x/=b.x; y/=b.y; return *this; }
        Vector2d  operator+ (const Vector2d& b) const { return { x+b.x, y+b.y }; }
        Vector2d  operator- (const Vector2d& b) const { return { x-b.x, y-b.y }; }
        Vector2d  operator* (const Vector2d& b) const { return { x*b.x, y*b.y }; }
        Vector2d  operator/ (const Vector2d& b) const { return { x/b.x, y/b.y }; }
        Vector2d  operator- () const { return {-x, -y}; }
    
        bool operator==(const Vector2d& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Vector2d& b) const { return x != b.x || y != b.y; }
    };

    inline Vector2d operator+(const Vector2d& a, double f) { return { a.x+f, a.y+f }; }
    inline Vector2d operator-(const Vector2d& a, double f) { return { a.x-f, a.y-f }; }
    inline Vector2d operator*(const Vector2d& a, double f) { return { a.x*f, a.y*f }; }
    inline Vector2d operator/(const Vector2d& a, double f) { return { a.x/f, a.y/f }; }
    inline Vector2d operator+(double f, const Vector2d& a) { return { f+a.x, f+a.y }; }
    inline Vector2d operator-(double f, const Vector2d& a) { return { f-a.x, f-a.y }; }
    inline Vector2d operator*(double f, const Vector2d& a) { return { f*a.x, f*a.y }; }
    inline Vector2d operator/(double f, const Vector2d& a) { return { f/a.x, f/a.y }; }

    inline constexpr Vector2d clamp(const Vector2d& value, const Vector2d& min, const Vector2d& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y) };
    }

    inline constexpr Vector2d lerp(double position, const Vector2d& start, const Vector2d& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position };
    }
    
    ////////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Point */
    struct RPPAPI int2
    {
        int x, y;
        constexpr int2(int x, int y) : x(x), y(y) {}
    };

    /**
     * @brief Utility for dealing with integer-only points. Pretty rare, but useful.
     */
    struct RPPAPI Point
    {
        int x, y;

        static constexpr int2 ZERO = { 0, 0 };

        Point() {}
        constexpr Point(int x, int y) : x(x), y(y) {}
        explicit constexpr Point(int xy) : x(xy), y(xy) {}

        explicit operator bool()  const { return  x || y;  }
        bool operator!() const { return !x && !y; }
        void set(int nx, int ny) { x = nx; y = ny; }

        bool isZero()  const { return !x && !y; }
        bool notZero() const { return  x || y;  }
    
        const char* toString() const;
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) {
            return toString(buffer, SIZE);
        }

        Point& operator+=(int i) { x+=i; y+=i; return *this; }
        Point& operator-=(int i) { x-=i; y-=i; return *this; }
        Point& operator*=(int i) { x*=i; y*=i; return *this; }
        Point& operator/=(int i) { x/=i; y/=i; return *this; }
        Point& operator*=(float f) { x = int(x*f); y = int(y*f); return *this; }
        Point& operator/=(float f) { x = int(x/f); y = int(y/f); return *this; }
        Point& operator+=(const Point& b) { x+=b.x; y+=b.y; return *this; }
        Point& operator-=(const Point& b) { x-=b.x; y-=b.y; return *this; }
        Point& operator*=(const Point& b) { x*=b.x; y*=b.y; return *this; }
        Point& operator/=(const Point& b) { x/=b.x; y/=b.y; return *this; }
        Point  operator+ (const Point& b) const { return { x+b.x, y+b.y }; }
        Point  operator- (const Point& b) const { return { x-b.x, y-b.y }; }
        Point  operator* (const Point& b) const { return { x*b.x, y*b.y }; }
        Point  operator/ (const Point& b) const { return { x/b.x, y/b.y }; }
        Point  operator- () const { return {-x, -y}; }

        bool operator==(const Point& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Point& b) const { return x != b.x || y != b.y; }
    };

    inline Point operator+(const Point& a, int i) { return { a.x+i, a.y+i }; }
    inline Point operator-(const Point& a, int i) { return { a.x-i, a.y-i }; }
    inline Point operator*(const Point& a, int i) { return { a.x*i, a.y*i }; }
    inline Point operator/(const Point& a, int i) { return { a.x/i, a.y/i }; }
    inline Point operator+(int i, const Point& a) { return { i+a.x, i+a.y }; }
    inline Point operator-(int i, const Point& a) { return { i-a.x, i-a.y }; }
    inline Point operator*(int i, const Point& a) { return { i*a.x, i*a.y }; }
    inline Point operator/(int i, const Point& a) { return { i/a.x, i/a.y }; }

    ////////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Vector4 and Rect */
    struct RPPAPI float4
    {
        float x, y, z, w;
        constexpr float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    };

    /** @brief Utility for dealing with 2D Rects */
    struct RPPAPI Rect
    {
        union {
            struct { float x, y, w, h; };
            struct { Vector2 pos; Vector2 size; };
            float4 xywh;
        };

        static constexpr float4 ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };

        Rect() {}
        constexpr Rect(float x, float y, float w, float h)      : x(x), y(y), w(w), h(h) {}
        constexpr Rect(const Vector2& pos, const Vector2& size) : pos(pos),   size(size) {}
        constexpr Rect(const float4& xywh)            : xywh(xywh) {}
        Rect(float x, float y, const Vector2& size)   : x(x), y(y), w(size.x), h(size.y) {}
        Rect(const Vector2& pos, float w, float h)    : x(pos.x), y(pos.y), w(w), h(h)   {}

        /** Print the Rect */
        void print() const;

        /** @return Temporary static string from this Rect in the form of "{pos %g;%g size %g;%g}" */
        const char* toString() const;

        char* toString(char* buffer) const;
        char* toString(char* buffer, int bufSize) const;
        template<int N> char* toString(char(&b)[N]) { return toString(b, N); }

        float area()   const { return w * h; }
        float left()   const { return x;     }
        float top()    const { return y;     }
        float right()  const { return x + w; }
        float bottom() const { return y + h; }
        const Vector2& topleft()  const { return pos; }
        Vector2        botright() const { return { x+w, y+h }; }

        /** @return TRUE if this Rect is equal to Rect::ZERO */
        bool isZero() const { return !x && !y && !w && !h; }
        /** @return TRUE if this Rect is NOT equal to Rect::ZERO */
        bool notZero() const { return w || h || x || y; }
    
        /** @return True if point is inside this Rect */
        bool hitTest(const Vector2& position) const;
        bool hitTest(const float xPos, const float yPos) const;
        /** @return TRUE if r is completely inside this Rect */
        bool hitTest(const Rect& r) const;

        /** @return TRUE if this Rect and r intersect */
        bool intersectsWith(const Rect& r) const;

        /** @brief Extrude the bounds of this rect by a positive or negative amount */
        void extrude(float extrude);
        void extrude(const Vector2& extrude);

        Rect extruded(float extrude) const { 
            Rect r = *this; 
            r.extrude(extrude); 
            return *this; 
        }

        Rect& operator+=(const Rect& b) { join(b); return *this; }

        // joins two rects, resulting in a rect that fits them both
        Rect joined(const Rect& b) const;

        // modifies this rect by joining rect b with this rect
        void join(const Rect& b);

        // clips this Rect with a potentially smaller frame
        // this Rect will then fit inside the frame Rect
        void clip(const Rect& frame);

        Rect operator+(const Rect& b) const { return joined(b); }
    
        bool operator==(const Rect& r) const { return x == r.x && y == r.y && w == r.w && h == r.h; }
        bool operator!=(const Rect& r) const { return x != r.x || y != r.y || w != r.w || h != r.h; }
    };

    inline Rect operator+(const Rect& a, float f) { return{ a.x+f, a.y+f, a.w, a.h }; }
    inline Rect operator-(const Rect& a, float f) { return{ a.x-f, a.y-f, a.w, a.h }; }
    inline Rect operator*(const Rect& a, float f) { return{ a.x, a.y, a.w*f, a.h*f }; }
    inline Rect operator/(const Rect& a, float f) { return{ a.x, a.y, a.w/f, a.h/f }; }
    inline Rect operator+(float f, const Rect& a) { return{ f+a.x, f+a.y, a.w, a.h }; }
    inline Rect operator-(float f, const Rect& a) { return{ f-a.x, f-a.y, a.w, a.h }; }
    inline Rect operator*(float f, const Rect& a) { return{ a.x, a.y, f*a.w, f*a.h }; }
    inline Rect operator/(float f, const Rect& a) { return{ a.x, a.y, f/a.w, f/a.h }; }
    
    ///////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Vector3 */
    struct RPPAPI float3
    { 
        float x, y, z;
        constexpr float3(float x, float y, float z) : x(x), y(y), z(z) {}
    };

    struct Vector3d;

    /** 
     * 3D Vector for matrix calculations
     * The coordinate system assumed in UP, FORWARD, RIGHT is OpenGL coordinate system:
     * +X is Right on the screen
     * +Y is Up on the screen
     * +Z is Forward INTO the screen
     */
    struct RPPAPI Vector3
    {
        union {
            struct { float x, y, z; };
            struct { float r, g, b; };
            //struct { Vector2 xy; };
            //struct { float _x; Vector2 yz; };
            float3 xyz;
        };

        static constexpr float3 ZERO           = { 0.0f, 0.0f, 0.0f };      // 0 0 0
        static constexpr float3 ONE            = { 1.0f, 1.0f, 1.0f };      // 1 1 1

        static constexpr float3 LEFT           = { -1.0f,  0.0f,  0.0f };   // -X axis
        static constexpr float3 RIGHT          = { +1.0f,  0.0f,  0.0f };   // +X axis
        static constexpr float3 UP             = {  0.0f, +1.0f,  0.0f };   // +Y axis
        static constexpr float3 DOWN           = {  0.0f, -1.0f,  0.0f };   // -Y axis
        static constexpr float3 FORWARD        = {  0.0f,  0.0f, +1.0f };   // +Z axis
        static constexpr float3 BACKWARD       = {  0.0f,  0.0f, -1.0f };   // -Z axis

        static constexpr float3 XAXIS          = { +1.0f,  0.0f,  0.0f };   // +X axis
        static constexpr float3 YAXIS          = {  0.0f, +1.0f,  0.0f };   // +Y axis
        static constexpr float3 ZAXIS          = {  0.0f,  0.0f, +1.0f };   // +Z axis
        
        static constexpr float3 WHITE          = { 1.0f, 1.0f, 1.0f };       // RGB 1 1 1
        static constexpr float3 BLACK          = { 0.0f, 0.0f, 0.0f };       // RGB 0 0 0
        static constexpr float3 RED            = { 1.0f, 0.0f, 0.0f };       // RGB 1 0 0
        static constexpr float3 GREEN          = { 0.0f, 1.0f, 0.0f };       // RGB 0 1 0
        static constexpr float3 BLUE           = { 0.0f, 0.0f, 1.0f };       // RGB 0 0 1
        static constexpr float3 YELLOW         = { 1.0f, 1.0f, 0.0f };       // 1 1 0
        static constexpr float3 ORANGE         = { 1.0f, 0.50196f, 0.0f };   // 1 0.502 0; 255 128 0
        static constexpr float3 MAGENTA        = { 1.0f, 0.0f, 1.0f };       // 1 0 1
        static constexpr float3 CYAN           = { 0.0f, 1.0f, 1.0f };       // 0 1 1
        static constexpr float3 SWEETGREEN     = { 0.337f, 0.737f, 0.223f }; // 86, 188, 57
        static constexpr float3 CORNFLOWERBLUE = { 0.33f, 0.66f, 1.0f };     // #55AAFF  85, 170, 255

        Vector3() {}
        constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
        constexpr Vector3(const float3& v) : xyz(v) {}
        constexpr Vector3(const Vector2& xy, float z) : x(xy.x), y(xy.y), z(z) {}
        constexpr Vector3(float x, const Vector2& yz) : x(x), y(yz.x), z(yz.y) {}
        explicit constexpr Vector3(float xyz) : x(xyz), y(xyz), z(xyz) {}
        
        explicit operator Vector3d() const;

        /** @brief Set new XYZ values */
        void set(float x, float y, float z);
    
        /** @return Length of the vector */
        float length() const;
    
        /** @return Squared length of the vector */
        float sqlength() const;

        /** @return Absolute distance from this vec3 to target vec3 */
        float distanceTo(const Vector3& v) const;
    
        /** @brief Normalize this vector */
        void normalize();
        void normalize(const float magnitude);
    
        /** @return A normalized copy of this vector */
        Vector3 normalized() const;
        Vector3 normalized(const float magnitude) const;
    
        /** @return Cross product with another vector */
        Vector3 cross(const Vector3& b) const;
    
        /** @return Dot product with another vector */
        float dot(const Vector3& b) const;

        /**
         * Creates a mask vector for each component
         * x = almostZero(x) ? 1.0f : 0.0f;
         */
        Vector3 mask() const;

        /**
         * @return Assuming this is a direction vector, gives XYZ Euler rotation in RADIANS
         * X: Roll
         * Y: Pitch
         * Z: Yaw
         */
        Vector3 toEulerAngles() const;

        /**
         * Transforms this Vector3 with a transformation functor
         * @param componentTransform   `float transform(float v)`
         */
        template<class Transform> void transform(const Transform& componentTransform)
        {
            x = componentTransform(x);
            y = componentTransform(y);
            z = componentTransform(z);
        }

        void print() const;
        const char* toString() const;
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized. 
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0f && y == 0.0f && z == 0.0f; }
        bool notZero() const { return x != 0.0f || y != 0.0f || z != 0.0f; }
        bool hasNaN() const { return isnan(x) || isnan(y) || isnan(z); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector3& b) const;

        Vector3& operator+=(float f) { x+=f; y+=f; z+=f; return *this; }
        Vector3& operator-=(float f) { x-=f; y-=f; z-=f; return *this; }
        Vector3& operator*=(float f) { x*=f; y*=f; z*=f; return *this; }
        Vector3& operator/=(float f) { x/=f; y/=f; z/=f; return *this; }
        Vector3& operator+=(const Vector3& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
        Vector3& operator-=(const Vector3& v) { x-=v.x; y-=v.y; z-=v.z; return *this; }
        Vector3& operator*=(const Vector3& v) { x*=v.x; y*=v.y; z*=v.z; return *this; }
        Vector3& operator/=(const Vector3& v) { x/=v.x; y/=v.y; z/=v.z; return *this; }
        Vector3  operator+ (const Vector3& v) const { return { x+v.x, y+v.y, z+v.z }; }
        Vector3  operator- (const Vector3& v) const { return { x-v.x, y-v.y, z-v.z }; }
        Vector3  operator* (const Vector3& v) const { return { x*v.x, y*v.y, z*v.z }; }
        Vector3  operator/ (const Vector3& v) const { return { x/v.x, y/v.y, z/v.z }; }
        Vector3  operator- () const { return {-x, -y, -z}; }
    
        bool operator==(const Vector3& v) const { return x == v.x && y == v.y && z == v.z; }
        bool operator!=(const Vector3& v) const { return x != v.x || y != v.y || z != v.z; }

        static Vector3 smoothColor(const Vector3& src, const Vector3& dst, float ratio);

        // don't use macros plz :)
        #undef RGB
        /** @return A 3-component float color from integer RGBA color */
        static constexpr Vector3 RGB(int r, int g, int b)
        {
            return { r / 255.0f, g / 255.0f, b / 255.0f };
        }

        /**
         * Parses any type of color string. Supported:
         * -) RGB HEX color strings: #rrggbb
         * -) Named color strings: 'orange'
         * -) RGB integer values: 255 0 128
         * -) RGB float values:   0.1 0.2 0.5
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0
         *       Example '#aa' would give 170 0 0  and '0.1' would give 25 0 0
         * @return Default color WHITE or parsed color in normalized float value range [0.0 ... 1.0]
         */
        static Vector3 parseColor(const strview& s) noexcept;

        /**
         * Some common 3D vector conversions
         * Some conversions are two way <->, but we still provide duplicate overloads for consistency
         *
         * This Vector library is based on OpenGL coordsys -- important when dealing with Matrix/Vector4(quat).
         * 
         * OpenGL coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
         *
         */

        // OpenGL to OpenCV coordinate conversion. Works both ways: GL <-> CV
        // OpenGL coordsys:  +X is Right on the screen, +Y is Up on the screen,   +Z is Forward INTO the screen
        // OpenCV coordsys:  +X is Right on the screen, +Y is Down on the screen, +Z is INTO the screen
        Vector3 convertGL2CV() const noexcept { return { x, -y, z }; }
        Vector3 convertCV2GL() const noexcept { return { x, -y, z }; }

        // 3ds Max to OpenCV coordinate conversion
        // 3ds Max coordsys: +X is Right on the screen, +Y is INTO the screen,    +Z is Up
        // OpenCV  coordsys: +X is Right on the screen, +Y is Down on the screen, +Z is INTO the screen
        Vector3 convertMax2CV() const noexcept { return { x, -z, y }; }
        Vector3 convertCV2Max() const noexcept { return { x, z, -y }; }

        // OpenGL to 3ds Max coordinate conversion
        // 3ds Max coordsys: +X is Right on the screen, +Y is INTO the screen,  +Z is Up
        // OpenGL coordsys:  +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
        Vector3 convertMax2GL() const noexcept { return { x, z, y }; }
        Vector3 convertGL2Max() const noexcept { return { x, z, y }; }

        // OpenGL to iPhone coordinate conversion
        // OpenGL coordsys:  +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
        // iPhone coordsys:  +X is Right on the screen, +Y is Up on the screen, +Z is OUT of the screen
        Vector3 convertGL2IOS() const noexcept { return { x, y, -z }; }
        Vector3 convertIOS2GL() const noexcept { return { x, y, -z }; }

        // Blender to OpenGL coordinate conversion
        // Blender coordsys: +X is Right on the screen, +Y is INTO the screen,  +Z is Up on the screen
        // OpenGL  coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
        Vector3 convertBlender2GL() const noexcept  { return { x, z, y }; }
        Vector3 convertGL2Blender() const noexcept  { return { x, z, y }; }
        
        // Blender to iPhone coordinate conversion
        // Blender coordsys: +X is Right on the screen, +Y is INTO the screen,  +Z is Up on the screen
        // iPhone  coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is OUT of the screen
        Vector3 convertBlender2IOS() const noexcept { return { x,  z, -y }; }
        Vector3 convertIOS2Blender() const noexcept { return { x, -z,  y }; }

        
        // DirectX to OpenGL coordinate conversion
        // DirectX default coordsys: +X is Right on the screen, +Y is Up, +Z is INTO the screen
        // -- D3D is identical to modern OpenGL coordsys --
        Vector3 convertDX2GL() const noexcept { return *this; }
        Vector3 convertGL2DX() const noexcept { return *this; }

        // Unreal Engine 4 to openGL coordinate conversion
        // UE4 coordsys: +X is INTO the screen, +Y is Right on the screen, +Z is Up
        Vector3 convertUE2GL() const noexcept { return { y, z, x }; }
        Vector3 convertGL2UE() const noexcept { return { z, x, y }; }

    };

    inline Vector3 operator+(const Vector3& a, float f) { return { a.x+f, a.y+f, a.z+f }; }
    inline Vector3 operator-(const Vector3& a, float f) { return { a.x-f, a.y-f, a.z-f }; }
    inline Vector3 operator*(const Vector3& a, float f) { return { a.x*f, a.y*f, a.z*f }; }
    inline Vector3 operator/(const Vector3& a, float f) { return { a.x/f, a.y/f, a.z/f }; }
    inline Vector3 operator+(float f, const Vector3& a) { return { f+a.x, f+a.y, f+a.z }; }
    inline Vector3 operator-(float f, const Vector3& a) { return { f-a.x, f-a.y, f-a.z }; }
    inline Vector3 operator*(float f, const Vector3& a) { return { f*a.x, f*a.y, f*a.z }; }
    inline Vector3 operator/(float f, const Vector3& a) { return { f/a.x, f/a.y, f/a.z }; }

    inline constexpr Vector3 clamp(const Vector3& value, const Vector3& min, const Vector3& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z) };
    }

    inline constexpr Vector3 lerp(float position, const Vector3& start, const Vector3& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position };
    }
    
    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI double3
    {
        double x, y, z;
    };

    struct RPPAPI Vector3d
    {
        union {
            struct { double x, y, z; };
            struct { double3 xyz; };
        };
        static constexpr double3 ZERO = { 0.0, 0.0, 0.0 };
        
        Vector3d() {}
        constexpr Vector3d(double x, double y, double z) : x(x),   y(y),   z(z)   {}
        constexpr Vector3d(const double3& v) : x(v.x), y(v.y), z(v.z) {}
        
        explicit operator Vector3() const { return {float(x), float(y), float(z)}; }
        
        /** @brief Set new XYZ values */
        void set(double x, double y, double z);
    
        /** @return Length of the vector */
        double length() const;
    
        /** @return Squared length of the vector */
        double sqlength() const;

        /** @return Absolute distance from this vec3 to target vec3 */
        double distanceTo(const Vector3d& v) const;
    
        /** @brief Normalize this vector */
        void normalize();
        void normalize(const double magnitude);
    
        /** @return A normalized copy of this vector */
        Vector3d normalized() const;
        Vector3d normalized(const double magnitude) const;
    
        /** @return Cross product with another vector */
        Vector3d cross(const Vector3d& b) const;
    
        /** @return Dot product with another vector */
        double dot(const Vector3d& b) const;
        
        void print() const;
        const char* toString() const;
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized. 
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0 && y == 0.0 && z == 0.0; }
        bool notZero() const { return x != 0.0 || y != 0.0 || z != 0.0; }
        bool hasNaN() const { return isnan(x) || isnan(y) || isnan(z); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector3d& b) const;

        Vector3d& operator+=(double f) { x+=f; y+=f; z+=f; return *this; }
        Vector3d& operator-=(double f) { x-=f; y-=f; z-=f; return *this; }
        Vector3d& operator*=(double f) { x*=f; y*=f; z*=f; return *this; }
        Vector3d& operator/=(double f) { x/=f; y/=f; z/=f; return *this; }
        Vector3d& operator+=(const Vector3d& b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
        Vector3d& operator-=(const Vector3d& b) { x-=b.x; y-=b.y; z-=b.z; return *this; }
        Vector3d& operator*=(const Vector3d& b) { x*=b.x; y*=b.y; z*=b.z; return *this; }
        Vector3d& operator/=(const Vector3d& b) { x/=b.x; y/=b.y; z/=b.z; return *this; }
        Vector3d  operator+ (const Vector3d& b) const { return { x+b.x, y+b.y, z+b.z }; }
        Vector3d  operator- (const Vector3d& b) const { return { x-b.x, y-b.y, z-b.z }; }
        Vector3d  operator* (const Vector3d& b) const { return { x*b.x, y*b.y, z*b.z }; }
        Vector3d  operator/ (const Vector3d& b) const { return { x/b.x, y/b.y, z/b.z }; }
        Vector3d  operator- () const { return {-x, -y, -z}; }
        
        bool operator==(const Vector3d& v) const { return x == v.x && y == v.y && z == v.z; }
        bool operator!=(const Vector3d& v) const { return x != v.x || y != v.y || z != v.z; }
        
        /**
         * Some common 3D vector conversions
         * Some conversions are two way <->, but we still provide duplicate overloads for consistency
         *
         * This Vector library is based on OpenGL coordsys -- important when dealing with Matrix/Vector4(quat).
         * 
         * OpenGL coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
         *
         */

        // OpenGL to OpenCV coordinate conversion. Works both ways: GL <-> CV
        // OpenCV coordsys: +X is Right on the screen, +Y is Down on the screen, +Z is INTO the screen
        Vector3d convertGL2CV() const noexcept { return { x, -y, z }; }
        Vector3d convertCV2GL() const noexcept { return { x, -y, z }; }

    };
    
    inline Vector3d operator+(const Vector3d& a, double f) { return { a.x+f, a.y+f, a.z+f }; }
    inline Vector3d operator-(const Vector3d& a, double f) { return { a.x-f, a.y-f, a.z-f }; }
    inline Vector3d operator*(const Vector3d& a, double f) { return { a.x*f, a.y*f, a.z*f }; }
    inline Vector3d operator/(const Vector3d& a, double f) { return { a.x/f, a.y/f, a.z/f }; }
    inline Vector3d operator+(double f, const Vector3d& a) { return { f+a.x, f+a.y, f+a.z }; }
    inline Vector3d operator-(double f, const Vector3d& a) { return { f-a.x, f-a.y, f-a.z }; }
    inline Vector3d operator*(double f, const Vector3d& a) { return { f*a.x, f*a.y, f*a.z }; }
    inline Vector3d operator/(double f, const Vector3d& a) { return { f/a.x, f/a.y, f/a.z }; }

    inline constexpr Vector3d clamp(const Vector3d& value, const Vector3d& min, const Vector3d& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z) };
    }

    inline constexpr Vector3d lerp(double position, const Vector3d& start, const Vector3d& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position };
    }
    
    inline Vector3::operator Vector3d() const { return { double(x), double(y), double(z) }; }

    ////////////////////////////////////////////////////////////////////////////////

    /** @brief 4D Vector for matrix calculations and quaternion rotations */
    struct RPPAPI Vector4
    {
        union {
            struct { float x, y, z, w; };
            struct { float r, g, b, a; };
            struct { Vector2 xy; Vector2 zw; };
            struct { Vector2 rg; Vector2 ba; };
            struct { Vector3 xyz; float _w; };
            struct { float3  rgb; float _a; };
            struct { float _x; Vector3 yzw; };
            struct { float _r; Vector3 gba; };
            float4 xyzw;
        };

        static constexpr float4 ZERO           = { 0.0f, 0.0f, 0.0f, 0.0f };   // XYZW 0 0 0 0
        static constexpr float4 ONE            = { 1.0f, 1.0f, 1.0f, 1.0f };   // XYZW 1 1 1 1
        
        static constexpr float4 WHITE          = { 1.0f, 1.0f, 1.0f, 1.0f };   // RGBA 1 1 1 1
        static constexpr float4 BLACK          = { 0.0f, 0.0f, 0.0f, 1.0f };   // RGBA 0 0 0 1
        static constexpr float4 RED            = { 1.0f, 0.0f, 0.0f, 1.0f };   // RGBA 1 0 0 1
        static constexpr float4 GREEN          = { 0.0f, 1.0f, 0.0f, 1.0f };   // RGBA 0 1 0 1
        static constexpr float4 BLUE           = { 0.0f, 0.0f, 1.0f, 1.0f };   // RGBA 0 0 1 1
        static constexpr float4 YELLOW         = { 1.0f, 1.0f, 0.0f, 1.0f };       // 1 1 0 1
        static constexpr float4 ORANGE         = { 1.0f, 0.50196f, 0.0f, 1.0f };   // 1 0.502 0 1; 255 128 0 255
        static constexpr float4 MAGENTA        = { 1.0f, 0.0f, 1.0f, 1.0f };       // 1 0 1 1
        static constexpr float4 CYAN           = { 0.0f, 1.0f, 1.0f, 1.0f };       // 0 1 1 1
        static constexpr float4 SWEETGREEN     = { 0.337f, 0.737f, 0.223f, 1.0f }; // 86, 188, 57
        static constexpr float4 CORNFLOWERBLUE = { 0.33f, 0.66f, 1.0f, 1.0f };     // #55AAFF  85, 170, 255
    
        Vector4() {}
        constexpr Vector4(float x, float y, float z,float w=1.f): x(x), y(y), z(z), w(w) {}
        constexpr Vector4(const float4& v)            : xyzw(v) {}
        Vector4(const Vector2& xy, const Vector2& zw) : xy(xy), zw(zw) {}
        Vector4(const Vector2& xy, float z, float w)  : x(xy.x), y(xy.y), z(z),    w(w)    {}
        Vector4(float x, float y, const Vector2& zw)  : x(x),    y(y),    z(zw.x), w(zw.y) {}
        Vector4(const Vector3& xyz, float w)          : xyz(xyz), _w(w) {}
        Vector4(float x, const Vector3& yzw)          : _x(x), yzw(yzw) {}
        

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized.
        * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f; }
        bool notZero() const { return x != 0.0f || y != 0.0f || z != 0.0f || w != 0.0f; }
        bool hasNaN() const { return isnan(x) || isnan(y) || isnan(z) || isnan(w); }
        
        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector4& b) const;

        void set(float x, float y, float z, float w);
    
        /** @return Dot product with another vector */
        float dot(const Vector4& b) const;

        /** Print the matrix */
        void print() const;

        /** @return Temporary static string from this Matrix4 */
        const char* toString() const;

        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char (&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }

        /** @brief Assuming this is a quaternion, gives the Euler XYZ angles (degrees) */
        Vector3 quatToEulerAngles() const;
        Vector3 quatToEulerRadians() const;

        /** @brief Creates a quaternion rotation from an Euler angle (degrees), rotation axis must be specified */
        static Vector4 fromAngleAxis(float degrees, const Vector3& axis)
        { return fromAngleAxis(degrees, axis.x, axis.y, axis.z); }
        static Vector4 fromRadianAxis(float radians, const Vector3& axis)
        { return fromRadianAxis(radians, axis.x, axis.y, axis.z); }


        /** @brief Creates a quaternion rotation from an Euler angle (degrees), rotation axis must be specified */
        static Vector4 fromAngleAxis(float angle, float x, float y, float z);
        static Vector4 fromRadianAxis(float radians, float x, float y, float z);
    
        /** @brief Creates a quaternion rotation from Euler XYZ (degrees) rotation */
        static Vector4 fromRotationAngles(const Vector3& rotationDegrees);
        static Vector4 fromRotationRadians(const Vector3& rotationRadians);

        /** @return A 3-component float color from integer RGBA color */
        static constexpr Vector3 RGB(int r, int g, int b)
        {
            return { r / 255.0f, g / 255.0f, b / 255.0f};
        }
        /** @return A 4-component float color from integer RGBA color */
        static constexpr Vector4 RGB(int r, int g, int b, int a)
        {
            return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        }
    
        /**
         * Parses a HEX color string, example: #rrggbb or #rrggbbaa
         * @warning The string must start with '#', otherwise WHITE is returned
         * @return Parsed color value
         */
        static Vector4 HEX(const strview& s) noexcept;

        /**
         * Parses a color by name. Supported colors:
         * white, black, red, green, blue, yellow, orange
         */
        static Vector4 NAME(const strview& s) noexcept;

        /**
         * Parses a color by number values:
         * -) RGBA integer values: 255 0 128 255
         * -) RGBA float values:   0.1 0.2 0.5 1
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0 and Alpha channel default is 255
         *       Example '#aa' would give 170 0 0 255  and '0.1' would give 25 0 0 255
         * @return Parsed color in normalized float value range [0.0 ... 1.0]
         */
        static Vector4 NUMBER(strview s) noexcept;

        /**
         * Parses any type of color string. Supported:
         * -) RGBA HEX color strings: #rrggbb or #rrggbbaa
         * -) Named color strings: 'orange'
         * -) RGBA integer values: 255 0 128 255
         * -) RGBA float values:   0.1 0.2 0.5 1
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0 and Alpha channel default is 255
         *       Example '#aa' would give 170 0 0 255  and '0.1' would give 25 0 0 255
         * @return Default color WHITE or parsed color in normalized float value range [0.0 ... 1.0]
         */
        static Vector4 parseColor(const strview& s) noexcept;

        /** 
         * @brief Rotates quaternion p with extra rotation q
         * q = additional rotation we wish to rotate with
         * p = original rotation
         */
        Vector4 rotate(const Vector4& q) const;
    
        Vector4& operator*=(const Vector4& q) { return (*this = rotate(q)); }
        Vector4  operator* (const Vector4& q) const { return rotate(q); }
    
        Vector4& operator+=(float f) { x+=f; y+=f; z+=f; w+=f; return *this; }
        Vector4& operator-=(float f) { x-=f; y-=f; z-=f; w-=f; return *this; }
        Vector4& operator*=(float f) { x*=f; y*=f; z*=f; w*=f; return *this; }
        Vector4& operator/=(float f) { x/=f; y/=f; z/=f; w/=f; return *this; }
        Vector4& operator+=(const Vector4& v) { x+=v.x; y+=v.y; z+=v.z; w+=v.w; return *this; }
        Vector4& operator-=(const Vector4& v) { x-=v.x; y-=v.y; z-=v.z; w-=v.w; return *this; }

        Vector4  operator+ (const Vector4& v) const { return { x+v.x, y+v.y, z+v.z, w+v.w }; }
        Vector4  operator- (const Vector4& v) const { return { x-v.x, y-v.y, z-v.z, w-v.w }; }
        Vector4  operator- () const { return {-x, -y, -z, -w }; }
    
        bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
        bool operator!=(const Vector4& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }
    };

    inline Vector4 operator+(const Vector4& a, float f) { return { a.x+f, a.y+f, a.z+f, a.w+f }; }
    inline Vector4 operator-(const Vector4& a, float f) { return { a.x-f, a.y-f, a.z-f, a.w-f }; }
    inline Vector4 operator*(const Vector4& a, float f) { return { a.x*f, a.y*f, a.z*f, a.w*f }; }
    inline Vector4 operator/(const Vector4& a, float f) { return { a.x/f, a.y/f, a.z/f, a.w/f }; }
    inline Vector4 operator+(float f, const Vector4& a) { return { f+a.x, f+a.y, f+a.z, f+a.w }; }
    inline Vector4 operator-(float f, const Vector4& a) { return { f-a.x, f-a.y, f-a.z, f-a.w }; }
    inline Vector4 operator*(float f, const Vector4& a) { return { f*a.x, f*a.y, f*a.z, f*a.w }; }
    inline Vector4 operator/(float f, const Vector4& a) { return { f/a.x, f/a.y, f/a.z, f/a.w }; }

    inline constexpr Vector4 clamp(const Vector4& value, const Vector4& min, const Vector4& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z),
                 value.w < min.w ? min.w : (value.w < max.w ? value.w : max.w) };
    }

    inline constexpr Vector4 lerp(float position, const Vector4& start, const Vector4& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position,
                 start.w + (end.w - start.w)*position };
    }

    ////////////////////////////////////////////////////////////////////////////////


    struct RPPAPI _Matrix3RowVis
    {
        float x, y, z;
    };


    /**
     * 3x3 Rotation Matrix for OpenGL in row-major order, which is best suitable for MODERN OPENGL development
     */
    struct RPPAPI Matrix3
    {
        union {
            struct {
                float m00, m01, m02; // row0  0-2
                float m10, m11, m12; // row1  0-2
                float m20, m21, m22; // row2  0-2
            };
            struct {
                Vector3 r0, r1, r2; // rows 0-3
            };
            struct {
                _Matrix3RowVis vis0, vis1, vis2;
            };
            float m[12]; // 3x3 float matrix

            Vector3 r[3]; // rows 0-2
        };

        inline Matrix3() {}

        inline constexpr Matrix3(
            float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22) :
            m00(m00), m01(m01), m02(m02),
            m10(m10), m11(m11), m12(m12),
            m20(m20), m21(m21), m22(m22) { }

        inline constexpr Matrix3(Vector3 r0, Vector3 r1, Vector3 r2)
            : r0(r0), r1(r1), r2(r2) { }

        /** @brief Global identity matrix for easy initialization */
        static const Matrix3& Identity();

        /** @brief Loads identity matrix */
        Matrix3& loadIdentity();
    
        /** @brief Multiplies this matrix: this = this * mb */
        Matrix3& multiply(const Matrix3& mb);

        /** @brief Multiplies this matrix ma with matrix mb and returns the result as mc */
        Matrix3 operator*(const Matrix3& mb) const;

        /** @brief Transforms 3D vector v with this matrix and return the resulting vec3 */
        Vector3 operator*(const Vector3& v) const;

        // Transposes THIS Matrix4
        Matrix3& transpose();

        // Returns a transposed copy of this matrix
        Matrix3 transposed() const;

        float norm() const;
        float norm(const Matrix3& b) const;

        /** @return TRUE if this matrix looks like a rotation matrix */
        bool isRotationMatrix() const;

        /** Assuming this is a rotation matrix, returns Euler XYZ angles in DEGREES */
        Vector3 toEulerAngles() const;
        Vector3 toEulerRadians() const;

        /**
         * Initializes this Matrix3 with euler angles
         * @param eulerAngles Euler XYZ rotation in degrees
         */
        Matrix3& fromRotationAngles(const Vector3& eulerAngles)
        {
            return fromRotationRadians({radf(eulerAngles.x), radf(eulerAngles.y), radf(eulerAngles.z)});
        }
        Matrix3& fromRotationRadians(const Vector3& eulerRadians);

        static Matrix3 createRotationFromAngles(const Vector3& eulerAngles)
        {
            return createRotationFromRadians({radf(eulerAngles.x), radf(eulerAngles.y), radf(eulerAngles.z)});
        }
        static Matrix3 createRotationFromRadians(const Vector3& eulerRadians);

        /** Print the matrix */
        void print() const;
    
        /** @return Temporary static string from this */
        const char* toString() const;
    
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char (&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }
    };

    
    struct RPPAPI _Matrix4RowVis
    {
        float x, y, z, w;
    };


    /**
     * 4x4 Affine Matrix for OpenGL in row-major order, which is best suitable for MODERN OPENGL development
     */
    struct RPPAPI Matrix4
    {
        union {
            struct {
                float m00, m01, m02, m03; // row0  0-3
                float m10, m11, m12, m13; // row1  0-3
                float m20, m21, m22, m23; // row2  0-3
                float m30, m31, m32, m33; // row3  0-3
            };
            struct {
                Vector4 r0, r1, r2, r3; // rows 0-3
            };
            struct {
                _Matrix4RowVis vis0, vis1, vis2, vis3;
            };
            float m[16]; // 4x4 float matrix

            Vector4 r[4]; // rows 0-3
        };
    
        /** @brief Global identity matrix for easy initialization */
        static const Matrix4& Identity();

        inline Matrix4() {}

        inline constexpr Matrix4(
            float m00, float m01, float m02, float m03,
            float m10, float m11, float m12, float m13,
            float m20, float m21, float m22, float m23,
            float m30, float m31, float m32, float m33) :
            m00(m00), m01(m01), m02(m02), m03(m03),
            m10(m10), m11(m11), m12(m12), m13(m13),
            m20(m20), m21(m21), m22(m22), m23(m23),
            m30(m30), m31(m31), m32(m32), m33(m33) { }

        inline constexpr Matrix4(Vector4 r0, Vector4 r1, Vector4 r2, Vector4 r3)
            : r0(r0), r1(r1), r2(r2), r3(r3) { }

        /** @brief Loads identity matrix */
        Matrix4& loadIdentity();
    
        /** @brief Multiplies this matrix: this = this * mb */
        Matrix4& multiply(const Matrix4& mb);

        /** @brief Multiplies this matrix ma with matrix mb and returns the result as mc */
        Matrix4 operator*(const Matrix4& mb) const;

        /** @brief Transforms 3D vector v with this matrix and return the resulting vec3 */
        Vector3 operator*(const Vector3& v) const;
    
        /** @brief Transforms 4D vector v with this matrix and returns the resulting vec4 */
        Vector4 operator*(const Vector4& v) const;
    
        /** @brief Translates object transformation matrix by given offset */
        Matrix4& translate(const Vector3& offset);
    
        /** @brief Rotates an object transformation matrix by given degree angle around a given axis */
        Matrix4& rotate(float angleDegs, const Vector3& axis);

        /** @brief Rotates an object transformation matrix by given degree angle around a given axis */
        Matrix4& rotate(float angleDegs, float x, float y, float z);
    
        /** @brief Scales an object transformation matrix by a given scale factor */
        Matrix4& scale(const Vector3& scale);
    
        /** @brief Loads an Ortographic projection matrix */
        Matrix4& setOrtho(float left, float right, float bottom, float top);
        static inline Matrix4 createOrtho(float left, float right, float bottom, float top)
        {
            Matrix4 view = {};
            view.setOrtho(left, right, bottom, top);
            return view;
        }
        // create a classical GUI friendly ortho: 0,0 is topleft
        // left [0, width]  right
        // top  [0, height] bottom
        static inline Matrix4 createOrtho(int width, int height)
        {
            return createOrtho(0.0f, float(width), float(height), 0.0f);
        }
    
        /** @brief Loads a perspective projection matrix */
        Matrix4& setPerspective(float fov, float width, float height, float zNear, float zFar);
        static inline Matrix4 createPerspective(float fov, float width, float height, float zNear, float zFar)
        {
            Matrix4 view = {};
            view.setPerspective(fov, width, height, zNear, zFar);
            return view;
        }
        static inline Matrix4 createPerspective(float fov, int width, int height, float zNear, float zFar)
        {
            Matrix4 view = {};
            view.setPerspective(fov, float(width), float(height), zNear, zFar);
            return view;
        }

        /** @brief Loads a lookAt view/camera matrix */
        Matrix4& setLookAt(const Vector3& eye, const Vector3& center, const Vector3& up);
        static inline Matrix4 createLookAt(const Vector3& eye, const Vector3& center, const Vector3& up)
        {
            Matrix4 view = {};
            view.setLookAt(eye, center, up);
            return view;
        }
    
        /** @brief Creates a translated matrix from XYZ position */
        Matrix4& fromPosition(const Vector3& position);
        static inline Matrix4 createTranslation(const Vector3& position)
        {
            Matrix4 mat = Identity();
            mat.translate(position);
            return mat;
        }
    
        /** @brief Creates a rotated matrix from euler XYZ rotation */
        Matrix4& fromRotation(const Vector3& rotationDegrees);
        static inline Matrix4 createRotation(const Vector3& rotationDegrees)
        {
            Matrix4 mat = {};
            mat.fromRotation(rotationDegrees);
            return mat;
        }
    
        /** @brief Creates a scaled matrix from XYZ scale */
        Matrix4& fromScale(const Vector3& scale);
        static inline Matrix4 createScale(const Vector3& scale)
        {
            Matrix4 mat = {};
            mat.fromScale(scale);
            return mat;
        }

        /** @return Extracts position data from this affine matrix */
        Vector3 getPositionColumn() const;

        float getPosX() const { return m30; }
        float getPosY() const { return m31; }
        float getPosZ() const { return m32; }
        void setPosX(float x) { m30 = x; }
        void setPosY(float y) { m31 = y; }
        void setPosZ(float z) { m32 = z; }

        /** 
         * @brief Creates an affine matrix from 2D pos, zOrder, rotDegrees and 2D scale
         * @param pos 2D position on screen
         * @param zOrder Layer z-depth on screen; Higher values are on top.
         * @param rotDegrees Simple rotation in degrees. Rotates the object around its Z-Axis
         * @param scale Relative (0.0-1.0) 2D scale
         */
        Matrix4& setAffine2D(const Vector2& pos, float zOrder, float rotDegrees, const Vector2& scale);
    
        /** 
         * @brief Creates an affine matrix from 2D pos, zOrder, rotDegrees and 2D scale
         * @param pos 2D position on screen
         * @param zOrder Layer z-depth on screen; Higher values are on top.
         * @param rotDegrees Simple rotation in degrees. Rotates the object around its Z-Axis
         * @param rotAxis Point around which to rotate. By default this is {0,0}
         * @param scale Relative (0.0-1.0) 2D scale
         */
        Matrix4& setAffine2D(const Vector2& pos, float zOrder, float rotDegrees, 
                             const Vector2& rotAxis, const Vector2& scale);

        /**
         * @brief Creates an affine 3D transformation matrix. Rotation is in Euler XYZ degrees.
         */
        Matrix4& setAffine3D(const Vector3& pos, const Vector3& scale, const Vector3& rotationDegrees);
        static inline Matrix4 createAffine3D(const Vector3& pos, const Vector3& scale, const Vector3& rotationDegrees)
        {
            Matrix4 affine = Matrix4::Identity();
            affine.setAffine3D(pos, scale, rotationDegrees);
            return affine;
        }

        // Transposes THIS Matrix4
        Matrix4& transpose();

        // Returns a transposed copy of this matrix
        Matrix4 transposed() const;

        // Matrix x InverseMatrix = Identity, useful for unprojecting
        Matrix4 inverse() const;

        /** Print the matrix */
        void print() const;
    
        /** @return Temporary static string from this Matrix4 */
        const char* toString() const;
    
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char (&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    /**
     * Viewport utility for creating a projection matrix and managing projection from
     * 2D to 3D space and vice-versa
     */
    struct RPPAPI PerspectiveViewport
    {
        float fov, width, height, zNear, zFar;
        Matrix4 projection;

        /**
         * Creates a new perspective viewport with a specific field of view.
         * The screen width and height parameters are used for setting the aspect ratio
         * and later for projections between screen space and world space
         */
        PerspectiveViewport(float fov, float width, float height, float zNear = 0.001f, float zFar = 10000.0f)
            : fov(fov), width(width), height(height), zNear(zNear), zFar(zFar)
        {
            projection.setPerspective(fov, width, height, zNear, zFar);
        }

        /**
         * Project from world space to screen space. Screen space is defined by PerspectiveViewport params
         * @param worldPos 3D position in the world
         * @param cameraView View matrix such as Matrix4::createLookAt
         * @return Position on screen. The resulting point can also be outside of the viewport,
         *         which naturally means worldPos is not visible on screen
         */
        Vector2 ProjectToScreen(Vector3 worldPos, const Matrix4& cameraView) const
        {
            Matrix4 viewProjection = cameraView; viewProjection.multiply(projection);
            return ViewProjectToScreen(worldPos, viewProjection);
            
        }
        // Same as ProjectToScreen, but using a premultiplied View-Projection matrix
        Vector2 ViewProjectToScreen(Vector3 worldPos, const Matrix4& viewProjection) const;

        /**
         * Project from screen space to world space. Screen space is defined by PerspectiveViewport params
         * @param screenPos 2D position on the screen
         * @param depth The Z depth from camera position, usually either 0.0 or 1.0
         * @param cameraView View matrix such as Matrix4::createLookAt
         * @return Position in the world. The resulting point depends greatly on depth value
         */
        Vector3 ProjectToWorld(Vector2 screenPos, float depth, const Matrix4& cameraView) const
        {
            Matrix4 viewProjection = cameraView; viewProjection.multiply(projection);
            return InverseViewProjectToWorld(screenPos, depth, viewProjection.inverse());
        }

        // Same as ProjectToWorld, but using a premultiplied View-Projection matrix
        Vector3 ViewProjectToWorld(Vector2 screenPos, float depth, const Matrix4& viewProjection) const
        {
            return InverseViewProjectToWorld(screenPos, depth, viewProjection.inverse());
        }

        // Same as ProjectToWorld, but using an inverse of a premultiplied View-Projection matrix
        // This is the fastest ProjectToWorld
        Vector3 InverseViewProjectToWorld(Vector2 screenPos, float depth, const Matrix4& inverseViewProjection) const;
    };

    ////////////////////////////////////////////////////////////////////////////////

    /** @brief Simple 4-component RGBA float color */
    using Color = Vector4;

    /** @brief Simple 3-component RGB float color */
    using Color3 = Vector3;

    ////////////////////////////////////////////////////////////////////////////////

    // Vector3 with an associated Vertex ID
    struct IdVector3 : Vector3
    {
        int ID; // vertex id, -1 means invalid Vertex ID, [0] based indices

        IdVector3(int id, const Vector3& v) : Vector3{ v }, ID{ id } {}
        IdVector3(int id, float x, float y, float z) : Vector3{ x,y,z }, ID{ id } {}
    };

    ////////////////////////////////////////////////////////////////////////////////
    
    /** 3D bounding box */
    struct RPPAPI BoundingBox
    {
        Vector3 min;
        Vector3 max;
    
        BoundingBox() : min(0.0f,0.0f,0.0f), max(0.0f,0.0f,0.0f) {}
        BoundingBox(const Vector3& bbMinMax) : min(bbMinMax), max(bbMinMax) {}
        BoundingBox(const Vector3& bbMin, const Vector3& bbMax) : min(bbMin), max(bbMax) {}

        explicit operator bool()  const { return min.notZero() && max.notZero(); }
        bool operator!() const { return min.isZero()  || max.isZero();  }

        bool isZero()  const { return min.isZero()  && max.isZero();  }
        bool notZero() const { return min.notZero() || max.notZero(); }

        float width()  const noexcept { return max.x - min.x; } // DX
        float height() const noexcept { return max.y - min.y; } // DY
        float depth()  const noexcept { return max.z - min.z; } // DZ

        // width*height*depth
        float volume() const noexcept;

        // get center of BoundingBox
        Vector3 center() const noexcept;

        // get the bounding radius: (max-min).length() / 2
        float radius() const noexcept;

        Vector3 compare(const BoundingBox& bb) const noexcept;

        // joins a vector into this bounding box, possibly increasing the volume
        void join(const Vector3& v) noexcept;

        // joins with another bounding box, possibly increasing the volume
        void join(const BoundingBox& bbox) noexcept;

        // @return TRUE if vec3 point is inside this bounding box volume
        bool contains(const Vector3& v) const noexcept;

        // @return Distance to vec3 point from this boundingbox's corners
        float distanceTo(const Vector3& v) const noexcept;

        // grow the bounding box by the given value across all axes
        void grow(float growth) noexcept;

        // calculates the bounding box of the given point cloud
        static BoundingBox create(const std::vector<Vector3>& points) noexcept;

        // calculates the bounding box using ID-s from IdVector3-s, which index the given point cloud
        static BoundingBox create(const std::vector<Vector3>& points, const std::vector<IdVector3>& ids) noexcept;

        // calculates the bounding box using vertex ID-s, which index the given point cloud
        static BoundingBox create(const std::vector<Vector3>& points, const std::vector<int>& ids) noexcept;

        /**
         * Calculates the bounding box from an arbitrary vertex array
         * @note Position data must be the first Vector3 element!
         * @param vertexData Array of vertices
         * @param vertexCount Number of vertices in vertexData
         * @param stride The step in bytes(!!) to the next element
         * 
         * Example:
         * @code
         *     vector<Vertex3UVNorm> vertices;
         *     auto bb = BoundingBox::create((Vector3*)vertices.data(), vertices.size(), sizeof(Vertex3UVNorm));
         * @endcode
         */
        static BoundingBox create(const Vector3* vertexData, int vertexCount, int stride) noexcept;

        /**
         * Calculates the bounding box from an arbitrary vertex array
         * @note Position data must be the first Vector3 element!
         * 
         * Example:
         * @code
         *     vector<Vertex3UVNorm> vertices;
         *     auto bb = BoundingBox::create(vertices);
         * @endcode
         */
        template<class Vertex> static BoundingBox create(const std::vector<Vertex>& vertices)
        {
            return create((Vector3*)vertices.data(), (int)vertices.size(), sizeof(Vertex));
        }
        template<class Vertex> static BoundingBox create(const Vertex* vertices, int vertexCount)
        {
            return create((Vector3*)vertices, vertexCount, sizeof(Vertex));
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI BoundingSphere
    {
        Vector3 center;
        float radius;
        BoundingSphere() : center(0.0f, 0.0f, 0.0f), radius(0.0f) {}
        BoundingSphere(const Vector3& center, float radius) : center(center), radius(radius) {}
        BoundingSphere(const BoundingBox& bbox) : center(bbox.center()), radius(bbox.radius()) {}

        // calculates the bounding sphere from basic bounding box of the point cloud
        static BoundingSphere create(const std::vector<Vector3>& points) noexcept
        {
            return { BoundingBox::create(points) };
        }
        template<class Vertex> static BoundingSphere create(const std::vector<Vertex>& vertices)
        {
            return { BoundingBox::create(vertices) };
        }
        template<class Vertex> static BoundingSphere create(const Vertex* vertices, int vertexCount)
        {
            return { BoundingBox::create(vertices, vertexCount) };
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI Ray
    {
        Vector3 origin;
        Vector3 direction;

        // Ray-Sphere intersection 
        // @return Distance from rayStart to intersection OR 0.0f if no solutions
        float intersectSphere(Vector3 sphereCenter, float sphereRadius) const noexcept;

        // Ray-Triangle intersection
        // @return Distance from rayStart to intersection OR 0.0f if no solutions 
        float intersectTriangle(Vector3 v0, Vector3 v1, Vector3 v2) const noexcept;
    };

    ////////////////////////////////////////////////////////////////////////////////

    inline string to_string(const Vector2& v)  { char buf[32];  return { v.toString(buf, sizeof(buf)) }; }
    inline string to_string(const Point& v)    { char buf[48];  return { v.toString(buf, sizeof(buf)) }; }
    inline string to_string(const Vector3& v)  { char buf[48];  return { v.toString(buf, sizeof(buf)) }; }
    inline string to_string(const Vector3d& v) { char buf[48];  return { v.toString(buf, sizeof(buf)) }; }
    inline string to_string(const Vector4& v)  { char buf[64];  return { v.toString(buf, sizeof(buf)) }; }
    inline string to_string(const Matrix4& v)  { char buf[256]; return { v.toString(buf, sizeof(buf)) }; }

    ////////////////////////////////////////////////////////////////////////////////
} // namespace rpp

#if _MSC_VER
#pragma warning(pop) // nameless struct/union warning
#endif
