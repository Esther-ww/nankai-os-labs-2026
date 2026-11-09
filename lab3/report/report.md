# OSLab3实验报告

## 实验基本信息

|**项目**|**内容**|
|---|---|
|**实验名称**<br>|Lab3：断，都可以断——中断与异常处理|
|**小组成员**|A：2413685\-刘晟槿；<br>B：2413978\-缪承佑；<br>C：2412000\-文玥|
|**完成日期**|11\.7|
|**代码仓库**|https://github\.com/Esther\-ww/nankai\-os\-labs\-2026|
|**提交分支**|lab3|

### 小组分工

练习如何分工？

|**成员**|**负责的练习/模块**|
|---|---|
|A:2413685\-刘晟槿|Lab2基础代码衔接；模块一：Trap入口与上下文恢复，完成RESTOREALL、\_alltraps|
|B:2413978\-缪承佑|模块二的中断处理：interrupt\_handler\(\)，包括时钟事件设置、计数、统计输出及关机|
|C:2412000\-文玥|模块二的异常处理：exception\_handler\(\)，包括非法指令、断点异常及epc更新|

实验报告如何分工？

|**成员**|**负责的报告内容**|
|---|---|
|A：2413685\-刘晟槿|基础衔接、Trap frame布局、汇编入口、上下文恢复；对应spec/prompt、迭代记录及测试证据|
|B：2413978\-缪承佑|中断分类、时钟事件、计数与输出、关机条件；对应spec/prompt、迭代记录及测试证据|
|C：2412000\-文玥|异常分类、输出、指令长度与epc更新；对应spec/prompt、迭代记录及测试证据|

---

## 一、实验目的

1. 理解RISC\-V中断与异常处理机制，以及Trap相关寄存器在进入、处理和返回过程中的作用。

2. 理解Trap frame的布局，完成与已有SAVE\_ALL对应的上下文恢复，使寄存器、栈和执行位置能够正确恢复。

3. 建立汇编入口\_\_alltraps与C语言trap\(struct trapframe \*tf\)之间的调用关系。

4. 实现连续时钟中断的处理、计数、周期统计输出及达到规定条件后的关机。

5. 实现非法指令与断点异常处理，依据实际指令长度更新返回位置，避免重复异常。

6. 使用结构化spec与prompt辅助代码生成，通过实际测试和GDB观察验证实现。

---

## 二、实验环境

### 2\.1 开发与运行环境

|**项目**|**已有记录或本次填写内容**|
|---|---|
|**宿主系统**|Windows \+ WSL2|
|**Linux发行版**|Ubuntu 22\.04\.5 LTS（此前环境）|
|**交叉编译器**|riscv64\-unknown\-elf\-gcc 10\.2\.0（此前环境）|
|**客户调试器**|riscv64\-unknown\-elf\-gdb 10\.1（此前环境）|
|**模拟器**|QEMU 4\.1\.1，RISC\-V virt（此前环境）|
|**Lab3源码来源**|课程提供的Lab3代码包或仓库|
|**构建与运行命令**|以Lab3 Makefile为准|

### 2\.2 使用的AI工具

|**成员**|**AI编程工具**|**底层模型**|**备注**|
|---|---|---|---|
|A：2413685\-刘晟槿|Codex<br>|GPT 6\.1 sol||
|B：2413978\-缪承佑|DeepSeek Harness|DeepSeek\-V4\-Flash||
|C：2412000\-文玥|Codex|GPT 6 Astra||

---

## 三、实验整体逻辑分析

### 3\.1 本章节的逻辑主线

本实验围绕“Trap进入、上下文保存、C语言分类处理、上下文恢复、返回原执行流”展开。在已有内存支持的基础上，为内核补充对时钟事件和指定异常的响应能力。

Trap发生时，处理器更新相关控制状态并转入设置好的入口；入口汇编负责保存软件需要维护的上下文，形成Trap frame。C语言处理函数根据原因区分中断与异常，完成相应处理；随后汇编恢复状态，由sret返回。返回的特权级由相关状态决定，不能把所有sret都理解为必然回到用户态。

```Plain Text
补齐Lab2基础功能、完成启动初始化
                  ↓
配置Trap入口与所需中断使能
                  ↓
时钟事件 / 非法指令 / 断点触发Trap
                  ↓
__alltraps → SAVE_ALL → Trap frame
                  ↓
trap(tf) → trap_dispatch(tf)
                  ↓
interrupt_handler(tf) 或 exception_handler(tf)
                  ↓
__trapret → RESTORE_ALL → sret
                  ↓
继续原执行流（达到规定条件时执行关机）
```

### 3\.2 功能的逐步实现

1. **补齐基础代码。** 根据Lab3中标注lab2的待补位置接入此前对应功能，确认框架能够编译，避免覆盖Lab3已有逻辑。

2. **建立Trap入口和恢复闭环。** 依据SAVE\_ALL与trapframe布局实现RESTORE\_ALL，正确传递tf指针，使C层处理能够调用和返回。

