#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <shlobj.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>
using std::max;
using std::min;
#include <gdiplus.h>
#include <psapi.h>
#include <cstdio>

#define LAUNCHER_TITLE L"SonsOfSparta-PS5 Native"
#define LAUNCHER_NAME L"God of War Sons of Sparta"
#define LAUNCHER_RUNTIME L"sos_runtime.exe"
#define LAUNCHER_MUTEX L"Local\\SonsOfSpartaLauncher"
#define LAUNCHER_ICON L"sons_of_sparta.ico"
#define LAUNCHER_TITLE_ID L"PPSA28997"
#define LAUNCHER_REQUIRED L"sce_sys;Media"

namespace {

const wchar_t* kTitle = LAUNCHER_TITLE;
const wchar_t* kRuntime = LAUNCHER_RUNTIME;

void Fail(const std::wstring& text) {
    MessageBoxW(nullptr, text.c_str(), kTitle, MB_OK | MB_ICONERROR);
}

bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

const unsigned char kSettingsHeader[12] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00};
const char kSettingsBody[] = R"json(ersion":"0.1","globalSaveDataV1ref":{"previouslyBootedGame":true,"previouslySavedDifficultyLevel":0,"previouslyInitializedToDefaults":true,"vsyncOn":false,"isPitUnlocked":false,"previouslySelectedSaveSlotIndex":-1,"masterAudioVolume":0.0,"sfxAudioVolume":0.0,"bgmAudioVolume":0.0,"keybindingsOverride":"","pitData":"","hasBeatenGame":false,"aimAssist":0,"enablePuzzleAimAssist":false,"puzzleTiming":0,"hintsEnabled":true,"hintWaitDuration":1,"disableTutorial":true,"GuidedExperience":true,"GameDifficulty":1,"viewControls":false,"AutoPickupEnabled":false,"menuHolds":0,"repeatedButtonPress":0,"traversalInputs":0,"cinematicSkip":false,"touchpadButton":3,"swipeUp":0,"swipeLeft":0,"swipeRight":0,"swipeDown":0,"controllerVisualization":false,"visualizationContrast":0,"hudCustomization":0,"combatHUD":1,"aimReticle":1,"bossHealthBars":1,"enemyHealthBars":1,"gameplayNotificationToggle":1,"volumeBalance":0,"outputDevice":0,"monoAudioSystemIntegratedSupport":false,"globalVolume":10.0,"dialogueVolume":10.0,"musicVolume":10.0,"sfxVolume":10.0,"controllerSpeakerVolume":10.0,"enableSubtitles":true,"subtitleTextColor":0,"displaySpeakerName":true,"speakerTextColor":0,"enableCaptions":false,"captionTextColor":0,"subtitleAndCaptionTextSize":0,"blurSubtitleAndCaptionBackground":false,"subtitleAndCaptionBackground":1,"audioCue":false,"audioCueVolume":10.0,"audioPanning":0.0,"centerPanDialogue":false,"voiceBoost":false,"uiTextSize":0,"iconSize":0,"highContrastHUD":0,"colorFilter":0,"filterStrength":10.0,"highContrastDisplay":0,"HeroColor":0,"CompanionColor":0,"BossColor":0,"EnemyColor":0,"NPCColor":0,"TargetColor":0,"InteractColor":0,"HazardColor":0,"traversalColor":0,"backgroundColor":0,"enableVsync":true,"screenCalibration":false,"filterMode":0,"enableBloodAndGore":0,"enableBrutalKillPrompt":true,"enableBloodVFX":true,"enableCorpseBloodVFX":true,"fullScreenMode":0,"resolutionIndex":0,"aspectRatio":0,"cameraPanningSpeed":10,"cameraShakeIntensity":10,"activationToggle":false,"horizontalSpeed":0,"verticalSpeed":0,"accelerationSpeed":0,"reduceSmallMotions":0,"textLanguage":0,"speechLanguage":0,"controllerVibration":2,"enableAimToggling":false,"enableBlockToggling":false,"enableHDR":true,"gammaValue":50.5,"brightnessValue":50.5,"personalRecordSpeedrunDataBoy":{"isValid":false,"overallTime":0.0},"chapterBestSpeedrunDataBoy":{"isValid":false,"overallTime":0.0},"personalRecordSpeedrunDataCadet":{"isValid":false,"overallTime":0.0},"chapterBestSpeedrunDataCadet":{"isValid":false,"overallTime":0.0},"personalRecordSpeedrunDataSpartan":{"isValid":false,"overallTime":0.0},"chapterBestSpeedrunDataSpartan":{"isValid":false,"overallTime":0.0},"version":2}})json";

