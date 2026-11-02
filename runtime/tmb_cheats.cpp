// Cheats, switched while playing from the Cheats tab of the settings window (F2), per human player; the environment
// gives their values at launch (TMB_UNLOCK_ALL, TMB_NO_AI, TMB_PLAYER_GOD, TMB_INFINITE_AMMO, TMB_INFINITE_ENERGY,
// TMB_ENEMY_HEALTH_CAP; TMB_PLAYER_HEALTH_CAP is the testing opposite, a cap on the player's health).
//
// What the game keeps, from its own code:
// - TheGame (`this` of TheGame::Update): its vehicles at TheGame + i * 0x2D00, their count at + 0x1B3DB4
//   (TheGame::GetVehicleFromName); the human players' vehicles as pointers at + 0x1B28B0, their count at + 0x1B3DB8
//   (TheGame::gameCheckPlayerHealth, 0x133360, which walks them).
// - A vehicle's health: float at + 0x3F4, its maximum at + 0x3F0 (gameCheckPlayerHealth's health fraction, the HUD's
//   health bars). Who drives it: + 0x580, 1 a player (vehicleUpdate calls playerUpdateInputs), 2 the computer
//   (aiUpdateInputs, 0x1014A0).
// - Weapons: the count of pickup weapon i (1 to 8; slot 1 is the car's special, which Launcher::FireSpecial spends)
//   at + 0x22C4 + 4 * i, the weapon selected at + 0x2140; the machine gun (slot 0) has no count. Every shot,
//   detonation and special spends through TheGame::DecrementWeaponCount (0x1515A8, called by FireCurrentWeapon,
//   FireSpecial, UpdateSpecial, UpdateLauncher, UpdateEnvWeapon, UpdateZoomies, UpdateWeaponToggle, KillWeapon),
//   which goes on to Weapons::DecrementWeaponCount (0x1BA050: count - 1, then the next weapon with ammo when it
//   reaches 0).
// - Energy (the freeze / mines / shield moves of the stick combos): int at + 0x2214 of + 0x2218 (the HUD's energy
//   bar, Hud::build4PEnergyBar).
// - Progress: the Shell (at 0x40BCC0, the pointer at gp - 0x4148) keeps what is locked as words at Shell + 0x18000 +
//   offset, 1 locked and 0 open; Shell::Shell (0x15F270) sets a fresh save's (what loadGlobalData then overwrites
//   from the card's globalFile): the vehicles at + 0x218C + 4 * car (16; uiCarSel and uiAiSel play the locked sound
//   on a 1; 9-12, 14 and 15 locked at first), the levels at + 0x21CC + 4 * level (21; uiLvlSel, the same; Shell::
//   EvaluateEnduranceStatus clears 2 Freeway, 15 and 18 when their endurance kills are reached), and per character its
//   movies, three arrays of 14 at + 0x2220, + 0x2258, + 0x2290 (uiMovies; 2 for a movie the character does not have).