3. **实现中断处理。** 接入连续时钟事件、计数、统计输出及关机，验证系统能持续响应异步事件。

4. **实现异常处理。** 正确识别指定异常、输出信息并更新epc，确保返回后不会在同一条异常指令上循环。

5. **联合验证。** 核对完整执行链路，分别验证各模块，并记录实际提示词、问题修正和运行证据。

---

## 四、实验内容与实现

### 4\.1 前置任务（A部分）：Lab2基础代码衔接

**负责人：** A：2413685\-刘晟槿

#### 模块功能描述

使用此前Lab2提示词和实现，补齐Lab3源码中标有lab2的对应位置，保持Lab3的初始化接口和已有逻辑。只迁移本次框架实际需要的功能，不直接复制整个Lab2目录覆盖Lab3。

|**项目**|**实际记录**|
|---|---|
|**找到的Lab2 待补位置**|在Lab3源码中找到两处//lab2请补充你在lab2中的代码。code/kern/mm/pmm\.c缺少pmm\_init\(\)调用的page\_init\(\)；code/kern/mm/best\_fit\_pmm\.c缺少Best\-Fit分配器的实现及best\_fit\_pmm\_manager接口表。|
|<br>**复用的Lab2 提示词**|参考Lab2 A部分实验报告§4\.3的page\_init\(\)最终提示词。主要约束是从设备树（DTB）读取物理内存范围，将内存末端限制在内核可管理的范围内，在内核镜像之后按页对齐放置struct Page数组，先将页描述符标记为保留，再通过init\_memmap\(\)注册剩余的完整空闲页。Best\-Fit部分复用了小组在Lab2已完成的实现。|
|**接口或框架差异**|Lab3未定义Lab2使用的PHYSICAL\_MEMORY\_END和KERNEL\_BEGIN\_PADDR。衔接时分别用KERNTOP \- PHYSICAL\_MEMORY\_OFFSET和KERNBASE \- PHYSICAL\_MEMORY\_OFFSET推导对应的物理地址。Lab3默认使用default\_pmm\_manager（First\-Fit），页分配和释放接口带有中断保护，迁移时沿用了这些逻辑。|
|**本次实际改动**|在pmm\.c中补齐page\_init\(\)，完成物理内存范围检查、页描述符初始化和空闲页注册。在best\_fit\_pmm\.c中补齐空闲链表管理、分配、释放、计数及best\_fit\_pmm\_manager接口表，保留Lab3原有的自检函数。|
|**编译与基础检查结果**|在WSL中执行make，成功生成bin/kernel和bin/ucore\.img。QEMU启动后，DTB识别到128 MB物理内存，page\_init\(\)报告31928个空闲页，并输出check\_alloc\_page\(\) succeeded\!。本次启动自检覆盖默认的First\-Fit；Best\-Fit已随源码编译，运行时自检待单独验证。启动证据见图1。|

### 4\.2 功能模块一（A部分）：Trap入口与上下文恢复

**负责人：** A：2413685\-刘晟槿

#### 需要实现的汇编功能

文件为 `kern/trap/trapentry.S`。`__alltraps` 调用已有的 `SAVE_ALL` 建立 Trap frame，再将帧指针传给 C 函数。C 函数返回后，由 `__trapret` 恢复上下文并执行 `sret`。

```Plain Text
.macro RESTORE_ALL
    #lab3 function1 2413685
    # 先借用 x5，将帧中的返回状态写回 CSR
    LOAD x5, 32*REGBYTES(sp)
    csrw sstatus, x5
    LOAD x5, 33*REGBYTES(sp)
    csrw sepc, x5

    # 按 SAVE_ALL 的槽位恢复通用寄存器
    # x0 恒为 0，x2（sp）留到最后恢复
    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)

    # 最后一次访问帧时，恢复 Trap 发生前的栈指针
    LOAD sp, 2*REGBYTES(sp)
    .endm

    .globl __alltraps
    .align(2)
__alltraps:
    SAVE_ALL
    #lab3 function1 2413685
    mv a0, sp                  # 第一个参数是 Trap frame 的地址
    call trap                  # C 函数返回到紧随其后的 __trapret

    .globl __trapret
__trapret:
    RESTORE_ALL
    sret                       # 根据 sepc 和 sstatus 返回
```

#### 已有 C 接口

`kern/trap/trap.h` 定义了 Trap frame 的布局及 `trap()` 的声明。`kern/trap/trap.c` 中的 `trap()` 将同一个帧指针交给 `trap_dispatch()`。以下代码按文件顺序摘录。

```Plain Text
/* kern/trap/trap.h */
struct trapframe {
    struct pushregs gpr;  /* 槽位 0–31，对应 x0–x31 */
    uintptr_t status;    /* 槽位 32，保存 sstatus */
    uintptr_t epc;       /* 槽位 33，保存 sepc，可由异常处理函数更新 */
    uintptr_t badvaddr;  /* 槽位 34，保存与 Trap 相关的地址信息 */
    uintptr_t cause;     /* 槽位 35，保存 scause */
};

void trap(struct trapframe *tf);

/* kern/trap/trap.c */
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        interrupt_handler(tf);
    } else {
        exception_handler(tf);
    }
}

void trap(struct trapframe *tf) {
    trap_dispatch(tf);
}
```

