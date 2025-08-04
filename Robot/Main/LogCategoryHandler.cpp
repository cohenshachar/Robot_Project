#include "LogCategoryHandler.h"
#include "DebugConfig.h"  // For DPRINTLN
#include "DBConfig.h"
#define DPRINT(x)    DPRINT_CAT(x)
#define DPRINTLN(x)  DPRINTLN_CAT(x)
#define LOG_FUNC(x) LOG_DB_MSG(x)

LogCategoryHandler::LogCategoryHandler(ILogManager* parent, const char* cat, IStorage* s, ISink* sk, ILogRecordFactory* rf)
    : ILogCategoryHandler(parent), category(cat), storage(s), sink(sk), recordFactory(rf), requestedFlush(false), isFlushing(false) {
    bufferMutex = xSemaphoreCreateMutex();
    totalRequests = 0;
    totalCommitedRequests = 0;
    totalFlushedRequests = 0;
    lastCreatedTimeUs = 0;
    rateLimit = RATE_LIMIT_MIN_INTERVAL_US;
    // DPRINT("[LogCategoryHandler] Created handler for category: ");
    DPRINTLN(category);
}

LogCategoryHandler::~LogCategoryHandler() {
    // DPRINT("[LogCategoryHandler] Destroying handler for category: ");
    DPRINTLN(category);
    if (bufferMutex) {
        vSemaphoreDelete(bufferMutex);
        // DPRINTLN("[LogCategoryHandler] Deleted buffer mutex");
    }
    for (auto rec : activeRecords) {
        delete rec;
    }
    for (auto rec : commitedRecords) {
        delete rec;
    }
    // DPRINTLN("[LogCategoryHandler] Deleted all active and committed records");
}

ILogRecord* LogCategoryHandler::createRecord(bool block) {
    int64_t nowUs = esp_timer_get_time();
    if (nowUs - lastCreatedTimeUs < rateLimit) {
        // DPRINTLN("[LogCategoryHandler] Skipped record: rate limit hit");
        return nullptr;
    }
    lastCreatedTimeUs = nowUs;

    if ((!block && isFlushing) || xSemaphoreTake(bufferMutex, block ? portMAX_DELAY : 0) != pdTRUE) {
        // DPRINTLN("[LogCategoryHandler] WARNING: Failed to acquire buffer mutex");
        return nullptr;
    }

    ILogRecord* rec = recordFactory->create(this);
    if (!rec) {
        // DPRINTLN("[LogCategoryHandler] WARNING: recordFactory returned nullptr");
    } else {
        activeRecords.push_back(rec);
        totalRequests++;

        DPRINT("[LogCategoryHandler] ");
        DPRINT(category);
        DPRINT(" Record created. Active:");
        DPRINTLN(activeRecords.size());
        // DPRINT(" | Total created: ");
        // DPRINTLN(totalRequests);
    }
    requestFlush();
    xSemaphoreGive(bufferMutex);
    return rec;
}


bool LogCategoryHandler::commitRecord(ILogRecord* rec, bool block) {
    // DPRINTLN("[LogCategoryHandler] commitRecord called");
    if ((!block && isFlushing) || xSemaphoreTake(bufferMutex, block ? portMAX_DELAY : 0) != pdTRUE) {
        // DPRINTLN("WARNING: commitRecord: failed to acquire mutex");
        return false;
    }

    auto it = std::find(activeRecords.begin(), activeRecords.end(), rec);
    if (it != activeRecords.end()) {
        commitedRecords.push_back(*it);
        activeRecords.erase(it);
        totalCommitedRequests++;
        
        DPRINT("[LogCategoryHandler] ");
        DPRINT(category);
        DPRINT(" Record committed. Total committed: ");
        DPRINTLN(totalCommitedRequests);
        xSemaphoreGive(bufferMutex);
        return true;
    }

    // DPRINTLN("WARNING: commitRecord: record not found in active list");
    xSemaphoreGive(bufferMutex);
    return false;
}

void LogCategoryHandler::requestFlush() {
    if (requestedFlush || commitedRecords.size() < LOG_BUFF_MAX_SIZE) {
        // DPRINTLN("[LogCategoryHandler] Not enough committed records to flush");
        return;
    }
    DPRINT("[LogCategoryHandler] ");
    DPRINT(category);
    DPRINT(" requestFlush called. Committed records count: ");
    DPRINTLN(commitedRecords.size());
    requestedFlush = parent->requestFlush(this);
}

void LogCategoryHandler::flushToFile() {
    size_t active = activeRecords.size();
    size_t comited = activeRecords.size();
    DPRINT_WATCHDOG("[LogWatchDog] Active: ");
    DPRINT_WATCHDOG(active);
    DPRINT_WATCHDOG(", Commited: ");
    DPRINT_WATCHDOG(comited);
    DPRINT_WATCHDOG(", Approx RAM (bytes): ");
    DPRINTLN_WATCHDOG(200*(active + comited));
    DPRINTLN("[LogCategoryHandler] flushToFile called");
    isFlushing = true;
    if (xSemaphoreTake(bufferMutex, portMAX_DELAY) != pdTRUE) {
        DPRINTLN("WARNING: flushToFile: failed to acquire mutex");
        return;
    }
  
    std::vector<ILogRecord*> buffer;
    while (!commitedRecords.empty()) {
        buffer.push_back(commitedRecords.front());
        commitedRecords.pop_front();
    }
    xSemaphoreGive(bufferMutex);
    isFlushing = false;

    if (buffer.empty()) {
        DPRINTLN("[FlushTask] No records to flush");
        return;
    }

    DPRINT("[FlushTask] Flushing ");
    DPRINT(buffer.size());
    DPRINTLN(" records to storage");

    storage->write(category, buffer, true);
    totalFlushedRequests += buffer.size();
    totalCommitedRequests -= buffer.size();

    for (auto r : buffer) {
        delete r;
    }
    // DPRINT("[LogCategoryHandler] Flushed records. Total flushed: ");
    DPRINTLN(totalFlushedRequests);
    requestedFlush = false;
}

void LogCategoryHandler::uploadIfNeeded() {
    sink->upload(category, category);
}

#include <esp_heap_caps.h>  // For esp_get_free_heap_size()

bool LogCategoryHandler::watchdogCheck() {
    DPRINT_WATCHDOG("[LogWatchDog] WATCHDOG CATEGORY: ");
    DPRINTLN_WATCHDOG(category);
    // DPRINT("[LogCategoryHandler] Total created: ");
    // DPRINTLN(totalRequests);
    // DPRINT("[LogCategoryHandler] Total committed: ");
    // DPRINTLN(totalCommitedRequests);
    // DPRINT("[LogCategoryHandler] Total flushed: ");
    // DPRINTLN(totalFlushedRequests);
    size_t fileSize = storage->getFileSize(category);
    DPRINT_WATCHDOG("[LogWatchDog] Backup file size: ");
    DPRINTLN_WATCHDOG(fileSize);
    // Print free heap memory
    size_t freeHeap = esp_get_free_heap_size();
    DPRINT_WATCHDOG("[LogWatchDog] Free heap size (bytes): ");
    DPRINTLN_WATCHDOG(freeHeap);
    bool res = false;
    if(fileSize > DB_PAYLOAD_SIZE_BYTE / 2){
      res = true;
      DPRINTLN_WATCHDOG("[LogWatchDog] Update needed");
    }
    if(fileSize > (DB_PAYLOAD_SIZE_BYTE * 3) / 2 ){
      rateLimit *= 1.5;
      DPRINTLN_WATCHDOG("[LogWatchDog] Rate limit increased");
    }
    return res;
}




