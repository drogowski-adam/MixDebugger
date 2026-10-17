#include <windows.h>
#include <shellapi.h>
#include <reaper_plugin.h>
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_AddExtensionsMainMenu
#define REAPERAPI_WANT_EnumProjects
#define REAPERAPI_WANT_GetProjectLength
#define REAPERAPI_WANT_GetSet_LoopTimeRange2
#define REAPERAPI_WANT_GetSetProjectInfo
#define REAPERAPI_WANT_GetSetProjectInfo_String
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_PCM_Source_CreateFromFile
#define REAPERAPI_WANT_PCM_Source_Destroy
#define REAPERAPI_WANT_ShowMessageBox
#define REAPERAPI_IMPLEMENT
#include <reaper_plugin_functions.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
reaper_plugin_info_t* reaper = nullptr;
int openCommandId = 0;
gaccel_register_t openAction {};
std::string jsonEscape(const std::string_view value)
{
    std::ostringstream out;
    for (const unsigned char character : value)
    {
        switch (character)
        {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (character < 0x20)
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(character) << std::dec;
                else
                    out << character;
        }
    }
    return out.str();
}

std::wstring widen(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    const auto length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                             static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                        result.data(), length);
    return result;
}

std::string narrow(const std::wstring& wide)
{
    if (wide.empty())
        return {};
    const auto length = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                             static_cast<int>(wide.size()), nullptr, 0,
                                             nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

std::filesystem::path localAppData()
{
    wchar_t buffer[MAX_PATH] {};
    const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    return length > 0 && length < MAX_PATH
        ? std::filesystem::path(buffer)
        : std::filesystem::temp_directory_path();
}

std::filesystem::path makeSessionDirectory()
{
    const auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto path = localAppData() / L"MixDebugger" / L"Bridge" / std::to_wstring(ticks);
    std::filesystem::create_directories(path / L"renders");
    return path;
}

std::filesystem::path installedApplicationPath()
{
    wchar_t environmentPath[32768] {};
    const auto environmentLength = GetEnvironmentVariableW(
        L"MIXDEBUGGER_APP_PATH", environmentPath,
        static_cast<DWORD>(std::size(environmentPath)));
    if (environmentLength > 0 && environmentLength < std::size(environmentPath))
        return environmentPath;

    wchar_t registryPath[32768] {};
    DWORD bytes = sizeof(registryPath);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\MixDebugger", L"InstallPath",
                     RRF_RT_REG_SZ, nullptr, registryPath, &bytes) == ERROR_SUCCESS)
        return std::filesystem::path(registryPath) / L"MixDebugger.exe";

    return localAppData() / L"Programs" / L"MixDebugger" / L"MixDebugger.exe";
}

