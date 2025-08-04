#pragma once
#include "IStorage.h"

class ISink {
protected:
  IStorage* storage;
public:
    ISink(IStorage* storage) : storage(storage){}
    virtual bool begin() = 0;
    virtual bool upload(const char* upload_name, const char* file_path) = 0;
    virtual bool download(const char* src_path, const char* dst_path) = 0;
    virtual bool downloadToStream(const char* src_path, Stream& out) =0;
    virtual bool isAvailable() = 0;
    virtual ~ISink() {}
};

