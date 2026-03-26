#pragma comment(lib, "shell32.lib")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>
#include "RecordingStore.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Write a plain POD value into an open FILE as raw bytes.
template<typename T>
bool WriteField(FILE* f, const T& value)
{
    return std::fwrite(&value, sizeof(T), 1, f) == 1;
}

// Read a plain POD value from an open FILE.
template<typename T>
bool ReadField(FILE* f, T& value)
{
    return std::fread(&value, sizeof(T), 1, f) == 1;
}

// Magic bytes at the start of every .acpr file.
const char kMagic[4] = { 'A', 'C', 'P', 'R' };
const uint32_t kVersion = 1;

// Write the file header (magic + version + recording name).
bool WriteHeader(FILE* f, const std::wstring& name)
{
    if (std::fwrite(kMagic, 1, 4, f) != 4)
        return false;

    if (!WriteField(f, kVersion))
        return false;

    uint32_t nameLen = static_cast<uint32_t>(name.size()); // wchar_t count, no null
    if (!WriteField(f, nameLen))
        return false;

    if (nameLen > 0)
    {
        if (std::fwrite(name.data(), sizeof(wchar_t), nameLen, f) != nameLen)
            return false;
    }

    return true;
}

// Read and validate the file header; populate recordingName on success.
bool ReadHeader(FILE* f, std::wstring& recordingName)
{
    char magic[4] = {};
    if (std::fread(magic, 1, 4, f) != 4)
        return false;
    if (magic[0] != kMagic[0] || magic[1] != kMagic[1] ||
        magic[2] != kMagic[2] || magic[3] != kMagic[3])
        return false;

    uint32_t version = 0;
    if (!ReadField(f, version))
        return false;
    if (version != kVersion)
        return false;

    uint32_t nameLen = 0;
    if (!ReadField(f, nameLen))
        return false;

    if (nameLen > 0)
    {
        recordingName.resize(nameLen);
        if (std::fread(recordingName.data(), sizeof(wchar_t), nameLen, f) != nameLen)
            return false;
    }
    else
    {
        recordingName.clear();
    }

    return true;
}

// Write one Action into an open FILE.
bool WriteAction(FILE* f, const Action& a)
{
    uint8_t  type        = static_cast<uint8_t>(a.type);
    double   timestamp   = a.timestamp;
    int32_t  x           = static_cast<int32_t>(a.x);
    int32_t  y           = static_cast<int32_t>(a.y);
    int32_t  mouseButton = static_cast<int32_t>(a.mouseButton);
    uint8_t  pressed     = a.pressed ? 1u : 0u;
    int32_t  scrollDelta = static_cast<int32_t>(a.scrollDelta);
    uint32_t vkCode      = static_cast<uint32_t>(a.vkCode);
    uint16_t scanCode    = static_cast<uint16_t>(a.scanCode);
    uint8_t  extendedKey = a.extendedKey ? 1u : 0u;

    return WriteField(f, type)
        && WriteField(f, timestamp)
        && WriteField(f, x)
        && WriteField(f, y)
        && WriteField(f, mouseButton)
        && WriteField(f, pressed)
        && WriteField(f, scrollDelta)
        && WriteField(f, vkCode)
        && WriteField(f, scanCode)
        && WriteField(f, extendedKey);
}

// Read one Action from an open FILE.
bool ReadAction(FILE* f, Action& a)
{
    uint8_t  type        = 0;
    double   timestamp   = 0.0;
    int32_t  x           = 0;
    int32_t  y           = 0;
    int32_t  mouseButton = 0;
    uint8_t  pressed     = 0;
    int32_t  scrollDelta = 0;
    uint32_t vkCode      = 0;
    uint16_t scanCode    = 0;
    uint8_t  extendedKey = 0;

    if (!ReadField(f, type)        ||
        !ReadField(f, timestamp)   ||
        !ReadField(f, x)           ||
        !ReadField(f, y)           ||
        !ReadField(f, mouseButton) ||
        !ReadField(f, pressed)     ||
        !ReadField(f, scrollDelta) ||
        !ReadField(f, vkCode)      ||
        !ReadField(f, scanCode)    ||
        !ReadField(f, extendedKey))
    {
        return false;
    }

    a.type        = static_cast<ActionType>(type);
    a.timestamp   = timestamp;
    a.x           = static_cast<int>(x);
    a.y           = static_cast<int>(y);
    a.mouseButton = static_cast<int>(mouseButton);
    a.pressed     = (pressed != 0);
    a.scrollDelta = static_cast<int>(scrollDelta);
    a.vkCode      = static_cast<DWORD>(vkCode);
    a.scanCode    = static_cast<WORD>(scanCode);
    a.extendedKey = (extendedKey != 0);

    return true;
}

