# OSLab2实验报告

# 操作系统实验报告

## 实验基本信息

### 小组分工

练习如何分工？

实验报告如何分工？

---

## 一、实验目的

1. 理解物理内存范围的获取方法，以及物理页描述符的初始化过程。

2. 掌握连续物理内存的分配、释放和空闲块合并。

3. 理解 First\-Fit，实现并验证 Best\-Fit 和 Buddy System。

4. 理解页表的建立和使用，通过 GDB 观察地址转换过程。

5. 练习设计提示词，结合源码分析和测试验证 AI 生成的实现。

---

## 二、实验环境

### 2\.1 开发与运行环境

### 2\.2 AI 工具

---

## 三、实验整体逻辑分析

### 3\.1 本章节的逻辑主线

本实验围绕“如何发现、组织和分配可用物理内存”展开。主要调用关系为：

`pmm_init()    → init_pmm_manager()：选择并初始化物理内存管理器    → page_init()：获取内存范围，建立物理页描述符    → init_memmap()：向管理器注册可用空闲页    → alloc_pages() / free_pages()：分配与回收物理页`

### 3\.2 功能的逐步实现

---

## 四、实验内容与实现

### 4\.1 功能模块一：物理内存初始化

负责人：2413685\-刘晟槿

### 4\.1\.1 模块功能描述

**需要实现的函数：**

`kern/mm/pmm.c` 中的 `static void page_init(void);`

**模块功能：**

`page_init()` 是 ucore 操作系统物理内存管理初始化过程中的核心函数，主要负责获取物理内存范围、建立物理页描述符数组、标记保留页面，并将剩余可用物理页注册到物理内存分配器中。

在内核启动过程中，`pmm_init()` 首先初始化物理内存管理器，随后调用 `page_init()` 建立物理内存管理所需的数据结构，为后续 First\-Fit、Best\-Fit 和 Buddy System 等物理页分配算法提供统一的物理页管理基础。

本模块的具体实现分为以下五个步骤。

**（1）获取物理内存范围**

通过设备树（DTB）提供的信息获取物理内存起始地址和大小，确定物理内存的可管理范围。

关键代码逻辑：

```Plain Text
uintptr_t mem_base = get_memory_base();
uintptr_t mem_size = get_memory_size();

uintptr_t mem_end = mem_base + mem_size;

if (mem_end > PHYSICAL_MEMORY_END) {
    mem_end = PHYSICAL_MEMORY_END;
}
```

设计理由：

设备树能够向操作系统提供实际硬件的内存配置信息，相较于直接使用固定内存大小，能够更好地适应不同运行环境。

实现过程中还需要检查内存起始地址、内存容量及地址加法溢出，并将管理范围限制在当前内核支持的物理内存边界内，防止访问非法物理地址。

**（2）初始化物理页管理相关变量**

关键代码：

```Plain Text
va_pa_offset = PHYSICAL_MEMORY_OFFSET;
npage = PPN(mem_end);
```

其中：

- `va_pa_offset`：记录内核直接映射区域中虚拟地址与物理地址之间的偏移量。

- `npage`：记录当前物理内存管理范围的物理页号上界。

- `nbase`：表示物理内存起始地址对应的物理页号。

物理页大小为 4 KiB，因此物理页号可通过物理地址除以 `PGSIZE` 获得。

本实验管理的物理内存从 `0x80000000` 开始，并非从物理地址 0 开始，所以实际需要建立的页描述符数量为：

`npage - nbase`

这样能够避免为不属于当前管理范围的物理地址创建无效的页面描述符。

**（3）建立物理页描述符数组**

使用 `struct Page` 为每个物理页记录管理信息，数组放置在内核镜像结束位置之后。

关键代码逻辑：

```Plain Text
pages = (struct Page *)pages_start;

for (size_t i = 0; i < npage - nbase; i++) {
    struct Page *page = &pages[i];

    page->flags = 0;
    page->property = 0;
    set_page_ref(page, 0);

    SetPageReserved(page);
    list_init(&(page->page_link));
}
```

其中，`pages_start` 表示根据内核结束符号 `end` 计算并向上进行页对齐后的描述符数组起始地址。

`struct Page` 主要包含以下字段：

- `ref`：物理页引用计数。

- `flags`：物理页状态标志。

- `property`：用于记录连续空闲物理页块的大小等属性。

- `page_link`：将页面描述符连接到空闲页管理链表中。

初始化时，首先将各页面标记为 `PageReserved`，避免尚未确认可用的物理内存被分配器提前使用。

同时，对描述符数组占用的内存进行空间预留，防止后续物理页分配覆盖内核自身的数据结构。

**（4）确定空闲物理页范围**

由于内核代码、全局数据以及物理页描述符数组本身都占用物理内存，因此不能直接将设备树报告的全部物理内存交给分配器。

具体实现中，先计算描述符数组结束位置，再将空闲内存起点向上对齐到 4 KiB 页面边界。

空闲页数量的计算方式为：

```Plain Text
size_t free_count =
    (mem_end - free_start) / PGSIZE;
```

其中：

- `free_start`：描述符数组结束后，第一个完整可用物理页的起始地址。

- `mem_end`：向下进行页对齐后的可管理物理内存末端。

- `free_count`：实际可用于后续分配的物理页数量。

这样能够确保可分配区域不与内核镜像、描述符数组以及其他已保留区域发生重叠。

**（5）将空闲页注册到物理内存分配器**

关键代码：

```Plain Text
init_memmap(pa2page(free_start), free_count);
```

`pa2page()` 将物理地址转换为对应的 `struct Page` 指针。

`init_memmap()` 则调用当前物理内存管理器提供的初始化接口，将该连续空闲物理页区间注册到分配器内部。

通过统一的 `pmm_manager` 接口，`page_init()` 不需要关心具体采用 First\-Fit、Best\-Fit 还是 Buddy System，只负责提供合法的空闲物理页范围。

因此，该模块具有较好的独立性和可扩展性，也便于小组成员分别实现和测试不同的物理内存分配算法。

**模块运行结果：**

