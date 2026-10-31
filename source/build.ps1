$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & windres app.rc -O coff -o app.res
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
    & g++ main.cpp app.res -std=c++17 -O2 -Wall -Wextra -Wno-misleading-indentation -municode -mwindows -static -s -o ../Island.exe -lcomctl32 -lole32 -luuid -ldwmapi -luxtheme -lwinmm -lshell32 -ladvapi32 -lgdi32 -lgdiplus -lcomdlg32
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
} finally {
    Remove-Item -LiteralPath (Join-Path $PSScriptRoot 'app.res') -ErrorAction SilentlyContinue
    Pop-Location
}
