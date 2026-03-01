# Spectrum MK-I Prototype

![License](https://img.shields.io/badge/license-CC--BY--NC--SA--4.0-blue.svg)

**Spectrum MK-I Prototype** 是一个基于 STM32G4 系列微控制器的高保真音频设备原型项目。本项目集成了 AK4493 DAC、NJW1195A 音量控制芯片以及 USB PD 诱骗等功能，旨在提供一个高性能、可扩展的音频处理平台。

## ✨ 主要特性 (Features)

*   **高性能核心**: 基于 STM32G491RE MCU，提供强大的运算能力。
*   **高保真音频**:
    *   板载 **AK4493** DAC，支持高规格音频解码。
    *   集成 **NJW1195A** 多通道模拟音量控制器。
    *   支持 **CT7302** 数字音频接收/升频。
*   **USB 接口**:
    *   基于 **CherryUSB** 协议栈。
    *   支持 USB CDC ACM 虚拟串口，用于命令行交互。
    *   支持 USB Audio (规划中/开发中)。
*   **电源管理**: 集成 **CH224Q** USB PD 诱骗芯片，支持高压快充输入，并可根据系统负载动态调整输入电压（5V/9V/12V/20V/28V）。
*   **交互系统**: 移植 **LetterShell**，提供丰富的 Shell 命令进行运行时参数调整（如音量、电源控制、数字滤波器设置等）。
*   **调试支持**: 集成 **Segger RTT**，方便实时调试与日志输出。

## 🛠️ 硬件架构 (Hardware Architecture)

*   **MCU**: STM32G491RE
*   **DAC**: AKM AK4493
*   **Volume Controller**: JRC NJW1195A
*   **USB PD Controller**: WCH CH224Q
*   **Audio Receiver**: CT7302

## 📂 目录结构 (Directory Structure)

```
Spectrum_MK-I_Prototype/
├── APP/                 # 应用层代码
│   ├── CDC_ACM/         # USB CDC 环形缓冲区处理
│   ├── LetterShell/     # 命令行交互组件
│   └── PDO/             # USB PD 协议对象
├── Core/                # STM32 CubeMX 生成的核心代码
├── Drivers/             # 驱动层
│   ├── AK4493/          # AK4493 DAC 驱动
│   ├── CH224Q/          # CH224Q PD 芯片驱动
│   ├── CT7302/          # CT7302 驱动
│   ├── NJW1195A/        # NJW1195A 驱动
│   ├── CherryUSB/       # USB 协议栈
│   └── SeggerRTT/       # RTT 调试组件
└── Doc/                 # 项目文档
```

## 🚀 快速开始 (Getting Started)

### 开发环境
*   **IDE**: Visual Studio Code + EIDE 插件 (推荐) 或 Keil MDK
*   **Compiler**: ARM GCC Toolchain
*   **Debugger**: J-Link / ST-Link

### 编译与烧录
1.  克隆本项目到本地：
    ```bash
    git clone https://github.com/PolarisYu/Spectrum_MK-I_Prototype.git
    ```
2.  使用 VSCode 打开项目文件夹。
3.  通过 EIDE 加载项目配置。
4.  点击编译按钮生成固件。
5.  连接调试器并下载固件到目标板。

## 💻 使用说明 (Usage)

系统启动后，通过 USB 连接电脑，识别为虚拟串口。使用串口终端工具（波特率任意）连接后，按 `Enter` 键即可进入 Shell 交互界面。

### 常用命令

*   **help**: 查看所有可用命令。
*   **akvol**: 设置 AK4493 音量。
    *   `akvol -20.5` (设置音量为 -20.5dB)
    *   `akvol 255` (设置音量为 0dB/最大)
*   **akvol_lr**: 独立设置 AK4493 左右声道音量。
    *   `akvol_lr -20.0 -10.0` (左声道 -20dB，右声道 -10dB)
*   **power**: 电源控制。
    *   `power amp 1` (开启耳放电源)
    *   `power dac 0` (关闭 DAC 电源)

## 📄 开源协议 (License)

本项目采用 **CC-BY-NC-SA 4.0** (署名-非商业性使用-相同方式共享 4.0 国际) 协议进行许可。

*   **署名 (BY)**: 您必须给出适当的署名，提供指向本许可协议的链接，同时标明是否（对原始作品）作了修改。
*   **非商业性使用 (NC)**: 您不得将本作品用于商业目的。
*   **相同方式共享 (SA)**: 如果您再混合、转换或者基于本作品进行创作，您必须基于与原先许可协议相同的许可协议分发您贡献的作品。

详细协议内容请参阅 [LICENSE](LICENSE) 文件（需自行添加）或访问 [Creative Commons](https://creativecommons.org/licenses/by-nc-sa/4.0/)。

## 🤝 贡献 (Contribution)

欢迎提交 Issue 和 Pull Request 来改进本项目！

## 👥 致谢 (Acknowledgements)

*   [CherryUSB](https://github.com/cherry-embedded/CherryUSB)
*   [LetterShell](https://github.com/NevermindZZT/letter-shell)
*   [STM32HAL](https://www.st.com/)
