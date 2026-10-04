#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <algorithm>
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

bool IsDirectory(const std::wstring& path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

FILETIME WriteTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return FILETIME{};
    return data.ftLastWriteTime;
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
    if (Exists(link) || !Exists(directory + L"\\" + name)) return;
    CreateDirectoryW((directory + L"\\app0").c_str(), nullptr);
    RunHidden(L"cmd.exe /c mklink /J \"" + link + L"\" \"" + directory + L"\\" + name + L"\"", directory, directory + L"\\logs\\link.log");
}

}

struct Splash {
    HANDLE thread = nullptr;
    HWND window = nullptr;
    HANDLE ready = nullptr;
    HWND status = nullptr;
    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_CLOSE) return 0;
        if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    static DWORD WINAPI Run(LPVOID parameter) {
        auto* self = static_cast<Splash*>(parameter);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = Proc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"SplashPrepare";
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&windowClass);
        const int width = 560, height = 240;
        self->window = CreateWindowExW(WS_EX_TOPMOST, windowClass.lpszClassName, LAUNCHER_TITLE, WS_CAPTION | WS_VISIBLE, (GetSystemMetrics(SM_CXSCREEN) - width) / 2, (GetSystemMetrics(SM_CYSCREEN) - height) / 2, width, height, nullptr, nullptr, windowClass.hInstance, nullptr);
        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        auto label = [&](const wchar_t* text, int y, int h) {
            HWND control = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, 20, y, width - 56, h, self->window, nullptr, windowClass.hInstance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return control;
        };
        label(L"Getting ready for the first start", 14, 24);
        label(L"The game is being prepared from your own files. This happens only once, takes about two minutes and needs no internet. "
              L"The game window opens by itself when it is done - please do not close this program.", 44, 90);
        self->status = label(L"Converting game files...", 150, 24);
        SetEvent(self->ready);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
        return 0;
    }
    void Start() {
        ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        thread = CreateThread(nullptr, 0, Run, this, 0, nullptr);
        WaitForSingleObject(ready, 5000);
    }
    void Status(const wchar_t* text) { if (status != nullptr) SetWindowTextW(status, text); }
    ~Splash() {
        if (window != nullptr) PostMessageW(window, WM_DESTROY, 0, 0);
        if (thread != nullptr) { WaitForSingleObject(thread, 2000); CloseHandle(thread); }
        if (ready != nullptr) CloseHandle(ready);
    }
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
    const FILETIME runtimeTime = WriteTime(runtime);
    const FILETIME elfTime = WriteTime(executable);
    const FILETIME relinkerTime = WriteTime(directory + L"\\tools\\relinker.exe");
    const bool stale = !Exists(runtime) || CompareFileTime(&runtimeTime, &elfTime) < 0 || CompareFileTime(&runtimeTime, &relinkerTime) < 0;
    if (stale) {
        Splash splash;
        splash.Start();
        {
            std::wstring failed;
            PrepareModules(directory, directory + L"\\prep", failed);
            if (!failed.empty()) {
                Fail(L"Some game modules could not be converted (" + failed + L").\n\nThey must be decrypted files from your own copy of the game.");
                return 1;
            }
        }
        splash.Status(L"Building the game program (the longest step)...");
        SetFileAttributesW(runtime.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(runtime.c_str());
        const std::wstring command = L"\"" + directory + L"\\tools\\relinker.exe\" --windows --registry --windows-diagnostics "

        L"--skip-syscall-check --exclude-sce-module CommonDialog.prx --exclude-sce-module VoiceInputPlugin.prx --exclude-sce-module libSceNpCppWebApi.prx "
        L"--defer-sce-module PSN.prx --defer-sce-module SaveData.prx --defer-sce-module lib_burst_generated.prx "
        L"\"" + executable + L"\" \"" + runtime + L"\"";
        const long code = RunHidden(command, directory, directory + L"\\logs\\prepare.log");
        if (code != 0 || !Exists(runtime)) {
            Fail(L"The game could not be prepared from your files (exit code " + std::to_wstring(code) + L").\n\nSee logs\\prepare.log. "
                 L"Make sure eboot.elf is the decrypted executable of " LAUNCHER_NAME L" (" LAUNCHER_TITLE_ID L").");
            return 1;
        }

        MoveFileExW((directory + L"\\windows-diagnostics-imports.txt").c_str(), (directory + L"\\logs\\windows-diagnostics-imports.txt").c_str(), MOVEFILE_REPLACE_EXISTING);
        SetFileAttributesW(runtime.c_str(), FILE_ATTRIBUTE_HIDDEN);
        SetFileAttributesW((directory + L"\\" + std::wstring(kRuntime, wcslen(kRuntime) - 4) + L".registry.json").c_str(), FILE_ATTRIBUTE_HIDDEN);
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
