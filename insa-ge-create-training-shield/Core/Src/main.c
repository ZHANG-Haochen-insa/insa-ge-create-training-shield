/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "ssd1306/ssd1306.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
static uint8_t rotary_state;
static uint8_t rotary_counter;
static uint8_t button_state;

// Power meter simulation variables
static float simulated_voltage = 0.0f;    // Simulated voltage (V)
static float simulated_current = 0.0f;    // Simulated current (A)
static float simulated_power = 0.0f;      // Calculated power (W)
static float accumulated_energy = 0.0f;   // Accumulated energy (Wh)
static uint32_t last_timestamp = 0;       // For energy integration

// Peak value tracking
static float peak_voltage = 0.0f;
static float peak_current = 0.0f;
static float peak_power = 0.0f;

// Button handling for reset functions
static uint32_t button_press_time = 0;
static uint8_t button_long_press_handled = 0;
static uint32_t button_last_interrupt_time = 0;  // For debouncing
static uint8_t button_stable_state = 0;          // Stable button state after debouncing

// Menu system variables
typedef enum {
    MENU_POWER_METER = 0,    // Main power meter display
    MENU_MAIN,               // Main menu
    MENU_PEAKS,              // Peak values display
    MENU_GRAPHICS,           // Graphics display menu
    MENU_GRAPHICS_SELECT,    // Graphics parameter selection
    MENU_SETTINGS,           // Settings menu
    MENU_RESET,              // Reset menu
    MENU_ABOUT               // About/Info
} MenuState_t;

static MenuState_t current_menu = MENU_POWER_METER;
static uint8_t menu_selection = 0;      // Current menu item selection
static uint8_t menu_changed = 1;        // Flag to trigger display update
static uint32_t last_activity_time = 0; // For auto-return to power meter

// Rotary encoder debouncing variables
static uint32_t rotary_last_interrupt_time = 0;

// Graphics functionality variables
#define GRAPH_DATA_POINTS 64  // Number of data points for graph (64 points across 128 pixel width)
static float voltage_history[GRAPH_DATA_POINTS];
static float current_history[GRAPH_DATA_POINTS];
static float power_history[GRAPH_DATA_POINTS];
static uint8_t graph_data_index = 0;
static uint8_t graphics_parameter = 0;  // 0 = Voltage, 1 = Current, 2 = Power
static uint32_t last_graph_update = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Convert ADC value to simulated voltage
  * @param  adc_value Raw ADC value (0-4095)
  * @retval Simulated voltage in volts (0-30V)
  */
float Convert_ADC_to_Voltage(uint32_t adc_value)
{
    // POT_1 (ADC_CHANNEL_10): 0-4095 → 0-30V
    return (float)adc_value * 30.0f / 4095.0f;
}

/**
  * @brief  Convert ADC value to simulated current
  * @param  adc_value Raw ADC value (0-4095)
  * @retval Simulated current in amperes (0-5A)
  */
float Convert_ADC_to_Current(uint32_t adc_value)
{
    // POT_2 (ADC_CHANNEL_11): 0-4095 → 0-5A
    return (float)adc_value * 5.0f / 4095.0f;
}

/**
  * @brief  Calculate simulated power from voltage and current
  * @param  voltage Simulated voltage in volts
  * @param  current Simulated current in amperes
  * @retval Calculated power in watts
  */
float Calculate_Power(float voltage, float current)
{
    return voltage * current;
}

/**
  * @brief  Update accumulated energy using trapezoidal integration
  * @param  power Current power in watts
  * @param  delta_time Time interval in milliseconds
  */
void Update_Energy(float power, uint32_t delta_time)
{
    // Convert delta_time from ms to hours for Wh calculation
    float delta_hours = (float)delta_time / (1000.0f * 3600.0f);
    
    // Simple rectangular integration: E += P * dt
    accumulated_energy += power * delta_hours;
}

/**
  * @brief  Update peak values if current values are higher
  * @param  voltage Current voltage value
  * @param  current Current current value
  * @param  power Current power value
  */
void Update_Peaks(float voltage, float current, float power)
{
    if (voltage > peak_voltage) peak_voltage = voltage;
    if (current > peak_current) peak_current = current;
    if (power > peak_power) peak_power = power;
}

/**
  * @brief  Reset all peak values to zero
  */
void Reset_Peaks(void)
{
    peak_voltage = 0.0f;
    peak_current = 0.0f;
    peak_power = 0.0f;
}

