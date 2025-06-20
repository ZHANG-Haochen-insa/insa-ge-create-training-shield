# Button State Display Issue / 按键状态显示问题

## Issue Description / 问题描述

### English

**Problem**: In the MENU_POWER_METER interface, after a short button press and release, there's a high probability that the screen displays "BTN:ON" and gets stuck in this state. The button state doesn't return to "OFF" until another short press and release cycle.

**Expected Behavior**: 
- Press button → Display "BTN:ON"
- Release button → Display "BTN:OFF"

**Actual Behavior**:
- Press button → Display "BTN:ON"
- Release button → Display remains "BTN:ON" (stuck)
- Press and release again → Display returns to "BTN:OFF"

**Occurrence Rate**: High probability (~70-80%)

**System Impact**: While the display shows incorrect button state, the menu navigation functions (short press/long press) appear to work correctly.

### 中文

**问题描述**：在 MENU_POWER_METER 界面，短按按键并释放后，有很大概率屏幕会显示 "BTN:ON" 并卡在这个状态。按键状态不会返回到 "OFF"，除非再次进行短按和释放操作。

**预期行为**：
- 按下按键 → 显示 "BTN:ON"
- 释放按键 → 显示 "BTN:OFF"

**实际行为**：
- 按下按键 → 显示 "BTN:ON"
- 释放按键 → 显示仍然是 "BTN:ON"（卡住）
- 再次按下并释放 → 显示返回到 "BTN:OFF"

**发生概率**：高概率（约 70-80%）

**系统影响**：虽然显示的按键状态不正确，但菜单导航功能（短按/长按）似乎正常工作。

## Technical Details / 技术细节

### Code Analysis / 代码分析

The button state is managed in the interrupt handler:

```c
// In User_Button_Interrupt_Handler()
if (button_stable_state && !button_state) {
    button_state = 1;  // Set to ON
}
else if (!button_stable_state && button_state) {
    button_state = 0;  // Set to OFF
}

// Display in MENU_POWER_METER
sprintf(line3_str, "ROT:%03u BTN:%s", rotary_counter, button_state ? "ON " : "OFF");
```

### Possible Causes / 可能原因

1. **Interrupt Missing / 中断丢失**: The button release interrupt might be missed occasionally
2. **State Synchronization / 状态同步**: Mismatch between `button_stable_state` and `button_state` variables
3. **Debouncing Logic / 消抖逻辑**: The 20ms debounce window might be filtering out legitimate state changes
4. **Edge Detection / 边沿检测**: Rising/falling edge detection might not be triggering correctly

### Environment / 环境信息

- **MCU**: STM32L053R8T6
- **Button GPIO**: PA4 with internal pull-up
- **Interrupt**: EXTI4_15_IRQn
- **Debounce Time**: 20ms software debouncing

## Steps to Reproduce / 复现步骤

1. Power on the device / 设备上电
2. Navigate to MENU_POWER_METER (default screen) / 进入 MENU_POWER_METER 界面（默认界面）
3. Short press and release the button quickly / 快速短按并释放按键
4. Observe the "BTN:" status on the display / 观察显示屏上的 "BTN:" 状态
5. Repeat multiple times to observe the stuck state / 多次重复以观察卡住状态

## Additional Notes / 补充说明

- The menu navigation functionality works correctly despite the display issue / 尽管显示有问题，菜单导航功能正常工作
- This issue appeared after implementing enhanced debouncing logic / 这个问题在实现增强的消抖逻辑后出现
- The rotary encoder display (ROT:xxx) updates correctly / 旋转编码器显示（ROT:xxx）正常更新

## Temporary Workaround / 临时解决方案

Press and release the button again to reset the display state to "OFF".
再次按下并释放按键以将显示状态重置为 "OFF"。