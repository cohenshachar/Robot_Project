#include "JsonCommandParser.h"
#include <ArduinoJson.h>
#include "DebugConfig.h" // for DPRINTLN etc. if needed
#include "MemPrint.h"

#define DPRINT(x)    DPRINT_PARSER(x)
#define DPRINTLN(x)  DPRINTLN_PARSER(x)

JsonCommandParser::JsonCommandParser(ISink* sink, const char* remotePath)
    : sink(sink), path(remotePath) {}

bool JsonCommandParser::fetchAndParseCommands() {
    reset();

    if (sink && !sink->isAvailable()) {
        DPRINTLN("[CommandParser] ❌ Sink not available");
        return false;
    }

    String buffer;
    StringStream stream(buffer);

    if (sink && !sink->downloadToStream(path, stream)) {
        DPRINTLN("[CommandParser] ❌ Download failed");
        return false;
    }

    DPRINTLN("[CommandParser] ✅ Download succeeded. Raw JSON:");
    DPRINTLN(buffer);

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, buffer);
    if (err) {
        DPRINTLN("[CommandParser] ❌ JSON parse failed: " + String(err.c_str()));
        return false;
    }

    if (!doc.is<JsonArray>()) {
        DPRINTLN("[CommandParser] ❌ Expected a JSON array");
        return false;
    }

    int count = 0;
    for (JsonVariant v : doc.as<JsonArray>()) {
        if (v.is<const char*>()) {
            String commandStr = v.as<const char*>();
            MazeCommand cmd = parseCommandString(commandStr);
            DPRINT("[CommandParser] Parsed: \""); DPRINT(commandStr); DPRINT("\" → ");

            if (cmd != MazeCommand::INVALID) {
                DPRINTLN("✅ Valid");
                commands.push_back(cmd);
                count++;
            } else {
                DPRINTLN("❌ Invalid");
            }
        } else {
            DPRINTLN("[CommandParser] ⚠️ Skipping non-string JSON entry.");
        }
    }

    DPRINT("[CommandParser] ✅ Parsed "); DPRINT(count); DPRINTLN(" valid commands");
    return true;
}


void JsonCommandParser::reset() {
    commands.clear();
}

int JsonCommandParser::commandCount() const {
    return commands.size();
}

MazeCommand JsonCommandParser::getCommand(size_t index) const {
    if (index >= commands.size()) return MazeCommand::INVALID;
    return commands[index];
}

MazeCommand JsonCommandParser::parseCommandString(const String& str) const {
    if (str == "forward") return MazeCommand::FORWARD;
    if (str == "left")    return MazeCommand::LEFT;
    if (str == "right")   return MazeCommand::RIGHT;
    if (str == "stop")    return MazeCommand::STOP;
    if (str == "end")     return MazeCommand::END;
    return MazeCommand::INVALID;
}