/**
  * @brief  Reset accumulated energy to zero
  */
void Reset_Energy(void)
{
    accumulated_energy = 0.0f;
}

/**
  * @brief  Handle rotary encoder input for menu navigation
  * @param  direction: 1 for clockwise, -1 for counter-clockwise
  */
void Handle_Menu_Navigation(int8_t direction)
{
    last_activity_time = HAL_GetTick();
    menu_changed = 1;
    
    switch (current_menu) {
        case MENU_MAIN:
            // Navigate main menu items
            if (direction > 0) {
                menu_selection = (menu_selection + 1) % 5; // 5 main menu items
            } else {
                menu_selection = (menu_selection == 0) ? 4 : menu_selection - 1;
            }
            break;
            
        case MENU_GRAPHICS_SELECT:
            // Navigate graphics parameter selection
            if (direction > 0) {
                menu_selection = (menu_selection + 1) % 4; // 4 graphics items (V, A, P, Back)
            } else {
                menu_selection = (menu_selection == 0) ? 3 : menu_selection - 1;
            }
            break;
            
        case MENU_SETTINGS:
            // Navigate settings menu items  
            if (direction > 0) {
                menu_selection = (menu_selection + 1) % 2; // 2 settings items
            } else {
                menu_selection = (menu_selection == 0) ? 1 : menu_selection - 1;
            }
            break;
            
        case MENU_RESET:
            // Navigate reset menu items
            if (direction > 0) {
                menu_selection = (menu_selection + 1) % 3; // 3 reset items
            } else {
                menu_selection = (menu_selection == 0) ? 2 : menu_selection - 1;
            }
            break;
            
        default:
            // Other menus don't have navigation
            break;
    }
}

/**
  * @brief  Handle button press for menu actions
  * @param  press_type: 0 = short press, 1 = long press
  */
void Handle_Menu_Action(uint8_t press_type)
{
    last_activity_time = HAL_GetTick();
    menu_changed = 1;
    
    if (press_type == 1) { // Long press - always go back/up
        switch (current_menu) {
            case MENU_POWER_METER:
                current_menu = MENU_MAIN;
                menu_selection = 0;
                break;
                
            default:
                current_menu = MENU_POWER_METER;
                menu_selection = 0;
                break;
        }
    } else { // Short press - enter/confirm
        switch (current_menu) {
            case MENU_POWER_METER:
                // No action on short press in power meter mode
                break;
                
            case MENU_MAIN:
                switch (menu_selection) {
                    case 0: current_menu = MENU_POWER_METER; break;  // Back to Power Meter
                    case 1: current_menu = MENU_PEAKS; break;        // Peak Values
                    case 2: current_menu = MENU_GRAPHICS_SELECT; menu_selection = 0; break; // Graphics
                    case 3: current_menu = MENU_SETTINGS; menu_selection = 0; break; // Settings
                    case 4: current_menu = MENU_RESET; menu_selection = 0; break;    // Reset
                }
                break;
                
            case MENU_PEAKS:
                current_menu = MENU_MAIN;
                menu_selection = 1;
                break;
                
            case MENU_GRAPHICS_SELECT:
                switch (menu_selection) {
                    case 0: // Voltage graph
                        graphics_parameter = 0;
                        current_menu = MENU_GRAPHICS;
                        break;
                    case 1: // Current graph
                        graphics_parameter = 1;
                        current_menu = MENU_GRAPHICS;
                        break;
                    case 2: // Power graph
                        graphics_parameter = 2;
                        current_menu = MENU_GRAPHICS;
                        break;
                    case 3: // Back
                        current_menu = MENU_MAIN;
                        menu_selection = 2;
                        break;
                }
                break;
                
            case MENU_GRAPHICS:
                current_menu = MENU_GRAPHICS_SELECT;
                menu_selection = graphics_parameter;
                break;
                
            case MENU_SETTINGS:
                switch (menu_selection) {
                    case 0: current_menu = MENU_ABOUT; break;      // About
                    case 1: current_menu = MENU_MAIN; menu_selection = 2; break; // Back
                }
                break;
                
            case MENU_RESET:
                switch (menu_selection) {
                    case 0: Reset_Peaks(); current_menu = MENU_MAIN; menu_selection = 3; break;   // Reset Peaks
                    case 1: Reset_Energy(); current_menu = MENU_MAIN; menu_selection = 3; break;  // Reset Energy
                    case 2: current_menu = MENU_MAIN; menu_selection = 3; break;                  // Cancel
                }
                break;
                
            case MENU_ABOUT:
                current_menu = MENU_SETTINGS;
                menu_selection = 0;
                break;
        }
    }
}

