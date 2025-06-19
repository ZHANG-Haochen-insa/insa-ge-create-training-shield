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