#pragma once

#include "IStorage.h"
#include "ICompressor.h"
#include <LittleFS.h>
#include <WString.h>
#include <map>
#include "DebugConfig.h"
#include "DBConfig.h"
#include "MemPrint.h"

#define DPRINT(x)    DPRINT_STOR(x)
#define DPRINTLN(x)  DPRINTLN_STOR(x)
#define LOG_FUNC(x) LOG_DB_MSG(x)

class FileSystemStorage : public IStorage {
public:
    FileSystemStorage(ICompressor* comp) : IStorage(comp) {
        DPRINTLN("[Storage] FileSystemStorage created");
    }

    ~FileSystemStorage() {
        if (fileCachesLock) {
            xSemaphoreTake(fileCachesLock, portMAX_DELAY);
            for (auto& pair : fileCaches) {
                delete pair.second;
            }
            fileCaches.clear();
            xSemaphoreGive(fileCachesLock);
            vSemaphoreDelete(fileCachesLock);
            fileCachesLock = nullptr;
        }
    }


bool begin() {
    DPRINTLN_INIT("[Storage] Initializing LittleFS...");

    const int maxAttempts = 5;
    int attempt = 0;

    while (attempt < maxAttempts) {
        attempt++;
        DPRINTLN_INIT("[Storage] Attempt #" + String(attempt));

        if (!LittleFS.begin()) {
            DPRINTLN_INIT("[Storage] LittleFS not mounted...");
        } else {
            DPRINTLN_INIT("[Storage] LittleFS mounted. Unmounting before formatting...");
            LittleFS.end();
        }

        DPRINTLN_INIT("[Storage] Formatting filesystem...");
        if (!LittleFS.format()) {
            DPRINTLN_INIT("[Storage] Format failed");
            delay(1000);
            continue;
        }

        DPRINTLN_INIT("[Storage] Format successful, mounting...");
        if (LittleFS.begin()) {
            DPRINTLN_INIT("[Storage] Filesystem formatted and mounted cleanly");

            fileCachesLock = xSemaphoreCreateMutex();
            if (fileCachesLock == NULL) {
                DPRINTLN_INIT("[Storage] ERROR: Failed to create fileCachesLock mutex");
                return false;
            }

            return true;
        }

        DPRINTLN_INIT("[Storage] Mount after format failed");
        delay(1000);
    }

    DPRINTLN_INIT("[Storage] ❌ Failed to initialize LittleFS after retries.");
    return false;
}

    void write(const char* category, const std::vector<ILogRecord*>& records, bool compress) override {
        DPRINT("[Storage] write called for category: ");
        DPRINTLN(category);

        FileCache* cache = getFileCache(category);
        if (!cache) {
            DPRINTLN("[Storage] ERROR: FileCache not found");
            return;
        }
        if (!cache->valid) {
            DPRINTLN("[Storage] ERROR: FileCache invalid");
            return;
        }

        if (xSemaphoreTake(cache->lock, portMAX_DELAY)) {
            DPRINT("[Storage] Acquired lock for writing category: ");
            DPRINTLN(category);

            // Always seek to EOF before writing (append)
            size_t pos = cache->fd.size();
            DPRINT("[Storage] Seeking to EOF at position: ");
            DPRINTLN(pos);
            cache->fd.seek(pos);

            for (auto* record : records) {
                uint8_t buffer[512];
                MemPrint memPrint(buffer, sizeof(buffer));

                bool success = compress && compressor
                    ? compressor->compressToStream({record}, memPrint)
                    : record->serialize(memPrint);

                if (success) {
                    uint32_t len32 = memPrint.size();
                    size_t written1 = cache->fd.write((uint8_t*)&len32, sizeof(len32));
                    size_t written2 = cache->fd.write(buffer, len32);
                    DPRINT("[Storage] Wrote length prefix bytes: ");
                    DPRINT(written1);
                    DPRINT(" and data bytes: ");
                    DPRINTLN(written2);

                    DPRINT("[Storage] Wrote message length: ");
                    DPRINTLN(len32);
                } else {
                    DPRINTLN("[Storage] Failed to serialize or compress record");
                }
            }
            cache->fd.flush();
            DPRINTLN("[Storage] Flushed writes to storage");

            xSemaphoreGive(cache->lock);
            DPRINTLN("[Storage] Released lock after writing");
        } else {
            DPRINTLN("[Storage] Failed to acquire lock for writing");
        }
    }