汇编中的 `a0` 对应 C 接口的参数 `tf`。`trap_dispatch()` 通过 `cause` 的最高位区分中断和异常。异常处理函数可以更新 `tf->epc`，因此 `RESTORE_ALL` 从帧的第 33 个槽位读回处理后的值，再写入 `sepc`，供 `sret` 使用。

#### 最终提示词

> 请先检查 Lab3 源码中的 `kern/trap/trapentry.S` 和 `kern/trap/trap.h`，根据 `SAVE_ALL` 与 `struct trapframe` 的实际布局，完成 A 部分的 `RESTORE_ALL` 和 `__alltraps`。在 `__alltraps` 中按 RISC\-V 调用约定，把当前 Trap frame 指针放入 `a0` 后调用 `trap()`。C 函数返回后进入已有的 `__trapret`，先从帧中恢复 `sstatus` 和处理后的 `sepc`，再恢复通用寄存器，最后恢复原 `sp` 并执行 `sret`。沿用 `SAVE_ALL`、Trap frame 布局和 `trap()` 的函数签名。给出 GDB 验证步骤，检查入口处 `a0` 是否等于帧基址，以及返回后的 `sp`、`pc` 和示例寄存器 `t0` 是否与帧中记录的值一致。测试结论以实际运行结果为准。
> 
> 

#### 实现说明与代码位置

|**关键点**|**实际实现记录**|
|---|---|
|**Trap frame字段与偏移**|RV64的每个槽位为8字节，整帧为288字节。gpr对应槽位0–31，寄存器xn的字节偏移为n×8。status、epc、badvaddr、cause分别位于槽位32–35，对应字节偏移256、264、272、280。|
|**sp保存、恢复方式**|SAVE\_ALL先执行csrw sscratch, sp暂存原栈指针，再执行addi sp, sp, \-36 \* REGBYTES分配帧空间。随后用csrrw s0, sscratch, x0取回原sp，通过STORE s0, 2\*REGBYTES\(sp\)写入槽位2。RESTORE\_ALL的最后一条加载指令是LOAD sp, 2\*REGBYTES\(sp\)，此前各条加载指令均以帧基址为基址。|
|**tf指针的传递**|\_\_alltraps执行SAVE\_ALL后，用mv a0, sp设置第一个参数，再用call trap调用C入口。call记录的返回地址对应紧随其后的\_\_trapret。|
|**epc及状态寄存器恢复**|通过LOAD x5, 32\*REGBYTES\(sp\)和csrw sstatus, x5恢复状态。再通过LOAD x5, 33\*REGBYTES\(sp\)和csrw sepc, x5写回处理后的返回地址。随后从槽位5恢复原x5，最终由sret使用恢复后的状态返回。|
|**最终代码位置/commit**|code/kern/trap/trapentry\.S中的RESTORE\_ALL、\_\_alltraps和\_\_trapret。|

#### 实现迭代过程

实际记录分为 5 轮，含初始实现、两次启动排查、GDB 验证和整合评分。

|**轮次**|**实际 prompt 或记录位置**|**现象与原因**|**修改方式**|**验证结果**|
|---|---|---|---|---|
|**1**|本节「最终提示词」、初次 make 输出|源码已有 SAVE\_ALL 和 \_\_trapret，RESTORE\_ALL 与 \_\_alltraps 的调用逻辑待补。|按现有帧布局完成寄存器恢复和 C 函数调用。|编译、链接成功，生成 bin/kernel 和 bin/ucore\.img。|
|**2**|lab3\-qemu\.log、lab3\-qemu\-kernel\.log|原 make qemu 使用 \-device loader，OpenSBI 显示下一阶段入口地址为 0。|改用 \-kernel bin/ucore\.img 启动。|OpenSBI 显示下一阶段入口为 0x80200000，随后仍停在固件输出。|
|**3**|entry\.S 反汇编、lab3\-qemu\-boot\.log、图 1|入口汇编写入 satp 后仍需在低地址取指，而启动页表当时只建立了高地址映射。|在 entry\.S 的启动页表第 2 项加入低地址恒等映射，保留高地址映射。|内核成功启动，物理页分配器通过自检，随后多次输出 100 ticks。|
|**4**|GDB 调试记录、图 2、图 3a 和图 3b|多行粘贴被 GDB 当作一条命令解析，导致语法错误。|改为逐行执行，在 trap、\_\_trapret 和预期返回地址处分别观测。|a0 == sp 返回 1。返回后 sp、pc、t0 与帧中对应值的比较也均返回 1。|
|**5**|lab3\-grade\.log、lab3\-grade\-rerun\.log|首次评分三项都编译成功，但 tools/grade\.sh 仍使用旧的 \-device loader，总分为 0/100。|将评分脚本的镜像参数改为 \-kernel bin/ucore\.img，重新运行 make grade。|Trap 入口、寄存器恢复与断点返回用例得 35/35，三项整合测试总分 100/100。|

