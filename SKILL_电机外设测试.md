# SKILL: M2006 & M3508 电机外设测试

## 概述

本文档记录了在 STM32F427 (RoboMaster A板) 上使用 C 语言驱动 M2006 和 M3508 电机的完整方案。代码从 `dart-ros2-workspace/src/dart_mcu` 项目的 `motor_controller.cpp` 迁移并简化为纯 C 实现，协议与 DJI 官方标准完全一致。

## 硬件配置

| 电机 | 电调 | CAN ID | 反馈帧 | 最大电流 | 备注 |
|------|------|--------|--------|----------|------|
| M2006 | **C610** | **1** | 0x201 | 10000 | 扳机丝杆电机 |
| M3508 #1 | **C620** | **2** | 0x202 | 16384 | 装填电机1 |
| M3508 #2 | **C620** | **3** | 0x203 | 16384 | 装填电机2 |

> **重要**：M2006 必须配 C610 电调，M3508 必须配 C620 电调，**不能混用**。C610 和 C620 均使用 DJI 标准 RM CAN 协议（0x200 帧），不需要 DM 专用协议。

## 文件结构

```
Core/
├── Inc/
│   ├── motor_M2006.h      # M2006 电机头文件
│   ├── motor_M3508.h      # M3508 电机头文件
│   └── can.h              # CAN 驱动头文件
└── Src/
    ├── motor_M2006.c      # M2006 电机驱动实现（状态机 + 过零检测）
    ├── motor_M3508.c      # M3508 电机驱动实现
    ├── can.c              # CAN 通信（初始化/滤波/中断接收/0x200发送）
    ├── main.c             # 主程序（硬件初始化 + 电机初始化）
    └── freertos.c         # FreeRTOS 控制任务（1KHz 控制循环）
```

## CAN 通信协议（DJI 官方标准）

### 控制帧（发送到电调）

- **帧 ID**: `0x200`（标准帧）
- **DLC**: 8 字节
- **频率**: **1KHz**（1ms 周期，电调要求严格 1KHz，否则不响应）
- **数据格式**（大端序 int16_t）：

| 字节 | 内容 | 说明 |
|------|------|------|
| [0-1] | 电机 ID=1 电流 | **M2006 (C610)** |
| [2-3] | 电机 ID=2 电流 | **M3508 #1 (C620)** |
| [4-5] | 电机 ID=3 电流 | **M3508 #2 (C620)** |
| [6-7] | 电机 ID=4 电流 | 未使用 (0) |

### 反馈帧（电调返回）

- **帧 ID**: `0x200 + 电机ID`（标准帧）
- **DLC**: 8 字节
- **数据格式**：

| 字节 | 内容 | 说明 |
|------|------|------|
| [0-1] | 当前角度 | 12位编码器，范围 0-8191 |
| [2-3] | 当前速度 | RPM，int16_t |
| [4-5] | 电流值 | int16_t |
| [6-7] | 温度 | int8_t |

### 反馈帧 ID 对照

