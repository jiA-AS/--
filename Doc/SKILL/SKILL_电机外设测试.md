# SKILL: M2006 & M3508 电机外设测试

## 概述

本文档记录了在 STM32F427 (RoboMaster A板) 上使用 C 语言驱动 **M2006 (C610电调) + M3508 (C620电调)** 的完整方案。代码从 `dart-ros2-workspace/src/dart_mcu` 项目的 `motor_controller.cpp` 迁移并简化，协议与 DJI 官方标准完全一致。**三个电机均已通过实机测试**。

## 硬件配置（已验证）

| 电机 | 电调 | CAN ID | 反馈帧 | 最大电流 | 备注 |
|------|------|--------|--------|----------|------|
| M2006 | **C610** | **4** | 0x204 | 10000 | 扳机丝杆电机，需大电流启动 |
| M3508 #1 | **C620** | **2** | 0x202 | 16384 | 装填电机1 |
| M3508 #2 | **C620** | **3** | 0x203 | 16384 | 装填电机2 |

> **关键事实**：
> - M2006 **必须配 C610** 电调，M3508 **必须配 C620** 电调
> - C610/C620 均使用标准 DJI RM CAN 协议（`0x200` 帧），**不是 DM 协议**
> - **控制频率必须是 1KHz**，电调对低频指令不响应

## 文件结构

```
Core/
├── Inc/
│   ├── motor_M2006.h      # M2006 电机头文件
│   ├── motor_M3508.h      # M3508 电机头文件
│   └── can.h              # CAN 驱动头文件
└── Src/
    ├── motor_M2006.c      # M2006 电机驱动（状态机 + 过零检测）
    ├── motor_M3508.c      # M3508 电机驱动
    ├── can.c              # CAN 通信（初始化/滤波/中断接收/发送）
    ├── main.c             # 主程序（硬件初始化 + 电机初始化）
    └── freertos.c         # FreeRTOS 控制任务（1KHz 控制循环）
```

## CAN 通信协议

### 控制帧（MCU → 电调）

- **帧 ID**: `0x200`（标准帧）
- **DLC**: 8 字节
- **频率**: **1KHz**（`vTaskDelayUntil(&xLastWakeTime, 1)`，1ms 周期）
- **数据格式**（大端序 int16_t）：

| 字节 | 内容 | 说明 |
|------|------|------|
| [0-1] | 电机 ID=1 电流 | 未使用 (0) |
| [2-3] | 电机 ID=2 电流 | **M3508 #1 (C620)** |
| [4-5] | 电机 ID=3 电流 | **M3508 #2 (C620)** |
| [6-7] | 电机 ID=4 电流 | **M2006 (C610)** |

### 反馈帧（电调 → MCU）

- **帧 ID**: `0x200 + 电机ID`
- **DLC**: 8 字节

| 字节 | 内容 | 说明 |
|------|------|------|
| [0-1] | 当前角度 | 12位编码器，范围 0-8191 |
| [2-3] | 当前速度 | RPM，int16_t |
| [4-5] | 电流值 | int16_t |
| [6-7] | 温度 | int8_t |

### 反馈帧 ID 对照

