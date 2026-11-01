// TMB Launcher: what a player double-clicks. Their disc image is checked, the game is built from it on their PC
// (tools/tmb_build.py run by the build kit's Python, in the background), then started. The look is the team's:
// RecompWatcher's PS2 BIOS blue, boot towers and glass (ui.cpp).
//
// The release folder it expects (its own folder, or --release <dir>):
//   TMB Launcher.exe
//   source\   the TMB_Recomp code (tools\tmb_build.py, PS2Recomp...)
//   kit\      the build kit (kit\python\python.exe, the compiler...)
//   build\    tmb_build's work folder (made here)
//   game\     the game folder the build stages (made here): ps2EntryRunner.exe, disc\, disc.iso, the packs
// The player's settings: launcher.ini in the runner's user folder (%APPDATA%\TMB_Recomp, or PS2X_USER_DIR), next to
// the game's video.ini, which the Settings tab writes in the runner's format.
//
// --selftest <iso> --shots <dir>: the whole flow without a hand on it, a screenshot of each screen (see selftest()).

#include "build_model.h"
#include "config.h"
#include "platform.h"
#include "ui.h"

#include "raylib.h"
#include "rlgl.h"
#include "imgui.h"
#include "rlImGui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using ui::S;

// The team's wordmark and the credits under it.
#define TEAM_NAME "The Bozos"
#define TEAM_CREDITS "by boubou666 & Arzaroth"
static const char *kSiteUrl = "https://boubou666.github.io/RecompWatcher/";
static const char *kVersion = "1.0";

// ---- language ----

static bool g_fr = false;
static const char *L(const char *en, const char *fr) { return g_fr ? fr : en; }

static const char *stepTitle(const std::string &id)
{
    if (id == "verify")
        return L("Checking the disc", "Vérification du disque");
    if (id == "extract")
        return L("Copying the disc's files", "Copie des fichiers du disque");
    if (id == "tools")
        return L("Preparing the recompiler", "Préparation du recompilateur");
    if (id == "recompile")
        return L("Recompiling the game's code", "Recompilation du code du jeu");
    if (id == "packs")
        return L("Converting textures and models", "Conversion des textures et modèles");
    if (id == "iop")
        return L("Recompiling sound and controllers", "Recompilation du son et des manettes");
    if (id == "configure")
        return L("Configuring the build", "Configuration de la compilation");
    if (id == "compile")
        return L("Compiling (the long part)", "Compilation (la partie longue)");
    if (id == "stage")
        return L("Assembling the game folder", "Assemblage du dossier du jeu");
    return id.c_str();
}

static std::string fmtDuration(double seconds)
{
    const int s = std::max(0, static_cast<int>(std::lround(seconds)));
    char buf[32];
    if (s >= 3600)
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    else
        std::snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return buf;
}

static std::string fileName(const std::string &path)
{
    return plat::str(plat::path(path).filename());
}

static bool exists(const std::string &path)
{
    std::error_code ec;
    return fs::exists(plat::path(path), ec);
}

// ---- the launcher ----

enum class Tab
{
    Play,
    Settings,
    Maintenance
};

enum class Phase
{
    Incomplete, // source\ or kit\ missing
    Welcome,    // no disc yet
    Ready,      // a disc: prepare or play
    Building,
    Failed,
};

struct Paths
{
    std::string release, source, kit, python, script, buildOut, buildLogs, game, gameExe, elf;
    std::string user, launcherIni, videoIni, logs, crashes;
};

struct App
{
    Paths p;
    LauncherConfig cfg;
    VideoConfig video;
    Phase phase = Phase::Welcome;
    Tab tab = Tab::Play;
    std::string missingPart; // Incomplete: what is not there

    // The disc check (tmb_build.py --check).
    plat::Process checker;
    bool checking = false;
    std::string checkingPath, discError, discErrorDetail;

    // The build.
    plat::Process builder;
    BuildModel model;
    bool building = false, cancelled = false;
    std::string notice; // a line under the Play card (done, stopped)
    unsigned noticeColor = ui::color::good;

    // The game.
    plat::Process game;
    bool gameRunning = false, hidden = false;
    std::set<std::string> crashesBefore;
    std::string crashReport;
    long gameExit = 0;
    bool gameProblem = false;
    std::string launchError;

    bool wantQuit = false, quit = false;
    double time = 0;
    std::string pendingShot; // the selftest's screenshot of this frame

    // -- paths and state

    void init(const std::string &releaseDir)
    {
        const fs::path r = plat::path(releaseDir);
        p.release = plat::str(r);
        p.source = plat::str(r / L"source");
        p.kit = plat::str(r / L"kit");
        p.python = plat::str(r / L"kit" / L"python" / L"python.exe");
        p.script = plat::str(r / L"source" / L"tools" / L"tmb_build.py");
        p.buildOut = plat::str(r / L"build");
        p.buildLogs = plat::str(r / L"build" / L"logs");
        p.game = plat::str(r / L"game");
        p.gameExe = plat::str(r / L"game" / L"ps2EntryRunner.exe");
        p.elf = plat::str(r / L"game" / L"disc" / L"SCUS_971.01");
        p.user = plat::userDirectory();
        const fs::path u = plat::path(p.user);
        p.launcherIni = plat::str(u / L"launcher.ini");
        p.videoIni = plat::str(u / L"video.ini");
        p.logs = plat::str(u / L"logs");
        p.crashes = plat::str(u / L"crashes");

        cfg.load(p.launcherIni);
        g_fr = (cfg.language.empty() ? plat::systemLanguage() : cfg.language) == "fr";
        video.load(p.videoIni);

        if (!exists(p.script))
            missingPart = "source\\tools\\tmb_build.py";
        else if (!exists(p.python) || !exists(plat::str(r / L"kit" / L"kit_manifest.json")))
            missingPart = "kit\\";
        phase = !missingPart.empty() ? Phase::Incomplete : cfg.disc.empty() ? Phase::Welcome : Phase::Ready;
    }

    bool built() const { return exists(p.gameExe) && exists(p.elf); }
    void saveConfig() { cfg.save(p.launcherIni); }

    std::vector<std::string> pythonArgs() const { return {"-I", "-X", "utf8", p.script}; }

    // -- the disc

    void pickDisc()
    {
        const std::string file = plat::pickDiscImage(GetWindowHandle(), L("Select your Twisted Metal: Black disc image",
                                                                        "Choisir l'image disque de Twisted Metal: Black"),
                                                     L("Disc images", "Images disque"), L("All files", "Tous les fichiers"));
        if (!file.empty())
            checkDisc(file);
    }

