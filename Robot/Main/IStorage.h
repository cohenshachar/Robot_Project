#pragma once
#include "ICompressor.h"
#include "ILogRecord.h"
#include <vector>
#include <Stream.h>

class IStorage {
protected:
    ICompressor* compressor;
public:
    IStorage(ICompressor* comp) : compressor(comp) {}

    virtual void write(const char* category, const std::vector<ILogRecord*>& records, bool compress) = 0;

    virtual bool readToStream(const char* category, Stream& out, size_t maxMessages, size_t maxBytes, bool decompress) = 0;

    virtual bool readMessages(const char* category, Stream& out, size_t maxMessages, bool decompress = false) {
        return readToStream(category, out, maxMessages, SIZE_MAX, decompress);
    }

    virtual bool readBytes(const char* category, Stream& out, size_t maxBytes, bool decompress = false) {
        return readToStream(category, out, SIZE_MAX, maxBytes, decompress);
    }
    virtual size_t getFileSize(const char* category) = 0;
    virtual ~IStorage() {}
};
