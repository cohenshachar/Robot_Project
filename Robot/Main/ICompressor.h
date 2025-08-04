#pragma once
#include "ILogRecord.h"
#include <vector>
#include <Stream.h> 

class ICompressor {
public:
    virtual bool compressToStream(const std::vector<ILogRecord*>& records, Stream& outStream) = 0;
    virtual bool decompressFromStream(Stream& inStream, Stream& outStream) = 0;
    virtual ~ICompressor() {}
};