    void checkDisc(const std::string &file)
    {
        if (checking)
            return;
        std::vector<std::string> args = pythonArgs();
        args.insert(args.end(), {"--check", "--disc", file});
        std::string error;
        discError.clear();
        discErrorDetail.clear();
        if (!checker.start(p.python, args, p.source, true, error))
        {
            discError = L("The disc could not be checked.", "Le disque n'a pas pu être vérifié.");
            discErrorDetail = error;
            return;
        }
        checking = true;
        checkingPath = file;
    }

    void pollCheck()
    {
        if (!checking || checker.running())
            return;
        checker.drain(1000);
        std::string ok, err;
        for (const std::string &line : checker.takeLines())
        {
            if (line.rfind("OK", 0) == 0)
                ok = line;
            else if (line.rfind("ERROR verify ", 0) == 0)
                err = line.substr(13);
            else if (err.empty() && !line.empty() && line.rfind("INFO", 0) != 0)
                err = line;
        }
        checking = false;
        if (!ok.empty() && checker.exitCode() == 0)
        {
            cfg.disc = checkingPath; // a new disc: tmb_build's verify key changes, the rest follows
            saveConfig();
            phase = Phase::Ready;
            tab = Tab::Play;
            notice.clear();
            return;
        }
        // In plain words, with tmb_build's own message under it.
        discErrorDetail = err;
        if (err.find("European") != std::string::npos)
            discError = L("This is the European disc. Only the USA disc (SCUS-97101) is supported.",
                          "C'est le disque européen. Seul le disque américain (SCUS-97101) est pris en charge.");
        else if (err.find("boots") != std::string::npos)
            discError = L("This disc image is another game.", "Cette image disque est un autre jeu.");
        else if (err.find("SHA-1") != std::string::npos)
            discError = L("This copy differs from the USA disc: modified or damaged. Make a fresh copy of your disc.",
                          "Cette copie diffère du disque américain : modifiée ou abîmée. Refaites une copie de votre disque.");
        else if (err.find("does not exist") != std::string::npos)
            discError = L("This file is no longer there.", "Ce fichier n'existe plus.");
        else
            discError = L("This file is not a PS2 disc image.", "Ce fichier n'est pas une image disque PS2.");
    }

    // -- the build

    void startBuild()
    {
        if (building || cfg.disc.empty())
            return;
        std::vector<std::string> args = pythonArgs();
        args.insert(args.end(), {"--disc", cfg.disc, "--out", p.buildOut, "--game", p.game});
        model.reset(plat::now());
        std::string error;
        cancelled = false;
        notice.clear();
        if (!builder.start(p.python, args, p.source, true, error))
        {
            model.errorStep = "verify";
            model.errorMessage = L("The build could not start: ", "La préparation n'a pas pu démarrer : ") + error;
            phase = Phase::Failed;
            return;
        }
        building = true;
        cfg.buildPending = true;
        saveConfig();
        phase = Phase::Building;
        tab = Tab::Play;
    }

    void cancelBuild()
    {
        if (!building)
            return;
        cancelled = true;
        builder.kill();
    }

    void pollBuild()
    {
        if (!building)
            return;
        const double t = plat::now();
        for (const std::string &line : builder.takeLines())
            model.feed(line, t);
        if (builder.running())
            return;
        builder.drain(2000);
        for (const std::string &line : builder.takeLines())
            model.feed(line, t);
        building = false;
        const long code = builder.exitCode();
        if (model.finished && code == 0)
        {
            cfg.built = true;
            cfg.buildPending = false;
            cfg.lastBuildSeconds = t - model.startedAt;
            saveConfig();
            phase = Phase::Ready;
            notice = std::string(L("The game is ready. Prepared in ", "Le jeu est prêt. Préparé en ")) + fmtDuration(cfg.lastBuildSeconds) + ".";
            noticeColor = ui::color::good;
        }
        else if (cancelled)
        {
            phase = Phase::Ready;
            notice = L("Stopped. Preparing continues from there next time.",
                       "Arrêté. La préparation reprendra à partir de là.");
            noticeColor = ui::color::wip;
        }
        else
        {
            if (model.errorMessage.empty())
            {
                model.errorStep = model.current() >= 0 ? model.steps[model.current()].id : "";
                model.errorMessage = std::string(L("The build stopped unexpectedly (exit code ", "La préparation s'est arrêtée (code ")) +
                                     std::to_string(code) + ").";
            }
            phase = Phase::Failed;
        }
    }

    std::string stepLog() const
    {
        const std::string step = model.errorStep.empty() ? "verify" : model.errorStep;
        return plat::str(plat::path(p.buildLogs) / plat::path(step + ".log"));
    }

    std::string details() const
    {
        std::string out = std::string("TMB Launcher ") + kVersion + "\n";
        out += "Step: " + model.errorStep + "\nMessage: " + model.errorMessage + "\n";
        out += "Disc: " + cfg.disc + "\nRelease folder: " + p.release + "\n\n--- tmb_build output ---\n";
        const size_t from = model.details.size() > 60 ? model.details.size() - 60 : 0;
        for (size_t i = from; i < model.details.size(); ++i)
            out += model.details[i] + "\n";
        std::ifstream log(plat::path(stepLog()));
        if (log)
        {
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(log, line))
                lines.push_back(line);
            out += "\n--- " + fileName(stepLog()) + " (last lines) ---\n";
            for (size_t i = lines.size() > 60 ? lines.size() - 60 : 0; i < lines.size(); ++i)
                out += lines[i] + "\n";
        }
        return out;
    }

    // -- the game

    void play()
    {
        if (gameRunning || building || !built())
            return;
        crashesBefore.clear();
        for (const auto &f : plat::listFiles(p.crashes, ".txt"))
            crashesBefore.insert(f.first);
        crashReport.clear();
        gameProblem = false;
        launchError.clear();
        notice.clear();
        std::string error;
        // Nothing to set: the runner's defaults are the native port, and it finds the packs and disc.iso beside it.
        if (!game.start(p.gameExe, {p.elf}, p.game, false, error))
        {
            launchError = error;
            return;
        }
        gameRunning = true;
        SetWindowState(FLAG_WINDOW_HIDDEN);
        hidden = true;
    }

    void pollGame()
    {
        if (!gameRunning || game.running())
            return;
        gameRunning = false;
        gameExit = game.exitCode();
        uint64_t newest = 0;
        for (const auto &f : plat::listFiles(p.crashes, ".txt"))
            if (!crashesBefore.count(f.first) && f.second >= newest)
            {
                newest = f.second;
                crashReport = f.first;
            }
        gameProblem = crashReport.empty() && gameExit != 0;
        ClearWindowState(FLAG_WINDOW_HIDDEN);
        hidden = false;
        SetWindowFocused();
    }
};

