/*
 * cpuid.c --
 *
 *      Реализует функции запроса CPUID
 *
 */
#include <stdint.h>

#include "globals.h"
#include "cpuid.h"

struct cpu_ident cpu_id;

void get_cpuid()
{
	unsigned int *v, dummy[3];
	char *p, *q;

	/* Получение максимального стандартного значения CPUID и ID производителя */
	cpuid(0x0, &cpu_id.max_cpuid, &cpu_id.vend_id.uint32_array[0],
	    &cpu_id.vend_id.uint32_array[2], &cpu_id.vend_id.uint32_array[1]);
	cpu_id.vend_id.char_array[11] = 0;

	/* Получение информации о семействе процессора и флагов возможностей */
	if (cpu_id.max_cpuid >= 1) {
	    cpuid(0x00000001, &cpu_id.vers.flat, &cpu_id.info.flat,
		&cpu_id.fid.uint32_array[1], &cpu_id.fid.uint32_array[0]);
	}

	/* Получение битов статуса цифрового термодатчика и управления питанием */
	if(cpu_id.max_cpuid >= 6)	{
		cpuid(0x00000006, &cpu_id.dts_pmp, &dummy[0], &dummy[1], &dummy[2]);
	}
	
	/* Получение максимального расширенного значения CPUID */
	cpuid(0x80000000, &cpu_id.max_xcpuid, &dummy[0], &dummy[1], &dummy[2]);

	/* Получение расширенных флагов возможностей, сохраняем только EDX */
	if (cpu_id.max_xcpuid >= 0x80000001) {
	    cpuid(0x80000001, &dummy[0], &dummy[1],
		&dummy[2], &cpu_id.fid.uint32_array[2]);
	}

	/* Получение идентификатора бренда (Brand ID) */
	if (cpu_id.max_xcpuid >= 0x80000004) {
	    v = (unsigned int *)&cpu_id.brand_id;
	    cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
	    cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
	    cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
	    cpu_id.brand_id.char_array[47] = 0;
	}
        /*
         * Чипы Intel выравнивают эту строку по правому краю по какой-то глупой причине;
         * исправляем это недоразумение:
         */
        p = q = &cpu_id.brand_id.char_array[0];
        while (*p == ' ')
                p++;
        if (p != q) {
                while (*p)
                        *q++ = *p++;
                while (q <= &cpu_id.brand_id.char_array[48])
                        *q++ = '\0';    /* Заполнение остатка нулями */
	}

	/* Получение информации о кэше */
	switch(cpu_id.vend_id.char_array[0]) {
        case 'A':
            /* Процессоры AMD */
	    /* Информация о кэше находится только в ecx и edx, поэтому 
	     * сохраняем только эти регистры */
	    if (cpu_id.max_xcpuid >= 0x80000005) {
		cpuid(0x80000005, &dummy[0], &dummy[1],
		    &cpu_id.cache_info.uint[0], &cpu_id.cache_info.uint[1]);
	    }
	    if (cpu_id.max_xcpuid >= 0x80000006) {
		cpuid(0x80000006, &dummy[0], &dummy[1],
		    &cpu_id.cache_info.uint[2], &cpu_id.cache_info.uint[3]);
	    }
	    break;
	case 'G':
                /* Процессоры Intel, это нужно сделать в init.c */
	    break;
	}

	/* Отключение бита mon, так как ожидание цикла на основе monitor может быть ненадежным */
	cpu_id.fid.bits.mon = 0;

	rdtsc_is_available = cpu_id.fid.bits.rdtsc;
}