bool SeedSettings(const std::wstring& directory) {
    CreateDirectoryW((directory + L"\\_sd").c_str(), nullptr);
    CreateDirectoryW((directory + L"\\_sd\\GOWSOSSAVE999").c_str(), nullptr);
    FILE* file = nullptr;
    if (_wfopen_s(&file, (directory + L"\\_sd\\GOWSOSSAVE999\\GoW_SoS999.dat").c_str(), L"wb") != 0 || file == nullptr) return false;
    fwrite(kSettingsHeader, 1, sizeof(kSettingsHeader), file);
    for (size_t i = 0; kSettingsBody[i] != 0; ++i) {
        const unsigned char unit[4] = {static_cast<unsigned char>(kSettingsBody[i]), 0, 0, 0};
        fwrite(unit, 1, 4, file);
    }
    fclose(file);
    return true;
}

void ApplyResolution(const std::wstring& directory) {
    FILE* choice = nullptr;
    if (_wfopen_s(&choice, (directory + L"\\resolution.txt").c_str(), L"r") != 0 || choice == nullptr) return;
    char word[32] = {};
    fgets(word, sizeof(word), choice);
    fclose(choice);
    std::string value = word;
    for (auto& character : value) character = static_cast<char>(tolower(static_cast<unsigned char>(character)));
    char wanted = 0;
    if (value.rfind("1080", 0) == 0) wanted = '8';
    else if (value.rfind("1440", 0) == 0) wanted = '6';
    else if (value.rfind("4k", 0) == 0 || value.rfind("2160", 0) == 0) wanted = '0';
    if (wanted == 0) return;
    const std::wstring path = directory + L"\\_sd\\GOWSOSSAVE999\\GoW_SoS999.dat";
    FILE* file = nullptr;
    if (!Exists(path) && !SeedSettings(directory)) return;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr) return;
    std::vector<unsigned char> data;
    unsigned char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) data.insert(data.end(), buffer, buffer + count);
    fclose(file);
    const char key[] = "\"resolutionIndex\":";
    std::vector<unsigned char> pattern;
    for (size_t i = 0; key[i] != 0; ++i) {
        pattern.push_back(static_cast<unsigned char>(key[i]));
        pattern.insert(pattern.end(), 3, 0);
    }
    const auto found = std::search(data.begin(), data.end(), pattern.begin(), pattern.end());
    if (found == data.end()) return;
    const size_t at = static_cast<size_t>(found - data.begin()) + pattern.size();
    if (at + 8 > data.size()) return;
    const bool digit = data[at] >= '0' && data[at] <= '9' && data[at + 1] == 0 && data[at + 2] == 0 && data[at + 3] == 0;
    const bool longer = data[at + 4] >= '0' && data[at + 4] <= '9' && data[at + 5] == 0;
    if (!digit || longer || data[at] == static_cast<unsigned char>(wanted)) return;
    data[at] = static_cast<unsigned char>(wanted);
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr) return;
    fwrite(data.data(), 1, data.size(), file);
    fclose(file);
}

bool IsDirectory(const std::wstring& path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

FILETIME WriteTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return FILETIME{};
    return data.ftLastWriteTime;
}

long long FileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return -1;
    return (static_cast<long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

std::wstring FinalPath(const std::wstring& path) {
    HANDLE handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return L"";
    wchar_t buffer[2048];
    const DWORD length = GetFinalPathNameByHandleW(handle, buffer, 2048, VOLUME_NAME_DOS);
    CloseHandle(handle);
    if (length == 0 || length >= 2048) return L"";
    std::wstring result = buffer;
    for (auto& character : result) character = static_cast<wchar_t>(towlower(character));
    return result;
}

std::wstring PreparedStamp(const std::wstring& directory, const std::wstring& source) {
    return L"1 " + std::to_wstring(FileSize(source)) + L" " + std::to_wstring(FileSize(directory + L"\\tools\\relinker.exe"));
}

std::wstring ReadStamp(const std::wstring& path) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"r") != 0 || file == nullptr) return L"";
    wchar_t line[256] = {};
    fgetws(line, 256, file);
    fclose(file);
    std::wstring text = line;
    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r')) text.pop_back();
    return text;
}

void WriteStamp(const std::wstring& path, const std::wstring& text) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"w") != 0 || file == nullptr) return;
    fputws(text.c_str(), file);
    fclose(file);
}

bool IsElf(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    unsigned char magic[4] = {};
    DWORD read = 0;
    const bool ok = ReadFile(file, magic, sizeof(magic), &read, nullptr) && read == 4 && magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F';
    CloseHandle(file);
    return ok;
}