/**
  * @brief  Display current menu on OLED
  */
void Display_Current_Menu(void)
{
    char line1[21] = {0};
    char line2[21] = {0};
    char line3[21] = {0};
    
    // Clear screen
    ssd1306_Fill(Black);
    
    switch (current_menu) {
        case MENU_POWER_METER:
            Display_Power_Meter();
            return; // Power meter has its own display logic
            
        case MENU_MAIN:
            {
                // Main menu items (total 5 items: 0-4)
                const char* menu_items[5] = {
                    " Power Meter",
                    " Peak Values",
                    " Graphics", 
                    " Settings",
                    " Reset Options"
                };
                
                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString("=== MAIN MENU ===", Font_6x8, White);
                
                // Calculate scroll window (show 3 items at a time)
                uint8_t start_item = 0;
                if (menu_selection >= 2) {
                    start_item = menu_selection - 1;  // Keep selected item in middle when possible
                    if (start_item > 2) start_item = 2;  // Don't scroll beyond last window (5-3=2)
                }
                
                // Display 3 visible items
                for (uint8_t i = 0; i < 3 && (start_item + i) < 5; i++) {
                    uint8_t item_index = start_item + i;
                    char display_line[21];
                    
                    // Add selection marker
                    sprintf(display_line, "%s%s", 
                           (item_index == menu_selection) ? ">" : " ",
                           menu_items[item_index]);
                    
                    ssd1306_SetCursor(0, 8 + (i * 8));  // Y positions: 8, 16, 24
                    ssd1306_WriteString(display_line, Font_6x8, White);
                }
                
                // Optional: Add scroll indicators
                if (start_item > 0) {
                    // Show "up arrow" indicator at top-right
                    ssd1306_SetCursor(120, 8);
                    ssd1306_WriteString("^", Font_6x8, White);
                }
                if (start_item + 3 < 5) {
                    // Show "down arrow" indicator at bottom-right  
                    ssd1306_SetCursor(120, 24);
                    ssd1306_WriteString("v", Font_6x8, White);
                }
            }
            break;
            
        case MENU_PEAKS:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("=== PEAK VALUES ===", Font_6x8, White);
            
            // Display peak values using integer formatting
            int pv_int = (int)peak_voltage;
            int pv_frac = (int)((peak_voltage - pv_int) * 10.0f);
            int pi_int = (int)peak_current;
            int pi_frac = (int)((peak_current - pi_int) * 100.0f);
            int pp_int = (int)peak_power;
            int pp_frac = (int)((peak_power - pp_int) * 10.0f);
            
            sprintf(line1, "V: %d.%dV", pv_int, pv_frac);
            sprintf(line2, "I: %d.%02dA", pi_int, pi_frac);
            sprintf(line3, "P: %d.%dW", pp_int, pp_frac);
            
            ssd1306_SetCursor(0, 10);
            ssd1306_WriteString(line1, Font_7x10, White);
            ssd1306_SetCursor(0, 20);
            ssd1306_WriteString(line2, Font_7x10, White);
            ssd1306_SetCursor(70, 20);
            ssd1306_WriteString(line3, Font_7x10, White);
            break;
            
        case MENU_SETTINGS:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("=== SETTINGS ===", Font_6x8, White);
            
            sprintf(line1, "%s About", (menu_selection == 0) ? ">" : " ");
            sprintf(line2, "%s Back", (menu_selection == 1) ? ">" : " ");
            
            ssd1306_SetCursor(0, 12);
            ssd1306_WriteString(line1, Font_7x10, White);
            ssd1306_SetCursor(0, 22);
            ssd1306_WriteString(line2, Font_7x10, White);
            break;
            
        case MENU_RESET:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("=== RESET ===", Font_6x8, White);
            
            sprintf(line1, "%s Reset Peaks", (menu_selection == 0) ? ">" : " ");
            sprintf(line2, "%s Reset Energy", (menu_selection == 1) ? ">" : " ");
            sprintf(line3, "%s Cancel", (menu_selection == 2) ? ">" : " ");
            
            ssd1306_SetCursor(0, 8);
            ssd1306_WriteString(line1, Font_6x8, White);
            ssd1306_SetCursor(0, 16);
            ssd1306_WriteString(line2, Font_6x8, White);
            ssd1306_SetCursor(0, 24);
            ssd1306_WriteString(line3, Font_6x8, White);
            break;
            
        case MENU_GRAPHICS_SELECT:
            {
                // Graphics menu items (total 4 items: 0-3)
                const char* graphics_items[4] = {
                    " Voltage (V)",
                    " Current (A)",
                    " Power (W)",
                    " Back"
                };
                
                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString("=== GRAPHICS ===", Font_6x8, White);
                
                // Calculate scroll window (show 3 items at a time)
                uint8_t start_item = 0;
                if (menu_selection >= 2) {
                    start_item = menu_selection - 1;  // Keep selected item in middle when possible
                    if (start_item > 1) start_item = 1;  // Don't scroll beyond last window (4-3=1)
                }
                
                // Display 3 visible items
                for (uint8_t i = 0; i < 3 && (start_item + i) < 4; i++) {
                    uint8_t item_index = start_item + i;
                    char display_line[21];
                    
                    // Add selection marker
                    sprintf(display_line, "%s%s", 
                           (item_index == menu_selection) ? ">" : " ",
                           graphics_items[item_index]);
                    
                    ssd1306_SetCursor(0, 8 + (i * 8));  // Y positions: 8, 16, 24
                    ssd1306_WriteString(display_line, Font_6x8, White);
                }
                
                // Optional: Add scroll indicators
                if (start_item > 0) {
                    // Show "up arrow" indicator at top-right
                    ssd1306_SetCursor(120, 8);
                    ssd1306_WriteString("^", Font_6x8, White);
                }
                if (start_item + 3 < 4) {
                    // Show "down arrow" indicator at bottom-right  
                    ssd1306_SetCursor(120, 24);
                    ssd1306_WriteString("v", Font_6x8, White);
                }
            }
            break;
            
        case MENU_GRAPHICS:
            Display_Graphics();
            return; // Graphics has its own display logic
            
        case MENU_ABOUT:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("Power Meter v1.0", Font_7x10, White);
            ssd1306_SetCursor(0, 12);
            ssd1306_WriteString("STM32L053R8", Font_6x8, White);
            ssd1306_SetCursor(0, 20);
            ssd1306_WriteString("Test Development", Font_6x8, White);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString("Board - INSA-GE", Font_6x8, White);
            break;
    }
    
    ssd1306_UpdateScreen();
}

