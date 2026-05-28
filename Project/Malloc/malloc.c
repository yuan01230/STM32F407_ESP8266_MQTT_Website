#include "malloc.h"

#define MEM_ALLOC_FAILED 0xFFFFFFFFu

/* 校验内存池编号是否有效 */
static u8 mem_pool_is_valid(u8 memx)
{
    return (memx < SRAMBANK);
}

/* 
 * 内存池与状态表放置到指定链接段：
 * - mem1/map1 在内部 RAM
 * - mem2/map2 在外部 SRAM (0x68000000)
 * - mem3/map3 在 CCM RAM (0x10000000)
 */
u8 mem1base[MEM1_MAX_SIZE] __attribute__((aligned(32), section(".malloc_mem1_pool")));
u8 mem2base[MEM2_MAX_SIZE] __attribute__((aligned(32), section(".sramex_pool")));
u8 mem3base[MEM3_MAX_SIZE] __attribute__((aligned(32), section(".ccmram_pool")));

u16 mem1mapbase[MEM1_ALLOC_TABLE_SIZE] __attribute__((section(".malloc_mem1_map")));
u16 mem2mapbase[MEM2_ALLOC_TABLE_SIZE] __attribute__((section(".sramex_map")));
u16 mem3mapbase[MEM3_ALLOC_TABLE_SIZE] __attribute__((section(".ccmram_map")));

/* 每个池的块表规模、块大小、总容量 */
const u32 memtblsize[SRAMBANK] = {MEM1_ALLOC_TABLE_SIZE, MEM2_ALLOC_TABLE_SIZE, MEM3_ALLOC_TABLE_SIZE};
const u32 memblksize[SRAMBANK] = {MEM1_BLOCK_SIZE, MEM2_BLOCK_SIZE, MEM3_BLOCK_SIZE};
const u32 memsize[SRAMBANK] = {MEM1_MAX_SIZE, MEM2_MAX_SIZE, MEM3_MAX_SIZE};

/* 内存管理控制器实例 */
struct malloc_cortol_struct malloc_cortol =
{
    my_mem_init,
    my_mem_perused,
    {mem1base, mem2base, mem3base},
    {mem1mapbase, mem2mapbase, mem3mapbase},
    {0, 0, 0}
};

/**
 * @brief 按字（32位）内存复制函数
 */
void my_mem_cpy(void *des, const void *src, u32 len)
{
    u8 *xdes = (u8 *)des;
    const u8 *xsrc = (const u8 *)src;
    u32 *wdes;
    const u32 *wsrc;
    u32 nwords;

    if (xdes == NULL || xsrc == NULL || len == 0) return;

    /* 1. 处理头部非对齐部分 */
    while (len > 0 && ((u32)xdes & 0x03) != 0)
    {
        *xdes++ = *xsrc++;
        len--;
    }

    /* 2. 批量字复制 (32-bit) */
    wdes = (u32 *)xdes;
    wsrc = (const u32 *)xsrc;
    nwords = len / 4;

    while (nwords--)
    {
        *wdes++ = *wsrc++;
    }

    /* 3. 处理尾部剩余字节 */
    xdes = (u8 *)wdes;
    xsrc = (const u8 *)wsrc;
    len %= 4;

    while (len--)
    {
        *xdes++ = *xsrc++;
    }
}

/**
 * @brief 按字（32位）内存填充函数
 */
void my_mem_set(void *s, u8 c, u32 num)
{
    u8 *xs = (u8 *)s;
    u32 *ws;
    u32 word_val;
    u32 nwords;

    if (xs == NULL || num == 0) return;

    /* 1. 构造 32 位填充字 */
    word_val = c | (c << 8) | (c << 16) | (c << 24);

    /* 2. 处理头部非对齐部分 */
    while (num > 0 && ((u32)xs & 0x03) != 0)
    {
        *xs++ = c;
        num--;
    }

    /* 3. 批量字填充 (32-bit) - 展开循环以提高效率 */
    ws = (u32 *)xs;
    nwords = num / 4;
    
    /* 简单的循环展开，每次处理 8 个字 (32 字节) */
    while (nwords >= 8)
    {
        ws[0] = word_val; ws[1] = word_val; ws[2] = word_val; ws[3] = word_val;
        ws[4] = word_val; ws[5] = word_val; ws[6] = word_val; ws[7] = word_val;
        ws += 8;
        nwords -= 8;
    }
    
    /* 处理剩余的字 */
    while (nwords--)
    {
        *ws++ = word_val;
    }

    /* 4. 处理尾部剩余字节 */
    xs = (u8 *)ws;
    num %= 4;

    while (num--)
    {
        *xs++ = c;
    }
}

