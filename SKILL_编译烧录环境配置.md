# SKILL: STM32 编译烧录环境配置

> 适用于 STM32CubeIDE 项目转换为 CMake + Ninja + OpenOCD 命令行构建/烧录方案
> 芯片: STM32F427IIHx (Cortex-M4F)

---

## 1. 环境依赖

### 1.1 工具链路径 (`D:\DevEnv`)

| 工具 | 路径 | 说明 |
|------|------|------|
| **ARM GCC** | `D:\DevEnv\GNU-tools-for-STM32\bin\` | arm-none-eabi-gcc/g++/objcopy 等 |
| **CMake** | `D:\DevEnv\BuildTools\CMake\bin\` | 构建系统生成器 |
| **Ninja** | `D:\DevEnv\BuildTools\Ninja\` | 高性能构建工具 |
| **OpenOCD** | `D:\DevEnv\Debuggers\OpenOCD\bin\` | 烧录/调试工具 |

### 1.2 硬件要求
- ST-Link 调试器（V2 或更高版本）
- STM32F427IIHx 开发板
- SWD 连接线（VCC, GND, SWDIO, SWCLK）

---

## 2. 项目文件清单

一个可编译烧录的 STM32 项目需要以下文件：

### 2.1 必需文件（由 STM32CubeMX 生成）
```
项目根目录/
├── Core/                    # 核心代码（Inc + Src）
├── Drivers/                 # HAL 驱动
├── Middlewares/              # 中间件（FreeRTOS 等）
├── USB_DEVICE/              # USB 设备代码
├── STM32F427IIHX_FLASH.ld   # 链接脚本
├── STM32F427IIHX_RAM.ld     # RAM 链接脚本
└── dart_mcu.ioc             # CubeMX 配置文件
```

### 2.2 需手动创建的文件
```
项目根目录/
├── CMakeLists.txt            # CMake 构建配置
├── stlink.cfg                # OpenOCD 烧录配置
└── .vscode/
    ├── tasks.json            # VSCode 编译/烧录任务
    └── settings.json         # VSCode CMake 设置
```

---

## 3. 文件模板

### 3.1 `CMakeLists.txt`

```cmake
#此文件从模板自动生成! 请勿更改!
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
cmake_minimum_required(VERSION 3.28)

# specify cross-compilers and tools
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# project settings
project(dart_mcu C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

#Uncomment for hardware floating point
add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING)
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

add_compile_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)

# Enable assembler files preprocessing
add_compile_options($<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else ()
    message(STATUS "No optimization, full debug info")
    add_compile_options(-O0 -g -fno-inline)
endif ()

# ===== 包含路径（根据项目实际目录修改） =====
include_directories(
    Core/Inc
    USB_DEVICE/App
    USB_DEVICE/Target
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
)

# ===== 宏定义 =====
add_definitions(-DDEBUG -DUSE_HAL_DRIVER -DSTM32F427xx)

# ===== 源文件 =====
file(GLOB_RECURSE SOURCES "Core/*.*" "Middlewares/*.*" "Drivers/*.*" "USB_DEVICE/*.*")

# ===== 链接脚本 =====
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F427IIHX_FLASH.ld)

add_link_options(-Wl,--print-memory-usage)
add_link_options(-Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_link_options(-T ${LINKER_SCRIPT})

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})

# ===== 生成 .hex 和 .bin 文件 =====
set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
        COMMENT "Building ${HEX_FILE}
Building ${BIN_FILE}")
```

> **注意**：如果芯片不同，需要修改：
> - `-mcpu=` 参数（如 cortex-m3, cortex-m7 等）
> - `-mfpu=` 和 `-mfloat-abi=` 参数
> - `add_definitions()` 中的 `-DSTM32F427xx`
> - 链接脚本文件名
> - `include_directories()` 中的路径

### 3.2 `stlink.cfg`

```tcl
# choose st-link/j-link/dap-link etc.
source [find interface/stlink.cfg]
#transport select swd
transport select hla_swd
# 根据芯片型号修改 target 配置文件
source [find target/stm32f4x.cfg]
# download speed = 10MHz
adapter speed 10000
```

> **注意**：不同芯片需要修改 `source [find target/xxx.cfg]`：
> - STM32F1: `target/stm32f1x.cfg`
> - STM32F4: `target/stm32f4x.cfg`
> - STM32F7: `target/stm32f7x.cfg`
> - STM32G0: `target/stm32g0x.cfg`
> - STM32H7: `target/stm32h7x.cfg`

### 3.3 `.vscode/tasks.json`

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "type": "cmake",
            "label": "CMake: Build",
            "command": "build",
            "targets": ["all"],
            "group": "build",
            "problemMatcher": [],
            "detail": "CMake 编译项目"
        },
        {
            "label": "Flash: OpenOCD",
            "type": "shell",
            "command": "D:/DevEnv/Debuggers/OpenOCD/bin/openocd.exe",
            "args": [
                "-f", "interface/stlink.cfg",
                "-f", "target/stm32f4x.cfg",
                "-c", "program build/dart_mcu.elf verify reset exit"
            ],
            "group": "build",
            "problemMatcher": [],
            "detail": "使用 OpenOCD 烧录固件"
        }
    ]
}
```

### 3.4 `.vscode/settings.json`