bool ExtractSelf(const std::wstring& selfPath, const std::wstring& elfPath) {
    HANDLE in = CreateFileW(selfPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (in == INVALID_HANDLE_VALUE) return false;
    auto readAt = [&](unsigned long long offset, void* buffer, DWORD size) {
        LARGE_INTEGER position;
        position.QuadPart = static_cast<LONGLONG>(offset);
        DWORD got = 0;
        return SetFilePointerEx(in, position, nullptr, FILE_BEGIN) && ReadFile(in, buffer, size, &got, nullptr) && got == size;
    };
    struct Entry { unsigned long long props, offset, fileSize, memSize; };
    struct Header { unsigned type, flags; unsigned long long offset, vaddr, paddr, fileSize, memSize, align; };
    unsigned char head[0x20] = {};
    bool ok = readAt(0, head, sizeof(head)) && head[0] == 0x4f && head[1] == 0x15 && head[2] == 0x3d && head[3] == 0x1d;
    const unsigned entryCount = ok ? *reinterpret_cast<unsigned short*>(head + 0x18) : 0;
    std::vector<Entry> entries(entryCount);
    unsigned char elfHead[64] = {};
    ok = ok && entryCount != 0 && entryCount < 4096 && readAt(0x20, entries.data(), entryCount * sizeof(Entry)) &&
         readAt(0x20 + entryCount * sizeof(Entry), elfHead, sizeof(elfHead)) && elfHead[0] == 0x7f && elfHead[1] == 'E';
    std::vector<Header> headers;
    unsigned long long tableOffset = 0;
    if (ok) {
        tableOffset = *reinterpret_cast<unsigned long long*>(elfHead + 0x20);
        const unsigned count = *reinterpret_cast<unsigned short*>(elfHead + 0x38);
        headers.resize(count);
        ok = count != 0 && count < 512 && readAt(0x20 + entryCount * sizeof(Entry) + tableOffset, headers.data(), count * sizeof(Header));
    }
    HANDLE out = INVALID_HANDLE_VALUE;
    if (ok) {

        for (const auto& entry : entries) {
            if ((entry.props & 0x800) != 0 && (entry.props & 0xA) != 0) ok = false;
        }
        out = ok ? CreateFileW(elfPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
        ok = out != INVALID_HANDLE_VALUE;
    }
    auto writeAt = [&](unsigned long long offset, const void* buffer, DWORD size) {
        LARGE_INTEGER position;
        position.QuadPart = static_cast<LONGLONG>(offset);
        DWORD put = 0;
        return SetFilePointerEx(out, position, nullptr, FILE_BEGIN) && WriteFile(out, buffer, size, &put, nullptr) && put == size;
    };
    if (ok) {
        ok = writeAt(0, elfHead, sizeof(elfHead)) && writeAt(tableOffset, headers.data(), static_cast<DWORD>(headers.size() * sizeof(Header)));
        std::vector<char> chunk(1 << 22);
        unsigned long long end = 0;
        for (const auto& header : headers) end = max(end, header.offset + header.fileSize);
        for (const auto& entry : entries) {
            if (!ok || (entry.props & 0x800) == 0) continue;
            const unsigned index = static_cast<unsigned>((entry.props >> 20) & 0xfff);
            if (index >= headers.size()) continue;
            unsigned long long remaining = min(entry.fileSize, headers[index].fileSize);
            unsigned long long from = entry.offset, to = headers[index].offset;
            while (ok && remaining != 0) {
                const DWORD part = static_cast<DWORD>(min<unsigned long long>(remaining, chunk.size()));
                ok = readAt(from, chunk.data(), part) && writeAt(to, chunk.data(), part);
                from += part;
                to += part;
                remaining -= part;
            }
        }
        if (ok) {
            LARGE_INTEGER size;
            size.QuadPart = static_cast<LONGLONG>(end);
            ok = SetFilePointerEx(out, size, nullptr, FILE_BEGIN) && SetEndOfFile(out);
        }
    }
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    CloseHandle(in);
    if (!ok) DeleteFileW(elfPath.c_str());
    return ok;
}

long RunHidden(const std::wstring& command, const std::wstring& directory, const std::wstring& log) {
    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    if (output != INVALID_HANDLE_VALUE) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = output;
        startup.hStdError = output;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    const BOOL started = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process);
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
    if (!started) return -1;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return static_cast<long>(code);
}

bool PngEncoderClsid(CLSID* clsid) {
    UINT count = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    if (size == 0) return false;
    std::vector<char> buffer(size);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    Gdiplus::GetImageEncoders(count, size, encoders);
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(encoders[i].MimeType, L"image/png") == 0) {
            *clsid = encoders[i].Clsid;
            return true;
        }
    }
    return false;
}

