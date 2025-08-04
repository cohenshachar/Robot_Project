#pragma once
#include "MazeCommands.h"
#include <cstddef>  // Add this to use size_t

class ICommandParser {
public:
    virtual ~ICommandParser() = default;

    // Download and parse raw commands into MAZE_COMMANDS
    virtual bool fetchAndParseCommands() = 0;

    // Reset internal buffer/state
    virtual void reset() = 0;

    // Number of parsed commands
    virtual int commandCount() const = 0;

    // Access parsed command by index
    virtual MazeCommand getCommand(size_t index) const = 0;
};

