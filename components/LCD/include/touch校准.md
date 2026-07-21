# 触摸屏五点校准记录

采集日期：2026-07-21。显示方向：`LCD_ORIENTATION_PORTRAIT_INVERTED`，逻辑分辨率：`240x320`。

## 整理后的校准数据

五个校准点依次对应校准页面上的 `(20,20)`、`(219,20)`、`(120,160)`、`(20,299)`、`(219,299)`。每组日志的全部有效采样均参与均值计算，并以五点最小二乘法拟合仿射变换。

| 触点 | 目标像素坐标 | 原始坐标均值 `(raw_x, raw_y)` | 拟合坐标 `(x, y)` | 残差 `(px)` |
| --- | --- | --- | --- | --- |
| 1 | `(20, 20)` | `(528.700, 3662.400)` | `(21.38, 19.80)` | `(1.38, -0.20)` |
| 2 | `(219, 20)` | `(3426.700, 3684.600)` | `(216.64, 19.79)` | `(-2.36, -0.21)` |
| 3 | `(120, 160)` | `(2036.727, 2112.818)` | `(121.98, 160.83)` | `(1.98, 0.83)` |
| 4 | `(20, 299)` | `(504.182, 573.818)` | `(17.73, 298.80)` | `(-2.27, -0.20)` |
| 5 | `(219, 299)` | `(3510.364, 597.000)` | `(220.27, 298.78)` | `(1.27, -0.22)` |

最大拟合残差为 `2.36 px`，低于 8 px 的验收阈值。原始 X 与屏幕 X 同向，原始 Y 与屏幕 Y 反向，无需交换轴。

最终校准公式如下；代码实现会将结果四舍五入并限制在屏幕范围 `x=0..239`、`y=0..319`：

```text
x = 0.06737225 * raw_x + 0.00064867 * raw_y - 16.61356263
y = 0.00069024 * raw_x - 0.09033900 * raw_y + 350.29080470
```

## 原始采样日志

【1触点】
I (107993) touch: (touch_raw_log_task:151): pressed: raw_x=538 raw_y=3679
I (108103) touch: (touch_raw_log_task:157): moved: raw_x=547 raw_y=3631
I (108783) touch: (touch_raw_log_task:151): pressed: raw_x=522 raw_y=3647
I (108883) touch: (touch_raw_log_task:157): moved: raw_x=475 raw_y=3672
I (109593) touch: (touch_raw_log_task:151): pressed: raw_x=514 raw_y=3679
I (109693) touch: (touch_raw_log_task:157): moved: raw_x=521 raw_y=3671
I (110453) touch: (touch_raw_log_task:151): pressed: raw_x=493 raw_y=3711
I (110553) touch: (touch_raw_log_task:157): moved: raw_x=572 raw_y=3661
I (111353) touch: (touch_raw_log_task:151): pressed: raw_x=554 raw_y=3636
I (111453) touch: (touch_raw_log_task:157): moved: raw_x=551 raw_y=3637

【2触点】
I (191853) touch: (touch_raw_log_task:151): pressed: raw_x=3440 raw_y=3679
I (191953) touch: (touch_raw_log_task:157): moved: raw_x=3431 raw_y=3687
I (192573) touch: (touch_raw_log_task:151): pressed: raw_x=3440 raw_y=3672
I (192673) touch: (touch_raw_log_task:157): moved: raw_x=3423 raw_y=3675
I (193333) touch: (touch_raw_log_task:151): pressed: raw_x=3456 raw_y=3679
I (193433) touch: (touch_raw_log_task:157): moved: raw_x=3447 raw_y=3682
I (194183) touch: (touch_raw_log_task:151): pressed: raw_x=3439 raw_y=3695
I (194283) touch: (touch_raw_log_task:157): moved: raw_x=3421 raw_y=3711
I (195043) touch: (touch_raw_log_task:151): pressed: raw_x=3375 raw_y=3680
I (195143) touch: (touch_raw_log_task:157): moved: raw_x=3395 raw_y=3686

【3触点】
I (219563) touch: (touch_raw_log_task:151): pressed: raw_x=2102 raw_y=2135
I (219673) touch: (touch_raw_log_task:157): moved: raw_x=2075 raw_y=2092
I (220293) touch: (touch_raw_log_task:151): pressed: raw_x=2007 raw_y=2106
I (220393) touch: (touch_raw_log_task:157): moved: raw_x=2159 raw_y=2111
I (220993) touch: (touch_raw_log_task:151): pressed: raw_x=1961 raw_y=2144
I (221093) touch: (touch_raw_log_task:157): moved: raw_x=2008 raw_y=2119
I (221753) touch: (touch_raw_log_task:151): pressed: raw_x=2000 raw_y=2103
I (221853) touch: (touch_raw_log_task:157): moved: raw_x=1985 raw_y=2136
I (222553) touch: (touch_raw_log_task:151): pressed: raw_x=2019 raw_y=2092
I (222653) touch: (touch_raw_log_task:157): moved: raw_x=2040 raw_y=2108
I (222753) touch: (touch_raw_log_task:157): moved: raw_x=2048 raw_y=2095

【4触点】
I (252043) touch: (touch_raw_log_task:151): pressed: raw_x=458 raw_y=574
I (252143) touch: (touch_raw_log_task:157): moved: raw_x=489 raw_y=570
I (252823) touch: (touch_raw_log_task:151): pressed: raw_x=512 raw_y=569
I (252953) touch: (touch_raw_log_task:157): moved: raw_x=561 raw_y=589
I (253493) touch: (touch_raw_log_task:151): pressed: raw_x=468 raw_y=576
I (253603) touch: (touch_raw_log_task:157): moved: raw_x=500 raw_y=581
I (254163) touch: (touch_raw_log_task:151): pressed: raw_x=485 raw_y=568
I (254263) touch: (touch_raw_log_task:157): moved: raw_x=546 raw_y=572
I (254363) touch: (touch_raw_log_task:157): moved: raw_x=568 raw_y=567
I (254903) touch: (touch_raw_log_task:151): pressed: raw_x=486 raw_y=570
I (255043) touch: (touch_raw_log_task:157): moved: raw_x=473 raw_y=576

【5触点】
I (285743) touch: (touch_raw_log_task:151): pressed: raw_x=3455 raw_y=600
I (285843) touch: (touch_raw_log_task:157): moved: raw_x=3496 raw_y=632
I (286313) touch: (touch_raw_log_task:151): pressed: raw_x=3536 raw_y=598
I (286413) touch: (touch_raw_log_task:157): moved: raw_x=3515 raw_y=618
I (286933) touch: (touch_raw_log_task:151): pressed: raw_x=3519 raw_y=578
I (287033) touch: (touch_raw_log_task:157): moved: raw_x=3516 raw_y=596
I (287533) touch: (touch_raw_log_task:151): pressed: raw_x=3527 raw_y=564
I (287633) touch: (touch_raw_log_task:157): moved: raw_x=3508 raw_y=603
I (288183) touch: (touch_raw_log_task:151): pressed: raw_x=3522 raw_y=568
I (288283) touch: (touch_raw_log_task:157): moved: raw_x=3500 raw_y=605
I (288383) touch: (touch_raw_log_task:157): moved: raw_x=3520 raw_y=605