HICON LoadGameIcon(const std::wstring& png, const std::wstring& icoPath) {
    Gdiplus::Bitmap source(png.c_str());
    if (source.GetLastStatus() != Gdiplus::Ok) return nullptr;
    Gdiplus::Bitmap scaled(256, 256, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics graphics(&scaled);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(&source, 0, 0, 256, 256);
    }
    HICON icon = nullptr;
    scaled.GetHICON(&icon);
    CLSID encoder{};
    IStream* stream = nullptr;
    if (PngEncoderClsid(&encoder) && SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
        if (scaled.Save(stream, &encoder, nullptr) == Gdiplus::Ok) {
            STATSTG stat{};
            stream->Stat(&stat, STATFLAG_NONAME);
            const auto size = static_cast<DWORD>(stat.cbSize.QuadPart);
            HGLOBAL memory = nullptr;
            if (SUCCEEDED(GetHGlobalFromStream(stream, &memory))) {
                const void* data = GlobalLock(memory);
                HANDLE file = CreateFileW(icoPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (data != nullptr && file != INVALID_HANDLE_VALUE) {
#pragma pack(push, 1)
                    struct { WORD reserved, type, count; BYTE width, height, colors, zero; WORD planes, bits; DWORD bytes, offset; } header =
                        {0, 1, 1, 0, 0, 0, 0, 1, 32, size, 22};
#pragma pack(pop)
                    DWORD written = 0;
                    WriteFile(file, &header, sizeof(header), &written, nullptr);
                    WriteFile(file, data, size, &written, nullptr);
                }
                if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
                if (data != nullptr) GlobalUnlock(memory);
            }
        }
        stream->Release();
    }
    return icon;
}

void CreateShortcut(const std::wstring& link, const std::wstring& target, const std::wstring& directory, const std::wstring& icoPath) {
    IShellLinkW* shell = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shell)))) return;
    shell->SetPath(target.c_str());
    shell->SetWorkingDirectory(directory.c_str());
    shell->SetDescription(LAUNCHER_NAME);
    if (Exists(icoPath)) shell->SetIconLocation(icoPath.c_str(), 0);
    IPersistFile* file = nullptr;
    if (SUCCEEDED(shell->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)))) {
        file->Save(link.c_str(), TRUE);
        file->Release();
    }
    shell->Release();
}

struct WindowSearch {
    DWORD process;
    HICON icon;
    bool found;
};

BOOL CALLBACK FindGameWindow(HWND window, LPARAM parameter) {
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    if (owner == search->process && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(search->icon));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(search->icon));
        search->found = true;
    }
    return TRUE;
}

bool ConvertOrCopy(const std::wstring& from, const std::wstring& to) {
    if (IsElf(from)) return CopyFileW(from.c_str(), to.c_str(), FALSE) != 0;
    return ExtractSelf(from, to);
}

void PrepareModules(const std::wstring& directory, const std::wstring& prep, std::wstring& failed) {
    CreateDirectoryW(prep.c_str(), nullptr);
    CreateDirectoryW((prep + L"\\sce_module").c_str(), nullptr);
    for (const wchar_t* sub : {L"\\sce_module", L"\\Media\\Modules", L"\\Media\\Plugins"}) {
        WIN32_FIND_DATAW found;
        HANDLE search = FindFirstFileW((directory + sub + L"\\*.prx").c_str(), &found);
        if (search == INVALID_HANDLE_VALUE) continue;
        do {
            if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
            const std::wstring target = prep + L"\\sce_module\\" + found.cFileName;
            const std::wstring source = directory + sub + L"\\" + found.cFileName;
            if (!ConvertOrCopy(source, target)) failed += (failed.empty() ? L"" : L", ") + std::wstring(found.cFileName);
        } while (FindNextFileW(search, &found));
        FindClose(search);
    }
}

void LinkIntoApp0(const std::wstring& directory, const wchar_t* name) {
    const std::wstring link = directory + L"\\app0\\" + name;
    if (!Exists(directory + L"\\" + name)) return;
    if (Exists(link)) {
        if (FinalPath(link) == FinalPath(directory + L"\\" + name)) return;
        RemoveDirectoryW(link.c_str());
        if (Exists(link)) return;
    }
    CreateDirectoryW((directory + L"\\app0").c_str(), nullptr);
    RunHidden(L"cmd.exe /c mklink /J \"" + link + L"\" \"" + directory + L"\\" + name + L"\"", directory, directory + L"\\logs\\link.log");
}

}

