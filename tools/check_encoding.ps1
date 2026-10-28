# ============================================================================
#  check_encoding.ps1 -- 核对工程内各文件的编码与 BOM 是否符合 AGENT.md 13.2/13.3
# ----------------------------------------------------------------------------
#  规则：
#    *.pro            必须 UTF-8 无 BOM（有 BOM 时 qmake 直接报错）
#    *.ps1            必须 UTF-8 带 BOM（无 BOM 时 PowerShell 5.1 按 GBK 读中文）
#    *.h / *.cpp / *.md / *.txt / *.tny / *.mc
#                     统一 UTF-8 无 BOM
#  用法：powershell -ExecutionPolicy Bypass -File tools\check_encoding.ps1
#  退出码：0 全部合规；1 存在不合规文件
# ============================================================================

[CmdletBinding()]
param(
    [string]$Root
)

$ErrorActionPreference = 'Stop'
if (-not $Root) {
    $Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}

function Test-Bom([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        return $true
    }
    return $false
}

$rules = @(
    @{ Ext = '.pro'; WantBom = $false },
    @{ Ext = '.ps1'; WantBom = $true  },
    @{ Ext = '.h';   WantBom = $false },
    @{ Ext = '.cpp'; WantBom = $false },
    @{ Ext = '.md';  WantBom = $false },
    @{ Ext = '.txt'; WantBom = $false },
    @{ Ext = '.tny'; WantBom = $false },
    @{ Ext = '.mc';  WantBom = $false }
)

# 跳过目录：build/（构建产物）与 reference/（调研资料 + 老师给的任务书/测试数据，
# 不是本项目的代码，编码不受本项目规范约束）
$skipDirs = @('build', 'reference', '.git')
$bad = 0
$checked = 0

foreach ($rule in $rules) {
    $files = Get-ChildItem -Path $Root -Recurse -File -Filter ("*" + $rule.Ext) -ErrorAction SilentlyContinue |
        Where-Object {
            $rel = $_.FullName.Substring($Root.Length)
            -not ($skipDirs | Where-Object { $rel -like "*\$_\*" })
        }
    foreach ($f in $files) {
        $checked++
        $hasBom = Test-Bom $f.FullName
        if ($hasBom -ne $rule.WantBom) {
            $want = if ($rule.WantBom) { '带 BOM' } else { '无 BOM' }
            $got  = if ($hasBom) { '带 BOM' } else { '无 BOM' }
            Write-Host ("[不合规] {0}  应为 {1}，实际 {2}" -f $f.FullName.Substring($Root.Length), $want, $got) -ForegroundColor Red
            $bad++
        }
    }
}

Write-Host ""
Write-Host ("编码检查完成：共检查 {0} 个文件，不合规 {1} 个。" -f $checked, $bad) -ForegroundColor $(if ($bad -eq 0) { 'Green' } else { 'Red' })
exit $(if ($bad -eq 0) { 0 } else { 1 })