// ---- drawing ----

static void heading(const char *s, unsigned rgb = ui::color::ink)
{
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ui::glowText(pos, s, 30, rgb, 0.5f, true, 0.5f);
    ImGui::Dummy(ui::measure(s, 30, 0.5f));
}

static void gap(float px) { ImGui::Dummy(ImVec2(1, S(px))); }

static void drawHeader(App &app, float W)
{
    const float gutter = S(44);
    ImGui::SetCursorScreenPos(ImVec2(gutter, S(26)));
    ui::glowText(ImGui::GetCursorScreenPos(), TEAM_NAME, 24, ui::color::ink, 5.0f, true, 0.9f);
    const ImVec2 mark = ui::measure(TEAM_NAME, 24, 5.0f);
    ui::glowText(ImVec2(gutter, S(26) + mark.y + S(2)), TEAM_CREDITS, 14, ui::color::inkDim, 1.6f, false, 0);

    // The tabs and the language, on the right.
    if (app.phase == Phase::Incomplete || app.phase == Phase::Welcome)
    {
        ImGui::SetCursorScreenPos(ImVec2(W - gutter - S(110), S(30)));
        if (ui::pill(g_fr ? "English##lang" : "Français##lang", false, 16))
        {
            g_fr = !g_fr;
            app.cfg.language = g_fr ? "fr" : "en";
            app.saveConfig();
        }
        return;
    }
    const char *names[3] = {L("Play", "Jouer"), L("Settings", "Réglages"), L("Maintenance", "Maintenance")};
    float total = 0;
    for (const char *n : names)
        total += ui::measure(n, 17).x + S(30) + S(8);
    ImGui::SetCursorScreenPos(ImVec2(W - gutter - total, S(30)));
    for (int i = 0; i < 3; ++i)
    {
        if (i)
            ImGui::SameLine(0, S(8));
        char id[64];
        std::snprintf(id, sizeof(id), "%s##tab%d", names[i], i);
        if (ui::pill(id, static_cast<int>(app.tab) == i, 17))
            app.tab = static_cast<Tab>(i);
    }
}

static void drawTitle(float x, float y)
{
    ui::glowText(ImVec2(x, y), "Twisted Metal: Black", 52, ui::color::ink, 1.0f, true, 1.0f);
    const float h = ui::measure("Twisted Metal: Black", 52, 1.0f).y;
    ui::glowText(ImVec2(x + S(2), y + h + S(2)), L("PC port \xE2\x80\x94 your own disc required", "Portage PC \xE2\x80\x94 votre propre disque requis"),
                 19, ui::color::inkDim, 1.2f, false, 0);
}

static void noteBox(const char *title, const std::string &detail, unsigned rgb, float width)
{
    // Measured first, then drawn: it sits inside a card, whose draw list is already split in channels.
    const float padX = S(18), padY = S(12), wrap = width - 2 * padX;
    const ImVec2 titleSz = ui::fonts.text->CalcTextSizeA(S(19), FLT_MAX, wrap, title);
    const ImVec2 detailSz = detail.empty() ? ImVec2(0, 0) : ui::fonts.mono->CalcTextSizeA(S(15), FLT_MAX, wrap, detail.c_str());
    const float h = padY * 2 + titleSz.y + (detail.empty() ? 0 : S(6) + detailSz.y);
    const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + width, a.y + h);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a, b, ui::rgba(rgb, 0.08f), S(12));
    dl->AddRect(a, b, ui::rgba(rgb, 0.45f), S(12), 0, S(1));
    dl->AddRectFilled(a, ImVec2(a.x + S(4), b.y), ui::rgba(rgb, 0.9f), S(12), ImDrawFlags_RoundCornersLeft);
    dl->AddText(ui::fonts.text, S(19), ImVec2(a.x + padX, a.y + padY), ui::rgba(rgb), title, nullptr, wrap);
    if (!detail.empty())
        dl->AddText(ui::fonts.mono, S(15), ImVec2(a.x + padX, a.y + padY + titleSz.y + S(6)), ui::rgba(ui::color::inkDim),
                    detail.c_str(), nullptr, wrap);
    ImGui::Dummy(ImVec2(width, h));
}

static void drawWelcome(App &app, float inner)
{
    heading(L("Welcome", "Bienvenue"));
    gap(4);
    ui::textWrapped(L("This launcher turns your own copy of Twisted Metal: Black into a PC game. Nothing from the game "
                      "comes with it: the game is built here, on your PC, from your disc.",
                      "Ce lanceur transforme votre propre copie de Twisted Metal: Black en jeu PC. Rien du jeu n'est "
                      "fourni : le jeu est construit ici, sur votre PC, à partir de votre disque."),
                    19, ui::color::ink, 0.92f);
    gap(2);
    ui::textWrapped(L("You need a disc image of the USA release (SCUS-97101): a .iso, or a .bin with its .cue. "
                      "The European disc is not supported.",
                      "Il vous faut une image disque de la version américaine (SCUS-97101) : un .iso, ou un .bin avec "
                      "son .cue. Le disque européen n'est pas pris en charge."),
                    19, ui::color::inkDim);
    gap(10);
    if (app.checking)
    {
        const ImVec2 c = ImGui::GetCursorScreenPos();
        ui::spinner(ImVec2(c.x + S(14), c.y + S(14)), 11, app.time);
        ImGui::SetCursorScreenPos(ImVec2(c.x + S(40), c.y + S(2)));
        ui::text(L("Checking the disc\xE2\x80\xA6", "Vérification du disque\xE2\x80\xA6"), 20, ui::color::ink);
        ui::text(fileName(app.checkingPath).c_str(), 16, ui::color::inkFaint);
    }
    else
    {
        if (ui::button(L("Select your disc image\xE2\x80\xA6", "Choisir votre image disque\xE2\x80\xA6"), ImVec2(0, S(56)),
                       ui::Kind::Primary, true, 21))
            app.pickDisc();
        ImGui::SetItemDefaultFocus();
    }
    if (!app.discError.empty())
    {
        gap(6);
        noteBox(app.discError.c_str(), app.discErrorDetail, ui::color::early, inner);
    }
}

