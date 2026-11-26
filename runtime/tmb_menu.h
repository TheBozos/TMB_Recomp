// PC SETTINGS in the game's own Options screen (runtime/tmb_menu.cpp): what the settings window (F2) offers besides the
// controls (video, cheats, save states), on pages drawn with the game's font and steered with the pad. The window's
// parts (its video settings and save states) come from the window program through these hooks; without them (the
// headless program) those pages are not offered.
#pragma once

#include "bozo/app/video_settings.h"

#include <functional>
#include <string>

namespace tmb::menu
{
    struct Hooks
    {
        // Video: what is applied now (with the sizes to offer), and a change applied and saved to video.ini.
        std::function<bozo::app::video::Shared()> videoShared;
        std::function<void(const bozo::app::video::Settings &)> videoApply;
        // The texture pack's folder a texture_pack setting names ("" none).
        std::function<std::string(const std::string &)> texturePackFolder;
        // Save states: the slot F5 / F7 use, its summary ("Slot 3: saved ..." or "Slot 3: empty"), save and load.
        std::function<int()> stateSlot;
        std::function<void(int)> setStateSlot;
        std::function<std::string(int)> slotSummary;
        std::function<bool(int)> saveState, loadState;
    };
    void setHooks(Hooks hooks);

    // A save state was loaded: the pages close (what they showed is gone).
    void stateLoaded();
}
