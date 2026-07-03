# build_and_flash.ps1 - 一键编译并烧录 STM32 固件
# 使用方法: powershell -ExecutionPolicy Bypass -File build_and_flash.ps1
# 或直接: .\build_and_flash.ps1

param(
    [string]$BuildType = "Debug"
)

# 设置环境变量
$ToolchainPath = "D:\DevEnv\GNU-tools-for-STM32\bin"
$CMakePath = "D:\DevEnv\BuildTools\CMake\bin"
$NinjaPath = "D:\DevEnv\BuildTools\Ninja"
$OpenOCDPath = "D:\DevEnv\Debuggers\OpenOCD\bin"
$env:PATH = "$ToolchainPath;$CMakePath;$NinjaPath;$OpenOCDPath;$env:PATH"

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ProjectDir

Write-Host "=== 1. CMake 配置 ===" -ForegroundColor Cyan
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=$BuildType
if ($LASTEXITCODE -ne 0) { 
    Write-Host "❌ CMake 配置失败!" -ForegroundColor Red
    exit $LASTEXITCODE 
}

Write-Host "=== 2. 编译 ===" -ForegroundColor Cyan
cmake --build build
if ($LASTEXITCODE -ne 0) { 
    Write-Host "❌ 编译失败!" -ForegroundColor Red
    exit $LASTEXITCODE 
}

Write-Host "=== 3. 烧录 ===" -ForegroundColor Cyan
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/dart_mcu.elf verify reset exit"
if ($LASTEXITCODE -ne 0) { 
    Write-Host "❌ 烧录失败!" -ForegroundColor Red
    exit $LASTEXITCODE 
}

Write-Host "=== ✅ 编译 + 烧录全部完成! ===" -ForegroundColor Green