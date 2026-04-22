#ifndef _MALLOC_H_
#define _MALLOC_H_

#include <stdint.h>
#include <stddef.h>

/* 类型定义 */
#ifndef u8
#define u8  uint8_t
#endif
#ifndef u16
#define u16 uint16_t
#endif
#ifndef u32
#define u32 uint32_t
#endif

#ifndef NULL
#define NULL 0
#endif

/* 内存池编号 */
#define SRAMIN   0   /* 内部 SRAM */
#define SRAMEX   1   /* 外部 SRAM (FSMC) */
#define SRAMCCM  2   /* CCM SRAM，仅 CPU 可访问 */

#define SRAMBANK 3   /* 支持的内存池总数 */

/* mem1：内部 SRAM 配置 */
#define MEM1_BLOCK_SIZE         32
#define MEM1_MAX_SIZE           (100 * 1024)
#define MEM1_ALLOC_TABLE_SIZE   (MEM1_MAX_SIZE / MEM1_BLOCK_SIZE)

/* mem2：外部 SRAM 配置 */
#define MEM2_BLOCK_SIZE         32
#define MEM2_MAX_SIZE           (960 * 1024)
#define MEM2_ALLOC_TABLE_SIZE   (MEM2_MAX_SIZE / MEM2_BLOCK_SIZE)

/* mem3：CCM SRAM 配置 */
#define MEM3_BLOCK_SIZE         32
#define MEM3_MAX_SIZE           (60 * 1024)
#define MEM3_ALLOC_TABLE_SIZE   (MEM3_MAX_SIZE / MEM3_BLOCK_SIZE)

/* 内存管理控制器结构体 */
struct malloc_cortol_struct
{
    void (*init)(u8);           /* 内存池初始化函数指针 */
    u8 (*perused)(u8);          /* 获取内存使用率函数指针 */
    u8 *membase[SRAMBANK];      /* 内存池起始地址数组 */
    u16 *memmap[SRAMBANK];      /* 内存块状态表数组 */
    u8 memrdy[SRAMBANK];        /* 内存池就绪标志数组 */
};

extern struct malloc_cortol_struct malloc_cortol;

/* 基础接口（内部调用） */
void my_mem_set(void *s, u8 c, u32 num);
void my_mem_cpy(void *des, const void *src, u32 len);
void my_mem_init(u8 memx);
u32 my_mem_malloc(u8 memx, u32 size);
u8 my_mem_free(u8 memx, u32 offset);
u8 my_mem_perused(u8 memx);

/* 用户接口（外部调用） */
void myfree(u8 memx, void *paddr);
void *mymalloc(u8 memx, u32 size);
void *myrealloc(u8 memx, void *paddr, u32 size);

#endif