#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "runtime/ps2_debug_ui.h"
#include "runtime/ps2_determinism.h"
#if defined(PS2X_ENABLE_DEBUG_UI)
#include "imgui.h"
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace tmb::cheats
{
    namespace
    {
        constexpr uint32_t kAiUpdateInputs = 0x001014A0u;
        constexpr uint32_t kAiUpdateFrozenInput = 0x001017E8u;
        constexpr uint32_t kDecrementWeaponCount = 0x001515A8u;
        constexpr uint32_t kVehicles = 0x00000000u, kVehicleStride = 0x2D00u, kVehicleCount = 0x001B3DB4u;
        constexpr uint32_t kPlayers = 0x001B28B0u, kPlayerCount = 0x001B3DB8u;
        constexpr uint32_t kHealth = 0x3F4u, kHealthMax = 0x3F0u, kDriver = 0x580u;
        constexpr uint32_t kWeaponCounts = 0x22C4u, kEnergy = 0x2214u, kEnergyMax = 0x2218u;
        constexpr int kMaxPlayers = 4;
        constexpr uint32_t kGpShell = 0x4148u, kShellProgress = 0x18000u;
        // The shell screens that read the locks, and the lock arrays (offset, count) from Shell + 0x18000.
        constexpr uint32_t kUiLvlSel = 0x001976B0u, kUiCarSel = 0x001963B0u, kUiAiSel = 0x001986A8u, kUiMovies = 0x00190760u;
        constexpr struct
        {
            uint32_t offset, count;
        } kLocks[] = {{0x218Cu, 16u}, {0x21CCu, 21u}, {0x2220u, 14u}, {0x2258u, 14u}, {0x2290u, 14u}};

        // One bit per player (bit 0: player 1).
        std::atomic<uint32_t> g_infiniteHealth{0}, g_infiniteAmmo{0}, g_infiniteEnergy{0};
        std::atomic<bool> g_noAi{false};
        std::atomic<bool> g_unlockAll{false};
        // Story: start at a level (-1: off). The story's state is the level being played, Shell + 0x18000 + 0x2354 (the
        // index of the game's level table: GetLevelName, LoadLevelDB), the one last won at + 0x2358, the mode at
        // + 0x235C (1: one-player story); a new story sets the level to 1, the Junkyard, and each level won sets the
        // next (EvaluateOnePlayerStoryStatus).
        std::atomic<int> g_storyStart{-1};
        constexpr uint32_t kStoryLevel = 0x2354u, kStoryLastWon = 0x2358u, kGameMode = 0x235Cu;
        constexpr int kLevelCount = 21;
        // The levels by the level table's index (its names at 0x426070, 16 bytes each), with the card the story shows
        // when it starts there (the stage, of 8). Two stages have two arenas (the story picks one); ASYLUM and ROOFTOP2
        // show no card (reached from inside another level); the others are arenas the story does not use.
        constexpr const char *kLevelNames[kLevelCount] = {
            "Stage 2: Midtown Suburbs (SUBURB)", "Stage 1: Junkyard (JUNKYARD)", "Stage 2: Midtown Expressway 104 (FREEWAY)",
            "Stage 5: Prison Passage to Blackfield Asylum (PRISON)", "Stage 7: Abandoned Skyscrapers (ROOFTOP)",
            "Stage 3: Downtown Business District (CITY)", "Stage 3: Black Vista Highway (LOOP)", "Arena DEATH1", "Arena DEATH2",
            "Arena DEATH3", "Arena DEATH4", "Arena DEATH5", "Arena DEATH6", "Rooftops, second part (ROOFTOP2)",
            "Stage 4: Minion's Stadium (STADIUM)", "Arena SHIP", "Blackfield Asylum (ASYLUM)", "Arena MINICITY", "Arena MINIBURB",
            "Arena JUNK2", "Stage 8: Warhawk's Rooftop (ROOFBOSS)"};
        // The picker's order: the story's, then the rest.
        constexpr int kLevelOrder[kLevelCount] = {1, 0, 2, 5, 6, 14, 3, 16, 4, 13, 20, 7, 8, 9, 10, 11, 12, 15, 17, 18, 19};
        std::vector<uint32_t> g_unlocked; // the lock words Unlock everything opened (put back when it goes off)
        std::atomic<float> g_enemyCap{-1.0f};  // weak enemies: every computer vehicle's health held at most this; < 0 off
        std::atomic<float> g_playerCap{-1.0f}; // TMB_PLAYER_HEALTH_CAP (testing)
        std::atomic<uint32_t> g_theGame{0};
        std::atomic<const uint8_t *> g_rdram{nullptr}; // for the tab (onFrame's)
        const std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now(); // the log's times
        std::atomic<uint32_t> g_decrements{0}, g_decrementsSkipped{0}; // TMB_CHEATS_LOG: weapon spends since the last line

        uint32_t word(const uint8_t *rdram, uint32_t address)
        {
            uint32_t value = 0;
            std::memcpy(&value, rdram + (address & 0x1FFFFFFu), 4);
            return value;
        }
        float flt(const uint8_t *rdram, uint32_t address)
        {
            float value = 0.0f;
            std::memcpy(&value, rdram + (address & 0x1FFFFFFu), 4);
            return value;
        }
        void setWord(uint8_t *rdram, uint32_t address, uint32_t value) { std::memcpy(rdram + (address & 0x1FFFFFFu), &value, 4); }
        void setFlt(uint8_t *rdram, uint32_t address, float value) { std::memcpy(rdram + (address & 0x1FFFFFFu), &value, 4); }

        int playerCount(const uint8_t *rdram, uint32_t game)
        {
            return game == 0u ? 0 : std::clamp(static_cast<int32_t>(word(rdram, game + kPlayerCount)), 0, kMaxPlayers);
        }
        uint32_t playerVehicle(const uint8_t *rdram, uint32_t game, int player)
        {
            return word(rdram, game + kPlayers + static_cast<uint32_t>(player) * 4u);
        }
        // The players whose bit is set in `mask` that own `vehicle`.
        bool forPlayer(const uint8_t *rdram, uint32_t vehicle, uint32_t mask)
        {
            const uint32_t game = g_theGame.load(std::memory_order_relaxed);
            if (mask == 0u || vehicle == 0u)
                return false;
            for (int player = 0; player < playerCount(rdram, game); ++player)
                if ((mask >> player & 1u) && playerVehicle(rdram, game, player) == vehicle)
                    return true;
            return false;
        }

        // The players a variable names: digits 1 to 4 (TMB_INFINITE_AMMO=12: players 1 and 2), any other value but 0
        // every player.
        uint32_t envMask(const char *name)
        {
            const char *value = std::getenv(name);
            if (!value || !*value || std::string(value) == "0")
                return 0u;
            uint32_t mask = 0u;
            for (const char *c = value; *c; ++c)
            {
                if (*c < '1' || *c > '0' + kMaxPlayers)
                    return (1u << kMaxPlayers) - 1u;
                mask |= 1u << (*c - '1');
            }
            return mask;
        }
        float envFloat(const char *name)
        {
            const char *value = std::getenv(name);
            return value && *value ? static_cast<float>(std::atof(value)) : -1.0f;
        }

        // No AI: Vehicle::aiUpdateInputs (each computer vehicle's pad, every frame) runs the game's own
        // aiUpdateFrozenInput instead (no throttle, no steering, no firing).
        void aiUpdateInputsOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_noAi.load(std::memory_order_relaxed))
            {
                ctx->pc = kAiUpdateFrozenInput;
                aiUpdateFrozenInput__7Vehicle_0x1017e8(rdram, ctx, runtime);
                return;
            }
            aiUpdateInputs__7Vehicle_0x1014a0(rdram, ctx, runtime);
        }

        // Infinite ammo: TheGame::DecrementWeaponCount(game, vehicle, weapon) returns at once for a player who has it,
        // so nothing the car fires or detonates is counted (and the selected weapon stays).
        void decrementWeaponCountOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            g_decrements.fetch_add(1, std::memory_order_relaxed);
            static const bool s_log = std::getenv("TMB_CHEATS_LOG") != nullptr;
            if (s_log && word(rdram, getRegU32(ctx, 5) + kDriver) == 1u)
                std::fprintf(stderr, "[tmb:cheats] %.1f s: player car %x spends weapon %u (count %d)%s\n",
                             std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count(), getRegU32(ctx, 5), getRegU32(ctx, 6),
                             static_cast<int32_t>(word(rdram, getRegU32(ctx, 5) + kWeaponCounts + getRegU32(ctx, 6) * 4u)),
                             forPlayer(rdram, getRegU32(ctx, 5), g_infiniteAmmo.load(std::memory_order_relaxed)) ? ", not counted" : "");
            if (forPlayer(rdram, getRegU32(ctx, 5), g_infiniteAmmo.load(std::memory_order_relaxed)))
            {
                g_decrementsSkipped.fetch_add(1, std::memory_order_relaxed);
                ctx->pc = getRegU32(ctx, 31); // jr $ra
                return;
            }
            DecrementWeaponCount__7TheGameP7Vehiclei_0x1515a8(rdram, ctx, runtime);
        }

        // Unlock everything: every lock word still 1 opened; off, the ones it opened closed again (the game's own unlocks
        // since then stay, the card is written only when the player saves).
        void applyUnlocks(uint8_t *rdram, uint32_t gp)
        {
            const uint32_t shell = word(rdram, gp - kGpShell);
            if (shell == 0u)
                return;
            const uint32_t progress = shell + kShellProgress;
            if (g_unlockAll.load(std::memory_order_relaxed))
            {
                for (const auto &locks : kLocks)
                    for (uint32_t i = 0; i < locks.count; ++i)
                    {
                        const uint32_t address = progress + locks.offset + i * 4u;
                        if (word(rdram, address) == 1u)
                        {
                            setWord(rdram, address, 0u);
                            g_unlocked.push_back(address);
                        }
                    }
            }
            else if (!g_unlocked.empty())
            {
                for (const uint32_t address : g_unlocked)
                    if (word(rdram, address) == 0u)
                        setWord(rdram, address, 1u);
                g_unlocked.clear();
            }
        }
        template <void (*Screen)(uint8_t *, R5900Context *, PS2Runtime *)>
        void shellScreenOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            applyUnlocks(rdram, getRegU32(ctx, 28));
            Screen(rdram, ctx, runtime);
        }