**最终结果。** 编译通过。GDB 确认 `trap()` 收到当前 Trap frame 的地址，并观察到本次时钟 Trap 返回后，`sp`、`pc` 和 `t0` 与帧中对应值一致。内核恢复执行到 `kern_init()` 的 `while (1)` 处。

### 4\.3 功能模块二（B部分）：中断处理

**负责人：** B：2413978\-缪承佑

#### 模块功能描述

**需要实现的函数：****void interrupt\_handler\(struct trapframe \*tf\);**

```Plain Text
void interrupt_handler(struct trapframe *tf) {
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
        case IRQ_S_TIMER:
            // 设置下一次时钟事件，保证时钟中断持续触发
            clock_set_next_event();
            // 每次时钟中断ticks加一
            ticks++;
            // ticks达到TICK_NUM的整数倍时输出一次统计信息
            if (ticks % TICK_NUM == 0) {
                print_ticks();
                num++;
            }
            // 累计输出次数达到SHUTDOWN_AFTER后调用关机接口
            if (num >= SHUTDOWN_AFTER) {
                sbi_shutdown();
            }
            break;
        case IRQ_U_SOFT:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_SOFT:
            cprintf("Supervisor software interrupt\n");
            break;
        case IRQ_H_SOFT:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_SOFT:
            cprintf("Machine software interrupt\n");
            break;
        case IRQ_U_TIMER:
            cprintf("User timer interrupt\n");
            break;
        case IRQ_H_TIMER:
            cprintf("Hypervisor timer interrupt\n");
            break;
        case IRQ_M_TIMER:
            cprintf("Machine timer interrupt\n");
            break;
        case IRQ_U_EXT:
            cprintf("User external interrupt\n");
            break;
        case IRQ_S_EXT:
            cprintf("Supervisor external interrupt\n");
            break;
        case IRQ_H_EXT:
            cprintf("Hypervisor external interrupt\n");
            break;
        case IRQ_M_EXT:
            cprintf("Machine external interrupt\n");
            break;
        default:
            cprintf("Unknown interrupt: cause %d\n", (int)cause);
            print_trapframe(tf);
            break;
    }
}
```

代码位置：`kern/trap/trap.c`。按照Trap原因处理中断，重点完成连续时钟事件、ticks计数、周期统计输出和规定条件下关机；其他类型按要求提示，无法识别的中断输出Trap frame。

#### 最终提示词

