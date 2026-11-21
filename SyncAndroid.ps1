# PC 版のソース (System.h / Include.h / Library) を Android 版のプロジェクトへコピーする。
# Android 版は PC 版と同じソースを NDK でビルドする (Android 専用の部分は #ifdef __ANDROID__ で分けてある)。
# Main.cpp と CMakeLists.txt は Android 版のものをそのまま使う。
param(
    [string]$Android = "$env:USERPROFILE\AndroidStudioProjects\FreedomApopicStyle\app\src\main\cpp"
)
$ErrorActionPreference = 'Stop'
$src = $PSScriptRoot
if (-not (Test-Path $Android)) { throw "Android 版のフォルダが見つかりません: $Android" }
Copy-Item "$src\System.h", "$src\Include.h" $Android -Force
New-Item -ItemType Directory -Force "$Android\Library" | Out-Null
Copy-Item "$src\Library\*.hpp", "$src\Library\Include.h" "$Android\Library" -Force
"コピーしました: $Android"
