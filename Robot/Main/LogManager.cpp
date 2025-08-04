#include "LogManager.h"
#include "DebugConfig.h"  // For DPRINTLN
#include "DBConfig.h"
#include "LogCategoryHandler.h"

#define DPRINT(x)    DPRINT_DB(x)
#define DPRINTLN(x)  DPRINTLN_DB(x)
#define LOG_FUNC(x)  LOG_DB_MSG(x)

LogManager::LogManager(ICategoryFactory* cf)
    : categoryFactory(cf)
    , categoriesMutex(nullptr)
    , flushTaskHandle(nullptr)
    , uploadTaskHandle(nullptr)
{
    categoriesMutex = xSemaphoreCreateMutex();
    flushQueue = xQueueCreate(MAX_PENDING_FLUSHES, sizeof(ILogCategoryHandler*));
    uploadQueue = xQueueCreate(MAX_PENDING_FLUSHES, sizeof(ILogCategoryHandler*));
}


LogManager::~LogManager() {
    if (categoriesMutex) {
        xSemaphoreTake(categoriesMutex, portMAX_DELAY);
        for (auto& [_, cat] : categories) {
            delete cat;
        }
        categories.clear();
        xSemaphoreGive(categoriesMutex);
        vSemaphoreDelete(categoriesMutex);
    }
}

bool LogManager::begin() {
    DPRINTLN_INIT("LogManager begin...");
    // Create FreeRTOS tasks here (similar to your previous code)
    xTaskCreatePinnedToCore(flushTaskWrapper, "FlushTask", 4096, this, 2, &flushTaskHandle, 0);
    xTaskCreatePinnedToCore(uploadTaskWrapper, "UploadTask", 2*8192, this, 2, &uploadTaskHandle, 0);
    ready = true;

    return true;
}

void LogManager::flushTaskWrapper(void* pvParameters) {
    LogManager* db = static_cast<LogManager*>(pvParameters);
    const TickType_t delayTicks = pdMS_TO_TICKS(LOG_FLUSH_INTERVAL_MS);
    int64_t lastLoopUs = esp_timer_get_time();

    while (true) {
        int64_t nowUs = esp_timer_get_time();
        ILogCategoryHandler* handler = nullptr;
        if (xQueueReceive(db->flushQueue, &handler, portMAX_DELAY) == pdTRUE && handler) {
            int64_t loopIntervalMs = (nowUs - lastLoopUs) / 1000;
            lastLoopUs = nowUs;
            DPRINT_TIMING("[FlushTask] Loop interval: on Core " + String(xPortGetCoreID()) + ": ");
            DPRINT_TIMING(loopIntervalMs);
            DPRINTLN_TIMING(" ms");
            handler->flushToFile();
            bool upload_flag = handler->watchdogCheck();
            if (upload_flag && db->uploadQueue != nullptr) {
              if (xQueueSend(db->uploadQueue, &handler, portMAX_DELAY) == pdTRUE) {
                  DPRINTLN("[FlushTask] ✅ Upload request queued successfully.");
              } else {
                  DPRINTLN("[FlushTask] ❌ Upload queue full or failed to send request.");
              }
            }
        }
    }
}


void LogManager::uploadTaskWrapper(void* pvParameters) {
    LogManager* db = static_cast<LogManager*>(pvParameters);
    const TickType_t delayTicks = pdMS_TO_TICKS(LOG_UPLOAD_INTERVAL_MS);
    int64_t lastLoopUs = esp_timer_get_time();

    while (true) {
        int64_t nowUs = esp_timer_get_time();
        ILogCategoryHandler* handler = nullptr;
        if (xQueueReceive(db->uploadQueue, &handler, portMAX_DELAY) == pdTRUE && handler) {
        int64_t loopIntervalMs = (nowUs - lastLoopUs) / 1000;
        lastLoopUs = nowUs;
        DPRINT_TIMING("[UploadTask] Loop interval on Core " + String(xPortGetCoreID()) + ": ");
        DPRINT_TIMING(loopIntervalMs);
        DPRINTLN_TIMING(" ms");
        handler->uploadIfNeeded();
    }
  }
}


