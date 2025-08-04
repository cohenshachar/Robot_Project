#pragma once

#include "ICommandParser.h"
#include "ISink.h"
#include "MazeCommands.h"
#include <vector>

class JsonCommandParser : public ICommandParser {
public:
    // Construct with a reference to ISink and JSON remote path
    JsonCommandParser(ISink* sink, const char* remotePath);
    
    // Download and parse commands into internal vector
    bool fetchAndParseCommands() override;

    // Reset parsed commands buffer
    void reset() override;

    // Number of parsed commands
    int commandCount() const override;

    // Access command by index
    MazeCommand getCommand(size_t index) const override;

private:
    ISink* sink;
    const char* path;
    std::vector<MazeCommand> commands;

    // Convert string to MAZE_COMMANDS enum
    MazeCommand parseCommandString(const String& str) const;
};
