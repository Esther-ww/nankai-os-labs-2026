# Lab2 C：实现与测试提示词

## 记录说明

实现提示词由用户于本对话提供。测试提示词由 AI 在 2026-10-21 的整合任务中整理并执行，复用并增强已存在的工程测试；不冒充用户先前发送的消息。本文保留两份提示词及过程摘要，完整对话另行保留。

## 一、实现提示词原文

```text
[PROMPT]

请基于提供的 ucore Lab2 源码，实现 Buddy System物理内存分配器：

先阅读以下文件，确认实际接口和已有实现：
- kern/mm/pmm.h
- kern/mm/pmm.c
- kern/mm/memlayout.h
- kern/mm/mmu.h
- kern/mm/default_pmm.c
- libs/list.h
- Makefile

新增以下文件：
- kern/mm/buddy_pmm.c
- kern/mm/buddy_pmm.h

采用按阶数组织空闲链表的方式管理内存：
第 k 阶对应 2^k 个连续物理页。申请时向上取整到合适阶数，必要时需要分裂大块，释放时需要尝试逐级合并空闲伙伴。

优先复用现有 struct Page，不修改其布局。明确说明 flags、property、page_link 在 Buddy 实现中的含义，以及如何区分空闲块首页、已分配块首页和块内其他页面。


先说明设计方案和关键不变量，再给出完整代码、接入步骤和验证方法。

[RELY]

可以依赖源码中实际存在的以下接口，不得自行假设存在其他辅助函数。

1. 物理内存管理器接口

pmm.h 中的 struct pmm_manager：
- name
- init
- init_memmap
- alloc_pages
- free_pages
- nr_free_pages
- check

接口签名以实际 pmm.h 为准。

2. 页面描述结构

memlayout.h 中的 struct Page：
- ref：引用计数
- flags：页面状态
- property：可用于记录块大小或阶数，需说明具体约定
- page_link：链表节点

可以使用已有页面状态宏：
- SetPageReserved
- ClearPageReserved
- PageReserved
- SetPageProperty
- ClearPageProperty
- PageProperty

不能仅凭 PageProperty 为假，就判断该页是合法的已分配块首页。

3. 页面与地址相关定义

可以使用：
- PGSIZE、PGSHIFT
- pages、npage、nbase
- page2ppn()
- page2pa()
- set_page_ref()
- 源码中已有的地址转换和对齐宏

注意：
page - pages 是描述符数组索引；
page2ppn(page) 是物理页号；
page2pa(page) 是物理地址。

三者不能混用。计算伙伴时必须明确采用哪一种坐标，以及该坐标对应的块对齐规则。

4. 链表与调试支持

可以使用 libs/list.h 中实际提供的链表类型和操作，以及：
- le2page()
- assert()
- panic()
- cprintf()

可以参考 default_pmm.c 的管理器注册和接口组织方式，但不能照搬 First-Fit 的选块策略作为 Buddy 实现。

5. 运行前提

由 page_init() 建立页面描述符，并将合法空闲范围传给 init_memmap()。

Buddy 初始化和分配逻辑不得依赖 malloc，也不得依赖尚未建立的动态内存分配器。

不得修改源码中已有的测试来适配错误实现。


[GUARANTEE]

必须提供以下函数，内部函数可声明为 static：

1. static void buddy_init(void);

2. static void buddy_init_memmap(struct Page *base, size_t n);

3. static struct Page *buddy_alloc_pages(size_t n);

4. static void buddy_free_pages(struct Page *base, size_t n);

5. static size_t buddy_nr_free_pages(void);

6. static void buddy_check(void);

必须定义管理器：
const struct pmm_manager buddy_pmm_manager;

在 buddy_pmm.h 中提供对应的 extern 声明，并设置头文件保护。

还必须交付：
- pmm.c 中包含头文件、切换到 buddy_pmm_manager 的最小修改说明；
- 编译方式和运行方式；
- 如何从启动输出确认实际选中了 Buddy；
- 数据结构、阶数上限、空间开销和边界处理说明。


[SPECIFICATION]

一、整体状态约定

1. 第 k 阶空闲块恰好包含 2^k 页。
2. 每个空闲块只能在对应阶的链表中出现一次。
3. 空闲块之间不得重叠，已分配块不得出现在空闲链表中。
4. 所有块必须位于已注册的可用页面范围内。
5. nr_free_pages 统计可分配的物理页总数，而不是空闲块数量。
6. 为已分配块保存足够的元数据，能够校验首页、原始申请页数和实际分配阶数。
7. 申请和释放的接口约定为：

   p = alloc_pages(n);
   free_pages(p, n);

   其中释放时传入原始申请页数 n。

   例如申请 3 页，实际分配 4 页；
   free_pages(p, 3) 应回收完整的 4 页。

8. 申请 n 页失败时，不得改变空闲链表、块状态和空闲页计数。
9. 明确最大阶数及其依据，避免移位溢出和页数计算溢出。
10. 内部实现不得调用公共 alloc_pages()/free_pages() 包装器来执行同名功能，以免递归进入自身。


二、buddy_init()

输入：
无。

行为：
- 初始化各阶空闲链表。
- 清零空闲页总数。
- 初始化管理器自身的状态记录。
- 不自行探测物理内存，不调用 page_init()。
- 不读取尚未由 page_init() 建立的页面描述符。

完成后：
- 所有阶的链表为空。
- buddy_nr_free_pages() 返回 0。


三、buddy_init_memmap(struct Page *base, size_t n)

输入：
- base：待注册范围的首个 Page 描述符。
- n：该范围包含的连续页面数。

前置条件：
- 描述符范围合法；
- 对应页面可交给分配器使用；
- 与已经注册的范围不重叠。

行为：
- 检查范围和参数，不允许越界或重复登记。
- 初始化该范围内的引用计数、页面标记和块元数据。
- 按选定的伙伴坐标和对齐规则，将范围分解成合法的 2 的幂大小块。
- 将各块加入对应阶空闲链表。
- 不要求 base 按整个范围大小对齐，也不要求 n 是 2 的幂。
- 不得遗漏尾部页面，也不得为了凑整块包含范围外的页面。
- 支持多次注册互不重叠的范围，不得重置此前的管理状态。
- 跨注册范围是否允许合并必须明确约定；无论采用何种方案，都不得跨越未注册或保留页面。

完成后：
- 新增空闲页数恰好为 n。
- 新范围内的每一页均被准确管理一次。


四、buddy_alloc_pages(size_t n)

输入：
申请的连续物理页数。

行为：
- n 为 0 时返回 NULL，不修改状态。
- 将 n 向上取整到最小的 2 的幂，得到目标阶数。
- 检查计算溢出和最大阶数限制；超出能力时返回 NULL。
- 从目标阶开始向更高阶搜索可用块。
- 找不到足够大的连续块时返回 NULL。
- 找到更高阶块时，将其从链表移除，并逐级对半分裂。
- 每次分裂将不继续分配的一半加入相应空闲链表。
- 标记最终块为已分配，记录原始请求和实际阶数。
- 从空闲页总数中扣除实际块页数。

返回：
- 成功：分配块首页的 struct Page 指针。
- 失败：NULL。

示例：
申请 3 页时，返回至少 4 页的连续区域，空闲页数减少 4。
返回区域不得与其他尚未释放的分配结果重叠。


五、buddy_free_pages(struct Page *base, size_t n)

输入：
- base：之前分配返回的首页描述符。
- n：当时申请的原始页数。

行为：
- 修改状态前，校验地址、首页标记、分配状态和原始申请页数。
- 对 NULL、零页数、重复释放、非首页释放和不匹配页数，
  使用明确的断言或 panic 拒绝，不得静默破坏状态。
- 根据保存的元数据取得实际分配阶数。
- 将实际占用页数归还空闲页统计，只增加一次。
- 计算同阶伙伴的位置。
- 仅当伙伴属于合法已注册范围、确实空闲且阶数相同时才合并。
- 合并前从空闲链表移除伙伴。
- 合并后选低地址作为新块首页，阶数加一，并继续尝试。
- 达到最大阶数或伙伴不满足条件时停止。
- 将最终块加入对应阶链表，更新相关页面标记。

注意：
- 相邻块不一定是伙伴。
- 合并本身不额外增加空闲页数。
- 已分配块不能参与合并。
- 不得留下旧块首页仍被标记为独立空闲块的状态。


六、buddy_nr_free_pages()

输入：
无。

行为：
- 返回当前空闲物理页总数。
- 不修改任何链表或元数据。

结果必须与所有空闲块页数之和一致。


七、buddy_check()

至少完成以下基本自检：
- 保存测试开始时的空闲页总数。
- 申请并释放 1 页。
- 申请并释放一个 2 的幂大小块。
- 申请 3 页，验证实际扣除 4 页，并通过 free_pages(p, 3) 完整回收。
- 验证零页申请返回 NULL，且状态不变。
- 验证超过管理能力的申请失败，且状态不变。
- 释放测试中成功申请的全部块后，验证空闲页总数恢复。
- 使用 assert 检查结果，只有断言全部通过后才能打印通过信息。

测试不得修改其他分配器的测试，也不得用修改预期结果的方式掩盖实现错误。
若空闲容量不足以运行某项测试，应明确处理并说明，不能访问失败申请返回的 NULL 指针。
```