static void drawBuilding(App &app, float inner)
{
    const BuildModel &m = app.model;
    heading(L("Preparing the game", "Préparation du jeu"));
    ui::textWrapped(L("This happens once. You can keep using your PC meanwhile.",
                      "Cela n'arrive qu'une fois. Vous pouvez continuer à utiliser votre PC."),
                    18, ui::color::inkDim);
    gap(8);
    const float fraction = static_cast<float>(m.fraction());
    const float barW = inner - S(90);
    ui::progressBar(fraction, ImVec2(barW, S(18)), app.time);
    ImGui::SameLine(0, S(16));
    char pct[16];
    std::snprintf(pct, sizeof(pct), "%d %%", static_cast<int>(fraction * 100));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(6));
    ui::text(pct, 22, ui::color::ink);
    const double now = plat::now();
    std::string times = std::string(L("Elapsed ", "Écoulé ")) + fmtDuration(now - m.startedAt);
    const double left = m.secondsLeft(now);
    if (left >= 0)
    {
        // Ninja's count runs ahead of the time (the small files compile first): a margin.
        const int minutes = static_cast<int>(std::ceil(left * 1.3 / 60.0));
        char buf[96];
        if (minutes <= 1)
            std::snprintf(buf, sizeof(buf), "%s", L("about a minute left", "environ une minute restante"));
        else
            std::snprintf(buf, sizeof(buf), L("about %d min left", "environ %d min restantes"), minutes);
        times += std::string("  \xC2\xB7  ") + buf;
    }
    ui::text(times.c_str(), 17, ui::color::inkDim);
    gap(8);

    // The steps, in two columns.
    const float colW = (inner - S(20)) / 2;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float rowH = S(34);
    const int perCol = 5;
    for (size_t i = 0; i < m.steps.size(); ++i)
    {
        const BuildStep &s = m.steps[i];
        const float x = origin.x + (i < perCol ? 0 : colW + S(20));
        const float y = origin.y + rowH * (i % perCol);
        const int state = static_cast<int>(s.state);
        ui::statusDot(ImVec2(x + S(10), y + rowH / 2), state, app.time);
        const bool running = s.state == BuildStep::State::Running;
        const unsigned c = running ? ui::color::ink : s.state == BuildStep::State::Pending ? ui::color::inkFaint
                                                       : s.state == BuildStep::State::Failed ? ui::color::early
                                                                                             : ui::color::inkDim;
        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddText(ui::fonts.text, S(18), ImVec2(x + S(30), y + (rowH - S(18)) / 2 - S(2)), ui::rgba(c), stepTitle(s.id));
        char right[32] = "";
        if (running && s.percent > 0)
            std::snprintf(right, sizeof(right), "%d %%", s.percent);
        else if (s.state == BuildStep::State::Done && s.seconds >= 1)
            std::snprintf(right, sizeof(right), "%s", fmtDuration(s.seconds).c_str());
        else if (s.state == BuildStep::State::Skipped)
            std::snprintf(right, sizeof(right), "%s", L("done before", "déjà fait"));
        if (*right)
        {
            const float tw = ui::fonts.text->CalcTextSizeA(S(15), FLT_MAX, 0, right).x;
            dl->AddText(ui::fonts.text, S(15), ImVec2(x + colW - tw, y + (rowH - S(15)) / 2 - S(1)),
                        ui::rgba(running ? ui::color::cross : ui::color::inkFaint), right);
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rowH * perCol + S(10)));
    if (ui::button(app.cancelled ? L("Stopping\xE2\x80\xA6", "Arrêt\xE2\x80\xA6") : L("Stop", "Arrêter"), ImVec2(S(150), S(48)),
                   ui::Kind::Danger, !app.cancelled))
        app.cancelBuild();
    ImGui::SameLine(0, S(16));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(12));
    ui::text(L("Stopping is safe: it continues from there next time.", "Arrêter est sans risque : la suite reprendra plus tard."),
             16, ui::color::inkFaint);
}

static void drawFailed(App &app, float inner)
{
    heading(L("Preparing stopped", "La préparation s'est arrêtée"), ui::color::early);
    gap(2);
    std::string at = std::string(L("At: ", "Étape : ")) + stepTitle(app.model.errorStep);
    ui::text(at.c_str(), 18, ui::color::inkDim);
    gap(4);
    const std::string &msg = app.model.errorMessage;
    const bool space = msg.find("free space") != std::string::npos || msg.find("No space") != std::string::npos;
    const bool gone = app.model.errorStep == "verify" && msg.find("does not exist") != std::string::npos;
    noteBox(space  ? L("The drive is full. Free some space and try again.", "Le disque est plein. Libérez de la place et réessayez.")
            : gone ? L("The disc image is no longer where it was. Select it again.",
                       "L'image disque n'est plus à son emplacement. Choisissez-la à nouveau.")
                   : L("Try again. If it stops at the same place, copy the details and send them to us.",
                       "Réessayez. Si cela s'arrête au même endroit, copiez les détails et envoyez-les nous."),
            msg, ui::color::early, inner);
    gap(10);
    if (ui::button(L("Try again", "Réessayer"), ImVec2(0, S(52)), ui::Kind::Primary))
        app.startBuild();
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine(0, S(12));
    if (ui::button(L("Open log", "Ouvrir le journal"), ImVec2(0, S(52))))
        plat::open(exists(app.stepLog()) ? app.stepLog() : app.p.buildLogs);
    ImGui::SameLine(0, S(12));
    static double copiedAt = -10;
    if (ui::button(app.time - copiedAt < 2 ? L("Copied", "Copié") : L("Copy details", "Copier les détails"), ImVec2(0, S(52))))
    {
        ImGui::SetClipboardText(app.details().c_str());
        copiedAt = app.time;
    }
    ImGui::SameLine(0, S(12));
    if (ui::button(L("Change disc image", "Changer d'image disque"), ImVec2(0, S(52))))
        app.pickDisc();
}