在本次实验中，使用 QEMU 模拟 128 MB 物理内存，设备树提供的物理内存范围为：

```Plain Text
Base: 0x0000000080000000
Size: 0x0000000008000000 (128 MB)
End:  0x0000000087ffffff
```

小组集成版本的运行结果如下：

```Plain Text
memory management: best_fit_pmm_manager
physical memory map:
  memory: 0x0000000008000000,
  [0x0000000080000000, 0x0000000087ffffff].
page_init: initialization completed
page_init: free pages = 31929
check_alloc_page() succeeded!
```

由运行结果可知，内核成功获取了物理内存范围，完成了页面描述符初始化，并将 31929 个可用物理页注册到当前选用的 Best\-Fit 物理内存管理器中。

同时，`check_alloc_page()` 基础检查通过，表明在本次测试场景下，物理页初始化结果能够支持后续页面分配操作。

**小结：**

通过实现 `page_init()`，完成了从设备树获取物理内存信息、建立物理页管理数据结构，到注册空闲物理页的完整初始化流程。

该模块为后续物理内存分配、释放和空闲块管理提供了基础，也体现了操作系统将物理内存资源管理与具体分配算法相分离的设计思想。

#### 4\.1\.2 最终提示词

*阅读 **`kern/mm/pmm.c`**、**`pmm.h`**、**`memlayout.h`** 与 DTB 读取接口，补全 **`page_init()`**。要求从 DTB 获取 RAM 起始地址与容量，校验与 **`DRAM_BASE`** 是否一致、范围是否溢出和越界，按照 **`PGSIZE`** 对齐；在内核 **`end`** 后安置 **`struct Page`** 数组，并逐页初始化 **`flags`**、**`property`**、引用计数及链表节点，默认设为保留页；扣除内核和页描述符数组占用后，将真正空闲的连续页面通过 **`init_memmap()`** 注册；保持 **`pmm_manager`** 统一接口，与 First\-Fit/Best\-Fit/Buddy 的实现解耦；提供必要的断言、运行日志及验证方法，并解释每个地址计算的含义。*

---

### 4\.2 功能模块二：物理内存分配

#### 4\.2\.1 任务一：理解 First\-Fit

负责人：2413978\-缪承佑

相关文件： kern/mm/default\_pmm\.c

完整分配与释放过程（举例说明）：

【答】**1\.申请 3 页**：调用 alloc\_pages\(3\)，First\-Fit 找到这个 8 页块。取出该块，分割：前 3 页分配出去；剩下 5 页作为新空闲块放回空闲链表。此时空闲链表存 5 页空闲块。

**2\.释放这 3 页**：调用 free\_pages\(分配得到的首地址，3\)。先把 3 页标记为空闲，插入空闲链表；检查前后邻居：后面刚好是 5 页空闲块，触发合并，合并成 8 页的连续空闲块，放回空闲链表。



思考题：原有 First\-Fit 算法是否有进一步的改进空间？

【答】原有 First\-Fit 算法有改进空间。

**1\.查找开销**：First\-Fit 每次从头遍历链表，遇到小空闲块也会逐个检查，大块内存分配时查找耗时高。

**2\.内存碎片**：频繁小分配、小释放，会产生大量很小的外部碎片，后面很难找到连续大内存块。

**3\.管理结构**：空闲块用链表组织，查找时只能线性遍历，无法按块大小快速定位。

可以得到一种**改进的思路**：分离空闲链表。按空闲块大小分成多条链表，不同大小的块放在不同链表，分配时直接去对应尺寸链表查找，减少遍历次数。但是此时需要维护多条链表，内存管理结构相应的更加复杂。

---

#### 4\.2\.2 任务二：实现 Best\-Fit

负责人：2413978\-缪承佑

相关文件：

- kern/mm/best\_fit\_pmm\.c

- kern/mm/pmm\.c 中的 init\_pmm\_manager\(\)

##### 模块功能描述

**1\.初始化空闲页。**

（1）**best\_fit\_init\(\)**：初始化空闲块双向链表 free\_list 为空，空闲页总数 nr\_free 置 0。

（2）**best\_fit\_init\_memmap\(base, n\)**：初始化一段连续的空闲物理页。先循环清空这 n 个页的 flags、property 字段并把引用计数置 0；再把首页 base 的 property 置为 n、打上 PageProperty 标记（表示这是一块连续 n 页的空闲块首页）；最后把该块按物理地址升序插入空闲链表，nr\_free \+= n。

**2\.分配连续物理页——Best\-Fit 核心**

best\_fit\_alloc\_pages\(n\) 的关键在于**如何选出“满足需求且尺寸最小”的空闲块**：

（1）**前置判断**：若 n \> nr\_free，说明空闲总页数都不够，直接返回 NULL。

（2）**全链表扫描**：从链表头出发，完整遍历整条空闲链表（这一点与 First\-Fit 不同——First\-Fit 找到第一个够用的就停，Best\-Fit 必须扫完才能确定“最小”）。

（3）**维护当前最优解**：用一个指针 page 记录当前找到的最优空闲块，初始化为 NULL。遍历中，对每个空闲块 p：

**·**先判断 p\-\>property \>= n（是否满足页数需求）；

**·**若满足，且 page == NULL 或 p\-\>property \< page\-\>property（比当前最优块更小），则把 page 更新为 p。

（4）遍历结束，page 指向的正是**满足需求且尺寸最小**的空闲块；若 page 仍为 NULL，说明不存在够大的块，返回 NULL。

**分割规则**（选中块 page 后）：

若 page \-\> property \> n（块比需求大），执行分割：剩余块起始页为 page \+ n，其 property = 原property − n，打上 PageProperty 标记（成为新的空闲块头）并 list\_add 插回原位置；

然后 nr\_free \-= n，并ClearPageProperty\(page\) 清除被分配块的标记；返回 page。

**3\.回收页面**

best\_fit\_free\_pages\(base, n\)：

（1）先重置这 n 个页的 flags、引用计数（并断言这些页不是保留页、也不是空闲块头，防止重复释放）。

