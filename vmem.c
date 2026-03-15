/* vmem.c - MemTest-86 
 *
 * Обработка виртуальной памяти (PAE)
 *
 * Выпущено под лицензией GNU Public License версии 2.
 * Автор: Крис Брейди (Chris Brady)
 */

#include <stdint.h>

#include "test.h"

#if HAS_FLAT_MEM
int map_page(unsigned long page)
{
	return 0;
}

void *mapping(unsigned long page_addr)
{
	return (void *)(page_addr << 12);
}

unsigned long page_of(void *addr)
{
	return ((uintptr_t)addr) >> 12;
}
#endif

void *emapping(unsigned long page_addr)
{
	void *result;
	result = mapping(page_addr -1);
	/* Заполнение младших битов адреса */
	result = ((unsigned char *)result) + 0xffc;
	return result;
}