struct Splash {
    static constexpr UINT kCloseMessage = WM_APP + 1;
    static constexpr UINT_PTR kTimer = 1;
    static constexpr UINT kGetDpiScaledSize = 0x02E4;  // WM_GETDPISCALEDSIZE (Windows 10 1703+), not declared for the default WINVER
    // The layout stops growing at 175%; the window stays DPI aware, so above that it keeps its 175% size and text is still drawn sharply.
    static constexpr UINT kMaxLayoutDpi = 168;
    HANDLE thread = nullptr;
    HWND window = nullptr;
    HANDLE ready = nullptr;
    HWND status = nullptr;
    HWND title = nullptr, intro = nullptr, progress = nullptr, elapsed = nullptr, note = nullptr;
    HFONT fonts[5] = {};
    ULONGLONG started = 0, shown = ~0ull;
    UINT currentDpi = USER_DEFAULT_SCREEN_DPI;
    template <typename Function> static Function User32(const char* name) {
        return reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), name)));
    }
    static UINT Dpi(HWND hwnd) {
        static const auto forWindow = User32<UINT(WINAPI*)(HWND)>("GetDpiForWindow");
        UINT dpi = forWindow != nullptr ? forWindow(hwnd) : 0;
        if (dpi == 0) {
            HDC screen = GetDC(nullptr);
            dpi = static_cast<UINT>(GetDeviceCaps(screen, LOGPIXELSY));
            ReleaseDC(nullptr, screen);
        }
        return dpi != 0 ? dpi : USER_DEFAULT_SCREEN_DPI;
    }
    static int LayoutDpi(UINT dpi) { return static_cast<int>(min(dpi, kMaxLayoutDpi)); }
    // Grows a client rectangle to the window rectangle; the caption and borders follow the real DPI.
    void Frame(RECT* rect, UINT dpi) const {
        const DWORD style = static_cast<DWORD>(GetWindowLongW(window, GWL_STYLE)), exStyle = static_cast<DWORD>(GetWindowLongW(window, GWL_EXSTYLE));
        static const auto adjust = User32<BOOL(WINAPI*)(RECT*, DWORD, BOOL, DWORD, UINT)>("AdjustWindowRectExForDpi");
        if (adjust == nullptr || !adjust(rect, style, FALSE, exStyle, dpi)) AdjustWindowRectEx(rect, style, FALSE, exStyle);
    }
    // Because of the cap the size is not proportional to the DPI, so Windows is told the size the window will get on the new monitor.
    SIZE ScaledSize(UINT dpi) const {
        RECT client{};
        GetClientRect(window, &client);
        RECT frame{0, 0, MulDiv(client.right, LayoutDpi(dpi), LayoutDpi(currentDpi)), MulDiv(client.bottom, LayoutDpi(dpi), LayoutDpi(currentDpi))};
        Frame(&frame, dpi);
        return SIZE{frame.right - frame.left, frame.bottom - frame.top};
    }
    // Everything is laid out in 96-DPI units scaled by the capped layout DPI; label heights come from measuring their text.
    void Layout(UINT dpi, const RECT* suggested) {
        currentDpi = dpi;
        auto scale = [layoutDpi = LayoutDpi(dpi)](int value) { return MulDiv(value, layoutDpi, USER_DEFAULT_SCREEN_DPI); };
        // GDI only fakes FW_SEMIBOLD for "Segoe UI", so the real semibold face is named; "Please keep this window open" alone is bold.
        auto font = [&](int size, int weight, const wchar_t* face) {
            return CreateFontW(-scale(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
        };
        decltype(fonts) old;
        std::copy(std::begin(fonts), std::end(fonts), old);
        fonts[0] = font(22, FW_SEMIBOLD, L"Segoe UI Semibold");
        fonts[1] = font(15, FW_SEMIBOLD, L"Segoe UI Semibold");
        fonts[2] = font(15, FW_NORMAL, L"Segoe UI");
        fonts[3] = font(13, FW_NORMAL, L"Segoe UI");
        fonts[4] = font(13, FW_BOLD, L"Segoe UI");
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(suggested != nullptr ? MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST) : MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor);
        const RECT work = monitor.rcWork;
        const int margin = scale(24), width = min(scale(500), static_cast<int>(work.right - work.left) - scale(32)) - 2 * margin;
        const struct { HWND control; HFONT font; int gap; } items[] = {
            {title, fonts[0], 0}, {intro, fonts[2], 10}, {progress, nullptr, 24}, {status, fonts[1], 16}, {elapsed, fonts[3], 2}, {note, fonts[4], 20}};
        HDC dc = GetDC(window);
        int y = margin;
        for (const auto& item : items) {
            y += scale(item.gap);
            int height = scale(14);
            if (item.font != nullptr) {
                SendMessageW(item.control, WM_SETFONT, reinterpret_cast<WPARAM>(item.font), FALSE);
                wchar_t text[512] = {};
                GetWindowTextW(item.control, text, 512);
                const bool single = (GetWindowLongW(item.control, GWL_STYLE) & SS_ENDELLIPSIS) != 0;
                RECT bounds{0, 0, width, 0};
                HGDIOBJ previous = SelectObject(dc, item.font);
                DrawTextW(dc, text[0] != 0 ? text : L"X", -1, &bounds, DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS | (single ? DT_SINGLELINE : DT_WORDBREAK));
                SelectObject(dc, previous);
                height = bounds.bottom;
            }
            MoveWindow(item.control, margin, y, width, height, FALSE);
            y += height;
        }
        ReleaseDC(window, dc);
        for (HFONT font : old) if (font != nullptr) DeleteObject(font);
        const int clientWidth = width + 2 * margin, clientHeight = y + margin;
        RECT frame{0, 0, clientWidth, clientHeight};
        Frame(&frame, dpi);
        const int windowWidth = frame.right - frame.left, windowHeight = frame.bottom - frame.top;
        const int x = suggested != nullptr ? suggested->left : work.left + max(0, static_cast<int>(work.right - work.left) - windowWidth) / 2;
        const int top = suggested != nullptr ? suggested->top : work.top + max(0, static_cast<int>(work.bottom - work.top) - windowHeight) / 2;
        SetWindowPos(window, nullptr, x, top, windowWidth, windowHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        // If the frame the system draws differs from the computed one, correct the size once so the client area fits the content exactly.
        RECT client{};
        GetClientRect(window, &client);
        if (client.right != clientWidth || client.bottom != clientHeight)
            SetWindowPos(window, nullptr, 0, 0, windowWidth + clientWidth - client.right, windowHeight + clientHeight - client.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    void Tick() {
        const ULONGLONG seconds = (GetTickCount64() - started) / 1000;
        if (seconds == shown) return;
        shown = seconds;
        wchar_t text[64];
        const unsigned hours = static_cast<unsigned>(seconds / 3600), minutes = static_cast<unsigned>(seconds / 60 % 60), rest = static_cast<unsigned>(seconds % 60);
        if (hours == 0) std::swprintf(text, 64, L"Elapsed time: %02u:%02u", minutes, rest);
        else std::swprintf(text, 64, L"Elapsed time: %u:%02u:%02u", hours, minutes, rest);
        SetWindowTextW(elapsed, text);
    }
    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCCREATE) SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        auto* self = reinterpret_cast<Splash*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message) {
        case WM_CLOSE: return 0;
        case kCloseMessage: DestroyWindow(hwnd); return 0;
        case WM_DESTROY: KillTimer(hwnd, kTimer); PostQuitMessage(0); return 0;
        case WM_TIMER:
            if (wParam == kTimer && self != nullptr) self->Tick();
            return 0;
        case kGetDpiScaledSize:
            if (self == nullptr) break;
            *reinterpret_cast<SIZE*>(lParam) = self->ScaledSize(static_cast<UINT>(wParam));
            return TRUE;
        case WM_DPICHANGED:
            if (self != nullptr) self->Layout(HIWORD(wParam), reinterpret_cast<const RECT*>(lParam));
            return 0;
        case WM_CTLCOLORSTATIC: {
            // Labels share the window background instead of the default grey; secondary text is muted unless high contrast is on.
            HDC dc = reinterpret_cast<HDC>(wParam);
            const HWND control = reinterpret_cast<HWND>(lParam);
            HIGHCONTRASTW contrast{};
            contrast.cbSize = sizeof(contrast);
            const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) && (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
            const bool muted = self != nullptr && (control == self->intro || control == self->elapsed);
            SetTextColor(dc, muted && !highContrast ? RGB(0x5c, 0x5c, 0x5c) : GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(dc, GetSysColor(COLOR_WINDOW));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    static DWORD WINAPI Run(LPVOID parameter) {
        auto* self = static_cast<Splash*>(parameter);
        // Only this UI thread becomes per-monitor DPI aware, so the window is drawn sharply at the real scaling while the rest of the
        // launcher keeps its DPI mode. Windows before 10 1607 lack the API and keep scaling the window as a bitmap, as before.
        if (const auto aware = User32<DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT)>("SetThreadDpiAwarenessContext")) {
            if (aware(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == nullptr) aware(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
        }
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&controls);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = Proc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"SplashPrepare";
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&windowClass);
        // Created hidden on the primary monitor so its DPI is known before the final size and position are chosen.
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor);
        self->window = CreateWindowExW(WS_EX_TOPMOST, windowClass.lpszClassName, LAUNCHER_TITLE, WS_CAPTION, monitor.rcWork.left, monitor.rcWork.top, 0, 0, nullptr, nullptr, windowClass.hInstance, self);
        if (self->window == nullptr) {
            SetEvent(self->ready);
            return 0;
        }
        auto label = [&](const wchar_t* text, DWORD style) {
            return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | style, 0, 0, 0, 0, self->window, nullptr, windowClass.hInstance, nullptr);
        };
        self->title = label(L"Preparing the game for first launch", 0);
        self->intro = label(L"The game is being prepared from your own files\nThis only happens once, usually takes around two minutes\nand does not require an internet connection", 0);
        self->progress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_MARQUEE, 0, 0, 0, 0, self->window, nullptr, windowClass.hInstance, nullptr);
        SendMessageW(self->progress, PBM_SETMARQUEE, TRUE, 0);
        self->status = label(L"Preparing game modules...", SS_ENDELLIPSIS);
        self->elapsed = label(L"", SS_ENDELLIPSIS);
        self->note = label(L"Please keep this window open", 0);
        self->Tick();
        self->Layout(Dpi(self->window), nullptr);
        SetTimer(self->window, kTimer, 250, nullptr);
        ShowWindow(self->window, SW_SHOW);
        SetEvent(self->ready);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
        for (HFONT font : self->fonts) if (font != nullptr) DeleteObject(font);
        return 0;
    }
    void Start() {
        started = GetTickCount64();
        ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        thread = CreateThread(nullptr, 0, Run, this, 0, nullptr);
        WaitForSingleObject(ready, 5000);
    }
    // WM_SETTEXT from this thread is sent to the UI thread and handled there.
    void Status(const wchar_t* text) { if (status != nullptr) SetWindowTextW(status, text); }
    // Destroying the window on its own thread also stops the timer and the progress animation; safe to call more than once.
    void Close() {
        if (window != nullptr) PostMessageW(window, kCloseMessage, 0, 0);
        if (thread != nullptr) { WaitForSingleObject(thread, 2000); CloseHandle(thread); }
        if (ready != nullptr) CloseHandle(ready);
        thread = ready = nullptr;
        window = status = nullptr;
    }
    ~Splash() { Close(); }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HANDLE single = CreateMutexW(nullptr, TRUE, LAUNCHER_MUTEX);
    if (single != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        Fail(L"The game is already running.");
        return 0;
    }

    wchar_t modulePath[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(sizeof(modulePath) / sizeof(wchar_t)));
    std::wstring directory = modulePath;
    directory.resize(directory.find_last_of(L"\\/"));
    SetCurrentDirectoryW(directory.c_str());

    const std::wstring elf = directory + L"\\prep\\eboot.elf";
    const std::wstring bin = Exists(directory + L"\\eboot.bin") ? directory + L"\\eboot.bin" : directory + L"\\eboot.elf";
    std::wstring executable;
    if (Exists(bin)) {
        CreateDirectoryW((directory + L"\\prep").c_str(), nullptr);
        const FILETIME binTime = WriteTime(bin), elfTimeNow = WriteTime(elf);
        if (!Exists(elf) || CompareFileTime(&elfTimeNow, &binTime) < 0) {
            if (!ConvertOrCopy(bin, elf)) DeleteFileW(elf.c_str());
        }
        if (Exists(elf)) executable = elf;
    }
    if (executable.empty()) {
        Fail(L"The game files were not found.\n\nCopy your own decrypted " LAUNCHER_NAME L" files (eboot.elf or eboot.bin, sce_sys, sce_module, ...) "
             L"into the same folder as this program, then start it again.\n\nFolder: " + directory);
        return 1;
    }
    if (!IsElf(executable)) {
        Fail(L"The game executable is not decrypted. Please provide a decrypted eboot.elf next to this program.");
        return 1;
    }
    {
        const std::wstring required = LAUNCHER_REQUIRED;
        std::wstring missing;
        for (size_t start = 0; start < required.size();) {
            size_t end = required.find(L';', start);
            if (end == std::wstring::npos) end = required.size();
            const std::wstring item = required.substr(start, end - start);
            if (!item.empty() && !Exists(directory + L"\\" + item)) missing += (missing.empty() ? L"" : L", ") + item;
            start = end + 1;
        }
        if (!missing.empty()) {
            Fail(L"Some game files are missing (" + missing + L"). Copy the complete game folder next to this program.");
            return 1;
        }
    }
    if (!IsDirectory(directory + L"\\libs") || !Exists(directory + L"\\tools\\relinker.exe")) {
        Fail(L"Program files are missing (libs and tools folders). Please extract the whole download again.");
        return 1;
    }

    CreateDirectoryW((directory + L"\\logs").c_str(), nullptr);
    const std::wstring runtime = directory + L"\\" + kRuntime;
    const std::wstring stampFile = directory + L"\\prep\\prepared.stamp";
    const std::wstring stampNow = PreparedStamp(directory, executable);
    const std::wstring stampOld = ReadStamp(stampFile);
    if (Exists(runtime) && stampOld.empty()) WriteStamp(stampFile, stampNow);
    const bool stale = !Exists(runtime) || (!stampOld.empty() && stampOld != stampNow);
    if (stale) {
        Splash splash;
        splash.Start();
        {
            std::wstring failed;
            PrepareModules(directory, directory + L"\\prep", failed);
            if (!failed.empty()) {
                splash.Close();
                Fail(L"Some game modules could not be converted (" + failed + L").\n\nThey must be decrypted files from your own copy of the game.");
                return 1;
            }
        }
        splash.Status(L"Building the game program...");
        SetFileAttributesW(runtime.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(runtime.c_str());
        const std::wstring registry = directory + L"\\" + std::wstring(kRuntime, wcslen(kRuntime) - 4) + L".registry.json";
        SetFileAttributesW(registry.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(registry.c_str());
        const std::wstring command = L"\"" + directory + L"\\tools\\relinker.exe\" --windows --registry --windows-diagnostics "

        L"--skip-syscall-check --exclude-sce-module CommonDialog.prx --exclude-sce-module VoiceInputPlugin.prx --exclude-sce-module libSceNpCppWebApi.prx "
        L"--defer-sce-module PSN.prx --defer-sce-module SaveData.prx --defer-sce-module lib_burst_generated.prx "
        L"\"" + executable + L"\" \"" + runtime + L"\"";
        const long code = RunHidden(command, directory, directory + L"\\logs\\prepare.log");
        if (code != 0 || !Exists(runtime)) {
            splash.Close();
            Fail(L"The game could not be prepared from your files (exit code " + std::to_wstring(code) + L").\n\nSee logs\\prepare.log. "
                 L"Make sure eboot.elf is the decrypted executable of " LAUNCHER_NAME L" (" LAUNCHER_TITLE_ID L").");
            return 1;
        }

        MoveFileExW((directory + L"\\windows-diagnostics-imports.txt").c_str(), (directory + L"\\logs\\windows-diagnostics-imports.txt").c_str(), MOVEFILE_REPLACE_EXISTING);
        SetFileAttributesW(runtime.c_str(), FILE_ATTRIBUTE_HIDDEN);
        SetFileAttributesW(registry.c_str(), FILE_ATTRIBUTE_HIDDEN);
        WriteStamp(stampFile, stampNow);
    }

    for (const wchar_t* name : {L"Media", L"sce_sys"}) LinkIntoApp0(directory, name);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    HICON gameIcon = nullptr;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) == Gdiplus::Ok) {
        const std::wstring png = directory + L"\\sce_sys\\icon0.png";
        const std::wstring ico = directory + L"\\logs\\" LAUNCHER_ICON;
        if (Exists(png)) {
            gameIcon = LoadGameIcon(png, ico);
            const std::wstring link = directory + L"\\" LAUNCHER_NAME L".lnk";
            if (!Exists(link) && SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
                CreateShortcut(link, modulePath, directory, ico);
                CoUninitialize();
            }
        }
    }

    for (int attempt = 1;; ++attempt) {

    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    HANDLE out = CreateFileW((directory + L"\\logs\\game.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE err = CreateFileW((directory + L"\\logs\\game.err").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = out;
    startup.hStdError = err;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    const std::wstring pipelineCache = directory + L"\\pipeline_cache.bin";
    SetFileAttributesW(pipelineCache.c_str(), FILE_ATTRIBUTE_NORMAL);
    ApplyResolution(directory);
    {

        FILE* debug = nullptr;
        if (_wfopen_s(&debug, (directory + L"\\debug_env.txt").c_str(), L"r") == 0 && debug != nullptr) {
            wchar_t entry[512];
            while (fgetws(entry, 512, debug) != nullptr) {
                std::wstring text = entry;
                while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L' ')) text.pop_back();
                const auto equals = text.find(L'=');
                if (equals != std::wstring::npos && equals != 0 && text[0] != L'#') SetEnvironmentVariableW(text.substr(0, equals).c_str(), text.substr(equals + 1).c_str());
            }
            fclose(debug);
        }
    }
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + runtime + L"\"";
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process)) {
        Fail(L"The game could not be started.");
        return 1;
    }
    CloseHandle(out);
    CloseHandle(err);
    const DWORD started = GetTickCount();
    if (gameIcon != nullptr) {

        WindowSearch search{process.dwProcessId, gameIcon, false};
        for (int i = 0; i < 240 && !search.found && WaitForSingleObject(process.hProcess, 250) == WAIT_TIMEOUT; ++i) {
            EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&search));
        }
    }

    bool hung = false;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);

    const bool crashed = (code & 0xf0000000u) == 0xc0000000u && code != 0xcfffffffu;
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    SetFileAttributesW(pipelineCache.c_str(), FILE_ATTRIBUTE_HIDDEN);
    {

        char line[160];
        const int length = std::snprintf(line, sizeof(line), "attempt %d: %s, exit code 0x%lx after %lu s\r\n", attempt, hung ? "hung during startup" : "ended", code, (GetTickCount() - started) / 1000);
        HANDLE journal = CreateFileW((directory + L"\\logs\\launcher.log").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (journal != INVALID_HANDLE_VALUE) { DWORD put = 0; WriteFile(journal, line, static_cast<DWORD>(length), &put, nullptr); CloseHandle(journal); }
        if (hung || code != 0) CopyFileW((directory + L"\\logs\\game.err").c_str(), (directory + L"\\logs\\game.err." + std::to_wstring(attempt)).c_str(), FALSE);
    }

    if (crashed && GetTickCount() - started < 60000) {
        Fail(L"The game stopped unexpectedly (code " + std::to_wstring(code) + L").\n\nDetails are in logs\\game.err.");
    }
    return static_cast<int>(code);
    }
}