/**
  * @brief  Update graphics data buffer with current values
  */
void Update_Graphics_Data(void)
{
    uint32_t current_time = HAL_GetTick();
    
    // Update data every 200ms for smooth animation
    if (current_time - last_graph_update > 200) {
        voltage_history[graph_data_index] = simulated_voltage;
        current_history[graph_data_index] = simulated_current;
        power_history[graph_data_index] = simulated_power;
        
        graph_data_index = (graph_data_index + 1) % GRAPH_DATA_POINTS;
        last_graph_update = current_time;
    }
}

/**
  * @brief  Display graphics curve
  */
void Display_Graphics(void)
{
    char title_str[21] = {0};
    float max_value = 0.0f;
    float min_value = 0.0f;
    float* data_array;
    
    // Clear screen
    ssd1306_Fill(Black);
    
    // Select data source and find min/max for scaling
    if (graphics_parameter == 0) {
        // Voltage - use integer formatting
        int v_int = (int)simulated_voltage;
        int v_frac = (int)((simulated_voltage - v_int) * 10.0f);
        sprintf(title_str, "Voltage: %d.%dV", v_int, v_frac);
        data_array = voltage_history;
        max_value = 30.0f;  // Max voltage range
        min_value = 0.0f;
    } else if (graphics_parameter == 1) {
        // Current - use integer formatting
        int i_int = (int)simulated_current;
        int i_frac = (int)((simulated_current - i_int) * 100.0f);
        sprintf(title_str, "Current: %d.%02dA", i_int, i_frac);
        data_array = current_history;
        max_value = 5.0f;   // Max current range
        min_value = 0.0f;
    } else {
        // Power - use integer formatting
        int p_int = (int)simulated_power;
        int p_frac = (int)((simulated_power - p_int) * 10.0f);
        sprintf(title_str, "Power: %d.%dW", p_int, p_frac);
        data_array = power_history;
        max_value = 150.0f;  // Max power range (30V * 5A = 150W)
        min_value = 0.0f;
    }
    
    // Display title
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(title_str, Font_6x8, White);
    
    // Draw graph area (leaving space for title and axis labels)
    uint8_t graph_height = 20;  // Graph height in pixels
    uint8_t graph_y_offset = 10; // Start Y position for graph
    
    // Draw axes
    // Y-axis
    for (uint8_t y = 0; y < graph_height; y++) {
        ssd1306_DrawPixel(10, graph_y_offset + y, White);
    }
    // X-axis
    for (uint8_t x = 0; x < 110; x++) {
        ssd1306_DrawPixel(10 + x, graph_y_offset + graph_height - 1, White);
    }
    
    // Plot data points
    for (uint8_t i = 0; i < GRAPH_DATA_POINTS - 1; i++) {
        uint8_t data_index = (graph_data_index + i) % GRAPH_DATA_POINTS;
        uint8_t next_index = (graph_data_index + i + 1) % GRAPH_DATA_POINTS;
        
        // Scale values to fit graph
        uint8_t y1 = graph_y_offset + graph_height - 1 - 
                     (uint8_t)((data_array[data_index] / max_value) * (graph_height - 2));
        uint8_t y2 = graph_y_offset + graph_height - 1 - 
                     (uint8_t)((data_array[next_index] / max_value) * (graph_height - 2));
        
        uint8_t x1 = 11 + (i * 110) / (GRAPH_DATA_POINTS - 1);
        uint8_t x2 = 11 + ((i + 1) * 110) / (GRAPH_DATA_POINTS - 1);
        
        // Draw line between points
        ssd1306_Line(x1, y1, x2, y2, White);
    }
    
    // Add scale labels
    ssd1306_SetCursor(0, graph_y_offset);
    if (graphics_parameter == 0) {
        ssd1306_WriteString("30", Font_6x8, White);  // Voltage
    } else if (graphics_parameter == 1) {
        ssd1306_WriteString("5", Font_6x8, White);   // Current
    } else {
        ssd1306_WriteString("150", Font_6x8, White); // Power
    }
    
    ssd1306_SetCursor(0, graph_y_offset + graph_height - 8);
    ssd1306_WriteString("0", Font_6x8, White);
    
    ssd1306_UpdateScreen();
}