（2）把首页的 base 的 property 置为 n、打上 PageProperty 标记，nr\_free \+= n。

（3）把回收块按**地址升序**插入空闲链表。

**4\.相邻空闲块合并规则**

插入链表后先后检查两侧邻居，若**物理地址严格相邻**则合并：

（1）**向前合并**：若前驱空闲块满足 p \+ p \-\> property == base（前驱块结束地址 == 本块起始地址），说明物理连续，则把前驱块的 property 加上本块页数，清除 base 的 PageProperty 标记并把 base 从链表删除。

（2）**向后合并**：若后续空闲块满足 base \+ base \-\> property == p（本块结束地址 == 后继块起始地址），则本块 property 加上后继块页数，清除后继块的 PageProperty 标记并删除后继块。

通过这两步，回收后能自动把物理上相邻的空闲块合并成一块更大的连续空闲块，避免外部碎片。



实际实现的函数和管理器：

与 First\-Fit 的区别：

First\-Fit 遍历空闲链表，**选取第一个满足页数需求的空闲块**，查找速度较快，但容易在链表前端留下大量小碎片；Best\-Fit 遍历全部空闲链表，**选取满足页数需求的尺寸最小的空闲块**，能尽量节约大空闲块，但每次分配都需要完整遍历链表，查找开销更大，更容易产生微小的外部碎片。



内核接入方式：

在 init\_pmm\_manager\(\) 函数内，将全局内存管理器指针 pmm\_manager 赋值为 \&best\_fit\_pmm\_manager，替换原本的 default\_pmm\_manager；修改后重新编译运行内核，可通过配套的 best\_fit\_check\(\) 测试函数执行测试，若测试全部通过，说明当前内核使用的是 Best\-Fit 内存分配器。

##### 最终提示词

```Plain Text
【任务说明】
任务：在文件kern/mm/best_fit_pmm.c中，标有lab2、YOUR CODE的位置，完整实现Best-Fit连续物理内存分配模块，参考default_pmm.c里First-Fit的整体框架、链表操作风格。
硬性约束：
1.禁止修改文件内已有的basic_check()、best_fit_check()两个测试函数，不能改动这两个函数任何代码；
2.需要自行定义内部空闲管理结构、管理函数，并且定义best_fit_pmm_manager这个pmm管理器注册结构体，适配pmm.h的struct pmm_manager（共7个字段：name、init、init_memmap、alloc_pages、free_pages、nr_free_pages、check）；
3.代码基于ucore的Page、环形双向list链表体系，逻辑风格对齐原First-Fit实现；
4.最终代码需要支持在kern/mm/pmm.c的init_pmm_manager()里把全局pmm管理器替换成best_fit_pmm_manager后正常运行、通过自带测试；
5.只在标记YOUR CODE区域写实现代码，其余原有内容保留不动。
验证标准：分配时选出满足申请页数n的“最小尺寸空闲块”；内存分割、空闲块前后相邻合并、空闲链表维护逻辑正确，能够顺利通过basic_check和best_fit_check测试。

【Rely依赖说明（仅使用下面这些已有定义，不要自己新增额外复杂结构）】
依赖头文件：pmm.h、list.h、memlayout.h
依赖核心定义：
struct Page、struct pmm_manager、free_area_t、list_entry_t全套双向环形链表工具宏（list_init/list_add/list_add_before/list_del/list_next/list_prev/list_empty/le2page等）
标志宏：PageReserved、PageProperty、SetPageProperty、ClearPageProperty
辅助函数：set_page_ref、assert
规则：空闲块仅空闲块首页的property字段有效，记录本块连续页数；块头带PageProperty标记，块内其余页无此标记。

【Guarantee接口清单】
需要实现pmm_manager对应的5个管理函数：
1.best_fit_init
2.best_fit_init_memmap
3.best_fit_alloc_pages
4.best_fit_free_pages
5.best_fit_nr_free_pages
内部结构照default_pmm.c第6~9行风格，在本文件声明：
static free_area_t free_area;
#define free_list (free_area.free_list)
#define nr_free  (free_area.nr_free)
然后填充结构体best_fit_pmm_manager，把7个字段全部挂载：
.name = "best_fit_pmm_manager"
.init = best_fit_init
.init_memmap = best_fit_init_memmap
.alloc_pages = best_fit_alloc_pages
.free_pages = best_fit_free_pages
.nr_free_pages = best_fit_nr_free_pages
.check = best_fit_check（直接复用已存在的测试函数，不修改）
允许声明本文件内的static全局变量、static内部辅助变量，不允许新增外部全局变量、不修改头文件。

【Specification行为规格，逐个函数定义前置后置与分支逻辑】
1.best_fit_init
前置：无
行为：调用list_init(&free_list)将空闲链表初始化为空环形链表，nr_free置0。
后置：free_list是空环形链表（list_empty(&free_list)为真），nr_free=0。

2.best_fit_init_memmap(struct Page *base, size_t n)
前置：n>0；传入的base开始的n个页是PageReserved保留页。
行为：assert(n>0)；循环遍历[base, base+n)，每页先assert(PageReserved(p))，再清空flags、property，引用计数置0；设置base首页property=n，SetPageProperty(base)；nr_free+=n；将这个空闲块按物理地址升序插入环形空闲链表——链表空则list_add直接插入，否则遍历找到第一个地址大于base的块插到它之前（list_add_before），若所有块地址都小于base则插到末尾（list_add）。
后置：该连续页块加入空闲链表，标记为空闲块。

3.best_fit_alloc_pages(size_t n)【Best-Fit核心】
前置：n>0
分支1：assert(n>0)；如果nr_free<n，直接返回NULL，链表、nr_free完全不变；
分支2：完整遍历整条空闲链表（与First-Fit不同，必须扫完才能确定最小块），维护当前最优解——用struct Page *page=NULL记录，遍历中若p->property>=n且（page==NULL或p->property<page->property），则更新page=p；遍历结束page即“满足需求且尺寸最小”的块，若page仍为NULL则返回NULL；
分支3：命中后，先list_entry_t *prev=list_prev(&(page->page_link))记录前驱，再list_del(&(page->page_link))摘除该块；
分支4：如果page->property>n，执行分割：剩余块起始页p=page+n，p->property=page->property-n，SetPageProperty(p)（剩余块成为新空闲块头），并用list_add(prev, &(p->page_link))插回原位置（不是重新排序）；
分支5：nr_free-=n；ClearPageProperty(page)清除被分配块首页的PageProperty标记；返回page；
后置：返回分配块起始Page指针；分配出去的区域不再留在空闲链表；若不足则返回NULL且全局状态不变。

4.best_fit_free_pages(struct Page *base, size_t n)
前置：base是待回收连续页的首地址，n>0；这块内存已经被分配出去。
行为：assert(n>0)；遍历[base, base+n)，每页先assert(!PageReserved(p) && !PageProperty(p))（防止释放保留页或重复释放已空闲块头），再重置flags、引用计数；设置首页base->property=n，SetPageProperty(base)，nr_free+=n；将回收块按物理地址升序插入环形空闲链表（同init_memmap插入方式）；然后相邻合并，先向左、再向右：
（1）向左合并：le=list_prev(&(base->page_link))，若le!=&free_list，取p=le2page(le, page_link)，若p+p->property==base（前块结束地址==本块起始地址，物理连续），则p->property+=base->property，ClearPageProperty(base)，list_del(&(base->page_link))，并令base=p；
（2）向右合并：le=list_next(&(base->page_link))，若le!=&free_list，取p=le2page(le, page_link)，若base+base->property==p（本块结束地址==后块起始地址），则base->property+=p->property，ClearPageProperty(p)，list_del(&(p->page_link))。
后置：nr_free更新正确；物理相邻的空闲块被合并为更大连续块，避免外部碎片。

5.best_fit_nr_free_pages(void)
前置：无
行为：return nr_free;
后置：返回当前空闲物理页总数。

额外编码要求：
代码注释风格参考default_pmm.c，关键逻辑增加注释；C语言语法适配ucore编译环境，不要引入stdlib、printf等无关库。
```

