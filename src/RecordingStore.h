#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include "Action.h"

struct SavedRecording {
    std::wstring name;
    std::wstring filePath;
    ActionList   actions;   // empty unless fully loaded
};

class RecordingStore {
public:
    // Returns the directory where recordings are stored.
    // Useful for displaying the path to the user.
    static std::wstring GetRecordingsDir();   // %USERPROFILE%\Documents\ActionPlay Recordings\

    static bool         Save(const std::wstring& name, const ActionList& actions);
    static bool         LoadActions(const std::wstring& filePath, ActionList& out);
    static std::vector<SavedRecording> ListAll();   // metadata only (no actions loaded)
    static bool         Delete(const std::wstring& filePath);
    static std::wstring SanitizeFilename(const std::wstring& name);
};