// Only temporary render configuration is writable. No track/FX mutation API is imported.
struct RenderSettings {
    ReaProject* project;
    std::vector<std::pair<std::string,double>> numbers;
    std::vector<std::pair<std::string,std::string>> strings;
    explicit RenderSettings(ReaProject* p) : project(p) {
        for(const auto* key : {"RENDER_SETTINGS","RENDER_BOUNDSFLAG","RENDER_SRATE","RENDER_CHANNELS",
             "RENDER_ADDTOPROJ","RENDER_NORMALIZE","RENDER_DITHER","RENDER_TAILFLAG","RENDER_STARTPOS","RENDER_ENDPOS"})
            numbers.emplace_back(key,GetSetProjectInfo(project,key,0,false));
        for(const auto* key : {"RENDER_FILE","RENDER_PATTERN","RENDER_FORMAT","RENDER_FORMAT2"}) {
            std::vector<char> buffer(32768);
            GetSetProjectInfo_String(project,key,buffer.data(),false);
            strings.emplace_back(key,buffer.data());
        }
    }
    ~RenderSettings() {
        for(const auto& [key,value]:numbers)GetSetProjectInfo(project,key.c_str(),value,true);
        for(auto& [key,value]:strings)GetSetProjectInfo_String(project,key.c_str(),value.data(),true);
    }
};
void renderAndOpen() {
    const auto application=installedApplicationPath();
    if(!std::filesystem::exists(application))throw std::runtime_error("MixDebugger.exe was not found. Install MixDebugger or set MIXDEBUGGER_APP_PATH.");
    auto* project=EnumProjects(-1,nullptr,0);
    if(!project)throw std::runtime_error("Open a REAPER project first.");
    double start=0,end=0;
    GetSet_LoopTimeRange2(project,false,false,&start,&end,false);
    if(end<=start){start=0;end=GetProjectLength(project);}
    if(end-start<0.4)throw std::runtime_error("The project or time selection must be at least 0.4 seconds long.");
    const auto directory=makeSessionDirectory()/L"renders";
    auto path=narrow(directory.wstring());
    std::string pattern="mix",wave="ZXZhdyABAQ==",empty;
    // WAV 32-bit float. All original render settings are restored on return/error.
    {
        RenderSettings restore(project);
        GetSetProjectInfo(project,"RENDER_SETTINGS",0,true);
        GetSetProjectInfo(project,"RENDER_BOUNDSFLAG",0,true);
        GetSetProjectInfo(project,"RENDER_STARTPOS",start,true);
        GetSetProjectInfo(project,"RENDER_ENDPOS",end,true);
        GetSetProjectInfo(project,"RENDER_SRATE",48000,true);
        GetSetProjectInfo(project,"RENDER_CHANNELS",2,true);
        GetSetProjectInfo(project,"RENDER_ADDTOPROJ",0,true);
        GetSetProjectInfo(project,"RENDER_NORMALIZE",0,true);
        GetSetProjectInfo(project,"RENDER_DITHER",0,true);
        GetSetProjectInfo(project,"RENDER_TAILFLAG",0,true);
        GetSetProjectInfo_String(project,"RENDER_FILE",path.data(),true);
        GetSetProjectInfo_String(project,"RENDER_PATTERN",pattern.data(),true);
        GetSetProjectInfo_String(project,"RENDER_FORMAT",wave.data(),true);
        GetSetProjectInfo_String(project,"RENDER_FORMAT2",empty.data(),true);
        Main_OnCommand(41824,0);
    }
    const auto audio=directory/L"mix.wav";
    if(!std::filesystem::exists(audio) || std::filesystem::file_size(audio)<=44)
        throw std::runtime_error("Render cancelled or no audio was produced.");
    auto* source=PCM_Source_CreateFromFile(narrow(audio.wstring()).c_str());
    if(!source)throw std::runtime_error("REAPER could not read the rendered snapshot.");
    const double renderedLength=source->GetLength();
    PCM_Source_Destroy(source);
    if(!std::isfinite(renderedLength) || std::abs(renderedLength-(end-start))>0.02)
        throw std::runtime_error("Render is incomplete or has an unexpected duration. Render again.");
    const auto args=std::wstring(L"--mix-file \"")+audio.wstring()+L"\"";
    const auto result=reinterpret_cast<std::intptr_t>(ShellExecuteW(reaper->hwnd_main,L"open",application.c_str(),args.c_str(),application.parent_path().c_str(),SW_SHOWNORMAL));
    if(result<=32)throw std::runtime_error("Could not launch MixDebugger.");
}
bool commandHook(int command,int) {
    if(command!=openCommandId)return false;
    try {renderAndOpen();}
    catch(const std::exception& e){ShowMessageBox(e.what(),"MixDebugger",0);}
    return true;
}
void menuHook(const char* menuId,void* menu,int flag) {
    if(flag==0 && std::string_view(menuId)=="Main extensions")
        AppendMenuW(static_cast<HMENU>(menu),MF_STRING,static_cast<UINT_PTR>(openCommandId),L"MixDebugger: compare mix with reference...");
}
void unregisterPlugin() {
    if(!reaper)return;
    reaper->Register("-hookcustommenu",reinterpret_cast<void*>(menuHook));
    reaper->Register("-hookcommand",reinterpret_cast<void*>(commandHook));
    reaper->Register("-gaccel",&openAction);
}
}
extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE,reaper_plugin_info_t* rec) {
    if(!rec){unregisterPlugin();return 0;}
    if(rec->caller_version!=REAPER_PLUGIN_VERSION || !rec->GetFunc || !rec->Register)return 0;
    reaper=rec;
    if(REAPERAPI_LoadAPI(rec->GetFunc)!=0)return 0;
    openCommandId=rec->Register("command_id",const_cast<char*>("MIXDEBUGGER_COMPARE_MASTER"));
    if(!openCommandId)return 0;
    openAction.accel.cmd=static_cast<WORD>(openCommandId);
    openAction.desc="MixDebugger: compare mix with reference (offline)";
    if(!rec->Register("gaccel",&openAction) || !rec->Register("hookcommand",reinterpret_cast<void*>(commandHook)) || !rec->Register("hookcustommenu",reinterpret_cast<void*>(menuHook))) {unregisterPlugin();return 0;}
    AddExtensionsMainMenu();
    return 1;
}