    bool readToStream(const char* category, Stream& out, size_t maxMessages, size_t maxBytes, bool decompress) override {
        DPRINT("[Storage] readToStream called for category: ");
        DPRINTLN(category);

        FileCache* cache = getFileCache(category);
        if (!cache) {
            DPRINTLN("[Storage] ERROR: FileCache not found");
            return false;
        }
        if (!cache->valid) {
            DPRINTLN("[Storage] ERROR: FileCache invalid");
            return false;
        }

        if (!xSemaphoreTake(cache->lock, portMAX_DELAY)) {
            DPRINTLN("[Storage] Failed to acquire lock for reading");
            return false;
        }
        DPRINT("[Storage] Acquired lock for reading category: ");
        DPRINTLN(category);

        String path = String("/") + category + ".log";

        // Start reading from beginning each time (or you can track position externally)
        cache->fd.seek(0);
        DPRINTLN("[Storage] Seeked to beginning for reading");

        size_t processed = 0;
        size_t totalBytes = 0;
        size_t readStart = cache->fd.position();
        size_t fileSize = cache->fd.size();

        DPRINT("[Storage] File size: ");
        DPRINTLN(fileSize);

        while (cache->fd.position() < fileSize && processed < maxMessages && totalBytes < maxBytes) {
            uint32_t msgLen;
            if (cache->fd.readBytes((char*)&msgLen, sizeof(msgLen)) != sizeof(msgLen)) {
                DPRINTLN("[Storage] Failed to read message length prefix");
                break;
            }
            DPRINT("[Storage] Read message length prefix: ");
            DPRINTLN(msgLen);

            if (totalBytes + sizeof(msgLen) + msgLen > maxBytes) {
                DPRINTLN("[Storage] Reached max bytes limit, stopping read");
                break;
            }

            std::unique_ptr<uint8_t[]> buf(new uint8_t[msgLen]);
            if (cache->fd.read(buf.get(), msgLen) != msgLen) {
                DPRINTLN("[Storage] Failed to read full message data");
                break;
            }
            DPRINT("[Storage] Read full message data of length: ");
            DPRINTLN(msgLen);

            if (decompress && compressor) {
                String strBuf((char*)buf.get(), msgLen);
                Stream* in = new StringStream(strBuf);
                compressor->decompressFromStream(*in, out);
                delete in;
                DPRINTLN("[Storage] Decompressed and streamed message");
            } else {
                out.write(buf.get(), msgLen);
                DPRINTLN("[Storage] Streamed message without decompression");
            }

            totalBytes += sizeof(msgLen) + msgLen;
            processed++;
            DPRINT("[Storage] Processed messages count: ");
            DPRINTLN(processed);
        }

        // Remove processed bytes by rewriting remainder at front
        if (cache->fd.position() > readStart) {
            DPRINTLN("[Storage] Removing processed bytes by rewriting remainder");

            // Open temporary file for writing remainder
            File temp = LittleFS.open(path + ".tmp", "w");
            if (!temp) {
                DPRINTLN("[Storage] Failed to open temp file for truncation");
                xSemaphoreGive(cache->lock);
                return false;
            }

            size_t remaining = fileSize - cache->fd.position();
            DPRINT("[Storage] Remaining bytes to copy to temp file: ");
            DPRINTLN(remaining);

            while (remaining--) {
                int b = cache->fd.read();
                if (b < 0) {
                    DPRINTLN("[Storage] Unexpected EOF while copying remainder");
                    break;
                }
                temp.write((uint8_t)b);
            }

            cache->fd.close();
            temp.close();

            DPRINTLN("[Storage] Replacing original file with temp file");

            if (!LittleFS.remove(path)) {
                DPRINTLN("[Storage] Failed to remove original log file");
            }
            if (!LittleFS.rename(path + ".tmp", path)) {
                DPRINTLN("[Storage] Failed to rename temp file to log file");
            }

            // Reopen cache->fd so it is fresh after file replace
            cache->fd = LittleFS.open(path, "r+");
            if (!cache->fd) {
                DPRINTLN("[Storage] Failed to reopen log file after truncation");
                cache->valid = false;
                xSemaphoreGive(cache->lock);
                return false;
            }

            DPRINTLN("[Storage] Successfully truncated log file");
        } else {
            DPRINTLN("[Storage] No bytes processed, no truncation needed");
        }

        xSemaphoreGive(cache->lock);
        DPRINTLN("[Storage] Released lock after reading");

        return true;
    }