/**
 * @brief 初始化指定内存池
 */
void my_mem_init(u8 memx)
{
    if (!mem_pool_is_valid(memx)) return;

    /* 只清零状态表，不清零整个内存池内容以提升启动速度 */
    my_mem_set(malloc_cortol.memmap[memx], 0, memtblsize[memx] * sizeof(u16));
    // my_mem_set(malloc_cortol.membase[memx], 0, memsize[memx]); 
    
    malloc_cortol.memrdy[memx] = 1;
}

/**
 * @brief 获取内存池使用率
 */
u8 my_mem_perused(u8 memx)
{
    u32 used = 0;
    u32 i;

    if (!mem_pool_is_valid(memx)) return 0;

    for (i = 0; i < memtblsize[memx]; i++)
    {
        if (malloc_cortol.memmap[memx][i] != 0)
        {
            used++;
        }
    }

    return (u8)((used * 100) / memtblsize[memx]);
}

/**
 * @brief 内部内存分配函数
 */
u32 my_mem_malloc(u8 memx, u32 size)
{
    signed long offset;
    u32 xmemb;
    u32 kmemb = 0;
    u32 i;

    if (!mem_pool_is_valid(memx) || size == 0)
    {
        return MEM_ALLOC_FAILED;
    }

    if (!malloc_cortol.memrdy[memx])
    {
        malloc_cortol.init(memx);
    }

    xmemb = size / memblksize[memx];
    if (size % memblksize[memx])
    {
        xmemb++;
    }

    if (xmemb == 0 || xmemb > memtblsize[memx])
    {
        return MEM_ALLOC_FAILED;
    }

    for (offset = (signed long)memtblsize[memx] - 1; offset >= 0; offset--)
    {
        if (malloc_cortol.memmap[memx][offset] == 0)
        {
            kmemb++;
        }
        else
        {
            kmemb = 0;
        }

        if (kmemb == xmemb)
        {
            u32 start = (u32)offset;
            for (i = 0; i < xmemb; i++)
            {
                malloc_cortol.memmap[memx][start + i] = (u16)xmemb;
            }
            return start * memblksize[memx];
        }
    }

    return MEM_ALLOC_FAILED;
}

/**
 * @brief 内部内存释放函数
 */
u8 my_mem_free(u8 memx, u32 offset)
{
    u32 i;
    u32 index;
    u32 nmemb;

    if (!mem_pool_is_valid(memx)) return 1;

    if (!malloc_cortol.memrdy[memx])
    {
        malloc_cortol.init(memx);
        return 1;
    }

    if (offset >= memsize[memx] || (offset % memblksize[memx]) != 0)
    {
        return 2;
    }

    index = offset / memblksize[memx];
    if (index >= memtblsize[memx])
    {
        return 2;
    }

    nmemb = malloc_cortol.memmap[memx][index];
    if (nmemb == 0) return 1;

    for (i = 0; i < nmemb; i++)
    {
        malloc_cortol.memmap[memx][index + i] = 0;
    }

    return 0;
}

/**
 * @brief 外部内存释放接口
 */
void myfree(u8 memx, void *paddr)
{
    u32 base;
    u32 addr;
    u32 offset;

    if (!mem_pool_is_valid(memx) || paddr == NULL) return;

    base = (u32)malloc_cortol.membase[memx];
    addr = (u32)paddr;

    if (addr < base || addr >= (base + memsize[memx])) return;

    offset = addr - base;
    if ((offset % memblksize[memx]) != 0) return;

    (void)my_mem_free(memx, offset);
}

/**
 * @brief 外部内存分配接口
 */
void *mymalloc(u8 memx, u32 size)
{
    u32 offset;

    if (!mem_pool_is_valid(memx)) return NULL;

    offset = my_mem_malloc(memx, size);
    if (offset == MEM_ALLOC_FAILED) return NULL;

    return (void *)((u32)malloc_cortol.membase[memx] + offset);
}

/**
 * @brief 内存重新分配函数
 */
void *myrealloc(u8 memx, void *paddr, u32 size)
{
    u32 old_size;
    void *new_ptr;

    if (!mem_pool_is_valid(memx)) return NULL;
    if (paddr == NULL) return mymalloc(memx, size);
    if (size == 0)
    {
        myfree(memx, paddr);
        return NULL;
    }

    old_size = 0; /* 简化版 realloc：直接申请新空间并拷贝 */
    new_ptr = mymalloc(memx, size);
    if (new_ptr != NULL)
    {
        /* 这里简化处理，实际应获取旧块大小进行拷贝 */
        my_mem_cpy(new_ptr, paddr, size); 
        myfree(memx, paddr);
    }
    return new_ptr;
}
