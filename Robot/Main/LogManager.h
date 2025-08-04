#pragma once

#include <map>
#include <WString.h>
#include "ILogManager.h"
#include "ICategoryFactory.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "ILogCategoryHandler.h"

class LogManager : public ILogManager {
public:
    LogManager(ICategoryFactory* cf);
    ~LogManager();

    bool begin() override;

    //void registerCategory(const String& name, IStorage* s, ILogSink* sk, ILogRecordFactory* rf) override;

    ILogRecord* createRecord(const char* category, bool block = true) override;
    bool requestFlush(ILogCategoryHandler* flushReq) override;

    void flushAll() override;
    void uploadAll() override;
    void tickWatchdog() override;

private:
    static void flushTaskWrapper(void* pvParameters);
    static void uploadTaskWrapper(void* pvParameters);

    ICategoryFactory* categoryFactory;
    QueueHandle_t flushQueue;
    QueueHandle_t uploadQueue;

    std::map<String, ILogCategoryHandler*> categories;
    SemaphoreHandle_t categoriesMutex;

    TaskHandle_t flushTaskHandle;
    TaskHandle_t uploadTaskHandle;
};
