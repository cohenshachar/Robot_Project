#include "Robot.h"
#include "Compressors.h"
#include "FileSystemStorage.h"
#include "LogCategoryFactory.h"
#include "FirebaseSink.h"
#include "JsonLogRecord.h"
#include "Config.h"
#include "positional_stl.h"
#include "JsonCommandParser.h"
#include "LogManager.h"
#include "PurePursuit.h"

struct InitFlags {
    bool storage = false;
    bool sink = false;
    bool logger = false;
};

struct RobotDependencies {
    NoCompression compressor;
    FileSystemStorage storage;
    FirebaseSink sink;
    JsonLogRecordFactory logFactory;
    LogCategoryFactory catFactory;
    LogManager logger;
    PurePursuitController ppc;
    JsonCommandParser* jcp;
    InitFlags init_flags;

    RobotDependencies()
        : compressor(),
          storage(&compressor),
          sink(&storage),
          logFactory(),
          catFactory(&storage, &sink, &logFactory),
          logger(&catFactory),
          ppc(67.0f, 2.0f),
          jcp(nullptr)
    {
        DPRINTLN_INIT("[Deps] Initializing storage...");
        init_flags.storage = storage.begin();

        DPRINTLN_INIT("[Deps] Initializing sink...");
        init_flags.sink = sink.begin();

        if (init_flags.sink) {
            DPRINTLN_INIT("[Deps] Sink OK, initializing logger...");
            init_flags.logger = logger.begin();

            DPRINT_INIT("[Deps] Allocating JsonCommandParser...");
            jcp = new JsonCommandParser(&sink, COMMAND_DOWNLOAD_PATH);
            DPRINTLN_INIT(" ✅");
        } else {
            DPRINTLN_INIT("[Deps] ❌ Sink unavailable, skipping logger and parser.");
        }
    }

    ~RobotDependencies() {
        DPRINT_INIT("[Deps] Cleaning up...");
        if (jcp) {
            delete jcp;
            DPRINT_INIT(" parser freed.");
        }
        DPRINTLN_INIT(" Done.");
    }
};

Robot* r2d2 = nullptr;
static int64_t lastTimeUs = 0; 

void setup() {
  Serial.begin(115200);
  delay(2000);
  DPRINTLN_INIT("[Setup]");
  static RobotDependencies deps;  // stack-allocated and persistent
  static Robot robi(deps.init_flags.logger ? &deps.logger : nullptr, &deps.ppc, deps.jcp);  // stack-allocated and persistent
  robi.begin();
  r2d2 = &robi;
  lastTimeUs = esp_timer_get_time();
}

void loop() {
    TickType_t lastWakeTime = xTaskGetTickCount();    // Initialize to current time
    int64_t nowUs = esp_timer_get_time();
    float dt = (nowUs - lastTimeUs) / 1e6f;
    lastTimeUs = nowUs;
    DPRINT_TIMING("[MainLoop] Loop timing: ");
    DPRINTLN_TIMING(dt);
    r2d2->tick(dt); // or more, to avoid starving other tasks
    vTaskDelay(30); // or more, to avoid starving other tasks
}