```Markdown
【任务说明】
任务：在文件 kern/trap/trap.c 中，标有 lab3 function2、YOUR CODE 的位置，完整实现中断处理函数 interrupt_handler(struct trapframe *tf)，重点是时钟中断的连续触发、ticks 计数、周期统计输出与规定条件下关机。参考同文件 trap_dispatch() 的中断/异常分类方式，以及 clock.h/sbi.h/riscv.h 提供的接口。

硬性约束：
1. 禁止修改 exception_handler()；禁止修改 trap_dispatch()、trap()、idt_init()、print_ticks()、print_trapframe() 等已有函数的签名与逻辑，只在 // lab3 function2 YOUR CODE 区域写实现。
2. 中断与异常的区分由 trap_dispatch() 完成：(intptr_t)tf -> cause < 0 判定为中断，转入 interrupt_handler，本函数不再自行判断符号位。
3. 时钟相关能力复用框架已有接口：clock_set_next_event() 设置下一次时钟事件，ticks 为 clock.h 声明的 volatile size_t 全局计数；关机使用 sbi.h 的 sbi_shutdown()。
4. 输出格式以 grade.sh 实际检查字符串为准，不自行发挥：Supervisor software interrupt、Unknown interrupt: cause 15、100 ticks 等必须逐字命中。
5. 中断使能（clock_init 中的 set_csr(sie, MIP_STIP）、kern_init 中的 intr_enable()）由框架负责，本模块不处理；本函数只负责“已使能中断到来后”的处理。

验证标准：时钟中断能持续触发而非只触发一次；每次中断 ticks 加一；ticks 达到 TICK_NUM 的整数倍时输出统计信息；累计输出次数达到 SHUTDOWN_AFTER 后关机，不提前关机也不无限运行；无法识别的中断调用 print_trapframe(tf) 输出上下文；最终通过 grade.sh 中 GRADE_LAB3_TIMER 与非法指令用例里的中断相关检查项。

【Rely 依赖说明（仅使用以下已有定义，不自行新增复杂结构）】
依赖头文件：clock.h、sbi.h、riscv.h、trap.h、stdio.h

依赖核心定义：
· struct trapframe（重点使用 tf -> cause，类型 uintptr_t）。
· volatile size_t ticks（clock.h 声明、clock.c 定义）。
· 中断原因宏 IRQ_U_SOFT...IRQ_M_EXT（riscv.h），其中 IRQ_S_SOFT=1、IRQ_S_TIMER=5、IRQ_S_EXT=9。
· 本文件已定义 TICK_NUM 100 辅助函数：clock_set_next_event()、sbi_shutdown()、cprintf()、print_ticks()、print_trapframe(rf)标志/规则：RISC-V 中 scause 最高位为中断标志位，实际中断原因编号用 (tf -> case << 1) >> 1 剥掉最高位得到；ticks 是“原始中断次数”，与“统计输出次数”是两个不同概念，需分开计数。

【Guarantee 接口清单】
需要实现一个函数：
void interrupt_handler(struct trapframe *tf);

内部结构：在本文件 TICK_NUM 附近声明 #define SHUTDOWN_AFTER 10 与 static volatile size_t num = 0;，num 记录“统计输出次数”（即 print_ticks 被调用的次数），与 ticks 区分；interrupt_handler 保持外部链接（test_lab3.c 会 extern 声明调用）。

【Specification 行为规格】
1. 原因提取
前置：tf -> cause 为中断类型（最高位已置位）。
行为：intptr_t cause = (tf -> cause << 1) >> 1；得到纯中断编号，随后 switch (cause) 分支。
后置：cause 为 IRQ_* 编号或无法识别的值。

2. 时钟中断 IRQ_S_TIMER
行为：
（1）先 clock_set_next_event() 设置下一次时钟事件——这是“持续触发”的关键，否则定时器只触发一次；
（2）ticks++ 每次中断加一；
（3）若 ticks % TICK_NUM == 0，调用 print_ticks() 输出统计信息，并num++；
（4）若num >= SHUTDOWN_AFTER，调用 sbi_shutdown() 关机。
后置：下一事件已设置；ticks、num 增量正确；达到关机条件后不再返回。

3. 其余已定义中断
分支：IRQ_U_SOFT/IRQ_S_SOFT/IRQ_H_SOFT/IRQ_M_SOFT/IRQ_U_TIMER/IRQ_H_TIMER/IRQ_M_TIMER/IRQ_U_EXT/IRQ_S_EXT/IRQ_H_EXT/IRQ_M_EXT，各自 cprintf 打印对应提示，例如 IRQ_S_SOFT 打印 Supervisor software interrupt。
后置：仅提示，不改变状态。

4. 无法识别的中断（default）
行为：cprintf("Unknown interrupt: cause %d\n, (int)cause); 随后 print_trapframe(tf); 输出完整上下文。
后置：Trap frame 被打印，便于定位未知中断来源。

额外代码要求：
代码注释风格对齐 trap.c 已有注释，关键逻辑（重新武装、ticks 与 num 的区别、关机条件）加注释；C 语言语法适配 ucore 编译环境（%d 需 (int) 强转避免 -Werror 格式告警），不引入 stdlib、printf 等无关库；exception_handler 不需完成。 
```

#### 实现说明与代码位置

|**关键点**|**实际实现记录**|
|---|---|
|**中断原因识别方式**|分发由 trap\_dispatch\(\)完成；\(intptr\_t\)tf\-\>cause\<0即scause最高位为1判为中断，转interrupt\_handler。函数内部用intptr\_t cause=\(tf\-\>cause\<\<1\)\>\>1; 左移再右移剥掉最高位中断标志，得到纯IRQ\_\*编号，再switch\(cause\)分支。|
|**TICKNUM / SHUTDOWNAFTER**|TICK\_NUM=100：框架原有；SHUTDOWN\_AFTER=10：本次新增，源码未提供，按经典ucore约定取 10；另新增static volatile size\_t num=0;单独记录“统计输出次数”，与ticks区分。|
|**下一次事件设置与计数顺序**|在IRQ\_S\_TIMER分支内，顺序为：先clock\_set\_next\_event\(\)重新武装下一次时钟事件，再ticks\+\+计数，随后判ticks%TICK\_NUM==0做周期输出，最后判num\>=SHUTDOWN\_AFTER关机。即“先武装→再计数→周期输出→判关机”。|
|**统计输出格式**|复用框架已有print\_ticks\(\)：cprintf\("%d ticks\\n",TICK\_NUM\);，实际输出100 ticks；触发条件ticks%TICK\_NUM==0（每满100次输出一次）。与grade\.sh检查串100 ticks逐字对应。DEBUG\_GRADE下的End of Test\./panic分支属遗留，当前grade\.sh未启用。|
|**关机接口与判断条件**|接口sbi\_shutdown\(\)（libs/sbi\.h 声明、libs/sbi\.c 实现，SBI\_SHUTDOWN=8）；判断条件num\>=SHUTDOWN\_AFTER，即累计输出10次“100 ticks”（ticks到达1000）后关机。|

#### 实现迭代过程

实际迭代次数：1次。

|**轮次**|**实际prompt或记录位置 **|**现象与原因**|**修改方式**|**验证结果**|
|---|---|---|---|---|
|1|prompt\.md|初始版本按照实验要求实现时钟中断处理逻辑，中断触发后ticks正常自增，可周期性打印ticks信息，达到设定阈值后自动关机；编译运行无崩溃、无报错，功能符合预期，不存在 bug，无需修改。|无|验证通过|

