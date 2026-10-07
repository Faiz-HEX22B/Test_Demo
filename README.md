项目简介

1. 轻量化现代开发模式：以 VSCode + ARM-GCC 为主力工具链，配合 Git + GitHub 做版本托管。
2. 兼容 Keil-MDK-ARM：仓库完整保留 Keil 工程源文件，Windows 下可直接打开 MDK 进行编译调试。


开发环境

主控芯片：STM32F407VET6
开发工具：VSCode + STM32 Extension + ARM-GCC 工具链
配置工具：STM32CubeMX
调试烧录：ST-LINK / J-LINK
版本管理：Git + GitHub（纯净仓库管理模式）


工程结构

Core/          内核核心代码、系统初始化、中断配置
Drivers/       STM32 HAL 库底层驱动文件
Inc/           用户应用层头文件
Src/           用户应用层源码文件
.ioc           CubeMX 原始配置工程文件（可二次修改重生成代码）
.gitignore     过滤编译产物、IDE 缓存、临时文件，保证仓库干净轻量化
README.md      工程说明文档

注：Inc/ 与 Src/ 也可能位于 Core/ 之下，具体以实际工程为准。


项目特点

1. 双环境兼容：同一套源码，既可使用 VSCode + GCC 跨平台开发，也可直接用 Keil-MDK 打开编译。
2. 纯净仓库管理：仅托管源码、CubeMX 配置、Keil 工程本体；编译输出、调试缓存全部忽略，仓库干净。
3. 标准现代化流程：本地开发 + Git 版本控制 + GitHub 云端托管，适合模板复用与二次迭代。


快速开始

1. 克隆仓库

git clone https://github.com/Faiz-HEX22B/Test_Demo.git
cd Test_Demo

克隆后编译产物与缓存文件不会存在，首次编译会自动生成。

2. 工程打开方式

VSCode：直接打开工程根目录。
STM32CubeMX：通过 .ioc 文件打开，可修改引脚、时钟、外设配置。
Keil-MDK：Windows 下直接双击工程文件（如 *.uvprojx）打开。
注意：若工程中新增了其他源码文件夹，需要在 Keil 的 Options for Target -> C/C++ -> Include Paths 中添加对应的头文件包含路径，否则编译时会提示找不到头文件。

3. 编译与烧录

VSCode + ARM-GCC：使用 STM32 Extension 完成编译。
Keil-MDK：使用 MDK 自带工具链编译。
下载调试：通过 ST-LINK 或 J-LINK 完成下载与在线调试。


补充说明

本工程为基础模板 Demo，仅完成系统最小初始化。可在此基础上自由拓展：

GPIO
串口（UART/USART）
ADC
定时器
中断
RTOS
外设驱动

所有编译产物、本地缓存文件均已通过 .gitignore 过滤，克隆后本地编译会自动生成对应文件，无需上传至仓库。


开源协议

可自由学习、修改、二次开发。