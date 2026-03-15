#include "arch.h"

#include "cpuid.h"

extern struct cpu_ident cpu_id;
static unsigned long mapped_win = 1;

void paging_off(void)
{
	if (!cpu_id.fid.bits.pae)
		return;
	__asm__ __volatile__ (
		/* Отключить пагинацию */
		"movl %%cr0, %%eax\n\t"
		"andl $0x7FFFFFFF, %%eax\n\t"
		"movl %%eax, %%cr0\n\t"
		: :
		: "ax"
		);
}

static void paging_on(void *pdp)
{
	if (!cpu_id.fid.bits.pae)
		return;
	__asm__ __volatile__(
		/* Загрузить адрес таблицы страниц */
		"movl %0, %%cr3\n\t"
		/* Включить пагинацию */
		"movl %%cr0, %%eax\n\t"
		"orl $0x80000000, %%eax\n\t"
		"movl %%eax, %%cr0\n\t"
		:
		: "r" (pdp)
		: "ax"
		);
}

static void paging_on_lm(void *pml)
{
	if (!cpu_id.fid.bits.pae)
		return;
	__asm__ __volatile__(
		/* Загрузить адрес таблицы страниц */
		"movl %0, %%cr3\n\t"
		/* Включить пагинацию */
		"movl %%cr0, %%eax\n\t"
		"orl $0x80000000, %%eax\n\t"
		"movl %%eax, %%cr0\n\t"
		:
		: "r" (pml)
		: "ax"
		);
}

int map_page(unsigned long page)
{
	unsigned long i;
	struct pde {
		unsigned long addr_lo;
		unsigned long addr_hi;
	};
	extern unsigned char pdp[];
	extern unsigned char pml4[];
	extern struct pde pd2[];
	unsigned long win = page >> 19;

	/* Менее 2 ГБ, поэтому маппинг не требуется */
	if (win == 0) {
		return 0;
	}
	if (cpu_id.fid.bits.pae == 0) {
		/* Ошибка: PAE не поддерживается */
		return -1;
	}
	if (cpu_id.fid.bits.lm == 0 && (page > 0x1000000)) {
		 /* Ошибка: запрашиваемый адрес выходит за пределы (> 64 ГБ)
		 *  для PAE без поддержки long mode (т.е. для 32-битного процессора).
		 */
		return -1;
	}
	/* Вычисление записей таблицы страниц... */
	for(i = 0; i < 1024; i++) {
		/*-----------------30.10.2004 12:37---------------
		 * 0xE3 --
		 * Бит 0 = Present bit.      1 = PDE присутствует
		 * Бит 1 = Read/Write.       1 = память доступна для записи
		 * Бит 2 = Supervisor/User.  0 = только супервизор (CPL 0-2)
		 * Бит 3 = Writethrough.     0 = политика кэширования writeback
		 * Бит 4 = Cache Disable.    0 = кэширование на уровне страниц включено
		 * Бит 5 = Accessed.         1 = к памяти осуществлялся доступ.
		 * Бит 6 = Dirty.            1 = в память производилась запись.
		 * Бит 7 = Page Size.        1 = размер страницы составляет 2 МБ
		 * --------------------------------------------------*/
		pd2[i].addr_lo = ((win & 1) << 31) + ((i & 0x3ff) << 21) + 0xE3;
		pd2[i].addr_hi = (win >> 1);
	}
	paging_off();
	if (cpu_id.fid.bits.lm == 1) {
		paging_on_lm(pml4);
	} else {
		paging_on(pdp);
	}
	mapped_win = win;
	return 0;
}

void *mapping(unsigned long page_addr)
{
	void *result;
	if (page_addr < 0x80000) {
		/* Если адрес меньше 1 ГБ, использовать адрес напрямую */
		result = (void *)(page_addr << 12);
	}
	else {
		unsigned long alias;
		alias = page_addr & 0x7FFFF;
		alias += 0x80000;
		result = (void *)(alias << 12);
	}
	return result;
}

unsigned long page_of(void *addr)
{
	unsigned long page;
	page = ((unsigned long)addr) >> 12;
	if (page >= 0x80000) {
		page &= 0x7FFFF;
		page += mapped_win << 19;
	}
#if 0
	cprint(LINE_SCROLL -2, 0, "page_of(        )->            ");
	hprint(LINE_SCROLL -2, 8, ((unsigned long)addr));
	hprint(LINE_SCROLL -2, 20, page);
#endif
	return page;
}