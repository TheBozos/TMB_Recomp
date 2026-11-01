#pragma once

// The launcher's operating-system layer (platform_win.cpp): everything that needs <windows.h>, kept out of the
// raylib / ImGui translation units (their names clash: CloseWindow, DrawText, Rectangle...). Strings are UTF-8.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace plat
{
    std::wstring widen(const std::string &utf8);
    std::string narrow(const std::wstring &wide);
    // A UTF-8 path as a filesystem path (a narrow std::filesystem::path is the ANSI code page on Windows).
    std::filesystem::path path(const std::string &utf8);
    std::string str(const std::filesystem::path &p);

    std::string exeDirectory();
    // The command line's arguments (without the program), UTF-8.
    std::vector<std::string> arguments();
    // The runner's user folder: PS2X_USER_DIR, else %APPDATA%\TMB_Recomp (created).
    std::string userDirectory();
    std::string getEnv(const char *name);
    void setEnv(const char *name, const std::string &value);
    // "fr" when Windows' display language is French, else "en".
    std::string systemLanguage();

    // The native Open dialog for a disc image; "" when cancelled.
    std::string pickDiscImage(void *ownerWindow, const std::string &title, const std::string &filterName,
                              const std::string &allFilesName);
    // Opens a file, folder or URL as Explorer would.
    void open(const std::string &target);
    void showInFolder(const std::string &file);

    // A resource of the launcher's exe (RCDATA), or nullptr.
    const void *resource(const char *name, size_t &size);

    // A child process. With output: stdout and stderr through one pipe, read line by line by a thread; started with
    // CREATE_NO_WINDOW and in a job object, so kill() ends it and everything it started (and so does the launcher
    // exiting). Without output: a plain GUI process (the game), not tied to the launcher.
    class Process
    {
    public:
        Process();
        ~Process();
        Process(const Process &) = delete;
        Process &operator=(const Process &) = delete;

        bool start(const std::string &exe, const std::vector<std::string> &args, const std::string &cwd,
                   bool captureOutput, std::string &error);
        bool running();
        // -1 while running.
        long exitCode();
        uint32_t pid() const;
        // The output lines read since the last call.
        std::vector<std::string> takeLines();
        // Ends the process (and with output, its whole tree).
        void kill();
        // Asks the process's top-level windows to close (WM_CLOSE), as the window's X would.
        void requestClose();
        // Waits until its output pipe is drained (after it exited).
        void drain(int timeoutMs);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    // Seconds since an arbitrary epoch (monotonic).
    double now();
    // The files of a folder with their write times (UTC 100 ns ticks): crash reports appearing.
    std::vector<std::pair<std::string, uint64_t>> listFiles(const std::string &folder, const std::string &suffix);
    uint64_t freeBytes(const std::string &folder);
}
