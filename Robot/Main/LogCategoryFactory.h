#pragma once
#include "ICategoryFactory.h"
#include "LogCategoryHandler.h"

class LogCategoryFactory : public ICategoryFactory {
private:
    IStorage* storage;
    ISink* sink;
    ILogRecordFactory* recordFactory;

public:
    LogCategoryFactory(IStorage* s, ISink* sk, ILogRecordFactory* rf)
        : storage(s), sink(sk), recordFactory(rf) {}

    ILogCategoryHandler* create(const char* category, ILogManager* manager) override {
        return new LogCategoryHandler(manager, category, storage, sink, recordFactory);
    }
};
