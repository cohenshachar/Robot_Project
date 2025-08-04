#include "FirebaseSink.h"
#include <WiFi.h>
#include "DebugConfig.h"
#include "DBConfig.h"
#include "timeUtil.h"
#include "MemPrint.h"

#define DPRINT(x)    DPRINT_SINK(x)
#define DPRINTLN(x)  DPRINTLN_SINK(x)
#define LOG_FUNC(x)  LOG_DB_MSG(x)

FirebaseSink::FirebaseSink(IStorage* storage)
    : ISink(storage) {}

bool FirebaseSink::begin() {
    const int maxRetries = 10;
    int wifiAttempts = 0;
    int dbAttempts = 0;
    int deleteAttempts = 0;

    // Retry WiFi connection
    while (!connectToWifi() && wifiAttempts < maxRetries) {
        DPRINT_INIT("[FirebaseSink] WiFi retry attempt ");
        DPRINTLN_INIT(wifiAttempts + 1);
        wifiAttempts++;
        delay(1000);  // wait 1 second before retry
    }

    if (wifiAttempts >= maxRetries) {
        DPRINTLN_INIT("[FirebaseSink] ❌ WiFi connection failed after retries.");
        return false;
    }

    // Retry Firebase DB connection
    while (!connectToDB() && dbAttempts < maxRetries) {
        DPRINT_INIT("[FirebaseSink] DB retry attempt ");
        DPRINTLN_INIT(dbAttempts + 1);
        dbAttempts++;
        delay(1000);  // wait 1 second before retry
    }

    if (dbAttempts >= maxRetries) {
        DPRINTLN_INIT("[FirebaseSink] ❌ Firebase DB connection failed after retries.");
        return false;
    }
    DPRINTLN_INIT("[FirebaseSink] ✅ Firebase connected.");
    while (!Firebase.RTDB.deleteNode(&fbdo, LOG_DIR) && deleteAttempts < maxRetries) {
        DPRINT_INIT("[FirebaseSink] DB retry attempt ");
        DPRINTLN_INIT(deleteAttempts + 1);
        deleteAttempts++;
        delay(1000);  // wait 1 second before retry
    }
    if (deleteAttempts >= maxRetries) {
        DPRINTLN_INIT("[FirebaseSink] ❌ Firebase DB connection failed after retries.");
        return false;
    }
    DPRINTLN_INIT("[FirebaseSink] 🔄 Cleared database root node.");
    return true;
}

bool FirebaseSink::connectToWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    DPRINT_INIT("WiFi Failed!\n");
    return false;
  }
  DPRINTLN_INIT("CONNECTED TO WIFI");
  return true;
}

bool FirebaseSink::connectToDB() {
    DPRINTLN_INIT("[FirebaseSink] Starting connectToDB...");

    // Validate config parameters first
    if (!DB_URL || strlen(DB_URL) == 0) {
        DPRINTLN_INIT("[FirebaseSink] ERROR: DB_URL is empty.");
        return false;
    }

    if (!DB_SECRET || strlen(DB_SECRET) == 0) {
        DPRINTLN_INIT("[FirebaseSink] ERROR: DB_SECRET is empty.");
        return false;
    }

    // Set up database URL and secret token
    config.database_url = DB_URL;
    config.signer.tokens.legacy_token = DB_SECRET;
    DPRINTLN_INIT("[FirebaseSink] Configured database URL and secret token.");

    // Initialize Firebase connection
    DPRINTLN_INIT("[FirebaseSink] Calling Firebase.begin...");
    Firebase.begin(&config, &auth);

    // Check if Wi-Fi is connected before enabling reconnect
    if (WiFi.status() != WL_CONNECTED) {
        DPRINTLN_INIT("[FirebaseSink] ERROR: Wi-Fi is not connected.");
        return false;
    }

    DPRINTLN("[FirebaseSink] Enabling Firebase WiFi reconnect...");
    Firebase.reconnectWiFi(true);

    // Wait for Firebase to be ready (up to 10 seconds)
    uint32_t t0 = millis();
    DPRINTLN("[FirebaseSink] Waiting for Firebase to be ready...");
    while (!Firebase.ready() && millis() - t0 < 10000) {
        delay(25);
    }

    if (!Firebase.ready()) {
        DPRINTLN("[FirebaseSink] ERROR: Firebase failed to initialize within timeout.");
        return false;
    }

    DPRINTLN("[FirebaseSink] Firebase successfully connected.");
    return true;
}


