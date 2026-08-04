/** @brief Wi-Fi 设置页。所有接口必须在 LVGL 任务上下文调用。 */
#pragma once

void wifi_settings_ui_create(void);

/** @brief 从控制中心打开 Wi-Fi 设置页。返回按钮将回到控制中心。 */
void wifi_settings_ui_create_from_control_center(void);
