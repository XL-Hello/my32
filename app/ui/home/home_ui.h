#pragma once

/** @brief 创建首页。必须在 LVGL 任务上下文调用。 */
void home_ui_create(void);

/** @brief 创建首页并直接显示相册 Tab。必须在 LVGL 任务上下文调用。 */
void home_ui_create_album_tab(void);

/** @brief 销毁首页的刷新定时器；在切换到其他页面前调用。 */
void home_ui_destroy(void);
