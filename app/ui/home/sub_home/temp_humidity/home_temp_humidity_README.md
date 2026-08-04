# 首页温湿度 Tab 模块说明

`home_temp_humidity` 负责首页第一个 Tab 的全部内容：温度、湿度、CPU、FPS 与 `activity.png`。公开入口为 `home_temp_humidity_create_tab(lv_obj_t *parent)`，由 `home_ui` 传入位于 `(0, 120)`、`240 × 148 px` 的内容容器。

模块独立管理传感器刷新定时器和性能刷新定时器；在 Tab 切换或离开首页前，`home_ui` 必须调用 `home_temp_humidity_destroy()`，再删除内容容器的子控件，避免定时器访问失效的 LVGL 对象。

视觉坐标均相对于内容容器：温度行 `Y=12`、湿度行 `Y=60`；活动数据容器位于 `Y=106`，其 `activity.png` 图标底座位于 `Y=108`，三项图标均以 `X=36` 左对齐。活动行右侧在 `Y=106` 和 `Y=127` 依次显示 `CPU:05%` 与 `FPS:05`；CPU、FPS 数值均固定为两位，不足位以 `0` 补足，数值槽固定宽度并右对齐，避免数值变化造成布局跳动。底部的翻页提示和 `1 / 2` 页码由 `home_ui` 统一创建，不属于本模块。