```json
{
    "cmake.cmakePath": "cmake",
    "cmake.generator": "Ninja",
    "cmake.configureArgs": [],
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureSettings": {
        "CMAKE_C_COMPILER": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-gcc.exe",
        "CMAKE_CXX_COMPILER": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-g++.exe",
        "CMAKE_ASM_COMPILER": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-gcc.exe",
        "CMAKE_AR": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-ar.exe",
        "CMAKE_OBJCOPY": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-objcopy.exe",
        "CMAKE_OBJDUMP": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-objdump.exe",
        "CMAKE_SIZE": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-size.exe"
    }
}
```

---

## 4. 编译步骤

### 4.1 一键编译（推荐）

在项目根目录执行：

```powershell
# 设置环境变量（临时）
$env:PATH = "D:\DevEnv\GNU-tools-for-STM32\bin;D:\DevEnv\BuildTools\CMake\bin;D:\DevEnv\BuildTools\Ninja;D:\DevEnv\Debuggers\OpenOCD\bin;$env:PATH"

# 配置 CMake（首次或修改 CMakeLists.txt 后执行）
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build
```

### 4.2 分步说明

| 步骤 | 命令 | 说明 |
|------|------|------|
| **配置** | `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | 生成构建系统文件 |
| **编译** | `cmake --build build` | 编译所有源文件 |
| **清理** | `cmake --build build --target clean` | 清理构建产物 |
| **重新编译** | `rm -rf build && cmake -B build -G Ninja ...` | 完全重新构建 |

### 4.3 编译产物

```
build/
├── dart_mcu.elf      # ELF 可执行文件（用于调试/烧录）
├── dart_mcu.hex      # HEX 格式（用于烧录）
├── dart_mcu.bin      # 二进制格式（用于烧录）
├── dart_mcu.map      # 内存映射文件
└── ...               # 中间 .obj 文件
```

---

## 5. 烧录步骤

### 5.1 硬件连接

```
ST-Link        STM32 开发板
------         ----------
VCC    <---->  VCC (3.3V)
GND    <---->  GND
SWDIO  <---->  SWDIO (PA13)
SWCLK  <---->  SWCLK (PA14)
```

### 5.2 烧录命令

```powershell
# 设置环境变量
$env:PATH = "D:\DevEnv\GNU-tools-for-STM32\bin;D:\DevEnv\BuildTools\CMake\bin;D:\DevEnv\BuildTools\Ninja;D:\DevEnv\Debuggers\OpenOCD\bin;$env:PATH"

# 烧录（使用项目中的 stlink.cfg）
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/dart_mcu.elf verify reset exit"

# 或者使用绝对路径的 stlink.cfg
openocd -f stlink.cfg -c "program build/dart_mcu.elf verify reset exit"
```

### 5.3 烧录参数说明

| 参数 | 说明 |
|------|------|
| `-f interface/stlink.cfg` | 指定调试器接口配置（ST-Link） |
| `-f target/stm32f4x.cfg` | 指定目标芯片配置 |
| `program xxx.elf` | 烧录 ELF 文件 |
| `verify` | 烧录后校验 |
| `reset` | 烧录后复位芯片 |
| `exit` | 烧录完成后退出 OpenOCD |

### 5.4 烧录成功标志

```
** Programming Started **
** Programming Finished **
** Verify Started **
** Verified OK **
** Resetting Target **
```

---

## 6. VSCode 操作指南

### 6.1 安装 VSCode 扩展

- **CMake Tools** (`ms-vscode.cmake-tools`)
- **C/C++** (`ms-vscode.cpptools`)
- **Cortex-Debug** (`marus25.cortex-debug`) - 可选，用于调试

### 6.2 编译

1. 在 VSCode 中打开项目文件夹
2. 按 `Ctrl+Shift+B` 或运行 Task `CMake: Build`

### 6.3 烧录

1. 连接 ST-Link 到开发板
2. 按 `Ctrl+Shift+P` → `Tasks: Run Task` → `Flash: OpenOCD`

---

## 7. 常见问题排查

### Q1: CMake 找不到编译器
```
-- The C compiler identification is unknown
```
**解决**：确保 `arm-none-eabi-gcc` 在 PATH 中，或在 `settings.json` 中指定完整路径。

### Q2: OpenOCD 无法连接
```
Error: open failed
```
**解决**：
- 检查 ST-Link 是否已连接
- 检查驱动是否安装
- 检查 SWD 接线是否正确
- 尝试降低 `adapter speed`

### Q3: 编译报错 "undefined reference to"
**解决**：检查 `CMakeLists.txt` 中的 `include_directories` 和源文件路径是否正确。

### Q4: 芯片型号不同
**解决**：修改 `CMakeLists.txt` 中的以下内容：
- `-mcpu=` 参数
- `-DSTM32F427xx` 宏定义
- 链接脚本文件名
- `stlink.cfg` 中的 `target/xxx.cfg`

---

## 8. 一键脚本

将以下内容保存为 `build_and_flash.ps1`，放在项目根目录：

```powershell
# build_and_flash.ps1 - 一键编译并烧录
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
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== 2. 编译 ===" -ForegroundColor Cyan
cmake --build build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== 3. 烧录 ===" -ForegroundColor Cyan
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/dart_mcu.elf verify reset exit"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== ✅ 完成! ===" -ForegroundColor Green
```

---

## 9. 参考项目

- 参考项目路径：`D:\robomaster\CH\madio\dart-ros2-workspace\src\dart_mcu`
- 该项目的 `CMakeLists.txt` 还包含 micro-ROS 集成，可作为进阶参考