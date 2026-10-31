$ErrorActionPreference = 'Stop'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('Island-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $testRoot 'work') -Force | Out-Null
$oldData = $env:ISLAND_DATA_DIR
$env:ISLAND_DATA_DIR = Join-Path $testRoot 'data'
Push-Location $PSScriptRoot
try {
    & windres app.rc -O coff -o (Join-Path $testRoot 'test.res')
    if ($LASTEXITCODE -ne 0) { throw 'Resource build failed' }
    $testExe = Join-Path $testRoot 'Island-test.exe'
    & g++ main.cpp (Join-Path $testRoot 'test.res') -DISLAND_SELF_TEST -std=c++17 -O2 -municode -mwindows -static -o $testExe -lcomctl32 -lole32 -luuid -ldwmapi -luxtheme -lwinmm -lshell32 -ladvapi32 -lgdi32 -lgdiplus -lcomdlg32
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
    $run = Start-Process -FilePath $testExe -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru
    if (-not $run.WaitForExit(20000)) { Stop-Process -Id $run.Id; throw 'Test timed out. Close any running Island instance and try again.' }
    if ($run.ExitCode -ne 0) { Get-Content (Join-Path $testRoot 'work/integration-v2-failure.txt') -ErrorAction SilentlyContinue; throw 'Integration test failed' }
    Get-Content (Join-Path $testRoot 'work/integration-v2-results.txt')
    & g++ timer_test.cpp -std=c++17 -O2 -static -o (Join-Path $testRoot 'timer-test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Timer test build failed' }
    & (Join-Path $testRoot 'timer-test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Timer test failed' }
    Write-Output ('Test images and fixtures: ' + $testRoot)
} finally {
    $env:ISLAND_DATA_DIR = $oldData
    Pop-Location
}
