#pragma once
#include <Print.h> // Ensure Print is defined

class ILogRecord {
public:
    virtual ILogRecord& set(const char* key, float value) = 0;
    virtual ILogRecord& set(const char* key, const char* value) = 0;
    virtual bool commit(bool block = true) = 0;
    virtual bool serialize(Print& p) = 0;
    virtual ~ILogRecord() {}
};