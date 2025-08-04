#pragma once

#include <ArduinoJson.h>
#include "ILogRecord.h"
#include "ILogCategoryHandler.h"
#include "timeUtil.h"

class JsonLogRecord : public ILogRecord {
private:
    StaticJsonDocument<256> doc;  // Tune size as needed
    ILogCategoryHandler* owner;

public:
    JsonLogRecord(ILogCategoryHandler* dir)
        : owner(dir) {
        doc["time"] = timeHMSms();
    }

    ILogRecord& set(const char* key, float value) override {
        doc[key] = value;
        return *this;
    }

    ILogRecord& set(const char* key, const char* value) override {
        doc[key] = value;
        return *this;
    }

    // This commits the record using the category handler
    bool commit(bool block) override {
        return owner->commitRecord(this);
    }

    // Serializes the log into a user-provided String
    bool serialize(Print& p) override{
        bool ok = serializeJson(doc, p) > 0;
        p.println();  // Add the newline
        return ok;
    }
};


class JsonLogRecordFactory : public ILogRecordFactory {
public:
    ILogRecord* create(ILogCategoryHandler* owner) override {
        return new JsonLogRecord(owner);
    }
};