## 二、测试提示词

```text
[PROMPT]
针对当前 buddy_pmm.c 设计并执行独立测试。直接编译实际分配器源码，复用仓库的 Page、链表及管理器接口，不另写一个算法模型代替被测代码。保留内核 buddy_check 基本自检，增加可在宿主机运行的测试程序。
不得改动 First-Fit、Best-Fit 或老师的原有测试。实际执行后保存日志，区分宿主机验证和 QEMU 验证。

[RELY]
可以使用 code/kern/mm/buddy_pmm.c、原项目 pmm.h、memlayout.h、list.h 等头文件。宿主机提供测试用 pages、npage、nbase 及 cprintf、panic 实现；这些替身只提供执行环境，不代替分配逻辑。
允许通过包含 buddy_pmm.c 检查内部元数据和链表。使用固定随机种子保证结果可复现。

[GUARANTEE]
交付 tests/host_buddy.c、编译运行脚本和实际日志。覆盖最大块、分裂、合并、非伙伴不合并、不规则初始区间、空洞、耗尽和恢复、非 2 的幂请求、非法操作、随机操作。
保留源码，不修改测试预期以掩盖错误。不把未运行的 RISC-V/QEMU 检查写成通过。

[SPECIFICATION]
1. 创建合成页面描述符数组，先标记 reserved，再调用真实管理器注册。
2. 申请并释放最大 32768 页块；运行基本自检，验证申请 3 页实际占用 4 页且完整回收。
3. 注册起点不对齐、长度不是 2 的幂的区间，并注册有空洞的第二个区间；申请到耗尽后验证数量，全部释放后恢复。
4. 分两次逆序注册相邻的 8 页区间，验证可以申请合并后的 16 页块。
5. 在 4 页区间中先分配全部单页，只释放中间两页；虽然相邻但非伙伴，申请 2 页必须失败。释放其余页面后可重新申请 4 页块。
6. 进行 10000 次固定种子的随机分配/释放，每步独立遍历验证块对齐、覆盖不重叠、内部页状态、双向链表和空闲总页数，最后释放全部剩余块。
7. 分别测试 NULL、零页、页数不匹配、内部页、重复释放、重复注册、越界注册、未注册页、未对齐描述符和越界描述符。使用独立进程捕获预期 panic，并检查拒绝前后元数据、Page 字段、链表和统计状态哈希一致。
8. 空管理器的基本自检必须明确打印 SKIP，不能把未实际发生的申请测试写成通过。
9. 输出每项结果，正向测试全部通过才返回 0；预期非法操作返回约定退出码，未拒绝或异常改变状态则判失败。
```