##### 实现迭代过程

原有测试是否保持不变：保持原有测试代码完全不变，不修改 basic\_check \(\) 和 best\_fit\_check \(\) 任何源码，仅使用这两个函数对 Best\-Fit 内存分配模块进行功能校验。

最终结果：将 pmm 管理器切换为 best\_fit\_pmm\_manager 后，内核正常启动，basic\_check \(\)、best\_fit\_check \(\) 两项测试全部顺利通过，Best\-Fit 分配、释放、内存分割、相邻空闲块合并功能均符合预期，无断言报错。



思考题：你的 Best\-Fit 算法是否有进一步的改进空间？

【答】现在的 Best\-Fit 算法还存在改进空间。当前实现每次分配都需要完整遍历整条空闲链表，时间复杂度为 O \(N\)，空闲块数量较多时查找效率偏低；同时该算法偏好消耗大小刚好匹配的块，容易遗留大量很难再被利用的极小外部碎片。

**改进方案**：可以采用分离空闲链表（分桶）或者红黑树 / 有序搜索树来管理空闲块，以空闲块大小作为索引，分配时直接快速定位满足条件的最小块，将查找复杂度降低到 O \(logN\)；代价是需要维护多组链表或者树结构，提升了内存管理的实现复杂度。

---

#### 4\.2\.3 任务三：Buddy System 分配算法

负责人：2412000\-文玥

##### 模块功能描述

新增 `kern/mm/buddy_pmm.c` 和 `kern/mm/buddy_pmm.h`，定义以下管理器接口：



```C
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);
const struct pmm_manager buddy_pmm_manager;
```



申请 n 页时将其向上取整为最小的 2 的幂；不足目标阶空闲块时从高阶取块并逐级分裂。释放时按保存的阶数找到伙伴，只有双方同阶、空闲且互为伙伴时才合并。



##### 数据结构设计



使用 16 个空闲链表，对应 order 0～15，即 1～32768 页。最大块为 128 MiB，覆盖当前框架从 0x80000000 到 0x88000000 的物理页描述范围。可分配内存还要扣除固件、内核和描述符占用，不能将最大块大小当成实际可用空间。



不修改 `struct Page`，额外使用固定大小的 `metadata` 数组：



|字段/状态|含义|
|---|---|
|UNREGISTERED|尚未交给 Buddy 的页面|
|INTERIOR|已注册的块内部页，或操作中的临时非首页状态|
|FREE\_HEAD|空闲块首页|
|ALLOC\_HEAD|已分配块首页|
|order|块首页对应的实际阶数|
|requested|分配时的原始申请页数|



`Page.flags` 中 reserved 位在注册时清除，property 位只标记空闲块首页。`Page.property` 在块首页记录实际页数，内部页为 0；`page_link` 只用于链接空闲块首页。注册及释放时将 ref 清零。



必须同时检查 ALLOC\_HEAD 和 requested，才能验证释放是否合法，不能只凭 PageProperty 为假判断已经分配。



##### 分裂、合并与地址基准



伙伴按物理页号计算：



```Plain Text
i = 描述符数组索引
ppn = nbase + i
buddy_ppn = ppn XOR (1 << order)
buddy_index = buddy_ppn - nbase
```



计算后先检查边界，再检查伙伴状态和阶数。相邻但非伙伴的块不能合并。不同次注册的相邻范围可以合并，但前提是双方均已注册、同阶且确实互为伙伴；空洞和保留区域不会参与。



初始范围按物理页号对齐和剩余长度拆成多个合法块，因此不要求起点按整个范围大小对齐，也不要求总长度是 2 的幂。



例如申请 3 页时实际占用 4 页：



```Plain Text
alloc_pages(3) → 记录 requested=3、order=2 → 空闲页减少 4
free_pages(p,3) → 校验原请求 → 回收完整 4 页 → 尝试伙伴合并
```



##### 每个函数的关键行为