// void LogManager::registerCategory(const String& name, IStorage* s, ILogSink* sk, ILogRecordFactory* rf) {
//     if (xSemaphoreTake(categoriesMutex, portMAX_DELAY) != pdTRUE) return;
//     if (categories.count(name) == 0) {
//         categories[name] = new LogCategoryHandler(name, s, sk, rf);
//     }
//     xSemaphoreGive(categoriesMutex);
// }

ILogRecord* LogManager::createRecord(const char* category, bool block) {
    DPRINTLN("[LogManager] createRecord called");
    DPRINT("[LogManager] Trying to take categoriesMutex... ");

    if (xSemaphoreTake(categoriesMutex, block ? portMAX_DELAY : 0) != pdTRUE) {
        DPRINTLN("[LogManager] FAILED to acquire mutex");
        return nullptr;
    }
    DPRINTLN("Mutex acquired");

    auto it = categories.find(category);
    if (it == categories.end()) {
        DPRINT("[LogManager] Category not found, creating new handler for: ");
        DPRINTLN(category);

        // Create new handler with defaults
        ILogCategoryHandler* handler = categoryFactory->create(category, this);
        categories[category] = handler;

        DPRINTLN("[LogManager] New LogCategoryHandler created and stored");
        it = categories.find(category);
    } else {
        DPRINT("[LogManager] Found existing category handler for: ");
        DPRINTLN(category);
    }

    DPRINTLN("[LogManager] Creating record via category handler");
    ILogRecord* record = it->second->createRecord(block);
    if (!record) {
        DPRINTLN("[LogManager] WARNING: Failed to create log record");
    } else {
        DPRINTLN("[LogManager] Log record created successfully");
    }

    xSemaphoreGive(categoriesMutex);
    DPRINTLN("[LogManager] Released categoriesMutex");

    return record;
}

bool LogManager::requestFlush(ILogCategoryHandler* flushReq) {
    if (flushQueue != nullptr && flushReq != nullptr) {
        BaseType_t res = xQueueSend(flushQueue, &flushReq, 0);
        if (res == pdTRUE) {
            DPRINTLN("[LogManager] ✅ Flush request queued successfully.");
            return true;
        } else {
            DPRINTLN("[LogManager] ❌ Flush queue full or failed to send request.");
            return false;
        }
    }

    if (flushQueue == nullptr) {
        DPRINTLN("[LogManager] ❌ Cannot queue flush: flushQueue is null.");
    }

    if (flushReq == nullptr) {
        DPRINTLN("[LogManager] ❌ Cannot queue flush: flushReq is null.");
    }

    return false;
}


void LogManager::flushAll() {
    std::vector<ILogCategoryHandler*> snapshot;
    if (xSemaphoreTake(categoriesMutex, portMAX_DELAY) == pdTRUE) {
        for (auto& [_, cat] : categories) {
            snapshot.push_back(cat);
        }
        xSemaphoreGive(categoriesMutex);
    }
    for (auto* cat : snapshot) {
        cat->flushToFile();
    }
}

void LogManager::uploadAll() {
    std::vector<ILogCategoryHandler*> snapshot;
    if (xSemaphoreTake(categoriesMutex, portMAX_DELAY) == pdTRUE) {
        for (auto& [_, cat] : categories) {
            snapshot.push_back(cat);
        }
        xSemaphoreGive(categoriesMutex);
    }
    for (auto* cat : snapshot) {
        cat->uploadIfNeeded();
    }
}

void LogManager::tickWatchdog() {
    std::vector<ILogCategoryHandler*> snapshot;
    if (xSemaphoreTake(categoriesMutex, portMAX_DELAY) == pdTRUE) {
        for (auto& [_, cat] : categories) {
            snapshot.push_back(cat);
        }
        xSemaphoreGive(categoriesMutex);
    }
    for (auto* cat : snapshot) {
        cat->watchdogCheck();
    }
}