## 三、执行记录

- 10 月 19 日：按用户实现提示词实现模块并执行宿主机测试，原始初始化尚缺失。
- 10 月 21 日：用户要求基于当前仓库完成 C 部分；读取最新 main 提交 02154da，保留队友代码，改为 USE_BUDDY 条件选择。
- 对新版本重新编译运行测试，补充负向测试对 Page.ref 的快照覆盖，全部通过。
- pmm.c 两种配置通过语法检查。
- WSL 仍返回 E_ACCESSDENIED，RISC-V/QEMU 未验证，未生成或伪造运行截图。
- 完整技术说明见 report.md，原始结果见 host-test-results.txt。

## 用户补充的实际运行证据

用户随后在 Ubuntu 中执行 tests/run_host.sh 并提供通过截图，再按指导执行 Buddy 编译运行步骤，提供显示 buddy_pmm_manager、基本自检完成及 check_alloc_page 成功的 QEMU 截图。助手据截图更新报告，未将宿主机完整测试冒充为 QEMU 内执行。


## 2026-10-26 上传整合记录

用户要求指导合并 C 部分，并保留队友代码。已确认原上传分支仍在旧基线；用户通过本地 Git bundle 快进到 main@42e5c8f。AI 随后准备安全导入脚本，仅加入 Buddy、独立测试及 C 材料，pmm.c 只增加管理器选择。此段为过程摘要，不伪造为用户逐字提问。新基线的 Ubuntu 测试和推送合并尚待执行。

