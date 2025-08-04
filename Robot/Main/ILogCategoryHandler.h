#pragma once
#include "ILogRecord.h"
#include <WString.h>
#include "ILogManager.h"

class ILogManager; // forward declaration only


class ILogCategoryHandler {
protected:
    ILogManager* parent;
public:
    ILogCategoryHandler(ILogManager* parent) : parent(parent){}
    virtual ILogRecord* createRecord(bool block = true) = 0;
    virtual bool commitRecord(ILogRecord* rec, bool block = true) = 0;
    virtual void requestFlush() = 0;
    virtual void flushToFile() = 0;
    virtual void uploadIfNeeded() = 0;
    virtual bool watchdogCheck() = 0;
    virtual ~ILogCategoryHandler() {}
};