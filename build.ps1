# build.ps1 — Rebuild WindowHelper dev (dynamic) tại build/bin/Release/.
# Tự kill exe đang chạy + setup VS Developer environment + cmake build.
# Usage: .\build.ps1
#        .\build.ps1 -NoKill         (không tự kill exe, dừng nếu file lock)
#        .\build.ps1 -Configure      (force re-configure preset trước build)

param(
    [switch]$NoKill,
    [switch]$Configure
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$buildDir = Join-Path $root "build"
$exeGlob = Join-Path $buildDir "bin\Release\svc_*.exe"

# 1. Locate VS + cmake
$vsRoot = "C:\Program Files\Microsoft Visual Studio\18\Insiders"
$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
$cmake  = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $vcvars)) { Write-Error "vcvars64.bat không tìm thấy tại $vcvars"; exit 1 }
if (-not (Test-Path $cmake))  { Write-Error "cmake.exe không tìm thấy tại $cmake"; exit 1 }

# 2. Kill running exe (tránh LNK1104)
if (-not $NoKill) {
    $running = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like $exeGlob }
    foreach ($p in $running) {
        Write-Host "[kill] PID $($p.Id): $($p.Path)" -ForegroundColor Yellow
        Stop-Process -Id $p.Id -Force
    }
    # Wait briefly để OS release file handle sau khi kill
    if ($running) { Start-Sleep -Milliseconds 300 }
}

# 2b. Delete stale exe(s) — random suffix svc_XXXX.exe stale có thể gây nhầm lẫn bản mới.
$stale = Get-ChildItem $exeGlob -ErrorAction SilentlyContinue
foreach ($f in $stale) {
    try {
        Remove-Item -LiteralPath $f.FullName -Force -ErrorAction Stop
        Write-Host "[clean] removed $($f.Name)" -ForegroundColor DarkYellow
    } catch {
        Write-Warning "[clean] không xóa được $($f.Name): $_"
    }
}

# 3. (Optional) configure
if ($Configure -or -not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    Write-Host "[configure] cmake --preset default" -ForegroundColor Cyan
    cmd /c "`"$vcvars`" && `"$cmake`" --preset default"
    if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }
}

# 4. Build
Write-Host "[build] WindowHelper Release..." -ForegroundColor Cyan
cmd /c "`"$vcvars`" && `"$cmake`" --build `"$buildDir`" --config Release --target WindowHelper"
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

# 5. Report
$exe = Get-ChildItem $exeGlob -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) {
    Write-Host "`n[ok] Built: $($exe.FullName)" -ForegroundColor Green
    Write-Host "     Size:  $([math]::Round($exe.Length / 1MB, 2)) MB"
    Write-Host "     Time:  $($exe.LastWriteTime)"
} else {
    Write-Warning "Build OK nhưng không tìm thấy svc_*.exe trong $($exeGlob)"
}