|函数|主要处理|
|---|---|
|buddy\_init|初始化链表和独立元数据，不提前读取 pages|
|buddy\_init\_memmap|先验证整个范围合法且未重复注册，再清除 reserved、初始化页面、按对齐分块并注册|
|buddy\_alloc\_pages|拒绝零和超上限请求；找块、分裂、记录首页状态、扣除实际页数|
|buddy\_free\_pages|检查描述符地址对齐、首页状态、原申请页数；逐级合并，空闲计数只增加一次|
|buddy\_nr\_free\_pages|返回总空闲页数，不修改状态|
|buddy\_check|基本申请回收和失败输入检查；无可用块时明确打印 SKIP|



非法释放采用 assert/panic 拒绝。申请失败返回 NULL，且不修改链表、元数据和空闲计数。最大页数在求阶前检查，避免无界移位。



##### 不变量与开销



- 每个空闲块仅出现在一个对应阶链表中。

- 空闲块和已分配块覆盖互不重叠，不能包含未注册页面。

- 空闲计数等于各阶空闲块页数总和。

- 合并前移除伙伴的旧链表节点并清除旧首页状态。

- 全部分配结果被正确释放后，空闲页总数恢复。

    

元数据通常每页 8 字节，共 256 KiB，另有 16 个链表头和计数变量。新增静态数据位于 BSS；最新 page\_init 使用链接后的 end 定位描述符数组，能够随内核大小变化调整位置，但实际内核验证仍需完成。



分配复杂度为 O\(log M\)；释放还检查块内部页面，因此为 O\(实际块页数 \+ log M\)。固定元数据数组使实现无需 malloc，但增加了静态内存开销，且容量限定为当前平台。



##### 最终提示词如下：

\[PROMPT\]



