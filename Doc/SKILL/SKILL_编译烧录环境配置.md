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
    ├── c_cpp_properties.json # C/C++ IntelliSense 配置
    ├── extensions.json       # 推荐扩展
    ├── launch.json           # 调试/烧录配置（提供 UI 按钮）
    ├── settings.json         # VSCode CMake 设置
    └── tasks.json            # VSCode 编译/烧录任务
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

# ===== 包含路径 =====
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

### 3.2 `stlink.cfg`

```tcl
# choose st-link/j-link/dap-link etc.
source [find interface/stlink.cfg]
transport select hla_swd
source [find target/stm32f4x.cfg]
adapter speed 10000
```

### 3.3 `.vscode/launch.json`

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "STM32 Debug (OpenOCD)",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "openocd",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/dart_mcu.elf",
            "serverpath": "D:/DevEnv/Debuggers/OpenOCD/bin/openocd.exe",
            "gdbPath": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-gdb.exe",
            "searchDir": ["D:/DevEnv/Debuggers/OpenOCD/share/openocd/scripts"],
            "configFiles": ["interface/stlink.cfg", "target/stm32f4x.cfg"],
            "device": "STM32F427IIHx",
            "interface": "swd",
            "runToEntryPoint": "main",
            "preLaunchTask": "CMake: Build",
            "showDevDebugOutput": "raw",
            "rtos": "FreeRTOS"
        },
        {
            "name": "Flash Only (OpenOCD)",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "openocd",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/dart_mcu.elf",
            "serverpath": "D:/DevEnv/Debuggers/OpenOCD/bin/openocd.exe",
            "gdbPath": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-gdb.exe",
            "searchDir": ["D:/DevEnv/Debuggers/OpenOCD/share/openocd/scripts"],
            "configFiles": ["interface/stlink.cfg", "target/stm32f4x.cfg"],
            "device": "STM32F427IIHx",
            "interface": "swd",
            "showDevDebugOutput": "raw",
            "rtos": "FreeRTOS"
        }
    ]
}
```

### 3.4 `.vscode/tasks.json`

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
                "-c", "init",
                "-c", "halt",
                "-c", "program build/dart_mcu.elf verify",
                "-c", "reset run",
                "-c", "shutdown"
            ],
            "group": "build",
            "problemMatcher": [],
            "detail": "使用 OpenOCD 烧录固件"
        }
    ]
}
```

### 3.5 `.vscode/settings.json`

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

### 3.6 `.vscode/c_cpp_properties.json`

```json
{
    "version": 4,
    "configurations": [{
        "name": "STM32",
        "compileCommands": "${workspaceFolder}/build/compile_commands.json",
        "compilerPath": "D:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-gcc.exe",
        "intelliSenseMode": "gcc-arm",
        "cStandard": "c11",
        "cppStandard": "c++17"
    }]
}
```

### 3.7 `.vscode/extensions.json`

```json
{
    "recommendations": [
        "marus25.cortex-debug",
        "ms-vscode.cmake-tools",
        "ms-vscode.cpptools"
    ]
}
```

---

## 4. 编译步骤

```powershell
$env:PATH = "D:\DevEnv\GNU-tools-for-STM32\bin;D:\DevEnv\BuildTools\CMake\bin;D:\DevEnv\BuildTools\Ninja;D:\DevEnv\Debuggers\OpenOCD\bin;$env:PATH"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

---

## 5. 烧录命令

```powershell
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "init; halt; program build/dart_mcu.elf verify; reset run; shutdown"
```

> **关键**：必须用 `reset run` 而非 `reset`，否则烧录后 MCU 处于 halt 状态，程序不运行。

---

## 6. 常见问题排查

### Q1: CMake 找不到编译器
确保 `arm-none-eabi-gcc` 在 PATH 中，或在 `settings.json` 中指定完整路径。

### Q2: OpenOCD 无法连接
检查 ST-Link 驱动、SWD 接线，尝试降低 `adapter speed`。

### Q3: 新增 .c 文件后链接报 `undefined reference`
**根因**：`file(GLOB_RECURSE)` 在 CMake 配置时缓存了文件列表，**新增 .c 文件不会自动被检测到**。

**解决**：**必须重新执行 `cmake -B build`** 让 CMake 重新扫描源文件，再编译。

### Q4: 中文路径导致 objcopy 失败
Windows 中文路径（如 `测试`）可能被 Ninja 编码错误，导致 `.hex`/`.bin` 生成失败。**`.elf` 链接成功即可使用，不影响调试/烧录**。

### Q5: 烧录后不运行，完全断电才恢复
烧录命令必须用 `reset run` + `shutdown`，不能用 `reset exit`（halt MCU）。

---

## 7. 参考项目
- `D:\robomaster\CH\madio\dart-ros2-workspace\src\dart_mcu`