#pragma once

#include "IStorage.h"
#include "ISink.h"
#include "ILogRecordFactory.h"
#include "ILogRecord.h"

#include <deque>
#include <algorithm>
#include <Arduino.h>

class LogCategoryHandler : public ILogCategoryHandler {
    const char* category;
    IStorage* storage;
    ISink* sink;
    ILogRecordFactory* recordFactory;

    std::deque<ILogRecord*> activeRecords;
    std::deque<ILogRecord*> commitedRecords;
    SemaphoreHandle_t bufferMutex;
    size_t totalRequests;
    size_t totalCommitedRequests;
    size_t totalFlushedRequests;
    int64_t lastCreatedTimeUs;  // Per-instance
    int64_t rateLimit;  // Per-instance
    bool requestedFlush;
    volatile bool isFlushing;
public:
    LogCategoryHandler(ILogManager* parent, const char* cat, IStorage* s, ISink* sk, ILogRecordFactory* rf);
    ~LogCategoryHandler();

    ILogRecord* createRecord(bool block = true) override;
    bool commitRecord(ILogRecord* rec, bool block = true) override;
    void requestFlush() override;
    void flushToFile() override;
    void uploadIfNeeded() override;
    bool watchdogCheck() override;
};


