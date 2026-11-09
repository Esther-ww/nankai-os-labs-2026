# OSLab3 提示词记录

> 本文件汇总 Lab3 各模块使用的 AI 生成提示词（最终版本）及迭代过程中的关键版本与问题。
>
> - 小组成员：A 2413685-刘晟槿；B 2413978-缪承佑；C 2412000-文玥
> - 对应实验报告：OSLab3实验报告（report.md）

## 目录

1. A 前置任务：Lab2 基础代码衔接
2. A 模块一：Trap 入口与上下文恢复（RESTORE_ALL / __alltraps）
3. B 模块二：中断处理（interrupt_handler）
4. C 模块二：异常处理（exception_handler）

附录：各模块 prompt 内化的常见问题

---

## 1. A 前置任务：Lab2 基础代码衔接

**提示词来源**：复用 Lab2 实验报告 §4.3 的 `page_init()` 最终提示词，此处记录本次复用的约束要点（完整版见 Lab2 报告）。

### 复用提示词（约束摘要）

> 补齐 Lab3 源码中标有 `//lab2 请补充你在lab2中的代码` 的位置。从设备树（DTB）读取物理内存范围，将内存末端限制在内核可管理的范围内；在内核镜像之后按页对齐放置 `struct Page` 数组；先将页描述符标记为保留，再通过 `init_memmap()` 注册剩余的完整空闲页。Best-Fit 部分复用小组在 Lab2 已完成的实现（空闲链表管理、分配、释放、计数及 `best_fit_pmm_manager` 接口表），保留 Lab3 原有的自检函数。

### 接口与框架差异（本次适配）

- Lab3 未定义 `PHYSICAL_MEMORY_END` 与 `KERNEL_BEGIN_PADDR`，分别用 `KERNTOP - PHYSICAL_MEMORY_OFFSET` 和 `KERNBASE - PHYSICAL_MEMORY_OFFSET` 推导对应物理地址。
- Lab3 默认使用 `default_pmm_manager`（First-Fit），页分配与释放接口带中断保护，迁移时沿用。

---

## 2. A 模块一：Trap 入口与上下文恢复

### 迭代版本

**v1（初版）**

> 根据 `SAVE_ALL` 与 `struct trapframe` 布局完成 `RESTORE_ALL` 与 `__alltraps`，按调用约定传帧指针并调用 `trap()`。

现象/结果：编译、链接成功，生成 `bin/kernel` 与 `bin/ucore.img`。

**v2（启动参数修正）**

- 新增问题：`make qemu` 使用 `-device loader`，OpenSBI 显示下一阶段入口为 0，内核不启动。
- prompt 补充：改用 `-kernel bin/ucore.img` 启动，验证 OpenSBI 下一阶段入口为 `0x80200000`。

**v3（启动页表低地址映射）**

- 新增问题：入口汇编写入 satp 后仍需在低地址取指，而启动页表当时只建立了高地址映射。
- prompt 补充：在 `entry.S` 启动页表第 2 项加入低地址恒等映射，保留高地址映射。

**v4（GDB 逐行调试）**

- 新增问题：多行粘贴被 GDB 当作一条命令解析，导致语法错误。
- prompt 补充：改为逐行执行，在 `trap`、`__trapret` 与预期返回地址处分别观测。

**v5（评分脚本适配）**

- 新增问题：`tools/grade.sh` 仍用旧 `-device loader`，首次评分三项合计 0/100。
- prompt 补充：将评分脚本镜像参数改为 `-kernel bin/ucore.img` 后重新执行 `make grade`。

### 最终提示词

> 请先检查 Lab3 源码中的 `kern/trap/trapentry.S` 和 `kern/trap/trap.h`，根据 `SAVE_ALL` 与 `struct trapframe` 的实际布局，完成 A 部分的 `RESTORE_ALL` 和 `__alltraps`。在 `__alltraps` 中按 RISC-V 调用约定，把当前 Trap frame 指针放入 `a0` 后调用 `trap()`。C 函数返回后进入已有的 `__trapret`，先从帧中恢复 `sstatus` 和处理后的 `sepc`，再恢复通用寄存器，最后恢复原 `sp` 并执行 `sret`。沿用 `SAVE_ALL`、Trap frame 布局和 `trap()` 的函数签名。给出 GDB 验证步骤，检查入口处 `a0` 是否等于帧基址，以及返回后的 `sp`、`pc` 和示例寄存器 `t0` 是否与帧中记录的值一致。测试结论以实际运行结果为准。

---

## 3. B 模块二：中断处理（interrupt_handler）

### 迭代版本

**v1（初版）**

> 在 `kern/trap/trap.c` 标有 `lab3 function2` 的位置实现 `interrupt_handler`：时钟中断重设下一事件、`ticks` 自增、周期输出、达到阈值关机。

现象/结果：编译运行无崩溃、无报错，功能符合预期，无 bug，未再迭代（实际迭代次数：1 次）。

### 最终提示词（完整）