/**
  * @brief  Display power meter data (separated from menu system)
  */
void Display_Power_Meter(void)
{
    char line1_str[21] = {0};
    char line2_str[21] = {0};
    char line3_str[21] = {0};
    
    // Convert float to integer parts for display (avoiding %f)
    // Voltage: XX.X format (1 decimal place)
    int v_int = (int)simulated_voltage;
    int v_frac = (int)((simulated_voltage - v_int) * 10.0f);
    if (v_frac < 0) v_frac = -v_frac;
    
    // Current: X.XX format (2 decimal places)  
    int i_int = (int)simulated_current;
    int i_frac = (int)((simulated_current - i_int) * 100.0f);
    if (i_frac < 0) i_frac = -i_frac;
    
    // Power: XXX.X format (1 decimal place)
    int p_int = (int)simulated_power;
    int p_frac = (int)((simulated_power - p_int) * 10.0f);
    if (p_frac < 0) p_frac = -p_frac;
    
    // Energy handling
    if (accumulated_energy < 1.0f) {
        // Display in Wh for small values (integer Wh)
        int e_wh = (int)(accumulated_energy * 1000.0f);
        sprintf(line1_str, "V:%d.%dV  I:%d.%02dA", v_int, v_frac, i_int, i_frac);
        sprintf(line2_str, "P:%d.%dW E:%dWh", p_int, p_frac, e_wh);
    } else {
        // Display in kWh for larger values (X.XXX format)
        int e_int = (int)accumulated_energy;
        int e_frac = (int)((accumulated_energy - e_int) * 1000.0f);
        if (e_frac < 0) e_frac = -e_frac;
        sprintf(line1_str, "V:%d.%dV  I:%d.%02dA", v_int, v_frac, i_int, i_frac);
        sprintf(line2_str, "P:%d.%dW E:%d.%03dkWh", p_int, p_frac, e_int, e_frac);
    }
    
    sprintf(line3_str, "ROT:%03u BTN:%s", rotary_counter, button_state ? "ON " : "OFF");
    
    // Clear screen and display power meter data
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(line1_str, Font_7x10, White);
    ssd1306_SetCursor(0, 11);
    ssd1306_WriteString(line2_str, Font_7x10, White);
    ssd1306_SetCursor(0, 22);
    ssd1306_WriteString(line3_str, Font_6x8, White);
    ssd1306_UpdateScreen();
}

