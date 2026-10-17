#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr int applicationResource = 101;
constexpr int bridgeResource = 102;

std::filesystem::path environmentPath(const wchar_t* name)
{
    std::vector<wchar_t> value(32768);
    const auto length = GetEnvironmentVariableW(name, value.data(),
                                                 static_cast<DWORD>(value.size()));
    return length > 0 && length < value.size() ? std::filesystem::path(value.data())
                                                : std::filesystem::path();
}

std::filesystem::path applicationDirectory()
{
    return environmentPath(L"LOCALAPPDATA") / L"Programs" / L"MixDebugger";
}

std::filesystem::path reaperPluginDirectory()
{
    return environmentPath(L"APPDATA") / L"REAPER" / L"UserPlugins";
}

std::filesystem::path reaperEffectDirectory()
{
    return environmentPath(L"APPDATA") / L"REAPER" / L"Effects" / L"MixDebugger";
}

std::filesystem::path startMenuShortcut()
{
    return environmentPath(L"APPDATA") / L"Microsoft" / L"Windows" / L"Start Menu"
         / L"Programs" / L"MixDebugger.lnk";
}

std::filesystem::path executablePath()
{
    std::vector<wchar_t> value(32768);
    const auto length = GetModuleFileNameW(nullptr, value.data(),
                                           static_cast<DWORD>(value.size()));
    return length > 0 && length < value.size() ? std::filesystem::path(value.data())
                                                : std::filesystem::path();
}

bool embeddedResourcePresent(const int resourceId)
{
    const auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    return resource != nullptr && SizeofResource(nullptr, resource) > 0;
}

bool writeEmbeddedFile(const int resourceId, const std::filesystem::path& destination)
{
    const auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (resource == nullptr)
        return false;
    const auto loaded = LoadResource(nullptr, resource);
    const auto* data = static_cast<const char*>(LockResource(loaded));
    const auto size = SizeofResource(nullptr, resource);
    if (data == nullptr || size == 0)
        return false;

    if (std::filesystem::exists(destination)
        && std::filesystem::file_size(destination) == size)
    {
        std::ifstream existing(destination, std::ios::binary);
        std::vector<char> contents(size);
        existing.read(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (existing && std::equal(contents.begin(), contents.end(), data))
            return true;
    }

    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error)
        return false;

    const auto temporary = destination.wstring() + L".installing";
    {
        std::ofstream output(std::filesystem::path(temporary),
                             std::ios::binary | std::ios::trunc);
        output.write(data, static_cast<std::streamsize>(size));
        if (!output)
            return false;
    }

    while (!MoveFileExW(temporary.c_str(), destination.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const auto choice = MessageBoxW(
            nullptr,
            L"Nie można zaktualizować plików MixDebugger. Zamknij MixDebugger i REAPER, a następnie kliknij Ponów próbę.",
            L"MixDebugger — instalacja", MB_RETRYCANCEL | MB_ICONWARNING);
        if (choice != IDRETRY)
        {
            DeleteFileW(temporary.c_str());
            return false;
        }
    }
    return true;
}

bool setRegistryString(HKEY key, const wchar_t* name, const std::wstring& value)
{
    return RegSetValueExW(key, name, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool registerInstallation(const std::filesystem::path& directory)
{
    HKEY productKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MixDebugger", 0, nullptr, 0,
                        KEY_WRITE, nullptr, &productKey, nullptr) != ERROR_SUCCESS)
        return false;
    const auto productOk = setRegistryString(productKey, L"InstallPath", directory.wstring());
    RegCloseKey(productKey);

    HKEY uninstallKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MixDebugger",
                        0, nullptr, 0, KEY_WRITE, nullptr, &uninstallKey, nullptr) != ERROR_SUCCESS)
        return false;
    const auto uninstaller = directory / L"Uninstall.exe";
    const auto uninstallCommand = L"\"" + uninstaller.wstring() + L"\" --uninstall";
    DWORD one = 1;
    const auto uninstallOk = setRegistryString(uninstallKey, L"DisplayName", L"MixDebugger")
        && setRegistryString(uninstallKey, L"DisplayVersion", L"0.2.0")
        && setRegistryString(uninstallKey, L"Publisher", L"MixDebugger")
        && setRegistryString(uninstallKey, L"InstallLocation", directory.wstring())
        && setRegistryString(uninstallKey, L"DisplayIcon",
                             (directory / L"MixDebugger.exe").wstring())
        && setRegistryString(uninstallKey, L"UninstallString", uninstallCommand)
        && RegSetValueExW(uninstallKey, L"NoModify", 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&one), sizeof(one)) == ERROR_SUCCESS
        && RegSetValueExW(uninstallKey, L"NoRepair", 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&one), sizeof(one)) == ERROR_SUCCESS;
    RegCloseKey(uninstallKey);
    return productOk && uninstallOk;
}