> 请基于提供的 ucore Lab2 源码，实现 Buddy System物理内存分配器：
> 
> 
> 
> 先阅读以下文件，确认实际接口和已有实现：
> 
> - kern/mm/pmm\.h
> 
> - kern/mm/pmm\.c
> 
> - kern/mm/memlayout\.h
> 
> - kern/mm/mmu\.h
> 
> - kern/mm/default\_pmm\.c
> 
> - libs/list\.h
> 
> - Makefile
> 
>     
> 
> 新增以下文件：
> 
> - kern/mm/buddy\_pmm\.c
> 
> - kern/mm/buddy\_pmm\.h
> 
>     
> 
> 采用按阶数组织空闲链表的方式管理内存：
> 
> 第 k 阶对应 2^k 个连续物理页。申请时向上取整到合适阶数，必要时需要分裂大块，释放时需要尝试逐级合并空闲伙伴。
> 
> 
> 
> 优先复用现有 struct Page，不修改其布局。明确说明 flags、property、page\_link 在 Buddy 实现中的含义，以及如何区分空闲块首页、已分配块首页和块内其他页面。
> 
> 
> 
> 
> 
> 先说明设计方案和关键不变量，再给出完整代码、接入步骤和验证方法。
> 
> 
> 
> \[RELY\]
> 
> 
> 
> 可以依赖源码中实际存在的以下接口，不得自行假设存在其他辅助函数。
> 
> 
> 
> 1. 物理内存管理器接口
> 
>     
> 
> pmm\.h 中的 struct pmm\_manager：
> 
> - name
> 
> - init
> 
> - init\_memmap
> 
> - alloc\_pages
> 
> - free\_pages
> 
> - nr\_free\_pages
> 
> - check
> 
>     
> 
> 接口签名以实际 pmm\.h 为准。
> 
> 
> 
> 2. 页面描述结构
> 
>     
> 
> memlayout\.h 中的 struct Page：
> 
> - ref：引用计数
> 
> - flags：页面状态
> 
> - property：可用于记录块大小或阶数，需说明具体约定
> 
> - page\_link：链表节点
> 
>     
> 
> 可以使用已有页面状态宏：
> 
> - SetPageReserved
> 
> - ClearPageReserved
> 
> - PageReserved
> 
> - SetPageProperty
> 
> - ClearPageProperty
> 
> - PageProperty
> 
> 
> 
> 3. 页面与地址相关定义
> 
>     
> 
> 可以使用：
> 
> - PGSIZE、PGSHIFT
> 
> - pages、npage、nbase
> 
> - page2ppn\(\)
> 
> - page2pa\(\)
> 
> - set\_page\_ref\(\)
> 
> - 源码中已有的地址转换和对齐宏
> 
>     
> 
> 注意：
> 
> page \- pages 是描述符数组索引；
> 
> page2ppn\(page\) 是物理页号；
> 
> page2pa\(page\) 是物理地址。
> 
> 
> 
> 三者不能混用。计算伙伴时必须明确采用哪一种坐标，以及该坐标对应的块对齐规则。
> 
> 
> 
> 4. 链表与调试支持
> 
>     
> 
> 可以使用 libs/list\.h 中实际提供的链表类型和操作，以及：
> 
> - le2page\(\)
> 
> - assert\(\)
> 
> - panic\(\)
> 
> - cprintf\(\)
> 
>     
> 
> 可以参考 default\_pmm\.c 的管理器注册和接口组织方式，但不能照搬 First\-Fit 的选块策略作为 Buddy 实现。
> 
> 
> 
> 5. 运行前提
> 
>     
> 
> 由 page\_init\(\) 建立页面描述符，并将合法空闲范围传给 init\_memmap\(\)。
> 
> 
> 
> Buddy 初始化和分配逻辑不得依赖 malloc，也不得依赖尚未建立的动态内存分配器。
> 
> 
> 
> 不得修改源码中已有的测试来适配错误实现。
> 
> 
> 
> 
> 
> \[GUARANTEE\]
> 
> 
> 
> 必须提供以下函数，内部函数可声明为 static：
> 
> 
> 
> 1. static void buddy\_init\(void\);
> 
>     
> 
> 2. static void buddy\_init\_memmap\(struct Page \*base, size\_t n\);
> 
>     
> 
> 3. static struct Page \*buddy\_alloc\_pages\(size\_t n\);
> 
>     
> 
> 4. static void buddy\_free\_pages\(struct Page \*base, size\_t n\);
> 
>     
> 
> 5. static size\_t buddy\_nr\_free\_pages\(void\);
> 
>     
> 
> 6. static void buddy\_check\(void\);
> 
>     
> 
> 必须定义管理器：
> 
> const struct pmm\_manager buddy\_pmm\_manager;
> 
> 
> 
> 在 buddy\_pmm\.h 中提供对应的 extern 声明，并设置头文件保护。
> 
> 
> 
> 还必须交付：
> 
> - pmm\.c 中包含头文件、切换到 buddy\_pmm\_manager 的最小修改说明；
> 
> - 编译方式和运行方式；
> 
> - 如何从启动输出确认实际选中了 Buddy；
> 
> - 数据结构、阶数上限、空间开销和边界处理说明。
> 
>     
> 
>     
> 
> \[SPECIFICATION\]
> 
> 
> 
> 一、整体状态约定
> 
> 
> 
> 1. 第 k 阶空闲块恰好包含 2^k 页。
> 
> 2. 每个空闲块只能在对应阶的链表中出现一次。
> 
> 3. 空闲块之间不得重叠，已分配块不得出现在空闲链表中。
> 
> 4. 所有块必须位于已注册的可用页面范围内。
> 
> 5. nr\_free\_pages 统计可分配的物理页总数，而不是空闲块数量。
> 
> 6. 为已分配块保存足够的元数据，能够校验首页、原始申请页数和实际分配阶数。
> 
> 7. 申请和释放的接口约定为：
> 
>     
> 
>     p = alloc\_pages\(n\);
> 
>     free\_pages\(p, n\);
> 
>     
> 
>     其中释放时传入原始申请页数 n。
> 
>     
> 
>     例如申请 3 页，实际分配 4 页；
> 
>     free\_pages\(p, 3\) 应回收完整的 4 页。
> 
>     
> 
> 8. 申请 n 页失败时，不得改变空闲链表、块状态和空闲页计数。
> 
> 9. 明确最大阶数及其依据，避免移位溢出和页数计算溢出。
> 
> 10. 内部实现不得调用公共 alloc\_pages\(\)/free\_pages\(\) 包装器来执行同名功能，以免递归进入自身。
> 
>     
> 
>     
> 
> 二、buddy\_init\(\)
> 
> 
> 
> 输入：
> 
> 无。
> 
> 
> 
> 行为：
> 
> - 初始化各阶空闲链表。
> 
> - 清零空闲页总数。
> 
> - 初始化管理器自身的状态记录。
> 
> - 不自行探测物理内存，不调用 page\_init\(\)。
> 
> - 不读取尚未由 page\_init\(\) 建立的页面描述符。
> 
>     
> 
> 完成后：
> 
> - 所有阶的链表为空。
> 
> - buddy\_nr\_free\_pages\(\) 返回 0。
> 
>     
> 
>     
> 
> 三、buddy\_init\_memmap\(struct Page \*base, size\_t n\)
> 
> 
> 
> 输入：
> 
> - base：待注册范围的首个 Page 描述符。
> 
> - n：该范围包含的连续页面数。
> 
>     
> 
> 前置条件：
> 
> - 描述符范围合法；
> 
> - 对应页面可交给分配器使用；
> 
> - 与已经注册的范围不重叠。
> 
>     
> 
> 行为：
> 
> - 检查范围和参数，不允许越界或重复登记。
> 
> - 初始化该范围内的引用计数、页面标记和块元数据。
> 
> - 按选定的伙伴坐标和对齐规则，将范围分解成合法的 2 的幂大小块。
> 
> - 将各块加入对应阶空闲链表。
> 
> - 不要求 base 按整个范围大小对齐，也不要求 n 是 2 的幂。
> 
> - 不得遗漏尾部页面，也不得为了凑整块包含范围外的页面。
> 
> - 支持多次注册互不重叠的范围，不得重置此前的管理状态。
> 
> - 跨注册范围是否允许合并必须明确约定；无论采用何种方案，都不得跨越未注册或保留页面。
> 
>     
> 
> 完成后：
> 
> - 新增空闲页数恰好为 n。
> 
> - 新范围内的每一页均被准确管理一次。
> 
>     
> 
>     
> 
> 四、buddy\_alloc\_pages\(size\_t n\)
> 
> 
> 
> 输入：
> 
> 申请的连续物理页数。
> 
> 
> 
> 行为：
> 
> - n 为 0 时返回 NULL，不修改状态。
> 
> - 将 n 向上取整到最小的 2 的幂，得到目标阶数。
> 
> - 检查计算溢出和最大阶数限制；超出能力时返回 NULL。
> 
> - 从目标阶开始向更高阶搜索可用块。
> 
> - 找不到足够大的连续块时返回 NULL。
> 
> - 找到更高阶块时，将其从链表移除，并逐级对半分裂。
> 
> - 每次分裂将不继续分配的一半加入相应空闲链表。
> 
> - 标记最终块为已分配，记录原始请求和实际阶数。
> 
> - 从空闲页总数中扣除实际块页数。
> 
>     
> 
> 返回：
> 
> - 成功：分配块首页的 struct Page 指针。
> 
> - 失败：NULL。
> 
>     
> 
> 示例：
> 
> 申请 3 页时，返回至少 4 页的连续区域，空闲页数减少 4。
> 
> 返回区域不得与其他尚未释放的分配结果重叠。
> 
> 
> 
> 
> 
> 五、buddy\_free\_pages\(struct Page \*base, size\_t n\)
> 
> 
> 
> 输入：
> 
> - base：之前分配返回的首页描述符。
> 
> - n：当时申请的原始页数。
> 
>     
> 
> 行为：
> 
> - 修改状态前，校验地址、首页标记、分配状态和原始申请页数。
> 
> - 对 NULL、零页数、重复释放、非首页释放和不匹配页数，
> 
> 使用明确的断言或 panic 拒绝，不得静默破坏状态。
> 
> - 根据保存的元数据取得实际分配阶数。
> 
> - 将实际占用页数归还空闲页统计，只增加一次。
> 
> - 计算同阶伙伴的位置。
> 
> - 仅当伙伴属于合法已注册范围、确实空闲且阶数相同时才合并。
> 
> - 合并前从空闲链表移除伙伴。
> 
> - 合并后选低地址作为新块首页，阶数加一，并继续尝试。
> 
> - 达到最大阶数或伙伴不满足条件时停止。
> 
> - 将最终块加入对应阶链表，更新相关页面标记。
> 
>     
> 
> 注意：
> 
> - 相邻块不一定是伙伴。
> 
> - 合并本身不额外增加空闲页数。
> 
> - 已分配块不能参与合并。
> 
> - 不得留下旧块首页仍被标记为独立空闲块的状态。
> 
>     
> 
>     
> 
> 六、buddy\_nr\_free\_pages\(\)
> 
> 
> 
> 输入：
> 
> 无。
> 
> 
> 
> 行为：
> 
> - 返回当前空闲物理页总数。
> 
> - 不修改任何链表或元数据。
> 
>     
> 
> 结果必须与所有空闲块页数之和一致。
> 
> 
> 
> 



