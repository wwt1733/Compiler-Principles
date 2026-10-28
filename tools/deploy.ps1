# ============================================================================
#  deploy.ps1 -- 拷贝 Qt 运行库到可执行文件目录并做冒烟测试
# ----------------------------------------------------------------------------
#  功能：用 Qt 自带的 windeployqt 把主程序依赖的 Qt DLL、平台插件
#        （platforms\qwindows.dll）等拷到 exe 同目录，使程序可以脱离
#        Qt 安装目录直接运行（也便于打包交付）；随后离屏跑一次 --selftest
#        做冒烟验证。
#  用法：powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 [-Zip]
#  注意：
#    1. 首次构建后或换机后执行一次即可；只改代码重新构建时无需重复执行；
#    2. windeployqt 会向 stderr 输出无害警告（如缺少可选的 dxcompiler.dll），
#       本脚本用 cmd 重定向并临时放宽 ErrorActionPreference，避免把警告
#       当成终止性错误；
#    3. 加 -Zip 会在 build\ 下生成发布压缩包。
# ============================================================================

[CmdletBinding()]
param(
    [switch]$Zip   # 额外生成发布压缩包
)

$ErrorActionPreference = 'Stop'

# ---- 路径常量（与 AGENT.md §2 一致）----
$QtRoot     = 'D:\Qt6.10\6.10.2\mingw_64'
$QtTools    = 'D:\Qt6.10\Tools\mingw1310_64'
$RepoRoot   = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildDir   = Join-Path $RepoRoot 'build\CompilerCourseDesign'
$ReleaseDir = Join-Path $BuildDir 'release'

function Write-Step([string]$Text) {
    Write-Host "`n==> $Text" -ForegroundColor Cyan
}

# ---- 1. 定位可执行文件 ----
$exe = @(
    (Join-Path $ReleaseDir 'CompilerCourseDesign.exe'),
    (Join-Path $BuildDir 'CompilerCourseDesign.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $exe) {
    throw "未找到 CompilerCourseDesign.exe。请先运行 tools\build.ps1 构建。"
}
$exeDir = Split-Path -Parent $exe
Write-Host "   目标程序: $exe"

# ---- 2. 清理陈旧的 Qt 运行库（关键步骤，勿删）----
# 背景：windeployqt 发现目标目录已存在同名 DLL 时会**跳过覆盖**。若该目录里
#       残留着其它 Qt 版本（或其它 MinGW 构建）的 Qt6Core.dll，就会出现
#       "Qt6Core 与 Qt6Gui/Qt6Widgets 版本不配套"的情况，程序启动即崩溃
#       （0xC0000005，且启动日志一行都写不出来，极难排查）。
#       注意：不同版本 Qt6Core.dll 的**文件大小可能完全相同**（本例均为
#       11100000 字节），因此只能靠删除后重新部署来保证一致性。
# 做法：部署前删除 exe 目录下的全部 Qt6*.dll、MinGW 运行库与平台插件目录，
#       再由 windeployqt 重新从锁定的 Qt 安装目录拷贝一份干净的。
Write-Step '清理陈旧的 Qt 运行库（防止 windeployqt 跳过覆盖）'
$stalePatterns = @(
    'Qt6*.dll',
    'libstdc++-6.dll',
    'libgcc_s_seh-1.dll',
    'libwinpthread-1.dll',
    'D3Dcompiler_47.dll',
    'opengl32sw.dll'
)
$removed = 0
foreach ($pattern in $stalePatterns) {
    Get-ChildItem -Path $exeDir -Filter $pattern -File -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item $_.FullName -Force
        $removed++
    }
}
foreach ($sub in @('platforms','imageformats','styles','generic','networkinformation','tls','iconengines')) {
    $subPath = Join-Path $exeDir $sub
    if (Test-Path $subPath) {
        Remove-Item $subPath -Recurse -Force
        $removed++
    }
}
Write-Host "   已清理 $removed 个陈旧文件/目录" -ForegroundColor Gray

