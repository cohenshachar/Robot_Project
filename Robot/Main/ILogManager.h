#pragma once

#include <WString.h>
#include "ILogRecord.h"
#include "IStorage.h"
#include "ICompressor.h"
#include "ISink.h"
#include "ILogRecordFactory.h"

class ILogCategoryHandler;

class ILogManager {
protected:
  bool ready = false; 
public:
    virtual ~ILogManager() = default;

    virtual bool begin() = 0;

    //virtual void registerCategory(const String& name, IStorage* s, ILogSink* sk, ILogRecordFactory* rf) = 0;

    virtual ILogRecord* createRecord(const char* category, bool block = true) = 0;

    virtual void flushAll() = 0;

    virtual void uploadAll() = 0;

    virtual bool requestFlush(ILogCategoryHandler* flushReq) =0;

    virtual void tickWatchdog() = 0;
    
    virtual bool isReady() const{return ready;}
};