    size_t getFileSize(const char* category) override {
        DPRINT("[Storage] getFileSize called for category: ");
        DPRINTLN(category);

        FileCache* cache = getFileCache(category);
        if (!cache) {
            DPRINTLN("[Storage] ERROR: FileCache not found");
            return 0;
        }
        if (!cache->valid) {
            DPRINTLN("[Storage] ERROR: FileCache invalid");
            return 0;
        }

        size_t size = 0;
        if (xSemaphoreTake(cache->lock, portMAX_DELAY)) {
            size = cache->fd.size();
            DPRINT("[Storage] File size for category ");
            DPRINT(category);
            DPRINT(" is ");
            DPRINTLN(size);
            xSemaphoreGive(cache->lock);
        } else {
            DPRINTLN("[Storage] Failed to acquire lock for getFileSize");
        }

        return size;
    }

private:
    struct FileCache {
        File fd;
        SemaphoreHandle_t lock;
        bool valid;

        FileCache(const char* category) : lock(nullptr), valid(false) {
            DPRINT("[Storage] Creating FileCache for category: ");
            DPRINTLN(category);

            lock = xSemaphoreCreateMutex();
            if (!lock) {
                DPRINTLN("[Storage] Failed to create mutex for FileCache");
                return;
            }

            String path = String("/") + category + ".log";
            fd = LittleFS.open(path, "r+");
            if (!fd) {
                DPRINT("[Storage] Log file doesn't exist, creating new file: ");
                DPRINTLN(path);
                fd = LittleFS.open(path, "w+");
                if (!fd) {
                    DPRINT("[Storage] Failed to open or create file: ");
                    DPRINTLN(path);
                    vSemaphoreDelete(lock);
                    lock = nullptr;
                    return;
                }
            }
            valid = true;

            DPRINTLN("[Storage] FileCache created and file opened");
        }

        ~FileCache() {
            DPRINTLN("[Storage] Destroying FileCache");
            if (fd) {
                DPRINTLN("[Storage] Closing file descriptor");
                fd.close();
            }
            if (lock) {
                DPRINTLN("[Storage] Deleting mutex");
                vSemaphoreDelete(lock);
            }
        }
    };

    std::map<String, FileCache*> fileCaches;
    SemaphoreHandle_t fileCachesLock = nullptr;

FileCache* getFileCache(const char* category) {
    if (!fileCachesLock) return nullptr;

    if (xSemaphoreTake(fileCachesLock, portMAX_DELAY)) {
        auto it = fileCaches.find(category);
        if (it == fileCaches.end()) {
            fileCaches[category] = new FileCache(category);
            DPRINT("[Storage] Created new FileCache for category: ");
            DPRINTLN(category);
        }
        FileCache* result = fileCaches[category];
        xSemaphoreGive(fileCachesLock);
        return result;
    }

    // Failed to take lock (shouldn't happen with portMAX_DELAY)
    return nullptr;
}

};
