#include "Application/SceneFileDialog.h"

#include <cstdio>
#include <string>

namespace MDSS
{
    namespace
    {
        std::optional<std::filesystem::path> RunNativeDialog(bool bSave)
        {
#if defined(_WIN32)
            const char* Command = bSave
                                     ? "powershell -NoProfile -STA -Command \"Add-Type -AssemblyName System.Windows.Forms; $d=New-Object System.Windows.Forms.SaveFileDialog; $d.Filter='Scene files (*.Scene)|*.Scene'; $d.DefaultExt='Scene'; $d.AddExtension=$true; if($d.ShowDialog() -eq 'OK'){[Console]::WriteLine($d.FileName)}\""
                                     : "powershell -NoProfile -STA -Command \"Add-Type -AssemblyName System.Windows.Forms; $d=New-Object System.Windows.Forms.OpenFileDialog; $d.Filter='Scene files (*.Scene)|*.Scene'; if($d.ShowDialog() -eq 'OK'){[Console]::WriteLine($d.FileName)}\"";
            FILE* Pipe = _popen(Command, "r");
#elif defined(__APPLE__)
            const char* Command = bSave
                                     ? "osascript -e 'POSIX path of (choose file name with prompt \"Save Scene\" default name \"Scene.Scene\")' 2>/dev/null"
                                     : "osascript -e 'POSIX path of (choose file with prompt \"Load Scene\")' 2>/dev/null";
            FILE* Pipe = popen(Command, "r");
#else
            const char* Command = bSave
                                     ? "if command -v zenity >/dev/null 2>&1; then zenity --file-selection --save --confirm-overwrite --filename=Scene.Scene --file-filter='Scene files | *.Scene' 2>/dev/null; else kdialog --getsavefilename \"$PWD/Scene.Scene\" '*.Scene' 2>/dev/null; fi"
                                     : "if command -v zenity >/dev/null 2>&1; then zenity --file-selection --file-filter='Scene files | *.Scene' 2>/dev/null; else kdialog --getopenfilename \"$PWD\" '*.Scene' 2>/dev/null; fi";
            FILE* Pipe = popen(Command, "r");
#endif
            if (Pipe == nullptr)
            {
                return std::nullopt;
            }

            std::string Result;
            char        Buffer[512];
            while (std::fgets(Buffer, sizeof(Buffer), Pipe) != nullptr)
            {
                Result += Buffer;
            }
#if defined(_WIN32)
            _pclose(Pipe);
#else
            pclose(Pipe);
#endif
            while (!Result.empty() && (Result.back() == '\n' || Result.back() == '\r'))
            {
                Result.pop_back();
            }
            if (Result.empty())
            {
                return std::nullopt;
            }
            return std::filesystem::path(Result);
        }
    } // namespace

    std::optional<std::filesystem::path> TSceneFileDialog::OpenScene()
    {
        return RunNativeDialog(false);
    }

    std::optional<std::filesystem::path> TSceneFileDialog::SaveScene()
    {
        return RunNativeDialog(true);
    }
} // namespace MDSS
