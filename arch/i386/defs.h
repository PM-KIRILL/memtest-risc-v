/* defs.h - MemTest-86 Версия 3.3
 * определения ассемблера/компилятора
 *
 * Выпущено под лицензией GNU Public License версии 2.
 * Автор: Крис Брэди
 */ 

#define HAS_SMP 1
#define HAS_FLAT_MEM 0

#define SETUPSECS	4		/* Количество секторов установки */

/*
 * Внимание!! В процессе сборки есть свои нюансы ("магия"). Прочтите
 * README.build-process, прежде чем что-либо менять.  
 * В отличие от предыдущих версий, все настройки находятся в defs.h,
 * поэтому процесс сборки должен быть более надежным.
 */
#define LOW_TEST_ADR	0x00010000		/* Конечный адрес для тестового кода */

#define BOOTSEG		0x07c0			/* Сегментный адрес для начальной загрузки */
#define INITSEG		0x9000			/* Сегментный адрес для перемещенной загрузки */
#define SETUPSEG	(INITSEG+0x20)		/* Сегментный адрес для перемещенной настройки */
#define TSTLOAD		0x1000			/* Сегментный адрес для загрузки теста */

#define KERNEL_CS	0x10			/* 32-битный сегментный адрес для кода */
#define KERNEL_DS	0x18			/* 32-битный сегментный адрес для данных */
#define REAL_CS		0x20			/* 16-битный сегментный адрес для кода */
#define REAL_DS		0x28			/* 16-битный сегментный адрес для данных */


#define E88     0x00
#define E801    0x04
#define E820NR  0x08           /* количество записей в E820MAP */
#define E820MAP 0x0c           /* наша карта */
#define E820MAX 127            /* количество записей в E820MAP */
#define E820ENTRY_SIZE 20
#define MEMINFO_SIZE (E820MAP + E820MAX * E820ENTRY_SIZE)

#define E820_RAM        1
#define E820_RESERVED   2
#define E820_ACPI       3 /* доступно как RAM после чтения таблиц ACPI */
#define E820_NVS        4


#define BARRIER_ADDR (0x9ff00)

#define RES_START	0xa0000
#define RES_END		0x100000

#define DMI_SEARCH_START  0x0000F000
#define DMI_SEARCH_LENGTH 0x000F0FFF

#define MAX_DMI_MEMDEVS 16


#define X86_FEATURE_PAE		(0*32+ 6) /* Расширение физического адреса (PAE) */
#define MAX_MEM_SEGMENTS E820MAX