bool FirebaseSink::isAvailable() {
    return Firebase.ready();
}

bool FirebaseSink::upload(const char* upload_name, const char* file) {
    if (!storage) {
        DPRINTLN("[FirebaseSink] ❌ Storage pointer is null.");
        return false;
    }

    size_t fileSize = storage->getFileSize(file);
    if (fileSize == 0) {
        DPRINTLN("[FirebaseSink] ⚠️ No data to upload for: " + String(file));
        return false;
    }

    size_t bufSize = DB_PAYLOAD_SIZE_BYTE;
    char* buffer = new char[bufSize + 1];

    MemPrint memPrint((uint8_t*)buffer, bufSize);
    if (!storage->readBytes(file, memPrint, bufSize, true)) {
        DPRINTLN("[FirebaseSink] ❌ Failed to read data from: " + String(file));
        delete[] buffer;
        return false;
    }

    buffer[memPrint.size()] = '\0';
    String decompressedData(buffer, memPrint.size());
    delete[] buffer;

    if (decompressedData.length() == 0) {
        DPRINTLN("[FirebaseSink] ⚠️ No usable JSON in: " + String(file));
        return false;
    }

    FirebaseJsonArray jsonArray;
    int start = 0;

    while (start < decompressedData.length()) {
        int end = decompressedData.indexOf('\n', start);
        if (end == -1) end = decompressedData.length();

        String line = decompressedData.substring(start, end);
        if (line.length() > 0 && line.startsWith("{")) {
            jsonArray.add(line);
        } else {
            DPRINTLN("[FirebaseSink] ⚠️ Skipping invalid/empty line");
        }

        start = end + 1;
    }

    String firebasePath = String(LOG_DIR) + upload_name + "/" + timeHMSms();
    DPRINTLN("[FirebaseSink] ⬆ Uploading to Firebase path: " + firebasePath);
    vTaskDelay(10);  // give watchdog breathing room

    if (!Firebase.ready()) {
        DPRINTLN("[FirebaseSink] ❌ Firebase not ready during upload");
        return false;
    }

    if (Firebase.RTDB.setArray(&fbdo, firebasePath.c_str(), &jsonArray)) {
        DPRINTLN("[FirebaseSink] ✅ Upload successful");
        return true;
    } else {
        DPRINTLN("[FirebaseSink] ❌ Upload failed: " + fbdo.errorReason());
        return false;
    }
}

bool FirebaseSink::download(const char* src_path, const char* dst_path) {
    DPRINTLN("[FirebaseSink] ⚠️ Download not yet implemented.");
    return false;
}

bool FirebaseSink::downloadToStream(const char* src_path, Stream& out) {
    FirebaseData localFbdo;  // local copy, not shared
    DPRINT_PARSER("[FirebaseSink] Downloading JSON from: ");
    DPRINTLN_PARSER(src_path);
    // Use Firebase Data object (fbdo) to get JSON
    if (!Firebase.RTDB.getArray(&localFbdo,src_path)) {
        DPRINT_PARSER("[FirebaseSink] ❌ Failed to fetch: ");
        DPRINTLN_PARSER(fbdo.errorReason());
        return false;
    }
    // Get FirebaseJson from fbdo
    FirebaseJsonArray jsonArr = localFbdo.to<FirebaseJsonArray>();
    // Check if json is empty (size == 0)
    if (jsonArr.iteratorBegin()== 0) {
        DPRINTLN_PARSER("[FirebaseSink] ❌ Received JSON is empty");
        return false;
    }
    // Convert JSON to String (pretty printed)
    out.print(jsonArr.raw());
    jsonArr.iteratorEnd();
    DPRINTLN_PARSER("[FirebaseSink] ✅ JSON streamed to memory");
    return true;
}