#if defined(PS2X_ENABLE_DEBUG_UI)
        void perPlayer(const char *label, std::atomic<uint32_t> &flags, int players)
        {
            ImGui::PushID(label);
            uint32_t mask = flags.load(std::memory_order_relaxed);
            // On for every player of the match (every player when there is none yet).
            const uint32_t present = players > 0 ? (1u << players) - 1u : (1u << kMaxPlayers) - 1u;
            bool all = (mask & present) == present;
            if (ImGui::Checkbox(label, &all))
                mask = all ? (1u << kMaxPlayers) - 1u : 0u;
            for (int player = 0; player < std::max(players, 1); ++player)
            {
                ImGui::SameLine(player == 0 ? 220.0f : 0.0f);
                bool on = mask >> player & 1u;
                char name[8];
                std::snprintf(name, sizeof(name), "P%d", player + 1);
                if (ImGui::Checkbox(name, &on))
                    mask = on ? mask | 1u << player : mask & ~(1u << player);
            }
            flags.store(mask, std::memory_order_relaxed);
            ImGui::PopID();
        }

        void drawTab()
        {
            // Deterministic mode: nothing the user does may change the run (only the environment's cheats, from launch).
            if (ps2x::det::enabled())
            {
                ImGui::TextDisabled("Off in deterministic mode (PS2X_DETERMINISTIC=1): the environment's cheats only.");
                return;
            }
            const uint8_t *rdram = g_rdram.load(std::memory_order_relaxed);
            const uint32_t game = g_theGame.load(std::memory_order_relaxed);
            const int players = rdram ? playerCount(rdram, game) : 0;
            ImGui::TextDisabled(players ? "In a match: %d player(s). Changes apply at once." : "They act in a match.", players);

            bool unlock = g_unlockAll.load(std::memory_order_relaxed);
            if (ImGui::Checkbox("Unlock everything (levels, vehicles, movies)", &unlock))
                g_unlockAll.store(unlock, std::memory_order_relaxed);
            ImGui::TextDisabled("Opened in the menus; saving the game while it is on keeps them on the card.");

            int start = g_storyStart.load(std::memory_order_relaxed);
            ImGui::SetNextItemWidth(420.0f);
            if (ImGui::BeginCombo("Story: start at", start < 0 ? "the first level (as the game)" : kLevelNames[start]))
            {
                if (ImGui::Selectable("the first level (as the game)", start < 0))
                    start = -1;
                for (const int i : kLevelOrder)
                    if (ImGui::Selectable(kLevelNames[i], start == i))
                        start = i;
                ImGui::EndCombo();
            }
            g_storyStart.store(start, std::memory_order_relaxed);
            ImGui::TextDisabled("For testing: a new story begins at this level, then goes on as the game chains them.");

            ImGui::SeparatorText("Players");
            perPlayer("Infinite health", g_infiniteHealth, players);
            perPlayer("Infinite ammo", g_infiniteAmmo, players);
            perPlayer("Infinite energy", g_infiniteEnergy, players);
            ImGui::TextDisabled("Health and energy held at their maximum; weapons and specials not spent.");

            ImGui::SeparatorText("Opponents");
            bool noAi = g_noAi.load(std::memory_order_relaxed);
            if (ImGui::Checkbox("No AI (the computer's cars do nothing)", &noAi))
                g_noAi.store(noAi, std::memory_order_relaxed);
            float cap = g_enemyCap.load(std::memory_order_relaxed);
            bool weak = cap >= 0.0f;
            if (ImGui::Checkbox("Weak enemies", &weak))
                cap = weak ? 10.0f : -1.0f;
            if (weak)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200.0f);
                ImGui::SliderFloat("health at most", &cap, 1.0f, 300.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
            }
            g_enemyCap.store(cap, std::memory_order_relaxed);
            if (rdram && players)
            {
                const uint32_t car = playerVehicle(rdram, game, 0);
                ImGui::TextDisabled("Player 1: health %.0f of %.0f, energy %d of %d", flt(rdram, car + kHealth), flt(rdram, car + kHealthMax),
                                    static_cast<int32_t>(word(rdram, car + kEnergy)), static_cast<int32_t>(word(rdram, car + kEnergyMax)));
            }
        }
