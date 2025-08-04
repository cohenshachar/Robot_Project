#pragma once
#include "ILogRecord.h"
#include "ILogCategoryHandler.h"

class ILogRecordFactory {
public:
    virtual ILogRecord* create(ILogCategoryHandler* owner) = 0;
    virtual ~ILogRecordFactory() {}
};