```markdown
【任务说明】
任务：在文件 kern/trap/trap.c 中，标有 lab3 function2、YOUR CODE 的位置，完整实现中断处理函数 interrupt_handler(struct trapframe *tf)，重点是时钟中断的连续触发、ticks 计数、周期统计输出与规定条件下关机。参考同文件 trap_dispatch() 的中断/异常分类方式，以及 clock.h/sbi.h/riscv.h 提供的接口。

硬性约束：
1. 禁止修改 exception_handler()；禁止修改 trap_dispatch()、trap()、idt_init()、print_ticks()、print_trapframe() 等已有函数的签名与逻辑，只在 // lab3 function2 YOUR CODE 区域写实现。
2. 中断与异常的区分由 trap_dispatch() 完成：(intptr_t)tf->cause < 0 判定为中断，转入 interrupt_handler，本函数不再自行判断符号位。
3. 时钟相关能力复用框架已有接口：clock_set_next_event() 设置下一次时钟事件，ticks 为 clock.h 声明的 volatile size_t 全局计数；关机使用 sbi.h 的 sbi_shutdown()。
4. 输出格式以 grade.sh 实际检查字符串为准，不自行发挥：Supervisor software interrupt、Unknown interrupt: cause 15、100 ticks 等必须逐字命中。
5. 中断使能（clock_init 中的 set_csr(sie, MIP_STIP)、kern_init 中的 intr_enable()）由框架负责，本模块不处理；本函数只负责“已使能中断到来后”的处理。

验证标准：时钟中断能持续触发而非只触发一次；每次中断 ticks 加一；ticks 达到 TICK_NUM 的整数倍时输出统计信息；累计输出次数达到 SHUTDOWN_AFTER 后关机，不提前关机也不无限运行；无法识别的中断调用 print_trapframe(tf) 输出上下文；最终通过 grade.sh 中 GRADE_LAB3_TIMER 与非法指令用例里的中断相关检查项。

【Rely 依赖说明（仅使用以下已有定义，不自行新增复杂结构）】
依赖头文件：clock.h、sbi.h、riscv.h、trap.h、stdio.h

依赖核心定义：
· struct trapframe（重点使用 tf->cause，类型 uintptr_t）。
· volatile size_t ticks（clock.h 声明、clock.c 定义）。
· 中断原因宏 IRQ_U_SOFT ... IRQ_M_EXT（riscv.h），其中 IRQ_S_SOFT=1、IRQ_S_TIMER=5、IRQ_S_EXT=9。
· 本文件已定义 TICK_NUM 100。
辅助函数：clock_set_next_event()、sbi_shutdown()、cprintf()、print_ticks()、print_trapframe()。
规则/标志：RISC-V 中 scause 最高位为中断标志位，实际中断原因编号用 (tf->cause << 1) >> 1 剥掉最高位得到；ticks 是“原始中断次数”，与“统计输出次数”是两个不同概念，需分开计数。

【Guarantee 接口清单】
需要实现一个函数：
void interrupt_handler(struct trapframe *tf);

内部结构：在本文件 TICK_NUM 附近声明 #define SHUTDOWN_AFTER 10 与 static volatile size_t num = 0;，num 记录“统计输出次数”（即 print_ticks 被调用的次数），与 ticks 区分；interrupt_handler 保持外部链接（test_lab3.c 会 extern 声明调用）。

【Specification 行为规格】
1. 原因提取
前置：tf->cause 为中断类型（最高位已置位）。
行为：intptr_t cause = (tf->cause << 1) >> 1; 得到纯中断编号，随后 switch (cause) 分支。
后置：cause 为 IRQ_* 编号或无法识别的值。

2. 时钟中断 IRQ_S_TIMER
行为：
（1）先 clock_set_next_event() 设置下一次时钟事件——这是“持续触发”的关键，否则定时器只触发一次；
（2）ticks++ 每次中断加一；
（3）若 ticks % TICK_NUM == 0，调用 print_ticks() 输出统计信息，并 num++；
（4）若 num >= SHUTDOWN_AFTER，调用 sbi_shutdown() 关机。
后置：下一事件已设置；ticks、num 增量正确；达到关机条件后不再返回。

3. 其余已定义中断
分支：IRQ_U_SOFT / IRQ_S_SOFT / IRQ_H_SOFT / IRQ_M_SOFT / IRQ_U_TIMER / IRQ_H_TIMER / IRQ_M_TIMER / IRQ_U_EXT / IRQ_S_EXT / IRQ_H_EXT / IRQ_M_EXT，各自 cprintf 打印对应提示，例如 IRQ_S_SOFT 打印 Supervisor software interrupt。
后置：仅提示，不改变状态。

4. 无法识别的中断（default）
行为：cprintf("Unknown interrupt: cause %d\n", (int)cause); 随后 print_trapframe(tf); 输出完整上下文。
后置：Trap frame 被打印，便于定位未知中断来源。

额外代码要求：
代码注释风格对齐 trap.c 已有注释，关键逻辑（重新武装、ticks 与 num 的区别、关机条件）加注释；C 语言语法适配 ucore 编译环境（%d 需 (int) 强转避免 -Werror 格式告警），不引入 stdlib、printf 等无关库；exception_handler 不需完成。
```

