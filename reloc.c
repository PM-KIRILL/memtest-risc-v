/* reloc.c - MemTest-86  Версия 3.3
 *
 * Выпущено под лицензией Gnu Public License версии 2.
 * Автор: Эрик Бидерман (Eric Biederman)
 */

#include "arch.h"
#include "stddef.h"
#include "stdint.h"
#include "elf.h"

/* Мы используем этот макрос для обращения к типам ELF независимо от собственного размера машинного слова.
   `ElfW(TYPE)' используется вместо `Elf32_TYPE' или `Elf64_TYPE'.  */

#define ElfW(type)	_ElfW (Elf, __ELF_NATIVE_CLASS, type)
#define _ElfW(e,w,t)	_ElfW_1 (e, w, _##t)
#define _ElfW_1(e,w,t)	e##w##t
/* Мы используем этот макрос для обращения к типам ELF независимо от собственного размера машинного слова.
   `ElfW(TYPE)' используется вместо `Elf32_TYPE' или `Elf64_TYPE'.  */
#define ELFW(type)	_ElfW (ELF, __ELF_NATIVE_CLASS, type)

#ifndef assert
#define assert(expr) ((void) 0)
#endif

  /* Этот #define создает встраиваемые (inline) функции динамического связывания для
     релокации при загрузке (bootstrap) вместо релокации общего назначения.  */
#define RTLD_BOOTSTRAP

struct link_map 
{
	ElfW(Addr) l_addr;  /* Текущий адрес загрузки */
	ElfW(Addr) ll_addr; /* Последний адрес загрузки */
	ElfW(Dyn)  *l_ld;
    /* Индексированные указатели на динамическую секцию.
       [0,DT_NUM) индексируются машинно-независимыми тегами.
       [DT_NUM,DT_NUM+DT_PROCNUM) индексируются тегом минус DT_LOPROC.
       [DT_NUM+DT_PROCNUM,DT_NUM+DT_PROCNUM+DT_EXTRANUM) индексируются
       с помощью DT_EXTRATAGIDX(tagvalue) и
       [DT_NUM+DT_PROCNUM,
        DT_NUM+DT_PROCNUM+DT_EXTRANUM)
       индексируются с помощью DT_EXTRATAGIDX(tagvalue) (см. <elf.h>).  */

	ElfW(Dyn)  *l_info[DT_NUM + DT_PROCNUM + DT_EXTRANUM];
};

#include "reloc.inc.c"

/* Прочитать динамическую секцию по адресу DYN и заполнить INFO индексами DT_*.  */

static inline void __attribute__ ((unused))
elf_get_dynamic_info(ElfW(Dyn) *dyn, ElfW(Addr) l_addr,
	ElfW(Dyn) *info[DT_NUM + DT_PROCNUM + DT_EXTRANUM])
{
	if (! dyn)
		return;
	
	while (dyn->d_tag != DT_NULL)
	{
		if (dyn->d_tag < DT_NUM)
			info[dyn->d_tag] = dyn;
		else if (dyn->d_tag >= DT_LOPROC &&
			dyn->d_tag < DT_LOPROC + DT_PROCNUM)
			info[dyn->d_tag - DT_LOPROC + DT_NUM] = dyn;
		else if ((Elf32_Word) DT_EXTRATAGIDX (dyn->d_tag) < DT_EXTRANUM)
			info[DT_EXTRATAGIDX (dyn->d_tag) + DT_NUM + DT_PROCNUM
				] = dyn;
#if 0
		else
			assert (! "плохой динамический тег");
#endif
		++dyn;
	}
	
	if (info[DT_PLTGOT] != NULL) 
		info[DT_PLTGOT]->d_un.d_ptr += l_addr;
	if (info[DT_STRTAB] != NULL)
		info[DT_STRTAB]->d_un.d_ptr += l_addr;
	if (info[DT_SYMTAB] != NULL)
		info[DT_SYMTAB]->d_un.d_ptr += l_addr;
#if ! ELF_MACHINE_NO_RELA
	if (info[DT_RELA] != NULL)
	{
		assert (info[DT_RELAENT]->d_un.d_val == sizeof (ElfW(Rela)));
		info[DT_RELA]->d_un.d_ptr += l_addr;
	}
#endif
#if ! ELF_MACHINE_NO_REL
	if (info[DT_REL] != NULL)
	{
		assert (info[DT_RELENT]->d_un.d_val == sizeof (ElfW(Rel)));
		info[DT_REL]->d_un.d_ptr += l_addr;
	}
#endif
	if (info[DT_PLTREL] != NULL)
	{
#if ELF_MACHINE_NO_RELA
		assert (info[DT_PLTREL]->d_un.d_val == DT_REL);
#elif ELF_MACHINE_NO_REL
		assert (info[DT_PLTREL]->d_un.d_val == DT_RELA);
#else
		assert (info[DT_PLTREL]->d_un.d_val == DT_REL
			|| info[DT_PLTREL]->d_un.d_val == DT_RELA);
#endif
	}
	if (info[DT_JMPREL] != NULL)
		info[DT_JMPREL]->d_un.d_ptr += l_addr;
}