| 电机 ID | 反馈帧 ID |
|---------|-----------|
| 2 (M3508 #1) | **0x202** |
| 3 (M3508 #2) | **0x203** |
| 4 (M2006) | **0x204** |

## 电机状态机

```
DISCONNECTED ──(收到CAN反馈)──> IDLE ──(SetCurrent)──> RUNNING
     ^                                                      │
     └──────────────(超时5秒无反馈)────────────────────────┘
```

| 状态 | 枚举值 | 含义 | 行为 |
|------|--------|------|------|
| **DISCONNECTED** | 0 | 未连接 | `SetCurrent` 拒绝设置电流，直接 return |
| **IDLE** | 1 | 已连接但未运行 | `SetCurrent` 自动切换为 RUNNING |
| **RUNNING** | 2 | 正常运行 | 持续发送电流指令 |

## API 方法

### M2006 电机

```c
void M2006_Init(M2006_HandleTypeDef *hm2006, uint8_t id, int16_t max_current);
// 示例: M2006_Init(&hm2006, 4, 10000);

void M2006_SetCurrent(M2006_HandleTypeDef *hm2006, int16_t current);
// 示例: M2006_SetCurrent(&hm2006, 3000);   // 30% 正转
// 注意: 负值反转，M2006 丝杆电机需要较大电流（≥2000）才能克服静摩擦

void M2006_DecodeCAN(M2006_HandleTypeDef *hm2006, uint8_t *data);
int16_t M2006_GetTargetCurrent(M2006_HandleTypeDef *hm2006);
uint8_t M2006_IsConnected(M2006_HandleTypeDef *hm2006);
void M2006_ResetRound(M2006_HandleTypeDef *hm2006);
```

### M3508 电机

```c
void M3508_Init(M3508_HandleTypeDef *hm3508, uint8_t id, int16_t max_current);
// 示例: M3508_Init(&hm3508_2, 2, 16384);

void M3508_SetCurrent(M3508_HandleTypeDef *hm3508, int16_t current);
// 示例: M3508_SetCurrent(&hm3508_2, 8000);  // ~50% 电流

void M3508_DecodeCAN(M3508_HandleTypeDef *hm3508, uint8_t *data);
int16_t M3508_GetTargetCurrent(M3508_HandleTypeDef *hm3508);
uint8_t M3508_IsConnected(M3508_HandleTypeDef *hm3508);
void M3508_ResetRound(M3508_HandleTypeDef *hm3508);
```

### 调试器 Watch 可见字段

```c
hm3508_2.state            // 0=DISCONNECTED, 1=IDLE, 2=RUNNING
hm3508_2.target_current   // 设定值
hm3508_2.current_current  // 电调反馈实际值，调试用
hm3508_2.current_velocity // RPM，正转正、反转负
hm3508_2.current_angle    // 当前角度 (0-8191)，过零持续变化
hm3508_2.current_round    // 累计圈数，自动过零计数
```

## 代码实现示例

### 1. main.c — 初始化

```c
/* USER CODE BEGIN Includes */
#include "motor_M2006.h"
#include "motor_M3508.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
M2006_HandleTypeDef hm2006;      /* ID=4, 反馈 0x204 */
M3508_HandleTypeDef hm3508_2;    /* ID=2, 反馈 0x202 */
M3508_HandleTypeDef hm3508_3;    /* ID=3, 反馈 0x203 */
/* USER CODE END PV */

int main(void) {
  // ... HAL_Init(), SystemClock_Config(), MX_CAN1_Init() ...

  /* 配置 CAN 滤波器 */
  CAN_FilterTypeDef sFilterConfig;
  // 接受所有 CAN 帧
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

  /* 初始化电机 */
  M2006_Init(&hm2006,   4, 10000);
  M3508_Init(&hm3508_2, 2, 16384);
  M3508_Init(&hm3508_3, 3, 16384);

  /* 等待电机上线 */
  uint32_t wait_start = HAL_GetTick();
  while (!M2006_IsConnected(&hm2006) || !M3508_IsConnected(&hm3508_2) || !M3508_IsConnected(&hm3508_3)) {
      if (HAL_GetTick() - wait_start > 3000) break;
  }

  osKernelInitialize();
  MX_FREERTOS_Init();
  osKernelStart();
  while (1) { }
}
```

### 2. freertos.c — 1KHz 控制循环

```c
void StartDefaultTask(void *argument) {
  MX_USB_DEVICE_Init();

  /* 1KHz 精确定时 */
  TickType_t xLastWakeTime = xTaskGetTickCount();

  int16_t target_2006 = 3000;    /* M2006: 30% */
  int16_t target_3508_2 = 2000;  /* M3508 #1: ~12% */
  int16_t target_3508_3 = 0;     /* M3508 #2: 停止 */

  for(;;) {
    /* 状态管理 */
    if (hm2006.state == M2006_IDLE || hm2006.state == M2006_RUNNING)
        M2006_SetCurrent(&hm2006, target_2006);
    else hm2006.target_current = 0;

    if (hm3508_2.state == M3508_IDLE || hm3508_2.state == M3508_RUNNING)
        M3508_SetCurrent(&hm3508_2, target_3508_2);
    else hm3508_2.target_current = 0;

    if (hm3508_3.state == M3508_IDLE || hm3508_3.state == M3508_RUNNING)
        M3508_SetCurrent(&hm3508_3, target_3508_3);
    else hm3508_3.target_current = 0;

    /* 组装 0x200 帧 */
    uint8_t can_data[8] = {0};
    int16_t c2 = M3508_GetTargetCurrent(&hm3508_2);  /* → bytes 2-3 */
    int16_t c3 = M3508_GetTargetCurrent(&hm3508_3);  /* → bytes 4-5 */
    int16_t c4 = M2006_GetTargetCurrent(&hm2006);      /* → bytes 6-7 */
    can_data[2] = (uint8_t)(c2 >> 8);
    can_data[3] = (uint8_t)(c2);
    can_data[4] = (uint8_t)(c3 >> 8);
    can_data[5] = (uint8_t)(c3);
    can_data[6] = (uint8_t)(c4 >> 8);
    can_data[7] = (uint8_t)(c4);

    CAN_TxHeaderTypeDef tx_header;
    tx_header.StdId = 0x200;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = 8;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t timeout = 0;
    uint32_t tx_mailbox;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
        if (++timeout > 5000) {
            HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            break;
        }
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
        HAL_CAN_AddTxMessage(&hcan1, &tx_header, can_data, &tx_mailbox);

    vTaskDelayUntil(&xLastWakeTime, 1);  /* 1KHz */
  }
}
```

### 3. can.c — 接收回调

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    if (hcan != &hcan1) return;

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);
    switch (rx_header.StdId) {
        case 0x204: M2006_DecodeCAN(&hm2006, rx_data);   break;
        case 0x202: M3508_DecodeCAN(&hm3508_2, rx_data); break;
        case 0x203: M3508_DecodeCAN(&hm3508_3, rx_data); break;
    }
}
```

## 编译与烧录

```powershell
# 编译
cube-cmake --build build --config Debug --target all

