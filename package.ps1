# package.ps1 — Build portable static exe + đóng gói zip để share.
# Usage: .\package.ps1                       (build + package)
#        .\package.ps1 -SkipBuild            (chỉ package từ build hiện tại)
#        .\package.ps1 -OutDir D:\release    (custom output dir)
#        .\package.ps1 -Version 1.2.3        (suffix vào tên zip)

param(
    [switch]$SkipBuild,
    [string]$OutDir = "dist",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source }
if (-not $cmake) { Write-Error "cmake.exe không tìm thấy"; exit 1 }

if (-not $SkipBuild) {
    Write-Host "[1/4] Configure portable preset..." -ForegroundColor Cyan
    if (-not (Test-Path "build-portable\CMakeCache.txt")) {
        & $cmake --preset portable
        if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }
    } else {
        Write-Host "      (đã configure trước đó, skip)"
    }

    Write-Host "[2/4] Build static exe..." -ForegroundColor Cyan
    & $cmake --build build-portable --config Release --target WindowHelper
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }
} else {
    Write-Host "[1-2/4] Skip build (SkipBuild flag)" -ForegroundColor Yellow
}

Write-Host "[3/4] Đóng gói..." -ForegroundColor Cyan
$exe = Get-ChildItem build-portable\bin\Release\svc_*.exe | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $exe) { Write-Error "Không tìm thấy svc_*.exe"; exit 1 }

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutDir | Out-Null

Copy-Item $exe.FullName $OutDir\
Copy-Item config.json $OutDir\
if (Test-Path docs\ui-parameters-guide.md) {
    Copy-Item docs\ui-parameters-guide.md "$OutDir\HUONG-DAN-CAU-HINH.md"
}

# README cho user
$readme = @"
# WindowHelper — PT Bot

## Cách dùng
1. Mở Priston Tale trước.
2. Double-click ``$($exe.Name)``.
3. Bật AUTO trong UI hoặc nhấn F8 toggle.

## Yêu cầu
- Windows 10 1809+ hoặc Windows 11.
- Không cần cài VC++ Redist hoặc bất kỳ thư viện nào khác.

## Cấu hình
Mở ``HUONG-DAN-CAU-HINH.md`` để xem giải thích các tham số.
File ``config.json`` lưu cấu hình — có thể edit trực tiếp hoặc qua UI.

## Log
Log chạy được ghi tại ``logs/WindowHelper.log`` cạnh exe.
"@
$readme | Out-File -Encoding UTF8 "$OutDir\README.txt"

Write-Host "[4/4] Zip..." -ForegroundColor Cyan
$ts = Get-Date -Format "yyMMdd-HHmm"
$suffix = if ($Version) { "-v$Version" } else { "-$ts" }
$zipName = "WindowHelper$suffix.zip"
if (Test-Path $zipName) { Remove-Item $zipName -Force }
Compress-Archive -Path "$OutDir\*" -DestinationPath $zipName

$exeSize = [math]::Round($exe.Length / 1MB, 2)
$zipSize = [math]::Round((Get-Item $zipName).Length / 1MB, 2)

Write-Host ""
Write-Host "DONE!" -ForegroundColor Green
Write-Host "  EXE:     $($exe.Name) ($exeSize MB)"
Write-Host "  Folder:  $OutDir\ ($((Get-ChildItem $OutDir).Count) files)"
Write-Host "  Zip:     $zipName ($zipSize MB)"
Write-Host ""
Write-Host "Gửi $zipName cho user. Họ extract -> chạy $($exe.Name) -> xong." -ForegroundColor Cyan