---



### 4\.3 支线任务：GDB 调试页表查询过程

**负责人：2413685\-刘晟槿**

#### 4\.3\.1 调试目标

本次调试主要研究 RISC\-V Sv39 分页机制下，CPU 发生 TLB Miss 后如何通过页表查询完成虚拟地址到物理地址的转换，以及 QEMU 如何将转换结果写入软件 TLB。

选取内核启动阶段的一次指令获取操作作为观察对象，其虚拟地址为 `0x80200038`。该地址位于内核早期启动代码区域，在当前启动页表中采用低地址恒等映射。

调试需要验证以下内容：

1. 观察 CPU 从 M\-mode、Bare 模式进入 S\-mode、Sv39 分页模式的过程。

2. 根据 `satp` 寄存器确定根页表的物理地址。

3. 分析 Sv39 页表查询中 VPN 索引的计算方法。

4. 通过 GDB 观察 QEMU 读取页表项（PTE）的实际过程，并检查页表项内容及访问权限。

5. 验证虚拟地址 `0x80200038` 是否正确转换为物理地址 `0x80200038`。

6. 跟踪 `riscv_cpu_tlb_fill()`，验证 QEMU 软件 TLB 填充流程是否成功完成。

通过上述调试，将 RISC\-V 特权级架构手册中的 Sv39 地址转换原理与 QEMU 的具体实现进行对应，加深对三级页表、超级页映射、TLB 和内存访问权限检查机制的理解。

#### 4\.3\.2 调试环境与方法

本实验采用宿主机 Linux GDB 与 RISC\-V 目标机 GDB 相结合的调试方式。其中，Linux GDB 用于分析 QEMU 内部 C 源码，RISC\-V GDB 用于观察被模拟处理器的寄存器和内核代码。实际的页表遍历与软件 TLB 填充验证主要通过 Linux GDB 完成。

**QEMU 源码调试环境配置：**

由于 Ubuntu 软件仓库提供的 QEMU 使用了编译优化，部分局部变量在 GDB 中可能显示为 `<optimized out>`。为实现源码级调试，安装与 QEMU 版本匹配的调试符号包，并获取对应版本的源代码。

使用以下命令启动宿主机 GDB：

```Plain Text
gdb -q /usr/bin/qemu-system-riscv64
```

配置源码映射：

```Plain Text
set substitute-path /usr/src/qemu-1:8.2.2+ds-0ubuntu1.18/b/qemu /home/maple/qemu-debug/qemu-8.2.2+ds
```

配置 QEMU 启动参数：

```Plain Text
set args -machine virt -m 128M -display none -monitor none -serial file:/tmp/ucore-lab2-serial.log -bios default -kernel /mnt/d/OSlab/lab2-group/lab2/bin/ucore.img
```

这种方式将内核串口输出保存到日志文件，避免 QEMU Monitor 与 GDB 共用终端时发生输入冲突。

#### 4\.3\.3 调试过程与观察结果

**（1）确定分页模式与根页表地址**

首次在 `riscv_cpu_tlb_fill()` 中设置断点时，GDB 观察到：

```Plain Text
env.pc   = 0x1000
env.satp = 0x0
env.priv = 3
```

这说明 CPU 仍处于启动 ROM 阶段，当前特权级为 M\-mode，尚未启用 Sv39。

随后通过条件断点筛选：

```Plain Text
condition 5 (((env->satp >> 60) == 8) && (mmu_idx == 1))
```

再次命中断点时，观察到：

```Plain Text
addr     = 0x80200038
satp     = 0x8000000000080205
mmu_idx  = 1
```

此前在 `riscv_cpu_tlb_fill()` 中还确认 `env.priv=1`，即 S\-mode。

对于 RV64 Sv39，`satp` 主要包含 MODE、ASID 和 PPN 字段，其中 MODE 为 8 表示 Sv39，PPN 表示根页表所在的物理页号。

本次根页表物理页号为：

\[

PPN=0x80205

\]

由于页面大小为 4 KiB，根页表物理地址为：

\[

PA\_\{\\text\{root\}\}=0x80205\\times0x1000

=\\boxed\{0x80205000\}

\]

**（2）计算根级页表索引**

Sv39 虚拟地址划分为：

- VPN\[2\]：9 位，对应根级页表索引；

- VPN\[1\]：9 位，对应第二级页表索引；

- VPN\[0\]：9 位，对应第三级页表索引；

- Offset：12 位，表示页内偏移。

对于目标虚拟地址：VA=0x80200038

根级索引为：VPN\[2\]=\(VA\\gg30\)\\ \&\\ 0x1FF=2

每个页表项占 8 字节，因此根级 PTE 的物理地址为：pte\_addr = 0x80205010

与 GDB 的实际输出一致：

```Plain Text
base     = 0x80205000
ptshift  = 18
idx      = 0x2
pte_addr = 0x80205010
```

**（3）读取 PTE 并判断页表项类型**

QEMU 在 `target/riscv/cpu_helper.c` 中使用以下语句读取页表项：