**最终结果：** 编译：通过；连续时钟中断：通过；统计输出：通过；关机条件：通过。

### 4\.4 功能模块二（C部分）：异常处理

**负责人：** C：2412000\-文玥

### 模块功能描述

在 `kern/trap/trap.c` 实现 `void exception_handler(struct trapframe *tf)`。根据保存的异常原因，区分非法指令、断点和其他异常。对于两类必做异常，输出触发地址，根据指令编码确定长度并更新 `tf->epc`；其他异常调用 `print_trapframe(tf)`，不擅自修改返回地址。

### 最终提示词

```Plain Text
[PROMPT]
基于提供的lab3.zip，实现C负责的exception_handler。
先核对trap.c、trap.h、riscv.h、测试指令与grade.sh，保留A/B待补代码及教师测试。
交付函数、最小修改、独立测试、报告C部分和提示词记录。

[RELY]
依赖原有struct trapframe、tf->cause、tf->epc、uint16_t、
CAUSE_ILLEGAL_INSTRUCTION、CAUSE_BREAKPOINT、cprintf、print_trapframe和panic。
必做异常的触发指令位于可读取且半字对齐的内核代码区。

[GUARANTEE]
实现void exception_handler(struct trapframe *tf)，不修改既有函数签名、
其他成员实现、SAVE_ALL、trapframe布局或教师测试。
提供真实执行的独立测试记录，完整QEMU与grade结果保留待验证。

[SPECIFICATION]
1. 原因码2输出Illegal instruction at及原始地址；原因码3输出Breakpoint at及原始地址。
2. 读取指令的首个16位半字：低2位不是11则长度为2字节；
   低2位为11且低5位不是11111则为4字节。
3. 只推进保存的epc，不修改其余Trap frame字段。
4. 不支持更长指令的恢复；遇到此类编码panic，不错误地按4字节跳过。
5. 未处理异常只打印Trap frame，不读取异常地址、不更改epc。
6. 对空地址或奇数地址拒绝处理；明确这不等于验证任意地址是否可读。
7. 测试压缩断点、普通断点、16/32位非法编码、兜底分支及边界行为。
```

### 实现说明与代码位置

|**关键点**|**实际实现**|
|---|---|
|**原因码**|`CAUSE_ILLEGAL_INSTRUCTION=0x2`；`CAUSE_BREAKPOINT=0x3`，来自 `libs/riscv.h`|
|**非法指令测试**|教师测试使用 `.word 0x0000007b`，属于32位编码，处理后EPC加4|
|**断点测试**|教师汇编中写 `ebreak`；实际机器码可能受压缩指令选项影响，按首半字判断，兼容16位C\.EBREAK和32位EBREAK|
|**输出**|`Illegal instruction at %p\n`、`Breakpoint at %p\n`，传入原始EPC的指针值；包含grade\.sh要求的文本|
|**兜底**|原因码20等其他异常调用 `print_trapframe(tf)`，保持EPC不变|
|**修改范围**|原始源码中只有 `kern/trap/trap.c` 有修改；新增C函数和B函数的前置声明，其余既有函数不变|
|**提交状态**|本地交付，尚未上传；无可填写的远程commit|

长度判定依据为RISC\-V指令编码规范中的首半字编码规则。



这里修改的是内存中Trap frame保存的返回地址。后续必须由A的 `RESTORE_ALL` 将其恢复到 `sepc`，再由 `sret` 返回。C函数本身不会执行 `sret`。若不推进EPC，返回时会再次执行同一条非法指令或断点，可能反复陷入异常。



读取首个半字仅适用于本实验的可读内核测试指令。空地址和奇数地址检查不保证其余地址一定已映射。未知异常不进行取指；48位及更长编码不在本次恢复范围内，遇到后调用panic。

## 五、测试与验证

### 5\.1 测试版本与执行方法