// Open a file using _wfopen with the given mode.
FILE* OpenFile(const std::wstring& path, const wchar_t* mode)
{
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), mode);
    return f;
}

// ---------------------------------------------------------------------------
// File-private helpers (not exposed in the header)
// ---------------------------------------------------------------------------
static std::wstring GetRecordingsDirImpl()
{
    wchar_t docsPath[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, docsPath)))
        return L"";

    std::wstring dir = docsPath;
    if (!dir.empty() && dir.back() != L'\\')
        dir += L'\\';
    dir += L"ActionPlay Recordings\\";
    return dir;
}

static bool EnsureDir()
{
    std::wstring dir = GetRecordingsDirImpl();
    if (dir.empty())
        return false;

    if (!CreateDirectoryW(dir.c_str(), nullptr))
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            return false;
    }
    return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// RecordingStore implementation
// ---------------------------------------------------------------------------

std::wstring RecordingStore::GetRecordingsDir()
{
    return GetRecordingsDirImpl();
}

bool RecordingStore::Save(const std::wstring& name, const ActionList& actions)
{
    if (!EnsureDir())
        return false;

    std::wstring dir      = GetRecordingsDirImpl();
    std::wstring safeName = SanitizeFilename(name);
    if (safeName.empty())
        safeName = L"recording";

    // Build a unique file path: name.acpr, then name_2.acpr, name_3.acpr …
    std::wstring filePath = dir + safeName + L".acpr";
    if (GetFileAttributesW(filePath.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        int suffix = 2;
        do
        {
            filePath = dir + safeName + L"_" + std::to_wstring(suffix) + L".acpr";
            ++suffix;
        } while (GetFileAttributesW(filePath.c_str()) != INVALID_FILE_ATTRIBUTES);
    }

    FILE* f = OpenFile(filePath, L"wb");
    if (!f)
        return false;

    bool ok = WriteHeader(f, name); // store original (unsanitized) name in file

    if (ok)
    {
        uint32_t count = static_cast<uint32_t>(actions.size());
        ok = WriteField(f, count);
    }

    if (ok)
    {
        for (const Action& a : actions)
        {
            if (!WriteAction(f, a))
            {
                ok = false;
                break;
            }
        }
    }

    std::fclose(f);

    if (!ok)
        DeleteFileW(filePath.c_str()); // clean up partial file

    return ok;
}

bool RecordingStore::LoadActions(const std::wstring& filePath, ActionList& out)
{
    out.clear();

    FILE* f = OpenFile(filePath, L"rb");
    if (!f)
        return false;

    std::wstring name;
    bool ok = ReadHeader(f, name);

    if (ok)
    {
        uint32_t count = 0;
        ok = ReadField(f, count);

        if (ok)
        {
            out.reserve(count);
            for (uint32_t i = 0; i < count && ok; ++i)
            {
                Action a{};
                ok = ReadAction(f, a);
                if (ok)
                    out.push_back(a);
            }
        }
    }

    std::fclose(f);

    if (!ok)
        out.clear();

    return ok;
}

std::vector<SavedRecording> RecordingStore::ListAll()
{
    std::vector<SavedRecording> results;

    std::wstring dir = GetRecordingsDirImpl();
    if (dir.empty())
        return results;

    std::wstring pattern = dir + L"*.acpr";

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return results;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        std::wstring filePath = dir + fd.cFileName;

        // Read only the header to get the recording name.
        FILE* f = OpenFile(filePath, L"rb");
        if (!f)
            continue;

        std::wstring recordingName;
        bool ok = ReadHeader(f, recordingName);
        std::fclose(f);

        if (!ok)
            continue; // skip corrupt / foreign files

        SavedRecording rec;
        rec.name     = recordingName;
        rec.filePath = filePath;
        // rec.actions intentionally left empty (metadata-only load)
        results.push_back(std::move(rec));
    }
    while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return results;
}

bool RecordingStore::Delete(const std::wstring& filePath)
{
    return DeleteFileW(filePath.c_str()) != FALSE;
}

std::wstring RecordingStore::SanitizeFilename(const std::wstring& name)
{
    // Characters forbidden in Windows filenames.
    const std::wstring forbidden = L"<>:\"/\\|?*";

    std::wstring result;
    result.reserve(name.size());

    for (wchar_t ch : name)
    {
        // Replace control characters and forbidden chars with underscore.
        if (ch < 32 || forbidden.find(ch) != std::wstring::npos)
            result += L'_';
        else
            result += ch;
    }

    return result;
}
