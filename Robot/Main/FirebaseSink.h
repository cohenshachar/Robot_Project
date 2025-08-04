#pragma once

#include "ISink.h"
#include <Firebase_ESP_Client.h>

class FirebaseSink : public ISink {
private:
    FirebaseData fbdo;
    FirebaseAuth auth;
    FirebaseConfig config;

    bool connectToDB();
    bool connectToWifi();

public:
    explicit FirebaseSink(IStorage* storage);
    bool begin() override;
    bool isAvailable() override;
    bool upload(const char* upload_name, const char* file) override;
    bool download(const char* src_path, const char* dst_path) override;
    bool downloadToStream(const char* src_path, Stream& out) override;
};
