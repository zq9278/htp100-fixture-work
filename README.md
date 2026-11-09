# HTP100 综合工装

分支 `综合工装` 整合 `耗材初始化检测` 与 `压力传感器检测`。H7 主界面保留眼盾初始化、演示耗材、出厂检测、清除使用标记，新增 **压力计** 入口。压力页可以返回主界面。

交付文件和详细操作见 [综合工装使用说明](交付/综合工装/使用说明.md)。**G0、H7 必须同时烧录合并版**，原两个单功能固件不要混用。屏幕烧录脚本可选新屏／旧屏。

压力计保留实时 mmHg/N/g、持续最大值、清除后重新捕捉、六点校准、三轮加载卸载复核和充电，新增极性反转及参数保存。H7 与 G0 仍通过原串口连接；PC 为 G0 USART1 的可选辅助工具。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_combined.ps1 -BuildHost
python tools/test_fixture.py
python tools/test_pressure_monitor.py
python tools/test_combined.py
python tools/test_combined_eeprom.py
python -m pytest -p no:faulthandler tools/pressure_sensor_host/tests -q
python tools/render_combined.py
python tools/render_combined.py --variant Old
```

`-BuildHost` 额外打包 PC EXE；不带该参数只生成三套固件。测试在 ARM 仿真中运行生产核心及实际 H7 LVGL ELF，耗材 EEPROM 驱动测试仅替换物理 HAL 总线。实物烧录、传感器精度、安装受力及充电仍需台架复核。

G0 主程序、模式切换、压力与耗材控制位于 `9C-U_HTP100_Main_vscode-200/App`；H7 入口与界面位于 `9C-U_HTP100_SCREEN_Picture -100/HardWare`。原两条分支保留。

## 桌面 LVGL 预览

运行 `powershell -ExecutionPolicy Bypass -File "9C-U_HTP100_SCREEN_Picture -100/lvgl/run_sdl.ps1"`，默认打开综合工装主界面，底部“压力计”进入压力检测页。仅模拟切换确认，不生成传感器数据。加 `-ProductUi` 可打开旧产品界面。
