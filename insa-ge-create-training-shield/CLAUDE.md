# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an STM32L0 embedded training shield project targeting the **STM32L053R8T6** microcontroller. The project demonstrates real-time sensor interfacing with dual potentiometers, rotary encoder, user button, and SSD1306 OLED display.

**Key Hardware Interfaces:**
- **ADC**: Two potentiometers (PC0/PC1 - channels 10/11)
- **I2C1**: SSD1306 OLED display 128x32 (PB8-SCL, PB9-SDA, address 0x3C)
- **GPIO/EXTI**: Rotary encoder (PA0/PA1) and user button (PA4) with interrupts
- **TIM6**: Periodic timer for display updates (~15Hz)

## Build Commands

**Primary Build (STM32CubeIDE):**
```bash
# Build project
make -C Debug clean
make -C Debug all
```

**Debug/Flash:**
- Use STM32CubeIDE: Run → Debug (F11)
- Hardware debugger: ST-Link via SWD

**Key Build Outputs:**
- `Debug/insa-ge-create-training-shield.elf` - Main executable
- `Debug/insa-ge-create-training-shield.map` - Memory map
- `Debug/insa-ge-create-training-shield.list` - Assembly listing

## Code Architecture

**Main Application Structure:**
- **`Core/Src/main.c`**: Main application logic with interrupt handlers
  - `Timer_Interrupt_Handler()`: ADC reading and display update (called by TIM6)
  - `User_Button_Interrupt_Handler()`: Button state management (EXTI4_15)
  - Rotary encoder handled via EXTI0_1 interrupts
- **`Core/Src/ssd1306/`**: Complete OLED display driver library
  - Multi-MCU compatible (STM32F0/F1/F4/L0/L1/L4/F3/H7/F7/G0/G4)
  - I2C communication, font rendering, graphics primitives
  - Test suite available in `ssd1306_tests.c`

**Memory Configuration:**
- **Flash**: 64KB at 0x8000000
- **RAM**: 8KB at 0x20000000  
- **Stack**: 1KB, **Heap**: 512 bytes

**Real-time Display Format:**
```
P1:nnnn   P2:nnnn
ROT:nnn SWITCH:ON/OFF
```

## STM32CubeMX Configuration

**System:**
- **MCU**: STM32L053R8T6 (ARM Cortex-M0+, LQFP64)
- **Clock**: 32MHz (HSI with PLL x4)
- **Project file**: `insa-ge-create-training-shield.ioc`

**Peripheral Configuration:**
- **ADC**: 12-bit resolution, channels 10-11
- **I2C1**: Fast mode 400kHz for display
- **TIM6**: Periodic interrupts for main loop timing
- **EXTI**: Lines 0,1 (rotary encoder), line 4 (user button)

## Development Workflow

1. **Modify Hardware Config**: Edit `.ioc` file in STM32CubeMX, regenerate code
2. **User Code**: Add between `/* USER CODE BEGIN */` and `/* USER CODE END */` markers
3. **Build**: Use STM32CubeIDE or `make -C Debug`
4. **Debug**: Connect ST-Link, use IDE debugger with live expressions
5. **Testing**: Use SSD1306 test functions in `ssd1306_tests.h`

## Important Implementation Notes

- **Interrupt-driven architecture**: TIM6 drives main sensor reading loop
- **ADC function**: Use `Get_ADC_Value(ADC_CHANNEL_x)` for potentiometer readings
- **Display updates**: Always call `ssd1306_UpdateScreen()` after drawing operations
- **Memory constraints**: 8KB RAM total, optimize for minimal heap usage
- **STM32CubeMX regeneration**: Preserves user code sections, safe to regenerate

## Testing

**SSD1306 Display Tests:**
```c
#include "ssd1306/ssd1306_tests.h"
ssd1306_TestAll();  // Run comprehensive test suite
```

Test categories: border drawing, fonts, FPS, geometric shapes, bitmaps.

## 项目开发进度记录

### ✅ 已完成功能