# 烧录（使用 ST-Link）
STM32_Programmer_CLI -c port=SWD -w build/dart_mcu.elf -rst
```

## 调试与验证

### 正常工作时 Watch 预期值

```c
hm3508_2.state            = 2  // RUNNING
hm3508_2.target_current   = 2000
hm3508_2.current_current  ≈ 2000  // 接近 target
hm3508_2.current_velocity ≠ 0     // 电机在转
hm3508_2.current_angle    持续变化

hm2006.state              = 2  // RUNNING
hm2006.target_current     = 3000
hm2006.current_current    ≈ 3000
hm2006.current_velocity   ≠ 0
```

### 电调 LED 状态编码

| LED 行为 | 含义 |
|----------|------|
| 闪 N 次 → 停顿 → 重复 | 开机显示 CAN ID = N（**不是故障**） |
| 绿灯常亮 | 运行正常 |
| 红灯闪烁 | 故障（缺相、过流等） |

### 常见问题排查

| 现象 | 可能原因 | 解决方案 |
|------|----------|----------|
| state=DISCONNECTED | CAN 通信未建立 | 检查接线、终端电阻(120Ω)、电调供电 |
| state=IDLE 不变 | `SetCurrent` 未被调用 | 检查 `StartDefaultTask` 是否进入 |
| target=3000, current≈0 | 控制频率不对 | **必须是 1KHz**，不要用 `osDelay(10)` |
| M2006 不转但 M3508 正常 | M2006 电流太小 | 丝杆电机静摩擦大，起步需 ≥2000 |
| LED 闪 4 下 | CAN ID=4，正常 | 不是错误 |
| "滴滴-嘀嘀"声 | 电调已识别电机但未输出 | 电流不够或方向导致被限位卡住 |
| 电机抖动 | 三相线接错 | 检查 UVW 三相线颜色匹配 |

## 调试经验与注意事项

1. **控制频率是成功的关键**：试过 `osDelay(10)` (100Hz) 不工作，改用 `vTaskDelayUntil(&xLastWakeTime, 1)` (1KHz) 后立即正常。C620/C610 电调对控制帧的间隔极其敏感。

2. **M2006 启动需要大电流**：丝杆电机静摩擦大，3000（30%）可以正常运动，但初次测试时可能需要 5000-9000 才能克服初始静摩擦力。

3. **M2006 的方向**：参考代码中扳机丝杆电机用了 `angle_reverse_=true`，实际输出电流会被取反。如果电机卡在限位处不动，尝试反向电流。

4. **CAN 滤波器**：配置为接受所有 ID（mask=0x0000），这样才能收到 0x202/0x203/0x204 反馈帧。

5. **电调电源先于 MCU 上电**：电调上电后约 1-2 秒才开始发送 CAN 反馈帧，`main.c` 中的等待循环正是为此设计的。

6. **FreeRTOS 任务栈**：`stack_size = 3000 * 4`（12KB），控制循环中使用局部变量，栈使用量很低。

7. **电调 ID 修改**：使用 RoboMaster 上位机软件修改 C620/C610 的 CAN ID，修改后电调重启 LED 会闪烁新 ID 次数。

8. **GM6020 是电压控制，不是电流控制**：与 M3508/M2006/M4310（用 `SetCurrent`，控制帧 `0x200`）不同，GM6020 用 **`SetVoltage`**，控制帧 **`0x1FF`**（ID 1-4 组）或 **`0x2FF`**（ID 5-7 组），电压范围 **-30000 ~ 30000**。反馈帧为 `0x204 + ID`（如 ID=1 → `0x205`）。

9. **GM6020 启动需要大电压**：满电压 30000 才能可靠启动。小电压（如 10000）时 `current_current` 仅几十，电机不动。**增大电压到 30000 后正常转动**。

10. **GM6020 的 PWM 线必须拔掉才能稳定用 CAN 控制**：
   - 上电时 PWM 信号脚如果悬空或有中间电平，电机可能进入 **PWM 信号行程校准模式**（绿灯常亮），完全忽略 CAN 指令
   - 症状：`state=RUNNING`、`target_voltage=30000`、`current_current≈0`，电机不转
   - **解决**：拔掉白色 3pin PWM 信号线，只保留 CAN 和电源，电机就能可靠工作在 CAN 模式
   - 如果曾短暂转动但重启后又不转，也是 PWM 模式切换导致——上电时序不同导致 PWM 脚电平不同

11. **新增 .c 文件后必须 `cmake -B build` 重新扫描**：`file(GLOB_RECURSE)` 缓存源文件列表，新增 `motor_GM6020.c` 后需要重新配置 CMake 才能被编译。