/* Выполнить релокации в MAP в образе запущенной программы, как указано
   в RELTAG, SZTAG. Если LAZY не равен нулю, это первый проход по PLT-
   релокациям; они должны быть настроены на вызов _dl_runtime_resolve,
   а не полностью разрешены сейчас.  */

static inline void
elf_dynamic_do_rel (struct link_map *map,
		    ElfW(Addr) reladdr, ElfW(Addr) relsize)
{
	const ElfW(Rel) *r = (const void *) reladdr;
	const ElfW(Rel) *end = (const void *) (reladdr + relsize);

	const ElfW(Sym) *const symtab =
		(const void *) map->l_info[DT_SYMTAB]->d_un.d_ptr;
	
	for (; r < end; ++r) {
		elf_machine_rel (map, r, &symtab[ELFW(R_SYM) (r->r_info)],
			(void *) (map->l_addr + r->r_offset));
	}
}

#if ! ELF_MACHINE_NO_RELA
static inline void
elf_dynamic_do_rela (struct link_map *map,
		    ElfW(Addr) relaaddr, ElfW(Addr) relasize)
{
	const ElfW(Rela) *r = (const void *) relaaddr;
	const ElfW(Rela) *end = (const void *) (relaaddr + relasize);

	const ElfW(Sym) *const symtab =
		(const void *) map->l_info[DT_SYMTAB]->d_un.d_ptr;
	
	for (; r < end; ++r) {
		elf_machine_rela (map, r, &symtab[ELFW(R_SYM) (r->r_info)],
			(void *) (map->l_addr + r->r_offset));
	}
}
#endif


void _dl_start(void)
{
	static ElfW(Addr) last_load_address = 0;
	struct link_map map;
	size_t cnt;


	/* Частично очистить структуру `map'. Не используйте `memset',
	   так как она может быть не встроенной или не инлайновой, а мы не можем
	   делать вызовы функций в этой точке.  */
	for (cnt = 0; cnt < sizeof(map.l_info) / sizeof(map.l_info[0]); ++cnt) {
		map.l_info[cnt] = 0;
	}

	/* Получить последний адрес загрузки */
	map.ll_addr = last_load_address;

	/* Определить адрес загрузки динамического линковщика в рантайме.  */
	last_load_address = map.l_addr = elf_machine_load_address();
	
	/* Прочитать нашу собственную динамическую секцию и заполнить массив info.  */
	map.l_ld = (void *)map.l_addr + elf_machine_dynamic();

	elf_get_dynamic_info (map.l_ld, map.l_addr - map.ll_addr, map.l_info);

	/* Выполнить релокацию самих себя, чтобы мы могли совершать обычные вызовы функций
	 * и осуществлять доступ к данным через глобальную таблицу смещений (GOT).  
	 */
#if !ELF_MACHINE_NO_REL
	elf_dynamic_do_rel(&map, 
		map.l_info[DT_REL]->d_un.d_ptr,
		map.l_info[DT_RELSZ]->d_un.d_val);
	if (map.l_info[DT_PLTREL]->d_un.d_val == DT_REL) {
		elf_dynamic_do_rel(&map, 
			map.l_info[DT_JMPREL]->d_un.d_ptr,
			map.l_info[DT_PLTRELSZ]->d_un.d_val);
	}
#endif

#if !ELF_MACHINE_NO_RELA
	elf_dynamic_do_rela(&map, 
		map.l_info[DT_RELA]->d_un.d_ptr,
		map.l_info[DT_RELASZ]->d_un.d_val);
	if (map.l_info[DT_PLTREL]->d_un.d_val == DT_RELA) {
		elf_dynamic_do_rela(&map, 
			map.l_info[DT_JMPREL]->d_un.d_ptr,
			map.l_info[DT_PLTRELSZ]->d_un.d_val);
	}
#endif

	/* Теперь все в порядке; мы можем вызывать функции и обращаться к глобальным данным.
	   Настроить использование возможностей операционной системы и выяснить у
	   загрузчика программ ОС, где найти таблицу заголовков программы в памяти.
	   Вынести остальную часть _dl_start в отдельную функцию — таким образом 
	   компилятор не сможет поместить обращения к GOT перед ELF_DYNAMIC_RELOCATE.  */
	return;
}