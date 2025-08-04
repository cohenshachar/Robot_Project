#include <Arduino.h>
#pragma once

class MemPrint : public Stream {
    uint8_t* buffer;
    size_t capacity;
    size_t pos;
    size_t readPos = 0; // For reading

public:
    MemPrint(uint8_t* buf, size_t cap) : buffer(buf), capacity(cap), pos(0), readPos(0) {}

    // Write implementations
    size_t write(uint8_t c) override {
        if (pos < capacity) {
            buffer[pos++] = c;
            return 1;
        }
        return 0;
    }
    size_t write(const uint8_t* data, size_t size) override {
        size_t n = 0;
        for (; n < size && pos < capacity; ++n) {
            buffer[pos++] = data[n];
        }
        return n;
    }

    // Read implementations - for completeness
    int available() override {
        return pos - readPos;
    }
    int read() override {
        if (readPos >= pos) return -1;
        return buffer[readPos++];
    }
    int peek() override {
        if (readPos >= pos) return -1;
        return buffer[readPos];
    }
    void flush() override {}

    size_t size() const { return pos; }
};


class StringStream : public Stream {
    String& buffer;
    size_t pos = 0;
public:
    explicit StringStream(String& buf) : buffer(buf) {}
    int available() override { return buffer.length() - pos; }
    int read() override {
        if (pos >= buffer.length()) return -1;
        return buffer.charAt(pos++);
    }
    int peek() override {
        if (pos >= buffer.length()) return -1;
        return buffer.charAt(pos);
    }
    size_t write(uint8_t c) override {
        buffer += (char)c;
        return 1;
    }
    void flush() override {}
    size_t readBytes(char* buf, size_t len) override {
        size_t avail = available();
        size_t toRead = (len < avail) ? len : avail;
        for (size_t i = 0; i < toRead; i++) {
            buf[i] = buffer.charAt(pos++);
        }
        return toRead;
    }
};