bool createShortcut(const std::filesystem::path& target,
                    const std::filesystem::path& shortcut)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return false;
    IShellLinkW* shellLink = nullptr;
    auto result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (SUCCEEDED(result))
    {
        shellLink->SetPath(target.c_str());
        shellLink->SetWorkingDirectory(target.parent_path().c_str());
        shellLink->SetDescription(L"MixDebugger — analiza stemów i maskowania");
        IPersistFile* persist = nullptr;
        result = shellLink->QueryInterface(IID_IPersistFile,
                                           reinterpret_cast<void**>(&persist));
        if (SUCCEEDED(result))
        {
            result = persist->Save(shortcut.c_str(), TRUE);
            persist->Release();
        }
        shellLink->Release();
    }
    CoUninitialize();
    return SUCCEEDED(result);
}

int install()
{
    const auto appDirectory = applicationDirectory();
    const auto app = appDirectory / L"MixDebugger.exe";
    const auto bridge = reaperPluginDirectory() / L"reaper_mixdebugger.dll";
    const auto uninstaller = appDirectory / L"Uninstall.exe";

    if (!writeEmbeddedFile(applicationResource, app)
        || !writeEmbeddedFile(bridgeResource, bridge))
    {
        MessageBoxW(nullptr, L"Instalacja nie została ukończona.",
                    L"MixDebugger", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::error_code error;
    std::filesystem::copy_file(executablePath(), uninstaller,
                               std::filesystem::copy_options::overwrite_existing, error);
    if (error || !registerInstallation(appDirectory) || !createShortcut(app, startMenuShortcut()))
    {
        MessageBoxW(nullptr,
                    L"Pliki zostały skopiowane, ale nie udało się dokończyć rejestracji programu.",
                    L"MixDebugger", MB_OK | MB_ICONWARNING);
        return 1;
    }

    const auto launch = MessageBoxW(
        nullptr,
        L"MixDebugger został zainstalowany. Przy pierwszej instalacji uruchom ponownie REAPER, aby pojawiła się akcja:\n\nExtensions > MixDebugger: analizuj projekt...\n\nUruchomić teraz aplikację?",
        L"MixDebugger — gotowe", MB_YESNO | MB_ICONINFORMATION);
    if (launch == IDYES)
        ShellExecuteW(nullptr, L"open", app.c_str(), nullptr, appDirectory.c_str(), SW_SHOWNORMAL);
    return 0;
}

int uninstall()
{
    if (MessageBoxW(nullptr, L"Odinstalować MixDebugger i integrację z REAPEREM?",
                    L"MixDebugger", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 0;

    DeleteFileW((reaperPluginDirectory() / L"reaper_mixdebugger.dll").c_str());
    DeleteFileW((reaperEffectDirectory() / L"Technical EQ").c_str());
    DeleteFileW((reaperEffectDirectory() / L"Technical Compressor").c_str());
    DeleteFileW(startMenuShortcut().c_str());
    DeleteFileW((applicationDirectory() / L"MixDebugger.exe").c_str());
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\MixDebugger");
    RegDeleteTreeW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MixDebugger");
    MoveFileExW(executablePath().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    MessageBoxW(nullptr, L"MixDebugger został odinstalowany.",
                L"MixDebugger", MB_OK | MB_ICONINFORMATION);
    return 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int)
{
    const std::wstring arguments(commandLine);
    if (arguments.find(L"--verify") != std::wstring::npos)
        return embeddedResourcePresent(applicationResource)
            && embeddedResourcePresent(bridgeResource) ? 0 : 2;
    return arguments.find(L"--uninstall") != std::wstring::npos ? uninstall() : install();
}