|**项目**|**内容**|
|---|---|
|被测提交|测试在WSL的/mnt/d/OSlab/lab3/code中进行，使用包含启动页表修正和评分脚本启动参数修正的Lab3源码。|
|编译命令及退出结果|在code/下执行make。编译和链接成功，生成bin/kernel和bin/ucore\.img。修正启动页表后重新执行make，entry\.S重新汇编，内核镜像重新生成。|
|QEMU运行命令|实际使用qemu\-system\-riscv64 \-machine virt \-nographic \-bios default \-kernel bin/ucore\.img 2\>\&1 \| tee \.\./lab3\-qemu\-boot\.log。启动后观察到DTB识别出128 MB物理内存，page\_init\(\)报告31,928个空闲页，并输出check\_alloc\_page\(\) succeeded\!和多次100 ticks。|
|自动评分命令与得分|将tools/grade\.sh的镜像加载参数改为\-kernel bin/ucore\.img后，执行make grade 2\>\&1 \| tee \.\./lab3\-grade\-rerun\.log。Trap入口、寄存器恢复与断点返回得35/35，非法指令处理与EPC推进得25/25，时钟中断重设与重复Trap返回得40/40，总分100/100。|
|GDB连接方式和关键断点|终端1在 code/下执行qemu\-system\-riscv64 \-machine virt \-nographic \-bios default \-kernel bin/ucore\.img \-s \-S。终端2执行make gdb，加载bin/kernel的符号并连接[localhost:1234](http://localhost:1234)。使用hbreak \*trap检查a0与sp，使用hbreak \*\_\_trapret读取帧中保存的sp、epc、t0，再用thbreak \*$resume在返回地址处验证恢复结果。|
|运行日志位置|日志保存在code/的上一级目录/mnt/d/OSlab/lab3/。lab3\-qemu\-boot\.log记录成功启动，lab3\-grade\-rerun\.log记录最终评分。GDB观测见图2和图3，启动证据见图1。|

|**项目**|**内容**|
|---|---|
|被测提交|\[待填写commit ID，须与截图一致\]|
|编译命令及退出结果|\[待根据Lab3 Makefile执行后填写\]|
|QEMU运行命令|\[待核对make qemu目标并记录实际执行命令\]|
|自动评分命令与得分|\[待核对make grade目标；未运行不填通过\]|
|GDB连接方式和关键断点|\[待填写实际可复现命令\]|
|运行日志位置|\[待填写\]|

### 5\.2 测试结果汇总

|**测试项目**|**负责人**|**检查内容/预期行为**|**实际结果**|**截图或日志**|
|---|---|---|---|---|
|基础衔接与编译启动|A|Lab2待补位置适配Lab3，编译完成，内核进入初始化流程。|通过。成功生成bin/kernel和bin/ucore\.img。QEMU启动后，DTB识别出128 MB物理内存，page\_init\(\)报告31,928个空闲页，默认物理页分配器通过自检。|图 1a、图 1b；lab3\-qemu\-boot\.log|
|Trap 入口与参数传递|A|将保存后的Trap frame地址正确传给trap\(tf\)。|通过。在trap入口断点处，GDB显示tf、a0和sp均为0xffffffffc0205ed0，p $a0 == $sp返回1。|图 2|
|上下文恢复与返回|A|恢复原栈指针、通用寄存器和执行位置，并继续执行。|通过。本次时钟Trap返回到kern\_init\(\)的while \(1\)。返回后的sp、pc、t0与帧中保存的原sp、返回epc和t0一致，三项比较均返回1。|图 3a、图 3b|
|连续时钟中断|B|每次设置下一事件，ticks每次增加1|通过。内核持续响应时钟中断，每次中断将ticks自增，并调用clock\_set\_next\_event\(\)设置下一次时钟事件；可以稳定循环输出100 ticks，证明时钟中断能够持续触发、计数器正常累加。|图4|
|周期输出与关机|B|达到TICKNUM整数倍时输出；达到SHUTDOWNAFTER次输出后关机|通过。当ticks达到TICKNUM的整数倍时打印输出；累计输出次数到达SHUTDOWN\_AFTER后，内核调用sbi\_shutdown\(\)，QEMU 干净退出并返回宿主机终端，无panic报错，满足自动关机要求。|图5|
|非法指令异常|C|原因和输出正确，epc按实际指令长度推进，不重复触发|已观察到 EPC 增加 4 字节|图6|
|断点异常|C|原因和输出正确，epc按实际指令长度推进，返回后继续执行|已观察到压缩断点指令，保存的 EPC 增加 2 字节|图7|
|其他类型与兜底分支|B、C|按框架要求提示或输出Trap frame；实际未触发的分支注明仅代码核对|\[待验证\]|\[待填写\]|
|整合与自动测试|全体|基于同一提交复测必做功能并记录实际评分|验证通过。将A、B、C模块代码整合至同一版本，执行make grade自动复测全部必做功能。所有测试项运行正常，编译、内存管理、中断处理、时钟计数、周期打印、自动关机以及异常分支均符合预期，自动测试脚本全部通过，达到实验评分要求。<br>|图8|

### 5\.3 测试截图

**图1：编译与启动。**

![图1\-1\.png](../image/图1-1.png)

![图1\-2\.png](../image/图1-2.png)

观察记录：图 1\-1 显示 `entry.S` 重新汇编，链接生成 `bin/kernel`，随后生成 `bin/ucore.img`。图 1\-2 显示内核进入初始化流程，DTB 识别的物理内存范围为 `0x80000000–0x87ffffff`，容量为 128 MB。`page_init()` 报告 `31928` 个空闲页，控制台输出 `check_alloc_page() succeeded!`，说明当前启用的默认物理页分配器通过了启动自检。



**图2：Trap frame传递。**

![图2\.png](../image/图2.png)

观察记录：使用 `hbreak *trap` 在 C 函数入口设置硬件断点。断点命中时，`a0` 和 `sp` 均为 `0xffffffffc0205ed0`，GDB 显示的参数 `tf` 也指向该地址。表达式 `p `$a0 == $`sp` 返回 `1`，验证了 `__alltraps` 将当前 Trap frame 的基址传给了 `trap()`。



**图3：上下文恢复与返回。**

![图3\-1\.png](../image/图3-1.png)

![图3\-2\.png](../image/图3-2.png)

观察记录：图 3\-1 记录恢复前 Trap frame 中的三个值。原栈指针 `sp` 为 `0xffffffffc0205ff0`，返回地址 `epc` 为 `0xffffffffc020009c`，示例通用寄存器 `t0` 为 `0x0`。图 3\-2 显示执行 `sret` 后，GDB 在 `0xffffffffc020009c` 命中临时断点，对应 `kern_init()` 中的 `while (1)`。此时实际 `sp`、`pc`、`t0` 与帧中对应值的比较均返回 `1`。



**图4：连续时钟中断与ticks。**

![图4\.png](../image/图4.png)

观察记录：

QEMU 串口持续输出`100 ticks`，证明：

1. 每次时钟中断执行了`clock_set_next_event()`，时钟可以**连续反复触发，不是只来一次中断**；

2. `ticks`计数器正常累加；

3. 达到`TICK_NUM`倍数会调用`print_ticks()`输出；

4. 后续图 5 进一步验证累计输出达到`SHUTDOWN_AFTER`会调用`sbi_shutdown()`关机。



**图5：周期统计输出与关机。**

![图5\.png](../image/图5.png)

观察记录：累计输出`100 ticks`达到`SHUTDOWN_AFTER`次，系统调用`sbi_shutdown()`完成关机，不会无限循环运行，也不会提前关机。证明关机条件判断逻辑实现正确。



**图6：非法指令异常。**

![图6：非法指令异常](../image/图6.png)

观察记录：GDB 在 exception_handler 中读到 cause=2，原 EPC 为 0xffffffffc0202f12，首半字为 0x007b。函数返回 trap_dispatch 后，保存的 EPC 为 0xffffffffc0202f16，增加了 4 字节。

**图7：断点异常。**

![图7：断点异常](../image/图7.png)

观察记录：GDB 在 exception_handler 中读到 cause=3，原 EPC 为 0xffffffffc0202fc4。该地址的半字 0x9002 对应 16 位压缩断点指令 C.EBREAK，GDB 反汇编显示为 ebreak。函数返回 trap_dispatch 后，保存的 EPC 为 0xffffffffc0202fc6，增加了 2 字节。

**图8：自动测试结果。**

![图8\.png](../image/图8.png)

观察记录：执行测试命令`make grade`，编译全部内核源码文件并自动运行测试；非法指令异常、时钟重触发与陷阱返回两项测试全部通过，最终输出总分`Total Score: 100/100`。

---

## 六、实验总结与收获

### 6\.1 对操作系统的理解

本实验重要知识点与对应OS原理的联系如下；完成后结合实际代码和现象补充实例。

|**实验知识点**|**对应的OS原理**|**二者的联系与差异**|
|---|---|---|
|Trap入口与分发|中断、异常与控制转移|共同入口保存现场后按原因分发；中断通常由异步事件触发，异常与当前指令执行有关。代码展示具体机制，OS原理讨论其一般作用。|
|Trap frame与上下文恢复|执行上下文的保存和恢复|为处理函数临时使用寄存器提供条件，并保证返回后继续执行；本实验的Trap现场管理不等于完整进程切换。|
|stvec、sepc、sstatus等状态|特权级与异常返回|寄存器连接硬件入口、软件处理及返回机制；不能将Trap返回一概理解为S态到U态切换。|
|时钟中断|周期事件和调度基础|本实验完成事件设置、计数、输出与关机，提供定时响应基础，尚不等于实现时间片调度。|
|异常指令长度与epc|同步异常的恢复策略|本实验对指定异常按要求跳过指令；实际OS还可能重试指令、终止执行或采用其他恢复方式，并非所有异常都应跳过。|
|共享Trap frame与调用约定|汇编/C边界和接口一致性|布局、字段偏移、传参和恢复顺序必须一致；高层接口说明还需落实为具体寄存器和栈操作。|

OS原理中重要，但本次必做内容未涉及或未完整实现的知识点：

|**知识点**|**为什么重要**|**本实验覆盖情况**|
|---|---|---|
|进程调度与时间片轮转|决定CPU如何在多个执行实体之间分配|时钟中断是基础，本次必做内容不要求实现完整调度器|
|完整系统调用与用户程序执行|提供用户态请求内核服务的受控接口|本次只要求Trap和指定事件处理；完整系统调用子系统不属于本次实现任务|
|缺页异常处理与虚拟内存恢复|支持按需分配、地址空间保护及其他内存机制|对未要求恢复的异常仅作兜底输出，不表示已实现缺页恢复|
|多核并发、中断嵌套与复杂设备处理|影响并发正确性、实时响应和设备协作|本次必做范围未完整覆盖，不能从单场景验证推导一般正确性|

