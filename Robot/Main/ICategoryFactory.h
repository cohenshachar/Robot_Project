#pragma once
#include "ILogCategoryHandler.h"

class ICategoryFactory {
public:
    virtual ILogCategoryHandler* create(const char* category, ILogManager* manager) = 0;
    virtual ~ICategoryFactory() = default;
};