```Plain Text
pte = address_space_ldq(cs->as, pte_addr, attrs, &res);
```

在该位置设置临时断点，并单步执行后观察到：

```Plain Text
pte = 0x200000cf
```

该页表项低位权限标志包括：

由于该 PTE 位于 Sv39 根级页表，且包含合法的 R、W、X 权限标志，因此 QEMU 将其作为叶子页表项处理。

该叶子项对应 1 GiB 超级页映射，页表查询可以在根级直接完成，不再访问后两级页表。

从 PTE 中提取物理页号：PPN=0x80000

对应物理映射基地址：PAbase=0x80000000

**（4）验证最终物理地址**

本次 1 GiB 超级页映射中，物理基地址为 `0x80000000`，虚拟地址相对于该超级页基地址的偏移为 `0x200038`。

因此，理论物理地址为：PA=0x80000000\+0x200038=0x80200038

调试时首先保存输出参数指针：

```Plain Text
set $pa_ptr = physical
```

然后执行：

```Plain Text
finish
```

GDB 输出：

```Plain Text
Value returned is $32 = 0
```

表示 `get_physical_address()` 返回 `TRANSLATE_SUCCESS`。

读取实际物理地址：

```Plain Text
p/x *$pa_ptr
```

得到：

```Plain Text
$34 = 0x80200038
```

与理论计算完全一致，证明本次 Sv39 地址转换成功完成。

**（5）观察 QEMU 软件 TLB 填充**

在 `riscv_cpu_tlb_fill()` 的第 1404 行，QEMU 使用以下代码完成软件 TLB 填充：

```Plain Text
if (ret == TRANSLATE_SUCCESS) {
    tlb_set_page(cs, address & ~(tlb_size - 1),
                 pa & ~(tlb_size - 1),
                 prot, mmu_idx, tlb_size);
    return true;
}
```

GDB 实际命中了 `tlb_set_page()` 调用位置，观察到：

```Plain Text
address = 0x80200038
pa      = 0x80200038
prot    = 7
```

随后继续跟踪 `tlb_set_page_with_attrs()` 相关实现，并使用 `finish` 执行到 `riscv_cpu_tlb_fill()` 返回。

最终得到：

```Plain Text
Value returned is $44 = true
```

说明本次软件 TLB 填充流程正常执行完成。

需要注意，Sv39 根级叶子项的映射范围为 1 GiB，而 QEMU 软件 TLB 本次填充使用的缓存粒度由 `tlb_size` 等参数决定，二者并不必然相同。

#### 4\.3\.4 关键地址与寄存器

#### 4\.3\.5 调试截图

建议将实际 GDB 终端截图保存至 `report/images/`，并按照以下顺序插入。

**图 1：Sv39 模式及地址转换函数入口**

![image\.png](图片和附件/image%201.png)

证明 CPU 已进入 Sv39 分页模式，当前正在进行内核启动阶段的指令地址转换。

**图 2：根级页表项地址与内容**

![image\.png](图片和附件/image%205.png)

证明 QEMU 根据根页表地址和 VPN 索引正确定位、读取根级 PTE。

**图 3：虚拟地址转换为物理地址**

![image\.png](图片和附件/image%203.png)

证明 `get_physical_address()` 返回成功，实际物理地址与理论计算一致。

**图 4：软件 TLB 填充成功**

![image\.png](图片和附件/image%204.png)

证明 QEMU 已进入软件 TLB 填充路径，`riscv_cpu_tlb_fill()` 正常返回成功。

#### 4\.3\.6 实验结论

本次实验通过 Linux GDB 对 QEMU 8\.2\.2 中的 RISC\-V 地址转换代码进行源码级调试，成功观察了从软件 TLB Miss 到 Sv39 页表查询、物理地址计算以及软件 TLB 填充的完整过程。

调试结果表明，当 CPU 处于 S\-mode 且 `satp.MODE=8` 时，QEMU 根据 `satp` 中记录的根页表物理页号，定位根页表地址 `0x80205000`，再利用虚拟地址的 VPN\[2\] 索引计算页表项地址 `0x80205010`。

实际读取的 PTE 为 `0x200000cf`。该页表项具有有效及读写执行权限标志，是合法的根级叶子页表项，因此能够直接完成 1 GiB 超级页地址映射，不需要继续查询后两级页表。

对于本次选择的虚拟地址 `0x80200038`，QEMU 最终计算出物理地址 `0x80200038`。`get_physical_address()` 返回 `TRANSLATE_SUCCESS`，随后 `riscv_cpu_tlb_fill()` 通过 `tlb_set_page()` 执行软件 TLB 填充，并最终返回 `true`。

实验结果验证了当前启动页表的低地址恒等映射配置能够正确工作，同时也说明：

1. Sv39 采用三级页表，但允许通过高层叶子页表项提前完成地址转换。

2. TLB Miss 不等于 Page Fault，未命中 TLB 后仍可通过页表查询获得有效映射。

3. 页表项中的有效位、访问权限位和物理页号共同决定地址转换的结果。

4. QEMU 通过软件 TLB 模拟地址转换缓存，其具体实现可以通过宿主机 GDB 进行源码级跟踪。

通过本次实验，将 RISC\-V Sv39 页表的理论结构与 QEMU 的实际执行流程建立了对应关系，进一步理解了虚拟内存机制、页表遍历、超级页以及 TLB 在操作系统内存管理中的作用。



---

### 

## 五、测试与验证



### 5\.1测试结果汇总

### 5\.32测试截图

图 1：编译及运行输出

![image\.png](图片和附件/image%202.png)

图2：First\-Fit 测试结果

![图1\.png](图片和附件/图1.png)

图 3：Best\-Fit 测试结果

![保存\.png](图片和附件/保存.png)

图 4：Buddy System 测试结果

![image\.png](图片和附件/image.png)





---

## 六、实验总结与收获

### 6\.1 对操作系统的理解

每位成员填写自己负责模块的内容。

OS 原理中很重要，但本实验未涉及或未完整实现的知识点：

### 6\.2 AI 协作开发的经验











