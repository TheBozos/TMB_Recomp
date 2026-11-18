// Save states: TMB's own part of BozoEngine's save states (BozoEngine/docs/save-states.md).
#pragma once

#include "bozo/runtime.h"

namespace bozo::host
{
    class FreeHost;
}

namespace tmb
{
    // Adds the "tmb" section to the host's save states: what TMB's overrides and native code keep about the game.
    // On a load, the caches decoded from the old run's guest memory are dropped.
    void addStateSection(bozo::host::FreeHost &host, Runtime *runtime);
}
