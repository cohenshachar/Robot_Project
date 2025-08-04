#pragma once

#include <WString.h>
#include <vector>
#include "ICompressor.h"
#include "ILogRecord.h"
#include "DebugConfig.h"
#include "MemPrint.h"

#define DPRINT(x)    DPRINT_COMP(x)
#define DPRINTLN(x)  DPRINTLN_COMP(x)
#define LOG_FUNC(x)  LOG_DB_MSG(x)

// ==============================
// No Compression
// ==============================
class NoCompression : public ICompressor {
public:
    bool compressToStream(const std::vector<ILogRecord*>& records, Stream& outStream) override {
        DPRINTLN("[NoCompression] Writing raw logs directly to stream");

        for (auto* record : records) {
            if (!record->serialize(outStream)) {
                DPRINTLN("[NoCompression] Serialize failed");
                return false;
            }
        }
        return true;
    }

    bool decompressFromStream(Stream& inStream, Stream& outStream) override {
        DPRINTLN("[NoCompression] Starting decompression (raw copy)");
        while (inStream.available()) {
            int c = inStream.read();
            if (c < 0) break;
            outStream.write((uint8_t)c);
        }
        DPRINTLN("[NoCompression] Decompression complete");
        return true;
    }
};


// ==============================
// Run-Length Encoding (RLE) Compression
// ==============================
class RLECompression : public ICompressor {
public:
    bool compressToStream(const std::vector<ILogRecord*>& records, Stream& outStream) override {
        DPRINTLN("[RLECompression] Starting compression");
        uint8_t inputBuffer[256];

        for (auto* record : records) {
            MemPrint memPrint(inputBuffer, sizeof(inputBuffer));
            if (!record->serialize(memPrint)) {
                DPRINTLN("[RLECompression] Serialize failed");
                continue;
            }

            size_t inputSize = memPrint.size();
            size_t i = 0;
            while (i < inputSize) {
                uint8_t value = inputBuffer[i];
                size_t count = 1;
                while (i + count < inputSize && inputBuffer[i + count] == value && count < 255) {
                    count++;
                }
                outStream.write(count);
                outStream.write(value);
                i += count;
            }
        }
        DPRINTLN("[RLECompression] Compression complete");
        return true;
    }
    bool decompressFromStream(Stream& inStream, Stream& outStream) override {
        DPRINTLN("[RLECompression] Starting decompression");
        while (inStream.available() > 1) {
            int runLength = inStream.read();
            int byteVal = inStream.read();
            if (runLength < 0 || byteVal < 0) break;

            for (int i = 0; i < runLength; i++) {
                outStream.write((uint8_t)byteVal);
            }
        }
        DPRINTLN("[RLECompression] Decompression complete");
        return true;
    }
};

