# ============================================================================
#  build.ps1 -- 编译原理课程设计系统 一键构建脚本
# ----------------------------------------------------------------------------
#  功能：按 AGENT.md §2 锁定的工具链（Qt 6.10.2 mingw_64 + Qt 自带 MinGW 13.1.0）
#        用 qmake + mingw32-make 构建主程序，并把 data/ 下的语言数据复制到
#        构建输出目录。
#  用法：powershell -ExecutionPolicy Bypass -File tools\build.ps1 [-Clean] [-Debug]
#  注意：
#    1. 务必使用 Qt 自带的 MinGW（Tools\mingw1310_64），
#       不要用 D:\mingw\mingw64（g++ 15.2.0），否则与 Qt 的 ABI 不匹配；
#    2. -Clean 会删除 build/ 目录后重建；
#    3. 构建输出：build\CompilerCourseDesign\release\CompilerCourseDesign.exe
# ============================================================================

[CmdletBinding()]
param(
    [switch]$Clean,        # 清除构建目录后重新构建
    [switch]$DebugBuild,   # 构建 Debug 版本（参数名避开 -Debug 保留参数）
    [int]$Jobs = 8         # 并行编译任务数
)

$ErrorActionPreference = 'Stop'

# ---- 路径常量（与 AGENT.md §2 一致，换机时只需改这里）----
$QtRoot   = 'D:\Qt6.10\6.10.2\mingw_64'
$QtTools  = 'D:\Qt6.10\Tools\mingw1310_64'
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildDir = Join-Path $RepoRoot 'build\CompilerCourseDesign'
$ProFile  = Join-Path $RepoRoot 'CompilerCourseDesign.pro'

function Write-Step([string]$Text) {
    Write-Host "`n==> $Text" -ForegroundColor Cyan
}

function Assert-Path([string]$Path, [string]$What) {
    if (-not (Test-Path $Path)) {
        throw "找不到$What：$Path`n请核对 AGENT.md §2 中的工具链路径。"
    }
}

# ---- 0. 检查工具链 ----
Write-Step '检查构建工具链'
Assert-Path $QtRoot  'Qt 安装目录'
Assert-Path $QtTools 'Qt 自带的 MinGW 目录'
$qmake   = Join-Path $QtRoot  'bin\qmake.exe'
$gxx     = Join-Path $QtTools 'bin\g++.exe'
$mingw32 = Join-Path $QtTools 'bin\mingw32-make.exe'
Assert-Path $qmake   'qmake.exe'
Assert-Path $gxx     'g++.exe'
Assert-Path $mingw32 'mingw32-make.exe'
Assert-Path $ProFile '工程文件 CompilerCourseDesign.pro'

# 把 Qt 的 MinGW 与 Qt 的 bin 放在 PATH 最前面，确保 qmake/make 使用配套的 g++
$env:Path = "$(Join-Path $QtTools 'bin');$(Join-Path $QtRoot 'bin');$env:Path"

$gccVersion = (& $gxx -dumpversion)
Write-Host "   Qt      : $QtRoot"
Write-Host "   MinGW   : $QtTools  (g++ $gccVersion)"
Write-Host "   工程文件: $ProFile"

# ---- 1. 准备构建目录 ----
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "清除构建目录 $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

# ---- 2. 生成 Makefile ----
Write-Step 'qmake 生成 Makefile'
Push-Location $BuildDir
try {
    $qmakeArgs = @($ProFile)
    if ($DebugBuild) { $qmakeArgs += 'CONFIG+=debug' } else { $qmakeArgs += 'CONFIG+=release' }
    & $qmake @qmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "qmake 失败（退出码 $LASTEXITCODE）" }

    # ---- 3. 编译 ----
    Write-Step "mingw32-make -j$Jobs"
    & $mingw32 "-j$Jobs"
    if ($LASTEXITCODE -ne 0) { throw "编译失败（退出码 $LASTEXITCODE）" }
}
finally {
    Pop-Location
}

# ---- 4. 报告产物 ----
Write-Step '构建结果'
$config = if ($DebugBuild) { 'debug' } else { 'release' }
$exeCandidates = @(
    (Join-Path $BuildDir "$config\CompilerCourseDesign.exe"),
    (Join-Path $BuildDir 'CompilerCourseDesign.exe')
)
$exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    throw "构建成功但未找到可执行文件，请检查 $BuildDir 下的输出。"
}
$sizeKb = [math]::Round((Get-Item $exe).Length / 1KB, 1)
Write-Host "   可执行文件: $exe  ($sizeKb KB)" -ForegroundColor Green

# 检查数据文件是否已复制
$dataDir = Join-Path $BuildDir 'data'
if (Test-Path $dataDir) {
    $count = (Get-ChildItem $dataDir -File).Count
    Write-Host "   数据文件  : $dataDir  ($count 个)" -ForegroundColor Green
} else {
    Write-Warning "数据目录未生成：$dataDir（请检查 .pro 中的复制规则）"
}

Write-Host "`n构建完成。" -ForegroundColor Green
Write-Host "运行： & '$exe'" -ForegroundColor Gray
Write-Host "部署 Qt DLL（首次/换机后执行一次）： & '$RepoRoot\tools\deploy.ps1'" -ForegroundColor Gray