#### 1. 模拟功率计算核心 (2024年完成)
- **ADC映射算法**: 电位器到物理量转换
  ```c
  float Convert_ADC_to_Voltage(uint32_t adc_value);  // 0-4095 → 0-30V
  float Convert_ADC_to_Current(uint32_t adc_value);  // 0-4095 → 0-5A
  ```
- **功率计算**: `P = V_sim × I_sim`
- **能量累积**: `E = ∫P·dt` 积分算法
- **峰值记录**: 电压、电流、功率的历史峰值追踪

#### 2. 显示系统优化
- **解决浮点printf问题**: 改用整数格式化避免`%f`显示空白
- **智能单位切换**: Wh ↔ kWh 自动切换
- **显示格式**: 
  ```
  V:12.5V  I:2.35A
  P:29.4W E:123Wh
  ROT:045 BTN:OFF
  ```

#### 3. 完整菜单系统
- **多级菜单架构**: 
  ```
  MENU_POWER_METER ↔ MENU_MAIN
                        ├── Power Meter
                        ├── Peak Values  
                        ├── Settings → About
                        └── Reset Options
                            ├── Reset Peaks
                            ├── Reset Energy
                            └── Cancel
  ```
- **智能滚动显示**: 解决128x32屏幕限制，支持4个菜单项的滚动
- **用户交互**:
  - 旋转编码器: 菜单导航
  - 短按: 进入/确认
  - 长按: 返回/取消
  - 30秒自动返回功率计界面

#### 4. 中断驱动架构
- **TIM6定时中断**: 100ms周期，ADC采集+显示更新
- **旋转编码器中断**: 四状态正交解码+5ms消抖
- **按钮中断**: 长短按检测+防抖处理
- **菜单集成**: 所有用户输入与菜单系统完美集成

#### 5. 数据管理
- **实时计算**: 电压、电流、功率实时更新
- **峰值追踪**: 自动记录并可手动重置
- **能量积分**: 基于时间戳的精确能量累积
- **数据持久性**: 重置功能分离（峰值/能量独立重置）

### 🚧 待开发功能

#### 1. 数据存储系统
- **RAM循环缓冲区**: 1000条记录存储
- **Flash非易失性存储**: 数据持久化
- **统计数据**: 平均值、最大值、最小值计算
- **数据结构**:
  ```c
  typedef struct {
      uint32_t timestamp;
      uint16_t voltage;
      uint16_t current; 
      uint16_t power;
  } MeasurementData;
  ```

#### 2. 通信功能
- **UART1串口通信**: 115200波特率
- **实时数据传输**: ASCII协议格式
- **数据导出**: 历史数据批量传输
- **PC端协议**:
  ```
  $DATA,timestamp,voltage,current,power,energy\r\n
  $STAT,avg_v,avg_i,avg_p,total_e,runtime\r\n
  ```

#### 3. 高级显示功能
- **图形显示模式**: 实时曲线绘制
- **多参数对比**: V/I/P曲线同时显示
- **历史数据查看**: 滚动查看存储的测量记录

#### 4. 系统配置
- **参数设置菜单**: 采样率、量程、显示选项
- **校准功能**: 电压/电流校准系数调整
- **系统信息**: 运行时间、数据统计

### 🔧 技术要点

#### 关键解决方案
1. **浮点显示问题**: 手动整数格式化代替`%f`
2. **菜单滚动**: 智能窗口算法适配小屏幕
3. **电位器模拟**: 安全的功率计功能开发环境
4. **中断集成**: 统一的事件处理架构

#### 移植准备
- **引脚映射**: 测试板(STM32L053R8) → 生产版(STM32L052K6)
- **ADC通道**: PC0/PC1 (电位器) → PA3/PA4 (真实信号)
- **Flash优化**: 64KB → 32KB 代码压缩策略

### 📋 下一步计划
1. **优先级1**: 数据存储系统实现
2. **优先级2**: UART通信协议开发  
3. **优先级3**: 图形显示功能增强
4. **优先级4**: 生产版本移植验证