/**
  * @brief  Interrupt handler for TIM6 timer
  * @note	This function is called when the timer is reloaded
  *         It reads ADC values from potentiometer inputs and update screen infos
  */
void Timer_Interrupt_Handler(void)
{
	// Read ADC values from potentiometers (always needed for power calculations)
	uint32_t pot1_value = Get_ADC_Value(ADC_CHANNEL_10);  // Voltage potentiometer
	uint32_t pot2_value = Get_ADC_Value(ADC_CHANNEL_11);  // Current potentiometer
	
	// Convert ADC values to simulated physical quantities
	simulated_voltage = Convert_ADC_to_Voltage(pot1_value);
	simulated_current = Convert_ADC_to_Current(pot2_value);
	simulated_power = Calculate_Power(simulated_voltage, simulated_current);
	
	// Calculate time delta for energy integration
	uint32_t current_timestamp = HAL_GetTick();
	uint32_t delta_time = current_timestamp - last_timestamp;
	last_timestamp = current_timestamp;
	
	// Update accumulated energy (only if not first call)
	if (delta_time > 0 && delta_time < 1000) {  // Sanity check: delta should be ~100ms
		Update_Energy(simulated_power, delta_time);
	}
	
	// Update peak values
	Update_Peaks(simulated_voltage, simulated_current, simulated_power);
	
	// Update graphics data buffer
	Update_Graphics_Data();
	
	// Check for button long press (moved from interrupt to timer for stability)
	if (button_state && !button_long_press_handled) {
		uint32_t press_duration = current_timestamp - button_press_time;
		if (press_duration >= 2000) {
			// Long press detected
			Handle_Menu_Action(1); // Long press
			button_long_press_handled = 1;
		}
	}
	
	// Check for auto-return to power meter (30 seconds timeout)
	if (current_menu != MENU_POWER_METER && 
	    (current_timestamp - last_activity_time) > 30000) {
		current_menu = MENU_POWER_METER;
		menu_selection = 0;
		menu_changed = 1;
	}
	
	// Update display for all menu states to prevent screen freeze during transitions
	if (menu_changed) {
		Display_Current_Menu();
		menu_changed = 0;
	}
	// Always update power meter and graphics display for real-time data
	else if (current_menu == MENU_POWER_METER || current_menu == MENU_GRAPHICS) {
		Display_Current_Menu();
	}
}

/**
  * @brief  Interrupt handler for User Button GPIO
  * @note	This function handles button press/release with enhanced debouncing
  *         - Short press: Enter/confirm menu action
  *         - Long press (>2s): Back/exit menu action
  */
void User_Button_Interrupt_Handler(void)
{
	uint32_t current_time = HAL_GetTick();
	uint8_t raw_button_state = HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin);
	
	// Software debouncing: ignore interrupts within 20ms of last interrupt
	if ((current_time - button_last_interrupt_time) < 20) {
		return;
	}
	button_last_interrupt_time = current_time;
	
	// Only process if state actually changed from stable state
	if (raw_button_state == button_stable_state) {
		return;
	}
	
	// State has changed and is stable
	button_stable_state = raw_button_state;
	
	if (button_stable_state && !button_state) {
		// Button pressed (rising edge after debouncing)
		button_press_time = current_time;
		button_long_press_handled = 0;
		button_state = 1;
	}
	else if (!button_stable_state && button_state) {
		// Button released (falling edge after debouncing)
		uint32_t press_duration = current_time - button_press_time;
		
		if (!button_long_press_handled && press_duration < 2000) {
			// Short press only if long press wasn't already handled
			Handle_Menu_Action(0); // Short press
		}
		button_state = 0;
	}
}

/**
  * @brief  Interrupt handler for Rotary Encoder Channel A
  * @note	This function is called when edges are detected on rotary encoder channels
  *         Uses software debouncing instead of HAL_Delay for better stability
  */
