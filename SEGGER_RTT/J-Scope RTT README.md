J-Scope + RTT：在 PC 上实时观察变量波形（README）
> 用 **J-Scope**（SEGGER 免费工具）通过 **RTT** 把 MCU 里的变量实时观察，不打断 CPU、不占 UART。
---
一、J-Scope 三种采样模式
模式	需要 elf	需改固件	变量名可见	带宽	备注
HSS（异步）	✅	❌	✅	中	J-Link 直接周期读 RAM 地址，零插桩，最快看变量
RTT-Normal（同步）	❌	✅	❌	高	固件自己打包发送，数据格式多样，本 README 介绍
RTT-Plus（同步）	✅	✅	✅	高	RTT 带宽 + 符号名都想要
快速上手选 RTT-Normal：不用管 elf/符号文件，固件端定义好数据格式，J-Scope 自动识别，最简单直接。
> HSS 是"完全不动固件"的备选；RTT-Plus 是进阶。本文聚焦 **RTT-Normal**。
---
二、快速上手（三步）
```
固件端加 RTT 组件 → 配置 JScope_ 通道并发送 → J-Scope 里 RTT 模式连上看波形
```
Step 1｜固件端：包含 SEGGER RTT
把 `SEGGER_RTT.c`、`SEGGER_RTT.h`、`SEGGER_RTT_Conf.h` 加进工程（J-Link 安装包自带，新版安装文件不再提供，可从github下载，地址：https://github.com/SEGGERMicro/RTT
添加后编译能过即可。
Step 2｜固件端：配 `JScope_` 通道 + 周期发送
核心就两条 API：`SEGGER_RTT_ConfigUpBuffer()` 声明通道，`SEGGER_RTT_Write()` 发数据。通道名必须叫 `JScope_<格式>`，J-Scope 靠它知道所发的内容是什么。当然别忘了在 `main()` 里初始化 RTT。
Step 3｜PC 端：J-Scope 连接
```
启动 J-Scope → New project → 采样模式选 "RTT" → 选芯片/接口 → OK
→ 自动搜到 RTT 控制块 → F5 开始采样 → 看到波形
```
---
三、核心机制：`JScope_<FORMAT>` 通道命名（务必看懂）
J-Scope 会扫描 RTT 控制块的上行通道，找名字以 `JScope_` 开头的通道，`FORMAT` 就是你的数据打包格式，用"类型字母 + 字节数"拼成：
类型	字节数	含义
`t`	4	时间戳（可选，必须放第一个，单位 µs）
`b`	1	布尔
`f`	4	32 位浮点
`i`	1 / 2 / 4	有符号 8 / 16 / 32 位
`u`	1 / 2 / 4	无符号 8 / 16 / 32 位
命名示例：
通道名	数据包内容（按顺序）
`JScope_u4i4`	unsigned int + signed int
`JScope_f4u4`	float + unsigned int
`JScope_t4u1i4`	32 位时间戳 + unsigned char + signed int
硬规则（违反会出乱码/错位）：
通道内数据顺序 = 通道名顺序，不能变；
一次只能写完整包，不能发半截；
采样会话期间不能改名/改格式；
变量总数（含时间戳）最多 12 个；
通道模式用 `SEGGER_RTT_MODE_NO_BLOCK_SKIP` 或 `SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL`。
Flag 值	模式	行为
`0`	`SEGGER_RTT_MODE_NO_BLOCK_SKIP`	缓冲区满时丢弃数据 (默认)
`1`	`SEGGER_RTT_MODE_NO_BLOCK_TRIM`	缓冲区满时截断写入
`2`	`SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL`	缓冲区满时阻塞等待
---
四、固件端插桩代码
4.1 最小示例：两个 int 交替画方波
```c
#include "SEGGER_RTT.h"

int main(void) {
    char JS_RTT_UpBuffer[4096];      // J-Scope 数据缓冲
    int  JS_RTT_Channel = 1;         // 用通道 1（通道 0 留给终端日志）

    // 声明 JScope 通道，格式 U4I4 = 一个 unsigned int + 一个 signed int
    SEGGER_RTT_ConfigUpBuffer(JS_RTT_Channel, "JScope_U4I4",
                              &JS_RTT_UpBuffer[0], sizeof(JS_RTT_UpBuffer),
                              SEGGER_RTT_MODE_NO_BLOCK_SKIP);

    // 打包结构体，顺序必须与通道名一致
    #pragma pack(push, 1)
    struct {
        unsigned int vU32;
        signed   int vI32;
    } pkt;
    #pragma pack(pop)

    int i = 0;
    while (1) {
        pkt.vU32 = (i++ & 1) ? 100 : 0;
        pkt.vI32 = (i   & 1) ? -100 : 0;
        // 发送完整包
        SEGGER_RTT_Write(JS_RTT_Channel, &pkt, sizeof(pkt));
    }
}
```
4.2 混合类型 + 时间戳：采集电机转速/电流等
```c
#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"

static char JS_RTT_UpBuffer[8192];
#define JS_CH 1

// 用 SysTick 或定时器提供 µs 时间戳（示意：换算到 µs）
static uint32_t JS_GetUs(void);

typedef struct {           // 对应 JScope_t4f4i4
    uint32_t timestamp;    // t4：µs 时间戳，必须第一个
    float    speed;        // f4
    int32_t  current;      // i4
} JS_Pkt_t;

void JS_Init(void) {
    SEGGER_RTT_ConfigUpBuffer(JS_CH, "JScope_t4f4i4",
                              JS_RTT_UpBuffer, sizeof(JS_RTT_UpBuffer),
                              SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
}

void JS_Sample(void) {     // 定时器/任务里调用
    JS_Pkt_t pkt;
    pkt.timestamp = JS_GetUs();
    pkt.speed     = motor_speed;   // 你的全局变量
    pkt.current   = motor_current;
    SEGGER_RTT_Write(JS_CH, &pkt, sizeof(pkt));
}
```
> 要点：结构体**打包对齐**要和 FORMAT 严格一致（用 `#pragma pack(1)` 或按类型逐字节写）；时间戳由你提供，J-Scope 不生成。
---
五、J-Scope 端操作步骤（GUI）
打开 J-Scope → New project；
Connection：选 `USB`（或 `TCP/IP`，可填序列号/IP 指定调试器）；
Sampling Mode 选 RTT（不用填 elf、不用选符号）；
选目标芯片、SWD/JTAG、速度 → OK；
J-Scope 自动搜索 RTT 控制块，把 `JScope_` 通道按格式解析成"符号"列出（RTT-Normal 下显示的是通道格式而非真实变量名）；
点 ▶（或 F5） 开始采样，波形出来。
> 导出数据：`File → Export Data…` 存成 CSV，用 Excel 打开分析。
---
六、提速：指定 RTT 控制块搜索范围（对应 RTT Viewer 的 `-RTTSearchRange`）
J-Scope 每次连接要搜 RTT 控制块。避免全 RAM 扫，用命令行 `-RTTSearchRanges` 限定范围：
```bash
# 语法：-RTTSearchRanges <起始地址>,<长度>[,<起始地址2>,<长度2>,...]
JScope -openprj my.jscope -RTTSearchRanges 0x20000000,0x10000
```
使用 .JLinkScript 链接脚本示例
示例 1：指定 User Data 区搜索范围
```c
/*********************************************************************
 * J-Link Script - RTT 位于外部 SRAM 的 User Data 区
 **********************************************************************/

int InitEMU(void) {
    // 搜索范围: 0x80000000 - 0x80007FFF (32KB)
    JLINK_RTT_SetSearchRanges(0x80000000, 0x8000);
    return 0;
}
```
示例 2：直接指定控制块精确地址
```c
/*********************************************************************
 * J-Link Script - 从 .map 文件中获取到控制块地址后直接指定
 * 适用于链接地址固定的场景，连接速度最快
 **********************************************************************/

int InitEMU(void) {
    // 直接指定 RTT 控制块地址，跳过搜索
    // 地址来自编译器 .map 文件中的 _SEGGER_RTT 符号
    JLINK_RTT_SetCBAddr(0x20000400);
    return 0;
}
```
其他命令行选项：
选项	作用
`-openprj <文件>`	打开已有 `.jscope` 项目
`-RTTSearchRanges <addr>,<size>[,...]`	限定 RTT 控制块搜索范围（提速关键）
`-USB <序列号/昵称>`	指定 USB 调试器
`-IP <IP/主机名/序列号>`	指定网络调试器
工程最佳实践（衔接 J-Link RTT 指南）：把 `_SEGGER_RTT` 控制块放进固定 linker section（`.rtthdr`），地址稳定后配合限定搜索范围，连接几乎瞬间完成。
---
七、快捷键速查
键	功能
`F5` / `F9`	开始 / 停止采样
`↑` / `↓`	缩放 X
`←` / `→`	左右移动图形
`HOME` / `END`	跳到起点 / 终点
`Ctrl + +/-`	选中符号的 Y 偏移
`+` / `-`（选中符号）	Y 分辨率
---
八、排障速查
现象	根因	解法
找不到 RTT 控制块 / 无波形	搜索范围没覆盖	`-RTTSearchRanges` 限定到 SRAM；确认固件已编入 RTT
找不到 `JScope_` 通道	通道名/格式不符	名字必须 `JScope_` 开头；用 `SEGGER_RTT_ConfigUpBuffer` 声明
波形数值错乱/跳变	打包顺序或类型与 FORMAT 不一致	结构体字段顺序 = FORMAT 顺序；检查 `#pragma pack(1)` 对齐
有数据但断断续续	缓冲小 / SKIP 丢包	缓冲放大到 4KB+，或改 `BLOCK_IF_FIFO_FULL`
变量显示为通道格式而非名字	RTT-Normal 特性	需要真实变量名 → 用 RTT-Plus 模式（需 elf）
想采样第 13 个变量	J-Scope+RTT 上限 12 个	精简变量，或把多个 u8 合并
---
九、相关文件与参考
J-Scope 官方手册：https://kb.segger.com/UM08028_J-Scope
RTT 官方文档：https://wiki.segger.com/RTT
> 版本：J-Scope 8.10d 起支持 bool/float/uint8..32/int8..32；命令行为 SEGGER J-Link Software V7.80+。