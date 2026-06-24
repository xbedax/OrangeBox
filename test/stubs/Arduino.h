#ifndef TEST_STUBS_ARDUINO_H
#define TEST_STUBS_ARDUINO_H

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <ostream>
#include <string>

using u_int64_t = uint64_t;
using byte = uint8_t;

class String {
public:
    String() = default;
    String(const char* value) : data(value ? value : "") {}
    String(char* value) : data(value ? value : "") {}
    String(const std::string& value) : data(value) {}
    String(char value) : data(1, value) {}
    String(int value) : data(std::to_string(value)) {}
    String(unsigned int value) : data(std::to_string(value)) {}
    String(long value) : data(std::to_string(value)) {}
    String(unsigned long value) : data(std::to_string(value)) {}
    String(long long value) : data(std::to_string(value)) {}
    String(unsigned long long value) : data(std::to_string(value)) {}

    const char* c_str() const { return data.c_str(); }
    size_t length() const { return data.length(); }

    String substring(size_t from, size_t to) const
    {
        if (from > data.length()) {
            from = data.length();
        }
        if (to > data.length()) {
            to = data.length();
        }
        if (to < from) {
            to = from;
        }
        return String(data.substr(from, to - from));
    }

    String& operator+=(const String& other)
    {
        data += other.data;
        return *this;
    }

    operator std::string() const { return data; }

private:
    std::string data;

    friend String operator+(const String& lhs, const String& rhs);
    friend bool operator==(const String& lhs, const String& rhs);
    friend std::ostream& operator<<(std::ostream& os, const String& value);
};

inline String operator+(const String& lhs, const String& rhs)
{
    return String(lhs.data + rhs.data);
}

inline bool operator==(const String& lhs, const String& rhs)
{
    return lhs.data == rhs.data;
}

inline std::ostream& operator<<(std::ostream& os, const String& value)
{
    os << value.data;
    return os;
}

class SerialStub {
public:
    void begin(unsigned long) {}

    void print(const char* value) { std::fputs(value, stdout); }
    void print(const String& value) { std::fputs(value.c_str(), stdout); }
    void print(char value) { std::fputc(value, stdout); }
    void print(int value) { std::printf("%d", value); }
    void print(unsigned int value) { std::printf("%u", value); }
    void print(long value) { std::printf("%ld", value); }
    void print(unsigned long value) { std::printf("%lu", value); }

    void println() { std::fputc('\n', stdout); }
    void println(const char* value) { std::printf("%s\n", value); }
    void println(const String& value) { std::printf("%s\n", value.c_str()); }
    void println(int value) { std::printf("%d\n", value); }
    void println(unsigned int value) { std::printf("%u\n", value); }
    void println(long value) { std::printf("%ld\n", value); }
    void println(unsigned long value) { std::printf("%lu\n", value); }

    int printf(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        int written = std::vprintf(format, args);
        va_end(args);
        return written;
    }
};

static SerialStub Serial;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define CHANGE 2
#define ARDUINO_ISR_ATTR

inline unsigned long testStubMillis = 0;
inline uint8_t testStubPinState[256] = {};

inline unsigned long millis()
{
    return testStubMillis;
}

inline void delay(unsigned long ms)
{
    testStubMillis += ms;
}

inline void advanceMillis(unsigned long ms)
{
    testStubMillis += ms;
}

inline void pinMode(uint8_t, uint8_t) {}

inline void digitalWrite(uint8_t pin, uint8_t value)
{
    testStubPinState[pin] = value;
}

inline int digitalRead(uint8_t pin)
{
    return testStubPinState[pin];
}

inline void attachInterrupt(uint8_t, void (*)(), int) {}

using std::min;
using std::max;

#endif // TEST_STUBS_ARDUINO_H