void Rotary_Encoder_Interrupt_Handler(void)
{
	static int8_t rotary_buffer = 0;
	uint32_t current_time = HAL_GetTick();
	
	// Software debouncing: ignore interrupts within 5ms of last interrupt
	if ((current_time - rotary_last_interrupt_time) < 5) {
		return;
	}
	rotary_last_interrupt_time = current_time;
	
	// Read encoder state
	uint8_t rotary_new = HAL_GPIO_ReadPin(ROT_CHA_GPIO_Port, ROT_CHA_Pin) << 1;
	rotary_new += HAL_GPIO_ReadPin(ROT_CHB_GPIO_Port, ROT_CHB_Pin);
	
	if (rotary_new != rotary_state) {
		int8_t direction = 0;
		
		// Clockwise state transitions
		if (((rotary_state == 0b00) && (rotary_new == 0b10)) || 
		    ((rotary_state == 0b10) && (rotary_new == 0b11)) ||
		    ((rotary_state == 0b11) && (rotary_new == 0b01)) || 
		    ((rotary_state == 0b01) && (rotary_new == 0b00))) {
			rotary_buffer++;
		}
		
		// Counter-clockwise state transitions
		if (((rotary_state == 0b00) && (rotary_new == 0b01)) || 
		    ((rotary_state == 0b01) && (rotary_new == 0b11)) ||
		    ((rotary_state == 0b11) && (rotary_new == 0b10)) || 
		    ((rotary_state == 0b10) && (rotary_new == 0b00))) {
			rotary_buffer--;
		}
		
		rotary_state = rotary_new;
		
		// Check if we have enough transitions for a full detent
		if (rotary_buffer > 3) {
			rotary_counter++;
			rotary_buffer = 0;
			direction = 1;  // Clockwise
		}
		if (rotary_buffer < -3) {
			rotary_counter--;
			rotary_buffer = 0;
			direction = -1; // Counter-clockwise
		}
		
		// Handle menu navigation if we detected a direction change
		if (direction != 0) {
			Handle_Menu_Navigation(direction);
		}
	}
}

/**
  * @brief  Start a conversion and return the value converted.
  * @note   This function waits for the end of conversion in a blocking way
  * @param  hadc ADC handle
  * @param  adc_channel Channel macro such as ADC_CHANNEL_0, ADC_CHANNEL_1, etc.
  * @retval Channel converted value
  */
uint32_t Get_ADC_Value(uint32_t adc_channel)
{
	/* Disable all previous channel configuration */
	hadc.Instance->CHSELR = 0;
	ADC_ChannelConfTypeDef sConfig = {0};
	sConfig.Channel = adc_channel;
	sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
	if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_ADC_Start(&hadc) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_ADC_PollForConversion(&hadc, 100) != HAL_OK)
	{
		Error_Handler();
	}
	return HAL_ADC_GetValue(&hadc);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC_Init();
  MX_I2C1_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  // Initialize OLED display
  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_SetCursor(0, 0);
  ssd1306_WriteString("Power Meter v1.0", Font_7x10, White);
  ssd1306_SetCursor(0, 11);
  ssd1306_WriteString("Initializing...", Font_7x10, White);
  ssd1306_UpdateScreen();
  
  // Initialize power meter variables
  last_timestamp = HAL_GetTick();
  last_activity_time = HAL_GetTick();
  Reset_Energy();
  Reset_Peaks();
  
  // Initialize menu system
  current_menu = MENU_POWER_METER;
  menu_selection = 0;
  menu_changed = 1;
  
  // Start timer for periodic measurements
  HAL_TIM_Base_Start_IT(&htim6);
  
  // Brief startup delay
  HAL_Delay(1000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  rotary_state = HAL_GPIO_ReadPin(ROT_CHA_GPIO_Port, ROT_CHA_Pin) << 1;
  rotary_state += HAL_GPIO_ReadPin(ROT_CHB_GPIO_Port, ROT_CHB_Pin);
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */

  /* USER CODE END ADC_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.OversamplingMode = DISABLE;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerFrequencyMode = DISABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_11;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */

  /* USER CODE END ADC_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x0060112F;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 2096;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 999;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : USER_BUTTON_Pin */
  GPIO_InitStruct.Pin = USER_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;  // Internal pull-up for button stability
  HAL_GPIO_Init(USER_BUTTON_GPIO_Port, &GPIO_InitStruct);
  
  /*Configure GPIO pin : ROT_CHA_Pin */
  GPIO_InitStruct.Pin = ROT_CHA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ROT_CHA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ROT_CHB_Pin */
  GPIO_InitStruct.Pin = ROT_CHB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ROT_CHB_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
