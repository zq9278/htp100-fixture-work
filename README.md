# HTP100 压力传感器检测

当前分支：`压力传感器检测`。包含 Windows 上位机和 STM32G070CB 下位机。

入口与完整校准说明见 [交付/压力传感器检测/使用说明.md](交付/压力传感器检测/使用说明.md)。

- Windows 程序：`交付/压力传感器检测/HTP100_Pressure_Sensor_Host.exe`
- 主板固件：`交付/压力传感器检测/HTP100-Pressure-Monitor.hex`
- 烧录入口：`交付/压力传感器检测/Burn_Pressure_Main.bat`
- 上位机源码：`tools/pressure_sensor_host/`
- 下位机测量和协议：`9C-U_HTP100_Main_vscode-200/App/Src/pressure_monitor.c`

保留原 ADS1220 硬件配置及 BQ25895/BQ27441 充电、电量驱动。USART2 用于 PC，屏幕关闭；加热和电机始终关闭。原耗材软件在 `耗材初始化检测` 分支；本目录根部的旧耗材交付文件是基线保留文件，本分支应使用“压力传感器检测”子目录中的程序和固件。

## 构建

安装依赖与 ARM GCC、CMake、Ninja：

```powershell
python -m pip install -r tools/pressure_sensor_host/requirements.txt
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_pressure_monitor.ps1 -BuildHost
```

不带 `-BuildHost` 只编译并复制下位机固件。脚本校验 ASCII junction 的实际目标，以兼容 ST CMake 的路径限制。

源码启动：

```powershell
python tools/pressure_sensor_host/pressure_sensor_host.py
```

## 验证

```powershell
python -m pip install unicorn pyelftools pytest
python tools/test_pressure_monitor.py
python -m pytest -p no:faulthandler tools/pressure_sensor_host/tests -q
```

第一项把生产测量和 UART DMA 代码编译为 ARM Thumb，在 Unicorn 中验证计时、峰值、转换、去皮、CRC、异常、EEPROM 保存，以及 DMA 队列的顺序、回绕、溢出和超时恢复。第二项包含砝码拟合、协议及真实 Qt 界面到生产 C 代码的串口仿真联调；Windows Unicorn 使用内部 SEH，关闭 pytest 的 faulthandler 插件可避免把其已处理异常输出为误导性的堆栈。

测试生成的界面预览明确标记为仿真数据。实物传感器精度、额定量程、加载结构有效面积、充电状态和烧录后的板级功能仍需台架复核。