static void drawReady(App &app, float inner)
{
    const bool built = app.built();
    if (built && !app.cfg.buildPending)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        if (ui::button(L("Play##play", "Jouer##play"), ImVec2(S(300), S(84)), ui::Kind::Primary, !app.gameRunning, 34))
            app.play();
        ImGui::SetItemDefaultFocus();
        ImGui::SetCursorScreenPos(ImVec2(start.x + S(330), start.y + S(4)));
        ImGui::BeginGroup();
        ui::text(L("Ready to play", "Prêt à jouer"), 22, ui::color::good);
        ui::text(fileName(app.cfg.disc).c_str(), 16, ui::color::inkFaint);
        ImGui::EndGroup();
        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + S(84)));
        gap(14);
        // F2 as a key cap.
        const ImVec2 k = ImGui::GetCursorScreenPos();
        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(k, ImVec2(k.x + S(38), k.y + S(30)), IM_COL32(30, 50, 120, 110), S(7));
        dl->AddRect(k, ImVec2(k.x + S(38), k.y + S(30)), ui::rgba(ui::color::cross, 0.6f), S(7), 0, S(1));
        dl->AddText(ui::fonts.text, S(16), ImVec2(k.x + S(10), k.y + S(5)), ui::rgba(ui::color::cross), "F2");
        ImGui::SetCursorScreenPos(ImVec2(k.x + S(50), k.y + S(3)));
        ui::text(L("In the game, F2 opens the settings: controls, video and cheats.",
                   "En jeu, F2 ouvre les réglages : commandes, vidéo et astuces."),
                 18, ui::color::inkDim);
    }
    else
    {
        heading(app.cfg.buildPending ? L("Preparing was stopped", "La préparation a été interrompue")
                                     : L("Your disc is ready", "Votre disque est prêt"));
        std::string line = std::string(L("USA disc (SCUS-97101)", "Disque américain (SCUS-97101)")) + "  \xC2\xB7  " + fileName(app.cfg.disc);
        ui::text(line.c_str(), 17, ui::color::good);
        gap(4);
        ui::textWrapped(app.cfg.buildPending
                            ? L("It continues where it left off.", "Elle reprend là où elle s'était arrêtée.")
                            : L("The game is built once, from your disc. It takes a few minutes and about 10 GB of free space.",
                                "Le jeu est construit une seule fois, à partir de votre disque. Cela prend quelques minutes "
                                "et environ 10 Go d'espace libre."),
                        19, ui::color::ink, 0.92f);
        const double freeGb = plat::freeBytes(app.p.release) / 1073741824.0;
        if (freeGb > 0 && freeGb < 10)
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf), L("Only %.1f GB free on this drive.", "Seulement %.1f Go libres sur ce disque."), freeGb);
            ui::text(buf, 17, ui::color::wip);
        }
        gap(10);
        if (!exists(app.cfg.disc))
        {
            noteBox(L("The disc image is no longer where it was. Select it again.",
                      "L'image disque n'est plus à son emplacement. Choisissez-la à nouveau."),
                    app.cfg.disc, ui::color::wip, inner);
            gap(8);
            if (ui::button(L("Select your disc image\xE2\x80\xA6", "Choisir votre image disque\xE2\x80\xA6"), ImVec2(0, S(56)), ui::Kind::Primary))
                app.pickDisc();
        }
        else
        {
            if (ui::button(app.cfg.buildPending ? L("Continue preparing", "Reprendre la préparation") : L("Prepare the game", "Préparer le jeu"),
                           ImVec2(S(300), S(64)), ui::Kind::Primary, true, 24))
                app.startBuild();
            ImGui::SetItemDefaultFocus();
        }
        if (app.checking)
            ui::text(L("Checking the disc\xE2\x80\xA6", "Vérification du disque\xE2\x80\xA6"), 17, ui::color::inkDim);
        if (!app.discError.empty())
        {
            gap(6);
            noteBox(app.discError.c_str(), app.discErrorDetail, ui::color::early, inner);
        }
    }
    if (!app.notice.empty())
    {
        gap(10);
        ui::text(app.notice.c_str(), 18, app.noticeColor);
    }
    if (!app.crashReport.empty() || app.gameProblem || !app.launchError.empty())
    {
        gap(10);
        std::string detail = !app.crashReport.empty() ? app.crashReport : !app.launchError.empty() ? app.launchError
                                                                                                      : "exit code " + std::to_string(app.gameExit);
        noteBox(!app.launchError.empty() ? L("The game could not start.", "Le jeu n'a pas pu démarrer.")
                                         : L("The game closed unexpectedly.", "Le jeu s'est fermé de façon inattendue."),
                detail, ui::color::early, inner);
        gap(8);
        if (!app.crashReport.empty())
        {
            if (ui::button(L("Open crash report", "Ouvrir le rapport de plantage"), ImVec2(0, S(48)), ui::Kind::Danger))
                plat::open(app.crashReport);
            ImGui::SameLine(0, S(12));
        }
        if (ui::button(L("Open log", "Ouvrir le journal"), ImVec2(0, S(48))))
            plat::open(app.p.logs);
    }
}

static void drawSettings(App &app, float inner)
{
    heading(L("Settings", "Réglages"));
    gap(2);
    ui::textWrapped(L("Controls, video and cheats are in the game: press F2 while playing. Here, the window the game opens with.",
                      "Les commandes, la vidéo et les astuces sont dans le jeu : appuyez sur F2 en jouant. Ici, la fenêtre "
                      "avec laquelle le jeu démarre."),
                    18, ui::color::inkDim);
    gap(10);
    VideoConfig &v = app.video;
    bool changed = false;
    ui::text(L("Window", "Fenêtre"), 17, ui::color::inkFaint);
    const char *modes[3] = {"windowed", "fullscreen", "borderless"};
    const char *modeNames[3] = {L("Windowed", "Fenêtré"), L("Full screen", "Plein écran"), L("Borderless window", "Fenêtre sans bordure")};
    for (int i = 0; i < 3; ++i)
    {
        if (i)
            ImGui::SameLine(0, S(8));
        char id[64];
        std::snprintf(id, sizeof(id), "%s##mode%d", modeNames[i], i);
        if (ui::pill(id, v.mode == modes[i], 18))
        {
            v.mode = modes[i];
            changed = true;
        }
    }
    gap(6);
    ui::text(L("Resolution", "Résolution"), 17, ui::color::inkFaint);
    const int monitor = GetCurrentMonitor();
    const auto sizes = offeredResolutions(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    auto label = [&](int w, int h)
    {
        char buf[96];
        if (w == 0)
            std::snprintf(buf, sizeof(buf), L("Game default (640 \xC3\x97 448)", "Par défaut (640 \xC3\x97 448)"));
        else
            std::snprintf(buf, sizeof(buf), "%d \xC3\x97 %d", w, h);
        return std::string(buf);
    };
    if (v.mode == "borderless")
        ui::text(L("The monitor's own size.", "La taille de l'écran."), 18, ui::color::inkDim);
    else
    {
        ImGui::SetNextItemWidth(S(320));
        ImGui::PushFont(ui::fonts.text, S(18));
        if (ImGui::BeginCombo("##resolution", label(v.width, v.height).c_str()))
        {
            if (ImGui::Selectable(label(0, 0).c_str(), v.width == 0))
            {
                v.width = v.height = 0;
                changed = true;
            }
            for (const auto &[w, h] : sizes)
                if (ImGui::Selectable(label(w, h).c_str(), v.width == w && v.height == h))
                {
                    v.width = w;
                    v.height = h;
                    changed = true;
                }
            ImGui::EndCombo();
        }
        ImGui::PopFont();
    }
    if (changed)
    {
        VideoConfig out = v;
        if (out.width == 0) // the runner's own default size, written out
        {
            out.width = 640;
            out.height = 448;
        }
        out.save(app.p.videoIni);
        app.video.exists = true;
    }
    if (v.exists)
        ui::text(L("Saved. The game uses it the next time it starts.", "Enregistré. Le jeu l'utilisera au prochain démarrage."), 16,
                 ui::color::good);
    gap(12);
    ui::text(L("Launcher", "Lanceur"), 17, ui::color::inkFaint);
    if (ui::pill("English##en", !g_fr, 18))
    {
        g_fr = false;
        app.cfg.language = "en";
        app.saveConfig();
    }
    ImGui::SameLine(0, S(8));
    if (ui::pill("Français##fr", g_fr, 18))
    {
        g_fr = true;
        app.cfg.language = "fr";
        app.saveConfig();
    }
    ImGui::SameLine(0, S(28));
    char id[96];
    std::snprintf(id, sizeof(id), "%s##motion", app.cfg.motion ? L("Background motion: on", "Animation du fond : oui")
                                                               : L("Background motion: off", "Animation du fond : non"));
    if (ui::pill(id, app.cfg.motion, 18))
    {
        app.cfg.motion = !app.cfg.motion;
        app.saveConfig();
    }
    (void)inner;
}

static void drawMaintenance(App &app, float inner)
{
    heading(L("Maintenance", "Maintenance"));
    gap(6);
    const float bw = S(270);
    auto row = [&](const char *label, const char *what, bool enabled, ui::Kind kind = ui::Kind::Normal)
    {
        const bool pressed = ui::button(label, ImVec2(bw, S(46)), kind, enabled, 18);
        ImGui::SameLine(0, S(18));
        const float y = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(y + S(11));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner - bw - S(20));
        ui::textWrapped(what, 17, ui::color::inkDim);
        ImGui::PopTextWrapPos();
        gap(2);
        return pressed;
    };
    const bool idle = !app.building && !app.gameRunning && !app.cfg.disc.empty();
    if (row(L("Rebuild", "Reconstruire"), L("After an update: builds the game again (only what changed).",
                                            "Après une mise à jour : reconstruit le jeu (seulement ce qui a changé)."),
            idle))
        app.startBuild();
    if (row(L("Change disc image", "Changer d'image disque"), L("Use another copy of your disc.", "Utiliser une autre copie de votre disque."),
            !app.building))
        app.pickDisc();
    std::string saves = std::string(L("Memory cards and settings: ", "Cartes mémoire et réglages : ")) + app.p.user;
    if (row(L("Open saves folder", "Dossier des sauvegardes"), saves.c_str(), true))
        plat::open(app.p.user);
    if (row(L("Open game logs", "Journaux du jeu"), L("What the game wrote, and crash reports.", "Ce que le jeu a écrit, et les rapports de plantage."),
            true))
        plat::open(exists(app.p.logs) ? app.p.logs : app.p.user);
    if (row(L("Open build logs", "Journaux de préparation"), L("One file per step of preparing the game.", "Un fichier par étape de la préparation."),
            exists(app.p.buildLogs)))
        plat::open(app.p.buildLogs);
}