### 最终实现要点（对应实现说明）

- 分发由 `trap_dispatch()` 完成；`(intptr_t)tf->cause < 0` 判中断，函数内 `(tf->cause << 1) >> 1` 剥符号位得纯 IRQ 编号。
- `TICK_NUM=100`（框架原有）；`SHUTDOWN_AFTER=10`（本次新增）；新增 `static volatile size_t num = 0` 单独记录“统计输出次数”。
- 处理顺序：先 `clock_set_next_event()` 重武装 → `ticks++` → 判 `ticks % TICK_NUM == 0` 周期输出 → 判 `num >= SHUTDOWN_AFTER` 关机。
- 统计输出复用 `print_ticks()`：`cprintf("%d ticks\n", TICK_NUM)`，逐字对应 grade.sh 检查串 `100 ticks`。
- 关机接口 `sbi_shutdown()`（`SBI_SHUTDOWN=8`）；累计输出 10 次 `100 ticks`（ticks 达 1000）后关机。

---

## 4. C 模块二：异常处理（exception_handler）

### 最终提示词

```plaintext
[PROMPT]
基于提供的 lab3.zip，实现 C 负责的 exception_handler。
先核对 trap.c、trap.h、riscv.h、测试指令与 grade.sh，保留 A/B 待补代码及教师测试。
交付函数、最小修改、独立测试、报告 C 部分和提示词记录。

[RELY]
依赖原有 struct trapframe、tf->cause、tf->epc、uint16_t、
CAUSE_ILLEGAL_INSTRUCTION、CAUSE_BREAKPOINT、cprintf、print_trapframe 和 panic。
必做异常的触发指令位于可读取且半字对齐的内核代码区。

[GUARANTEE]
实现 void exception_handler(struct trapframe *tf)，不修改既有函数签名、
其他成员实现、SAVE_ALL、trapframe 布局或教师测试。
提供真实执行的独立测试记录，完整 QEMU 与 grade 结果保留待验证。

[SPECIFICATION]
1. 原因码 2 输出 Illegal instruction at 及原始地址；原因码 3 输出 Breakpoint at 及原始地址。
2. 读取指令的首个 16 位半字：低 2 位不是 11 则长度为 2 字节；
   低 2 位为 11 且低 5 位不是 11111 则为 4 字节。
3. 只推进保存的 epc，不修改其余 Trap frame 字段。
4. 不支持更长指令的恢复；遇到此类编码 panic，不错误地按 4 字节跳过。
5. 未处理异常只打印 Trap frame，不读取异常地址、不更改 epc。
6. 对空地址或奇数地址拒绝处理；明确这不等于验证任意地址是否可读。
7. 测试压缩断点、普通断点、16/32 位非法编码、兜底分支及边界行为。
```

### 实现要点（对应实现说明）

- `CAUSE_ILLEGAL_INSTRUCTION=0x2`、`CAUSE_BREAKPOINT=0x3`（`libs/riscv.h`）。
- 非法指令测试用 `.word 0x0000007b`（32 位编码），EPC 加 4；压缩断点 `C.EBREAK`（半字 `0x9002`）EPC 加 2。
- 输出 `Illegal instruction at %p\n`、`Breakpoint at %p\n`，传入原始 EPC 指针值，包含 grade.sh 要求的文本。
- 兜底：原因码 20 等其他异常调用 `print_trapframe(tf)`，保持 EPC 不变。
- 只推进内存中 Trap frame 保存的返回地址，后续由 A 的 `RESTORE_ALL` 恢复到 `sepc` 再由 `sret` 返回；C 函数本身不执行 `sret`。

---

## 附录：各模块 prompt 内化的常见问题

| 模块 | 常见问题 | prompt 中的规避方式 |
|---|---|---|
| A 模块一 | `-device loader` 导致内核不启动 | 改用 `-kernel bin/ucore.img` 启动 |
| A 模块一 | 写 satp 后低地址无法取指 | 启动页表加入低地址恒等映射 |
| A 模块一 | GDB 多行粘贴被当作一条命令 | 逐行执行调试命令 |
| A 模块一 | grade.sh 旧参数导致 0 分 | 评分脚本镜像参数改 `-kernel` |
| B 模块二 | 时钟只触发一次 | 先 `clock_set_next_event()` 重武装 |
| B 模块二 | 提前或无限延迟关机 | `num` 与 `ticks` 分开计数，`num >= SHUTDOWN_AFTER` 才关机 |
| B 模块二 | 输出串与 grade.sh 不符 | 以 grade.sh 检查串为准，逐字命中 |
| B 模块二 | `%d` 格式告警（`-Werror`） | 对 `size_t` 做 `(int)` 强转 |
| C 模块二 | 返回后重复触发同一条异常 | 按指令长度推进 `epc` |
| C 模块二 | 16/32 位编码判定错误 | 按首半字低 2 位/低 5 位规则判断长度 |