#Lab2 First-fit & Best-fit 算法实现部分。
```
1. 在连续物理内存分配里，first-fit 和 best-fit 的核心差别只在于分配时如何遍历空闲链表。请从“分配一次要扫描多少节点”和“长期运行后的外部碎片程度”两个角度，帮我梳理这两种策略各自的取舍：为什么 first-fit 平均分配更快，而 best-fit 通常能留下更小的剩余碎片？

2. ucore 用一个按物理地址升序排列的双向循环链表管理空闲块，链表的头页用 property 字段表示这个块包含多少个连续页。请帮我分析：为什么要把空闲链表按地址排序，而不是按块大小排序？这个顺序对后面 free 时的相邻合并操作有什么决定性的帮助？

3. 每个空闲块的“头页”用 PG_property 标志和 property 字段来标记，块内其它页的这两个字段都是 0。请从“如何快速判断一个页是不是空闲块的头”这个角度，说明为什么只需要把头页标记出来就够了，而不是把块内所有页都标记上。

4. 实现 alloc_pages(n) 时，best-fit 要找出“满足 property >= n 的空闲块中 property 最小的那个”。请问这种“记录当前最优候选，边扫描边比较更新”的思路，在循环里应该怎么组织？和 first-fit 的“找到第一个就 break”相比，多出来的开销体现在哪里？

5. 分配时如果找到的块比需要的 n 大，就要把块切成两半、把后半部分重新挂回链表。请帮我想清楚：切下来的后半块起始 Page 该怎么用指针运算表示？它的 property 应设为多少？原来那个头页的 PG_property 标志又需要做什么处理，才能保证链表依然只有“头页”被标记？

6. free_pages 把一个块放回链表后，要和前后物理相邻的空闲块合并。请问：怎么利用页指针判断“前一块末尾 + 前一块大小 == 当前块”这种物理相邻关系？合并时为什么必须把被并进去的块从链表摘除、并清掉它的 PG_property？

7. nr_free 是当前空闲页总数。请帮我梳理 init_memmap、alloc_pages、free_pages 三个函数里各自应该怎么增减 nr_free；以及分配开头那句“n > nr_free 就直接返回 NULL”为什么是必要的提前判断，而不是可省的优化。

8. best_fit_check 里有一句 assert(p0 + 4 == p1)。请结合这个测试的具体步骤（先释放 p0+1 两页、再释放 p0+4 一页，然后 alloc_pages(1)），帮我分析：为什么 best-fit 会选中 p0+4 而不是 p0+1？这个断言是如何精准区分 best-fit 和 first-fit 的？

9. page_init 里要先算出 npage、把 pages 数组放在内核镜像之后，再算出第一块空闲内存交给分配器。请帮我梳理这条“整块 DRAM → 全部标记 reserved → 只把内核和 Page 数组之后的空间释放”的划分思路：为什么不直接只标记被占用页，而是先全量 reserved 再逐步释放？

10. 把新空闲块放进按地址升序的空闲链表时，需要在循环里用 list_next 不断找“第一个地址比它大的块”，再用 list_add_before 插到它前面；如果一直走到链表末尾（list_next(le) == &free_list），就用 list_add 接到最后。请问：这个“循环遍历 + 判断插入位置”的过程具体该怎么组织？为什么链表为空（list_empty）的情况要单独处理？

11. init_memmap(base, n) 要把从 base 开始的 n 个连续页初始化成一个空闲块：逐个清空 flags 和 property、set_page_ref(p, 0)，然后把 base 设为头页、property 置为 n、SetPageProperty(base)、nr_free 加 n。请帮我梳理：它和 best_fit_init（只做 list_init 和 nr_free=0）的职责分工是什么？为什么这里要先 assert(PageReserved(p))？

12. pmm_manager 结构体里声明了 name、init、init_memmap、alloc_pages、free_pages、nr_free_pages、check 这些成员。请问：我实现的 best_fit_init / best_fit_init_memmap / best_fit_alloc_pages / best_fit_free_pages / best_fit_nr_free_pages / best_fit_check 该怎么一一对应地填进 best_fit_pmm_manager，让 pmm.c 把 pmm_manager 指到它之后，alloc_pages 等对外接口能正确分发到 best-fit 的实现？
```