#endif
    }

    void setTheGame(uint32_t game) { g_theGame.store(game, std::memory_order_relaxed); }

    // Once a frame (timerEndOfRealTimeProcessing, the frame's processing done).
    // TMB_LOG_PROGRESS=1 (finding the story's state): each word of Shell + 0x18000 + 0x2180 to + 0x2500 that changes,
    // with the time, from every frame end (the menus' too).
    void logProgress(uint8_t *rdram, uint32_t gp)
    {
        const uint32_t shell = word(rdram, gp - kGpShell);
        if (shell == 0u)
            return;
        // Story: start at a level. When a new story sets its first level (the Junkyard, nothing won yet), that level
        // is replaced, before the game loads it (it does once the character is chosen); the game's own chain goes on
        // from there.
        {
            const uint32_t progress = shell + kShellProgress;
            const int level = static_cast<int32_t>(word(rdram, progress + kStoryLevel));
            static int s_lastLevel = -2;
            const int start = g_storyStart.load(std::memory_order_relaxed);
            if (start >= 0 && level == 1 && s_lastLevel != 1 && word(rdram, progress + kGameMode) == 1u &&
                word(rdram, progress + kStoryLastWon) == 0u && start != 1)
            {
                setWord(rdram, progress + kStoryLevel, static_cast<uint32_t>(start));
                std::fprintf(stderr, "[tmb:cheats] story starts at level %s\n", kLevelNames[start]);
            }
            s_lastLevel = static_cast<int32_t>(word(rdram, progress + kStoryLevel));
        }
        static const bool s_on = std::getenv("TMB_LOG_PROGRESS") != nullptr;
        if (!s_on)
            return;
        static uint32_t s_last[0xE0] = {};
        static bool s_seen = false;
        static const auto s_start = std::chrono::steady_clock::now();
        const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_start).count();
        for (uint32_t i = 0; i < 0xE0u; ++i)
        {
            const uint32_t value = word(rdram, shell + kShellProgress + 0x2180u + i * 4u);
            if (s_seen && value != s_last[i])
                std::fprintf(stderr, "[tmb:progress] t=%.1f +0x%x: %d -> %d\n", t, 0x2180u + i * 4u, static_cast<int32_t>(s_last[i]),
                             static_cast<int32_t>(value));
            s_last[i] = value;
        }
        s_seen = true;
    }

    void onFrame(uint8_t *rdram)
    {
        g_rdram.store(rdram, std::memory_order_relaxed);
        const uint32_t game = g_theGame.load(std::memory_order_relaxed);
        if (game == 0u)
            return;
        // TMB_CHEATS_TOGGLE=<s> (testing): every cheat switched on and off every <s> seconds of match frames, as the
        // tab would (checks that each acts while playing, not only from launch).
        static const double s_toggle = []
        {
            const char *value = std::getenv("TMB_CHEATS_TOGGLE");
            return value && *value ? std::atof(value) : 0.0;
        }();
        if (s_toggle > 0.0 && !ps2x::det::enabled())
        {
            const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            static double s_next = now + s_toggle;
            static bool s_on = false;
            if (now >= s_next)
            {
                s_next = now + s_toggle;
                s_on = !s_on;
                const uint32_t all = s_on ? (1u << kMaxPlayers) - 1u : 0u;
                g_infiniteHealth.store(all);
                g_infiniteAmmo.store(all);
                g_infiniteEnergy.store(all);
                g_noAi.store(s_on);
                g_enemyCap.store(s_on ? 10.0f : -1.0f);
                std::fprintf(stderr, "[tmb:cheats] all cheats %s\n", s_on ? "on" : "off");
            }
        }
        const uint32_t health = g_infiniteHealth.load(std::memory_order_relaxed), energy = g_infiniteEnergy.load(std::memory_order_relaxed);
        const float playerCap = g_playerCap.load(std::memory_order_relaxed);
        const int players = playerCount(rdram, game);
        for (int player = 0; player < players; ++player)
        {
            const uint32_t car = playerVehicle(rdram, game, player);
            if (car == 0u)
                continue;
            const float most = flt(rdram, car + kHealthMax), now = flt(rdram, car + kHealth);
            // Topped up while alive (a car already dead is left to respawn).
            if ((health >> player & 1u) && most > 0.0f && now > 0.0f && now < most)
                setFlt(rdram, car + kHealth, most);
            if (energy >> player & 1u)
                setWord(rdram, car + kEnergy, word(rdram, car + kEnergyMax));
            if (player == 0 && playerCap >= 0.0f)
            {
                static uint32_t s_frames = 0;
                if (++s_frames % 250u == 0u)
                    std::fprintf(stderr, "[tmb:health] %.1f of %.1f\n", now, most);
                if (now > playerCap)
                    setFlt(rdram, car + kHealth, playerCap);
            }
        }
        const float enemyCap = g_enemyCap.load(std::memory_order_relaxed);
        if (enemyCap >= 0.0f)
        {
            const int32_t count = std::min(static_cast<int32_t>(word(rdram, game + kVehicleCount)), 64);
            static uint32_t s_enemyFrames = 0;
            const bool log = ++s_enemyFrames % 500u == 0u;
            std::string line;
            for (int32_t index = 0; index < count; ++index)
            {
                const uint32_t vehicle = game + kVehicles + static_cast<uint32_t>(index) * kVehicleStride;
                const float now = flt(rdram, vehicle + kHealth);
                if (log)
                {
                    char text[48];
                    std::snprintf(text, sizeof(text), " %x:%.0f", vehicle, now);
                    line += text;
                }
                if (word(rdram, vehicle + kDriver) != 1u && now > enemyCap)
                    setFlt(rdram, vehicle + kHealth, enemyCap);
            }
            if (log)
                std::fprintf(stderr, "[tmb:enemies] TheGame %x, %d vehicles:%s\n", game, count, line.c_str());
        }
        // TMB_CHEATS_LOG=1: every 250 frames, each player's health, energy and weapon counts (scripted checks).
        static const bool s_log = std::getenv("TMB_CHEATS_LOG") != nullptr;
        static uint32_t s_logFrames = 0;
        if (s_log && ++s_logFrames % 250u == 0u)
        {
            std::fprintf(stderr, "[tmb:cheats] weapon spends %u (%u not counted: infinite ammo)\n", g_decrements.exchange(0),
                         g_decrementsSkipped.exchange(0));
            for (int player = 0; player < players; ++player)
            {
                const uint32_t car = playerVehicle(rdram, game, player);
                std::string counts;
                for (uint32_t slot = 1; slot <= 8; ++slot)
                    counts += " " + std::to_string(static_cast<int32_t>(word(rdram, car + kWeaponCounts + slot * 4u)));
                std::fprintf(stderr, "[tmb:cheats] P%d car %x: health %.0f/%.0f, energy %d/%d, weapon %d, counts%s\n", player + 1, car,
                             flt(rdram, car + kHealth), flt(rdram, car + kHealthMax), static_cast<int32_t>(word(rdram, car + kEnergy)),
                             static_cast<int32_t>(word(rdram, car + kEnergyMax)), static_cast<int32_t>(word(rdram, car + 0x2140u)), counts.c_str());
            }
        }
    }

    void install(PS2Runtime &runtime)
    {
        g_noAi.store(std::getenv("TMB_NO_AI") && *std::getenv("TMB_NO_AI") == '1');
        g_unlockAll.store(std::getenv("TMB_UNLOCK_ALL") && *std::getenv("TMB_UNLOCK_ALL") == '1');
        if (const char *level = std::getenv("TMB_STORY_LEVEL"); level && *level)
            g_storyStart.store(std::clamp(std::atoi(level), -1, kLevelCount - 1));
        g_infiniteHealth.store(envMask("TMB_PLAYER_GOD"));
        g_infiniteAmmo.store(envMask("TMB_INFINITE_AMMO"));
        g_infiniteEnergy.store(envMask("TMB_INFINITE_ENERGY"));
        g_enemyCap.store(envFloat("TMB_ENEMY_HEALTH_CAP"));
        g_playerCap.store(envFloat("TMB_PLAYER_HEALTH_CAP"));
        runtime.registerFunction(kAiUpdateInputs, &aiUpdateInputsOverride);
        runtime.registerFunction(kDecrementWeaponCount, &decrementWeaponCountOverride);
        runtime.registerFunction(kUiLvlSel, &shellScreenOverride<&uiLvlSel__Fv_0x1976b0>);
        runtime.registerFunction(kUiCarSel, &shellScreenOverride<&uiCarSel__Fv_0x1963b0>);
        runtime.registerFunction(kUiAiSel, &shellScreenOverride<&uiAiSel__Fv_0x1986a8>);
        runtime.registerFunction(kUiMovies, &shellScreenOverride<&uiMovies__Fv_0x190760>);
#if defined(PS2X_ENABLE_DEBUG_UI)
        ps2x::debugui::registerTab("Cheats", &drawTab);
#endif
    }
}