| 电机 ID | 反馈帧 ID |
|---------|-----------|
| 1 (M2006) | **0x201** |
| 2 (M3508 #1) | **0x202** |
| 3 (M3508 #2) | **0x203** |

## 电机状态机

```
DISCONNECTED ──(收到CAN反馈)──> IDLE ──(SetCurrent)──> RUNNING
     ^                                                      │
     └──────────────(超时5秒无反馈)────────────────────────┘
```

| 状态 | 含义 | 行为 |
|------|------|------|
| **DISCONNECTED** | 未连接 | `SetCurrent` 拒绝设置电流并直接 return |
| **IDLE** | 已连接但未运行 | `SetCurrent` 自动切换到 RUNNING 并设置电流 |
| **RUNNING** | 正常运行 | 持续发送电流指令 |

## API 说明

### M2006 电机

```c
// 初始化（在 main.c 中调用）
void M2006_Init(M2006_HandleTypeDef *hm2006, uint8_t id, int16_t max_current);
// 示例: M2006_Init(&hm2006, 1, 10000);  // CAN ID=1, 最大电流10000

// 设置目标电流（自动限幅 + 状态转换）
void M2006_SetCurrent(M2006_HandleTypeDef *hm2006, int16_t current);
// 示例: M2006_SetCurrent(&hm2006, 5000);  // 50% 电流正转

// 解码 CAN 反馈数据（在 CAN RX 中断回调中调用）
void M2006_DecodeCAN(M2006_HandleTypeDef *hm2006, uint8_t *data);

// 获取当前应发送的电流值（自动检查超时断开）
int16_t M2006_GetTargetCurrent(M2006_HandleTypeDef *hm2006);

// 检查电机是否已连接
uint8_t M2006_IsConnected(M2006_HandleTypeDef *hm2006);

// 重置圈数
void M2006_ResetRound(M2006_HandleTypeDef *hm2006);
```

### M3508 电机

```c
// 初始化（在 main.c 中调用）
void M3508_Init(M3508_HandleTypeDef *hm3508, uint8_t id, int16_t max_current);
// 示例: M3508_Init(&hm3508_2, 2, 16384);  // CAN ID=2, 最大电流16384

// 设置目标电流（自动限幅 + 状态转换）
void M3508_SetCurrent(M3508_HandleTypeDef *hm3508, int16_t current);
// 示例: M3508_SetCurrent(&hm3508_2, 8000);  // ~50% 电流

// 解码 CAN 反馈数据（在 CAN RX 中断回调中调用）
void M3508_DecodeCAN(M3508_HandleTypeDef *hm3508, uint8_t *data);

// 获取当前应发送的电流值（自动检查超时断开）
int16_t M3508_GetTargetCurrent(M3508_HandleTypeDef *hm3508);

// 检查电机是否已连接
uint8_t M3508_IsConnected(M3508_HandleTypeDef *hm3508);

// 重置圈数
void M3508_ResetRound(M3508_HandleTypeDef *hm3508);
```

### 电机句柄结构体字段

```c
// 以下字段可在调试器中直接观察：
hm3508_2.state            // 电机状态: 0=DISCONNECTED, 1=IDLE, 2=RUNNING
hm3508_2.target_current   // 目标电流（你设置的）
hm3508_2.current_current  // 电调反馈的实际电流（正常运行时接近 target}
hm3508_2.current_velocity // 当前速度 (RPM)，正转为正，反转为负
hm3508_2.current_angle    // 当前角度 (0-8191)，12位编码器
hm3508_2.current_round    // 累计圈数，过零时自动 ±1
```

## 使用示例

### 1. main.c — 硬件初始化

```c
/* USER CODE BEGIN 2 */
// 配置 CAN 滤波器（允许所有 ID）
CAN_FilterTypeDef sFilterConfig;
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

// 初始化电机
M2006_Init(&hm2006,   1, 10000);   // M2006 + C610: ID=1
M3508_Init(&hm3508_2, 2, 16384);   // M3508 + C620: ID=2
M3508_Init(&hm3508_3, 3, 16384);   // M3508 + C620: ID=3

// 等待电机连接
uint32_t wait_start = HAL_GetTick();
while (!M2006_IsConnected(...) || !M3508_IsConnected(...)) {
    if (HAL_GetTick() - wait_start > 3000) break;
}
```

### 2. freertos.c — 1KHz 控制循环

```c
void StartDefaultTask(void *argument) {
  // 1KHz 精确定时
  TickType_t xLastWakeTime = xTaskGetTickCount();

  int16_t target_2006 = 5000;
  int16_t target_3508_2 = 8000;
  int16_t target_3508_3 = 8000;

  for(;;) {
    // 状态管理：IDLE→RUNNING 转换
    if (hm2006.state == M2006_IDLE || hm2006.state == M2006_RUNNING)
        M2006_SetCurrent(&hm2006, target_2006);
    else
        hm2006.target_current = 0;

    if (hm3508_2.state == M3508_IDLE || hm3508_2.state == M3508_RUNNING)
        M3508_SetCurrent(&hm3508_2, target_3508_2);
    else
        hm3508_2.target_current = 0;

    // 组装 0x200 帧并发送
    uint8_t can_data[8] = {0};
    int16_t c1 = M2006_GetTargetCurrent(&hm2006);    // → bytes 0-1
    int16_t c2 = M3508_GetTargetCurrent(&hm3508_2);  // → bytes 2-3
    int16_t c3 = M3508_GetTargetCurrent(&hm3508_3);  // → bytes 4-5
    can_data[0] = (uint8_t)(c1 >> 8);
    can_data[1] = (uint8_t)(c1);
    can_data[2] = (uint8_t)(c2 >> 8);
    can_data[3] = (uint8_t)(c2);
    can_data[4] = (uint8_t)(c3 >> 8);
    can_data[5] = (uint8_t)(c3);

    CAN_TxHeaderTypeDef tx_header;
    tx_header.StdId = 0x200;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = 8;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t tx_mailbox;
    uint32_t timeout = 0;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
        if (++timeout > 5000) {
            HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            break;
        }
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
        HAL_CAN_AddTxMessage(&hcan1, &tx_header, can_data, &tx_mailbox);

    vTaskDelayUntil(&xLastWakeTime, 1);  // 1KHz
  }
}
```

### 3. can.c — CAN 接收回调

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    if (hcan != &hcan1) return;

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);
    switch (rx_header.StdId) {
        case 0x201: M2006_DecodeCAN(&hm2006, rx_data);  break;
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

### 正常工作时 Watch 窗口预期值

```c
hm3508_2.state            = 2 (M3508_RUNNING)
hm3508_2.target_current   = 8000
hm3508_2.current_current  ≈ 8000（接近 target）
hm3508_2.current_velocity ≠ 0（电机在转）
hm3508_2.current_angle    持续变化（编码器在更新）

hm3508_3.state            = 2 (M3508_RUNNING)
hm2006.state              = 2 (M2006_RUNNING)
```

### 常见问题排查

| 现象 | 可能原因 | 解决方案 |
|------|----------|----------|
| state=DISCONNECTED | CAN 通信未建立 | 检查接线、终端电阻、电调供电 |
| state=IDLE 不变 | `SetCurrent` 未被调用 | 检查控制循环是否进入 |
| target=8000, current≈0 | 电调不响应指令 | 检查电调是否为 C620（非 DM 电调）、ID 配置 |
| 电机不转但 current≠0 | 电流太小或负载太大 | 增大电流到 50% 以上 |
| 电机抖动 | 三相线接错 | 检查 UVW 三相线颜色匹配 |

## 注意事项

1. **CAN 总线**: 确保 CAN1 总线已正确连接（A板: PD0=RX, PD1=TX），总线两端 120Ω 终端电阻
2. **电调类型**: M2006 必须用 C610，M3508 必须用 C620，**不能互换**
3. **控制频率**: **必须 1KHz**，电调对控制帧超时敏感，低于 1KHz 会被忽略
4. **电流限幅**: M2006 最大 10000，M3508 最大 16384，超出自动限幅
5. **7-pin 数据线**: 电调到电机的传感器线必须插紧，否则无反馈数据
6. **三相线**: 电调到电机的动力线必须按颜色匹配连接
7. **电调 ID 配置**: 可通过 RoboMaster 上位机软件修改电调 CAN ID
8. **测试安全**: 首次测试建议先用 10% 电流确认方向，再逐步增大