# HTP100 压力传感器检测

当前分支：`压力传感器检测`。**H7 是上位机**，使用 LVGL 驱动原屏幕；**G0 是下位机**，负责压力采集、定时峰值、校准保存与充电。PC 工具保留为独立 USART1 辅助调试工具。

入口与完整校准说明见 [交付/压力传感器检测/使用说明.md](交付/压力传感器检测/使用说明.md)。

- H7 上位机固件：`交付/压力传感器检测/HTP100-Pressure-H7-New.hex` / `HTP100-Pressure-H7-Old.hex`
- G0 下位机固件：`交付/压力传感器检测/HTP100-Pressure-Monitor.hex`
- 烧录入口：`Burn_Pressure_Main.bat`（G0）和 `Burn_Pressure_Screen.bat`（H7，选择屏幕类型）
- H7 上位机源码：`9C-U_HTP100_SCREEN_Picture -100/HardWare/Src/pressure_ui.c` / `pressure_client.c`
- PC 辅助程序：`交付/压力传感器检测/HTP100_Pressure_Sensor_Host.exe`，源码在 `tools/pressure_sensor_host/`
- 下位机测量和协议：`9C-U_HTP100_Main_vscode-200/App/Src/pressure_monitor.c`

保留原 ADS1220 硬件配置及 BQ25895/BQ27441 充电、电量驱动。两板沿用 **G0 USART2 PA2/PA3 ↔ H7 USART1 PA9/PA10**；PC 改用 **G0 USART1 PB6/PB7**。两个接口均为 115200、8N1，独立解析和回包，遥测同时发送；充电时保持屏幕工作，加热和电机关闭。原耗材软件在 `耗材初始化检测` 分支；本分支应使用“压力传感器检测”子目录中的程序和两端固件。

## 构建

安装依赖与 ARM GCC、CMake、Ninja：

```powershell
python -m pip install -r tools/pressure_sensor_host/requirements.txt
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_pressure_monitor.ps1 -BuildHost
```

不带 `-BuildHost` 编译并交付 G0 和 H7 新／旧屏三套固件；带该参数额外打包 PC 辅助程序。脚本校验 ASCII junction 的实际目标，以兼容 ST CMake 的路径限制。`build_fixture.ps1` 在本分支会提示改用压力检测构建入口。

源码启动：

```powershell
python tools/pressure_sensor_host/pressure_sensor_host.py
```

## 验证

```powershell
python -m pip install unicorn pyelftools pytest
python tools/test_pressure_monitor.py
python -m pytest -p no:faulthandler tools/pressure_sensor_host/tests -q
python tools/render_pressure_screen.py
python tools/render_pressure_screen.py --variant Old
```

第一项把生产测量和 UART DMA 代码编译为 ARM Thumb，在 Unicorn 中验证计时、峰值、转换、去皮、CRC、异常、EEPROM 保存，以及 DMA 队列的顺序、回绕、溢出和超时恢复。第二项包含砝码拟合、协议及真实 Qt 界面到生产 C 代码的串口仿真联调；Windows Unicorn 使用内部 SEH，关闭 pytest 的 faulthandler 插件可避免把其已处理异常输出为误导性的堆栈。

还会将生产 H7 客户端与生产 G0 测量核心直接联调，验证跨串口事务隔离，并运行实际 H7 LVGL ELF 的触屏校准、峰值、键盘与断线流程。界面预览在 `交付/压力传感器检测/H7界面预览（仿真）/`。实物传感器精度、额定量程、有效面积、充电状态和烧录后的板级功能仍需台架复核。