static void drawIncomplete(App &app)
{
    heading(L("Some files are missing", "Des fichiers manquent"), ui::color::early);
    gap(4);
    std::string text = std::string(L("The launcher did not find ", "Le lanceur ne trouve pas ")) + app.missingPart +
                       L(" next to it. Unzip the whole download again, keeping its folders together.",
                         " à côté de lui. Décompressez à nouveau tout le téléchargement, en gardant ses dossiers ensemble.");
    ui::textWrapped(text.c_str(), 19, ui::color::ink);
    ui::text(app.p.release.c_str(), 15, ui::color::inkFaint);
}

static void drawFrame(App &app)
{
    const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    drawHeader(app, W);
    const float gutter = S(44);
    drawTitle(gutter, S(100));

    const float cardW = std::min(W - 2 * gutter, S(1040));
    const float pad = 30;
    const float inner = cardW - 2 * S(pad);
    ImGui::SetCursorScreenPos(ImVec2(gutter, S(208)));
    ui::beginCard(cardW, pad);
    switch (app.phase)
    {
    case Phase::Incomplete:
        drawIncomplete(app);
        break;
    case Phase::Welcome:
        drawWelcome(app, inner);
        break;
    default:
        if (app.tab == Tab::Settings)
            drawSettings(app, inner);
        else if (app.tab == Tab::Maintenance)
            drawMaintenance(app, inner);
        else if (app.phase == Phase::Building)
            drawBuilding(app, inner);
        else if (app.phase == Phase::Failed)
            drawFailed(app, inner);
        else
            drawReady(app, inner);
        break;
    }
    ui::endCard();

    // The footer: the site, the version.
    ImGui::SetCursorScreenPos(ImVec2(gutter - S(2), H - S(46)));
    if (ui::button("RecompWatcher \xE2\x80\xBA##site", ImVec2(0, 0), ui::Kind::Quiet, true, 16))
        plat::open(kSiteUrl);
    ImGui::SameLine(0, S(16));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(2));
    std::string ver = std::string("TMB_Recomp  \xC2\xB7  ") + L("launcher ", "lanceur ") + kVersion;
    ui::text(ver.c_str(), 15, ui::color::inkFaint);
    if (app.building && app.tab != Tab::Play)
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), L("Preparing\xE2\x80\xA6 %d %%", "Préparation\xE2\x80\xA6 %d %%"), static_cast<int>(app.model.fraction() * 100));
        const float tw = ui::measure(buf, 16).x;
        ImGui::GetWindowDrawList()->AddText(ui::fonts.text, S(16), ImVec2(W - gutter - tw, H - S(42)), ui::rgba(ui::color::cross), buf);
    }

    // Esc: back to Play from the other tabs.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && app.tab != Tab::Play)
        app.tab = Tab::Play;

    // Closing while the build runs: ask.
    if (app.wantQuit)
    {
        ImGui::OpenPopup("##quit");
        app.wantQuit = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(28), S(24)));
    ImGui::SetNextWindowPos(ImVec2(W / 2, H / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##quit", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize))
    {
        heading(L("Stop preparing and quit?", "Arrêter la préparation et quitter ?"));
        ui::text(L("It continues from there next time.", "Elle reprendra à partir de là."), 18, ui::color::inkDim);
        gap(8);
        if (ui::button(L("Keep going", "Continuer"), ImVec2(S(200), S(48)), ui::Kind::Primary))
            ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine(0, S(12));
        if (ui::button(L("Stop and quit", "Arrêter et quitter"), ImVec2(S(220), S(48)), ui::Kind::Danger))
        {
            app.cancelBuild();
            app.quit = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

// ---- the selftest ----

struct Selftest
{
    std::string iso, shots, wrong;
    bool errorDemo = false, cancel = false;
    double playSeconds = 40, crashSeconds = 0;
    std::FILE *log = nullptr;

    struct Step
    {
        std::string name;
        std::function<bool(App &, double)> run; // (app, seconds in this step) -> done
    };
    std::vector<Step> steps;
    size_t index = 0;
    double stepStart = 0;
    double buildStart = 0;

    void say(const char *fmt, const std::string &a = "", const std::string &b = "")
    {
        if (!log)
            return;
        std::fprintf(log, "[%8.1f] ", plat::now() - t0);
        std::fprintf(log, fmt, a.c_str(), b.c_str());
        std::fputc('\n', log);
        std::fflush(log);
    }
    double t0 = 0;

    // A step that takes a screenshot once the screen has settled.
    Step shot(const std::string &name, double settle = 0.8)
    {
        return {"shot " + name, [this, name, settle, asked = false](App &app, double t) mutable
                {
                    if (t < settle)
                        return false;
                    if (!asked)
                    {
                        app.pendingShot = plat::str(plat::path(shots) / plat::path(name + ".png"));
                        asked = true;
                        return false;
                    }
                    if (!app.pendingShot.empty())
                        return false;
                    say("screenshot %s", name);
                    return true;
                }};
    }

    void build()
    {
        steps.push_back(shot("01-welcome", 1.5));
        steps.push_back({"french welcome", [](App &app, double) { g_fr = true; (void)app; return true; }});
        steps.push_back(shot("02-welcome-fr"));
        steps.push_back({"english", [](App &, double) { g_fr = false; return true; }});
        if (!wrong.empty())
        {
            steps.push_back({"check the wrong disc", [this](App &app, double t)
                             {
                                 if (t == 0.0)
                                     app.checkDisc(wrong);
                                 return t > 0.2 && !app.checking;
                             }});
            steps.push_back(shot("03-wrong-disc"));
        }
        if (errorDemo)
        {
            // A hard link to the ISO, checked, then removed before the build: tmb_build's verify fails.
            const std::string link = plat::str(plat::path(shots) / L"vanishing-disc.iso");
            steps.push_back({"check a disc that then vanishes", [this, link](App &app, double t)
                             {
                                 if (t == 0.0)
                                 {
                                     std::error_code ec;
                                     fs::remove(plat::path(link), ec);
                                     fs::create_hard_link(plat::path(iso), plat::path(link), ec);
                                     app.checkDisc(link);
                                 }
                                 if (t < 0.2 || app.checking)
                                     return false;
                                 std::error_code ec;
                                 fs::remove(plat::path(link), ec); // the link only; the ISO stays
                                 say("disc check: %s", app.discError.empty() ? "ok" : app.discError);
                                 app.startBuild();
                                 return true;
                             }});
            steps.push_back({"wait for the error", [](App &app, double) { return !app.building; }});
            steps.push_back(shot("04-build-error"));
        }
        steps.push_back({"check the disc", [this](App &app, double t)
                         {
                             if (t == 0.0)
                                 app.checkDisc(iso);
                             if (t < 0.2 || app.checking)
                                 return false;
                             say("disc check: %s", app.discError.empty() ? "ok" : app.discError);
                             return true;
                         }});
        steps.push_back(shot("05-ready"));
        steps.push_back({"build", [this, seen = std::set<std::string>(), cancelled = false, half = false,
                                   stoppedShot = 0](App &app, double t) mutable
                         {
                             if (t == 0.0)
                             {
                                 buildStart = plat::now();
                                 app.startBuild();
                                 say("build started");
                                 return false;
                             }
                             if (!app.pendingShot.empty())
                                 return false;
                             if (stoppedShot == 1) // the stopped screen was shot: resume
                             {
                                 stoppedShot = 2;
                                 say("build resumed");
                                 app.startBuild();
                                 return false;
                             }
                             if (app.building)
                             {
                                 const int cur = app.model.current();
                                 if (cur >= 0)
                                 {
                                     const BuildStep &s = app.model.steps[cur];
                                     if (!seen.count(s.id) && plat::now() - s.startedAt > 1.0)
                                     {
                                         seen.insert(s.id);
                                         say("step %s", s.id);
                                         app.pendingShot = plat::str(plat::path(shots) / plat::path("06-build-" + std::to_string(cur + 1) + "-" + s.id + ".png"));
                                     }
                                     if (s.id == "compile" && cancel && !cancelled && s.percent >= 40)
                                     {
                                         cancelled = true;
                                         say("stop at compile %s %%", std::to_string(s.percent));
                                         app.cancelBuild();
                                     }
                                     if (s.id == "compile" && !half && s.percent >= 50)
                                     {
                                         half = true;
                                         app.pendingShot = plat::str(plat::path(shots) / L"07-build-compile-half.png");
                                     }
                                 }
                                 return false;
                             }
                             if (app.phase == Phase::Ready && cancelled && stoppedShot == 0)
                             {
                                 app.pendingShot = plat::str(plat::path(shots) / L"08-stopped.png");
                                 stoppedShot = 1;
                                 return false;
                             }
                             for (const BuildStep &s : app.model.steps)
                                 say("  %s", s.id + " " + (s.state == BuildStep::State::Skipped ? std::string("skipped") : std::to_string(s.seconds) + " s"));
                             say("build ended after %s s: %s", std::to_string(plat::now() - buildStart),
                                 app.phase == Phase::Ready ? "ready" : "failed: " + app.model.errorMessage);
                             return true;
                         }});
        steps.push_back(shot("09-built", 1.0));
        steps.push_back({"settings", [](App &app, double) { app.tab = Tab::Settings; return true; }});
        steps.push_back(shot("10-settings"));
        steps.push_back({"video change", [](App &app, double)
                         {
                             app.video.mode = "windowed";
                             app.video.width = 1280;
                             app.video.height = 960;
                             VideoConfig v = app.video;
                             v.save(app.p.videoIni);
                             app.video.exists = true;
                             return true;
                         }});
        steps.push_back(shot("11-settings-saved"));
        steps.push_back({"maintenance", [](App &app, double) { app.tab = Tab::Maintenance; return true; }});
        steps.push_back(shot("12-maintenance"));
        steps.push_back({"french", [](App &app, double) { g_fr = true; app.tab = Tab::Play; return true; }});
        steps.push_back(shot("13-play-fr"));
        steps.push_back({"french settings", [](App &app, double) { app.tab = Tab::Settings; return true; }});
        steps.push_back(shot("14-settings-fr"));
        steps.push_back({"english", [](App &app, double) { g_fr = false; app.tab = Tab::Play; return true; }});
        steps.push_back({"play", [this](App &app, double t)
                         {
                             if (t == 0.0)
                             {
                                 captureInto("play");
                                 app.play();
                                 say("game started (pid %s)", std::to_string(app.game.pid()));
                                 return false;
                             }
                             if (app.gameRunning && t >= playSeconds && t < playSeconds + 0.1)
                                 say("asking the game to close");
                             if (app.gameRunning && t >= playSeconds)
                                 app.game.requestClose();
                             if (app.gameRunning && t >= playSeconds + 30)
                             {
                                 say("the game did not close: ending it");
                                 app.game.kill();
                             }
                             if (app.gameRunning)
                                 return false;
                             say("game exited, code %s, crash report: %s", std::to_string(app.gameExit), app.crashReport.empty() ? "none" : app.crashReport);
                             return true;
                         }});
        steps.push_back(shot("15-after-play", 1.5));
        if (crashSeconds > 0)
        {
            steps.push_back({"crash", [this](App &app, double t)
                             {
                                 if (t == 0.0)
                                 {
                                     captureInto("crash");
                                     plat::setEnv("PS2X_TEST_CRASH", std::to_string(static_cast<int>(crashSeconds)));
                                     plat::setEnv("PS2X_NO_DIALOGS", "1");
                                     app.play();
                                     plat::setEnv("PS2X_TEST_CRASH", "");
                                     plat::setEnv("PS2X_NO_DIALOGS", "");
                                     say("game started to crash (pid %s)", std::to_string(app.game.pid()));
                                     return false;
                                 }
                                 if (app.gameRunning && t > crashSeconds + 90)
                                     app.game.kill();
                                 if (app.gameRunning)
                                     return false;
                                 say("game exited, code %s, crash report: %s", std::to_string(app.gameExit), app.crashReport.empty() ? "none" : app.crashReport);
                                 return true;
                             }});
            steps.push_back(shot("16-crash", 1.5));
        }
        steps.push_back({"quit", [this](App &app, double)
                         {
                             say("selftest done");
                             app.quit = true;
                             return true;
                         }});
    }

    // The game's window captures (PS2X_CAPTURE_WINDOW_DIR, when set) of each run in a folder of their own.
    std::string captureRoot;
    void captureInto(const char *name)
    {
        if (captureRoot.empty())
            captureRoot = plat::getEnv("PS2X_CAPTURE_WINDOW_DIR");
        if (captureRoot.empty())
            return;
        const fs::path dir = plat::path(captureRoot) / plat::path(name);
        std::error_code ec;
        fs::create_directories(dir, ec);
        plat::setEnv("PS2X_CAPTURE_WINDOW_DIR", plat::str(dir));
    }

    void tick(App &app)
    {
        if (index >= steps.size())
            return;
        const double now = plat::now();
        if (stepStart == 0)
            stepStart = now;
        const double t = now - stepStart;
        if (steps[index].run(app, t == 0 ? 0.0 : std::max(t, 1e-6)))
        {
            ++index;
            stepStart = 0;
        }
    }
};

// ---- main ----

int main(int argc, char **argv)
{
    std::string release, selftestIso, shots, wrong;
    bool errorDemo = false, cancel = false;
    double playSeconds = 40, crashSeconds = 0;
    (void)argc;
    (void)argv;
    const std::vector<std::string> args = plat::arguments(); // UTF-8, from the wide command line
    for (size_t i = 0; i < args.size(); ++i)
    {
        const std::string &a = args[i];
        auto next = [&] { return i + 1 < args.size() ? args[++i] : std::string(); };
        if (a == "--release")
            release = next();
        else if (a == "--selftest")
            selftestIso = next();
        else if (a == "--shots")
            shots = next();
        else if (a == "--wrong")
            wrong = next();
        else if (a == "--error-demo")
            errorDemo = true;
        else if (a == "--cancel")
            cancel = true;
        else if (a == "--play-seconds")
            playSeconds = std::atof(next().c_str());
        else if (a == "--crash-seconds")
            crashSeconds = std::atof(next().c_str());
    }
    if (release.empty())
        release = plat::exeDirectory();

    App app;
    app.init(release);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1120, 760, "TMB Launcher");
    {
        const Vector2 dpi = GetWindowScaleDPI();
        ui::scale = std::clamp(dpi.x, 1.0f, 3.0f);
        if (ui::scale > 1.0f)
        {
            const int w = static_cast<int>(1120 * ui::scale), h = static_cast<int>(760 * ui::scale);
            const int m = GetCurrentMonitor();
            SetWindowSize(std::min(w, GetMonitorWidth(m) - 40), std::min(h, GetMonitorHeight(m) - 80));
            SetWindowPosition((GetMonitorWidth(m) - GetScreenWidth()) / 2, (GetMonitorHeight(m) - GetScreenHeight()) / 2);
        }
    }
    SetWindowMinSize(static_cast<int>(980 * ui::scale), static_cast<int>(700 * ui::scale));
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    rlImGuiBeginInitImGui();
    ImGui::StyleColorsDark();
    ui::setup();
    rlImGuiEndInitImGui();

    Selftest test;
    if (!selftestIso.empty())
    {
        test.iso = selftestIso;
        test.shots = shots.empty() ? plat::str(plat::path(release) / L"selftest") : shots;
        std::error_code ec;
        fs::create_directories(plat::path(test.shots), ec);
        test.wrong = wrong;
        test.errorDemo = errorDemo;
        test.cancel = cancel;
        test.playSeconds = playSeconds;
        test.crashSeconds = crashSeconds;
        test.log = _wfopen((plat::path(test.shots) / L"selftest.log").c_str(), L"w");
        test.t0 = plat::now();
        test.say("release %s, user folder %s", app.p.release, app.p.user);
        test.build();
    }

    const double t0 = plat::now();
    while (!app.quit)
    {
        if (WindowShouldClose())
        {
            if (app.building)
                app.wantQuit = true;
            else
                break;
        }
        app.time = plat::now() - t0;
        app.pollCheck();
        app.pollBuild();
        app.pollGame();
        if (!selftestIso.empty())
            test.tick(app);
        if (app.hidden)
        {
            PollInputEvents();
            WaitTime(0.1);
            continue;
        }
        BeginDrawing();
        ClearBackground(Color{1, 3, 10, 255});
        ui::drawBackground(app.time, app.cfg.motion);
        rlImGuiBegin();
        drawFrame(app);
        rlImGuiEnd();
        if (!app.pendingShot.empty())
        {
            rlDrawRenderBatchActive();
            Image img = LoadImageFromScreen();
            ExportImage(img, app.pendingShot.c_str());
            UnloadImage(img);
            app.pendingShot.clear();
        }
        EndDrawing();
    }
    app.cancelBuild();
    rlImGuiShutdown();
    ui::releaseBackground();
    CloseWindow();
    if (test.log)
        std::fclose(test.log);
    return 0;
}