# ---- 3. 调用 windeployqt ----
$windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
if (-not (Test-Path $windeployqt)) {
    throw "找不到 windeployqt.exe：$windeployqt"
}

# 让 windeployqt 使用配套的 MinGW 工具链
$env:Path = "$(Join-Path $QtTools 'bin');$(Join-Path $QtRoot 'bin');$env:Path"

Write-Step 'windeployqt 拷贝 Qt 运行库'
$prevEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$deployLog = Join-Path $env:TEMP 'windeployqt.log'
& cmd /c "`"$windeployqt`" --release --no-translations `"$exe`" > `"$deployLog`" 2>&1"
$deployCode = $LASTEXITCODE
$ErrorActionPreference = $prevEap
if ($deployCode -ne 0) {
    Write-Warning "windeployqt 退出码为 $deployCode，改按关键文件检查部署结果。"
    Get-Content $deployLog -ErrorAction SilentlyContinue |
        Select-Object -First 5 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkGray }
}

# ---- 3. 检查关键文件 ----
Write-Step '检查部署结果'
$required = @(
    'Qt6Core.dll',
    'Qt6Gui.dll',
    'Qt6Widgets.dll',
    'platforms\qwindows.dll'
)
$missing = @()
foreach ($item in $required) {
    $path = Join-Path $exeDir $item
    if (Test-Path $path) {
        Write-Host "   [有] $item" -ForegroundColor Green
    } else {
        Write-Host "   [缺] $item" -ForegroundColor Red
        $missing += $item
    }
}

# ---- 4. 冒烟测试 ----
# 说明：分两步验证，避免依赖"离屏平台插件"这一可选组件
#   （1）--console-selftest：只创建 QCoreApplication，不建任何 GUI 对象，
#       不需要平台插件，可稳定验证"Qt DLL 能加载、进程能跑、算法/数据正确"；
#   （2）GUI 心跳：以真实平台启动主窗口，若 4 秒内没有退出即认为界面可用，
#       随后主动关闭。这一步正是当初"陈旧 Qt6Core.dll 导致启动即崩"的检出手段。
Write-Step '冒烟测试（1/2：无界面自测）'
Push-Location $exeDir
try {
    & $exe --console-selftest
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}
if ($code -eq 0) {
    Write-Host "   自测通过（退出码 0）" -ForegroundColor Green
} else {
    Write-Warning "自测退出码为 $code，请查看 $exeDir\selftest_report.txt"
}

Write-Step '冒烟测试（2/2：GUI 心跳）'
$guiOk = $false
$proc = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 4
$proc.Refresh()
if ($proc.HasExited) {
    Write-Warning "主窗口进程已提前退出（ExitCode=$($proc.ExitCode)）；" +
                  "若为 0xC0000005 通常是 Qt DLL 版本不配套，请重跑本脚本。"
} else {
    $guiOk = $true
    Write-Host "   主窗口已启动并存活（4 秒心跳正常）" -ForegroundColor Green
    $proc.CloseMainWindow() | Out-Null
    Start-Sleep -Milliseconds 1000
    if (-not $proc.HasExited) { $proc.Kill() }
}

# ---- 5. 可选打包 ----
if ($Zip) {
    Write-Step '生成发布压缩包'
    $stamp = Get-Date -Format 'yyyyMMdd-HHmm'
    $zipPath = Join-Path $BuildDir "CompilerCourseDesign-release-$stamp.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path (Join-Path $exeDir '*') -DestinationPath $zipPath -CompressionLevel Optimal
    $sizeMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
    Write-Host "   压缩包: $zipPath  ($sizeMb MB)" -ForegroundColor Green
}

if ($missing.Count -gt 0) {
    throw "部署不完整，缺少：$($missing -join ', ')"
}

Write-Host "`n部署完成，可直接运行： & '$exe'" -ForegroundColor Green
