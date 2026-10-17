J-Scope + RTT：在PC上实时观察变量波形
用J-Scope（SEGGER免费工具）通过RTT把MCU里的变量实时观察，不打断CPU、不占UART。
一、J-Scope三种采样模式
HSS（异步）：需要elf文件，不需要改固件，变量名可见，带宽中等。J-Link直接周期读RAM地址，零插桩，最快看变量。
RTT-Normal（同步）：不需要elf文件，需要改固件，变量名不可见，带宽高。固件自己打包发送，数据格式多样。本文重点介绍此模式。
RTT-Plus（同步）：需要elf文件和改固件，变量名可见，带宽高。同时拥有RTT带宽和符号名。
快速上手选RTT-Normal：不用管elf/符号文件，固件端定义好数据格式，J-Scope自动识别，最简单直接。
HSS是"完全不动固件"的备选；RTT-Plus是进阶。本文聚焦RTT-Normal。
二、快速上手（三步）
固件端加RTT组件 → 配置JScope通道并发送 → J-Scope里RTT模式连上看波形
Step 1 | 固件端：包含SEGGER RTT
把SEGGER_RTT.c、SEGGER_RTT.h、SEGGER_RTT_Conf.h加进工程（J-Link安装包自带，新版安装文件不再提供，可从github下载，地址：https://github.com/SEGGERMicro/RTT）。添加后编译能过即可。
Step 2 | 固件端：配JScope通道 + 周期发送
核心就两条API：SEGGER_RTT_ConfigUpBuffer()声明通道，SEGGER_RTT_Write()发数据。通道名必须叫JScope_<格式>，J-Scope靠它知道所发的内容是什么。当然别忘了在main()里初始化RTT。
Step 3 | PC端：先用RTT Viewer指定RTT控制块地址，再启动J-Scope直接连接
传统做法需要在J-Scope中手动搜索RTT控制块，有时搜索慢或找不到。推荐做法是先用J-Link RTT Viewer建立RTT连接并指定控制块地址，之后J-Scope即可直接连接，无需重复配置。具体步骤如下：
1. 打开J-Link RTT Viewer
2. 连接目标板（选择芯片型号、调试接口SWD/JTAG等）
3. 在RTT配置对话框中指定控制块地址（SetRTTAddr）或搜索范围（SetRTTSearchRanges），使J-Link软件正确定位RTT控制块
4. 验证RTT通信正常后关闭RTT Viewer（地址配置已保存在J-Link会话中）
5. 启动J-Scope → New project → 采样模式选RTT → 选芯片/接口 → OK
6. J-Scope自动搜到RTT控制块 → 按F5开始采样 → 看到波形
这样的好处是RTT控制块的地址搜索范围只需配置一次，后续J-Scope直接复用连接，启动更快更稳定。
三、核心机制：JScope_<FORMAT>通道命名
J-Scope会扫描RTT控制块的上行通道，找名字以JScope_开头的通道，FORMAT就是你的数据打包格式，用"类型字母+字节数"拼成：
类型说明：
t - 4字节 - 时间戳（可选，必须放第一个，单位微秒）
b - 1字节 - 布尔
f - 4字节 - 32位浮点
i - 1/2/4字节 - 有符号8/16/32位
u - 1/2/4字节 - 无符号8/16/32位
命名示例：
JScope_u4i4：unsigned int + signed int
JScope_f4u4：float + unsigned int
JScope_t4u1i4：32位时间戳 + unsigned char + signed int
硬规则（违反会出乱码/错位）：
1. 通道内数据顺序=通道名顺序，不能变
2. 一次只能写完整包，不能发半截
3. 采样会话期间不能改名/改格式
4. 变量总数（含时间戳）最多12个
5. 通道模式用SEGGER_RTT_MODE_NO_BLOCK_SKIP或SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
缓冲模式说明：
0 (SEGGER_RTT_MODE_NO_BLOCK_SKIP)：缓冲区满时丢弃数据（默认）
1 (SEGGER_RTT_MODE_NO_BLOCK_TRIM)：缓冲区满时截断写入
2 (SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL)：缓冲区满时阻塞等待
四、固件端插桩代码

4.1 最小示例：两个int交替画方波
#include "SEGGER_RTT.h"

int main(void) {
    char JS_RTT_UpBuffer[4096];      // J-Scope数据缓冲
    int  JS_RTT_Channel = 1;         // 用通道1（通道0留给终端日志）

    // 声明JScope通道，格式U4I4 = 一个unsigned int + 一个signed int
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
4.2 混合类型+时间戳：采集电机转速/电流等
#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"

static char JS_RTT_UpBuffer[8192];
#define JS_CH 1

// 用SysTick或定时器提供微秒时间戳（示意：换算到微秒）
static uint32_t JS_GetUs(void);

typedef struct {           // 对应JScope_t4f4i4
    uint32_t timestamp;    // t4：微秒时间戳，必须第一个
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
要点：结构体打包对齐要和FORMAT严格一致（用#pragma pack(1)或按类型逐字节写）；时间戳由你提供，J-Scope不生成。
五、J-Scope端操作步骤（GUI）
1. 打开J-Scope → New project
2. Connection：选USB（或TCP/IP，可填序列号/IP指定调试器）
3. Sampling Mode选RTT（不用填elf、不用选符号）
4. 选目标芯片、SWD/JTAG、速度 → OK
5. J-Scope自动搜索RTT控制块，把JScope_通道按格式解析成"符号"列出（RTT-Normal下显示的是通道格式而非真实变量名）
6. 点播放按钮（或F5）开始采样，波形出来
导出数据：File → Export Data... 存成CSV，用Excel打开分析。
六、排障速查
找不到RTT控制块/无波形：搜索范围没覆盖。解法：用-RTTSearchRanges限定到SRAM；确认固件已编入RTT
找不到JScope_通道：通道名/格式不符。解法：名字必须JScope_开头；用SEGGER_RTT_ConfigUpBuffer声明
波形数值错乱/跳变：打包顺序或类型与FORMAT不一致。解法：结构体字段顺序=FORMAT顺序；检查#pragma pack(1)对齐
有数据但断断续续：缓冲小/SKIP丢包。解法：缓冲放大到4KB+，或改BLOCK_IF_FIFO_FULL
变量显示为通道格式而非名字：RTT-Normal特性。解法：需要真实变量名则用RTT-Plus模式（需elf）
想采样第13个变量：J-Scope+RTT上限12个。解法：精简变量，或把多个u8合并
七、相关文件与参考
J-Scope官方手册：https://kb.segger.com/UM08028_J-Scope
RTT官方文档：https://wiki.segger.com/RTT
版本：J-Scope 8.10d起支持bool/float/uint8..32/int8..32；命令行为SEGGER J-Link Software V7.80+。