#ifndef STRELA_SYSTEM_INTERNAL_H
#define STRELA_SYSTEM_INTERNAL_H
#include "strela/system.h"
#include "strela.h"
struct strela_machine {
    strela_t core;
    bool paused;
};
#endif
