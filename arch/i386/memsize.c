/* memsize.c - MemTest-86  Версия 3.3
 *
 * Выпущено на условиях версии 2 лицензии Gnu Public License.
 * Автор: Крис Брэди (Chris Brady)
 */

#include "arch.h"
#include "test.h"
#include "config.h"

short e820_nr;
short memsz_mode = SZ_MODE_BIOS;

static ulong alt_mem_k;
static ulong ext_mem_k;
static struct e820entry e820[E820MAX];

ulong p1, p2;
ulong *p;

static void sort_pmap(void);
//static void memsize_bios(void);
static void memsize_820(void);
static void memsize_801(void);
static int sanitize_e820_map(struct e820entry *orig_map,
struct e820entry *new_bios, short old_nr);
static void memsize_linuxbios();

/*
 * Определить количество имеющейся памяти.
 */
void mem_size(void)
{
	int i, flag=0;
	v->test_pages = 0;

	/* Получить размер памяти из BIOS */
        /* Определить карту памяти */
	if (query_linuxbios()) {
		flag = 1;
	} else if (query_pcbios()) {
		flag = 2;
	}

	/* Только при первом проходе */
	/* Сделать копию таблицы информации о памяти, чтобы мы могли заново оценить */
	/* карту памяти позже */
	if (e820_nr == 0 && alt_mem_k == 0 && ext_mem_k == 0) {
		ext_mem_k = mem_info.e88_mem_k;
		alt_mem_k = mem_info.e801_mem_k;
		e820_nr   = mem_info.e820_nr;
		for (i=0; i< mem_info.e820_nr; i++) {
			e820[i].addr = mem_info.e820[i].addr;
			e820[i].size = mem_info.e820[i].size;
			e820[i].type = mem_info.e820[i].type;
		}
	}
	if (flag == 1) {
		memsize_linuxbios();
	} else if (flag == 2) {
		memsize_820();
	}

	/* Гарантировать, что записи pmap расположены в порядке возрастания */
	sort_pmap();
	v->plim_lower = 0;
	v->plim_upper = v->pmap[v->msegs-1].end;

	adj_mem();
}

static void sort_pmap(void)
{
	int i, j;
	/* Выполнить сортировку вставками для pmap; на уже отсортированном
	 * списке это должен быть алгоритм O(1).
	 */
	for(i = 0; i < v->msegs; i++) {
		/* Найти место для вставки текущего элемента */
		for(j = i -1; j >= 0; j--) {
			if (v->pmap[i].start > v->pmap[j].start) {
				j++;
				break;
			}
		}
		/* Вставить текущий элемент */
		if (i != j) {
			struct pmap temp;
			temp = v->pmap[i];
			memmove(&v->pmap[j], &v->pmap[j+1], 
				(i -j)* sizeof(temp));
			v->pmap[j] = temp;
		}
	}
}
static void memsize_linuxbios(void)
{
	int i, n;
	/* Построить карту памяти для тестирования */
	n = 0;
	for (i=0; i < e820_nr; i++) {
		unsigned long long end;

		if (e820[i].type != E820_RAM) {
			continue;
		}
		end = e820[i].addr;
		end += e820[i].size;
		v->pmap[n].start = (e820[i].addr + 4095) >> 12;
		v->pmap[n].end = end >> 12;
		v->test_pages += v->pmap[n].end - v->pmap[n].start;
		n++;
	}
	v->msegs = n;
}
static void memsize_820()
{
	int i, n, nr;
	struct e820entry nm[E820MAX];
	unsigned long long start;
	unsigned long long end;

	/* Очистить, скорректировать и скопировать предоставленную BIOS карту E820. */
	nr = sanitize_e820_map(e820, nm, e820_nr);

	/* Если нет подходящей карты 820, использовать информацию BIOS 801/88 */
	if (nr < 1 || nr > E820MAX) {
		memsize_801();
		return;
	}

	/* Построить карту памяти для тестирования */
	n = 0;
	for (i=0; i<nr; i++) {
		if (nm[i].type == E820_RAM || nm[i].type == E820_ACPI) {
			start = nm[i].addr;
			end = start + nm[i].size;

			/* Никогда не использовать память между 640 и 1024 КБ */
			if (start > RES_START && start < RES_END) {
				if (end < RES_END) {
					continue;
				}
				start = RES_END;
			}
			if (end > RES_START && end < RES_END) {
				end = RES_START;
			}
			v->pmap[n].start = (start + 4095) >> 12;
			v->pmap[n].end = end >> 12;
			v->test_pages += v->pmap[n].end - v->pmap[n].start;
			n++;
#if 0			
	 		int epmap = 0;
	 		int lpmap = 0;
	 		if(n > 12) { epmap = 34; lpmap = -12; }
			hprint (11+n+lpmap,0+epmap,v->pmap[n-1].start);
			hprint (11+n+lpmap,10+epmap,v->pmap[n-1].end);
			hprint (11+n+lpmap,20+epmap,v->pmap[n-1].end - v->pmap[n-1].start);
			dprint (11+n+lpmap,30+epmap,nm[i].type,0,0);	
#endif				
		}
	}
	v->msegs = n;
}
	
static void memsize_801(void)
{
	ulong mem_size;

	/* сравнить результаты методов 88 и 801 и выбрать большее значение */
	/* Эти размеры указаны для расширенной памяти в единицах по 1 КБ. */

	if (alt_mem_k < ext_mem_k) {
		mem_size = ext_mem_k;
	} else {
		mem_size = alt_mem_k;
	}
	/* Сначала отображаем первые 640 КБ */
	v->pmap[0].start = 0;
	v->pmap[0].end = RES_START >> 12;
	v->test_pages = RES_START >> 12;

	/* Теперь расширенная память */
	v->pmap[1].start = (RES_END + 4095) >> 12;
	v->pmap[1].end = (mem_size + 1024) >> 2;
	v->test_pages += mem_size >> 2;
	v->msegs = 2;
}

/*
 * Очистить (нормализовать) карту BIOS e820.
 *
 * Некоторые ответы e820 содержат перекрывающиеся записи. Следующий код 
 * заменяет исходную карту e820 новой, удаляя перекрытия.
 *
 */
static int sanitize_e820_map(struct e820entry *orig_map, struct e820entry *new_bios,
	short old_nr)
{
	struct change_member {
		struct e820entry *pbios; /* указатель на исходную запись BIOS */
		unsigned long long addr; /* адрес для этой точки изменения */
	};
	struct change_member change_point_list[2*E820MAX];
	struct change_member *change_point[2*E820MAX];
	struct e820entry *overlap_list[E820MAX];
	struct e820entry biosmap[E820MAX];
	struct change_member *change_tmp;
	ulong current_type, last_type;
	unsigned long long last_addr;
	int chgidx, still_changing;
	int overlap_entries;
	int new_bios_entry;
	int i;

	/*
		Визуально мы выполняем следующее (1,2,3,4 = типы памяти)...
		Пример карты памяти (с перекрытиями):
		   ____22__________________
		   ______________________4_
		   ____1111________________
		   _44_____________________
		   11111111________________
		   ____________________33__
		   ___________44___________
		   __________33333_________
		   ______________22________
		   ___________________2222_
		   _________111111111______
		   _____________________11_
		   _________________4______

		Очищенный эквивалент (без перекрытий):
		   1_______________________
		   _44_____________________
		   ___1____________________
		   ____22__________________
		   ______11________________
		   _________1______________
		   __________3_____________
		   ___________44___________
		   _____________33_________
		   _______________2________
		   ________________1_______
		   _________________4______
		   ___________________2____
		   ____________________33__
		   ______________________4_
	*/
	/* Сначала сделать копию карты */
	for (i=0; i<old_nr; i++) {
		biosmap[i].addr = orig_map[i].addr;
		biosmap[i].size = orig_map[i].size;
		biosmap[i].type = orig_map[i].type;
	}

	/* выйти, если в карте BIOS обнаружены недопустимые адреса */
	for (i=0; i<old_nr; i++) {
		if (biosmap[i].addr + biosmap[i].size < biosmap[i].addr)
			return 0;
	}

	/* создать указатели для начальной информации о точках изменения (для сортировки) */
	for (i=0; i < 2*old_nr; i++)
		change_point[i] = &change_point_list[i];

	/* записать все известные точки изменения (начальные и конечные адреса) */
	chgidx = 0;
	for (i=0; i < old_nr; i++)	{
		change_point[chgidx]->addr = biosmap[i].addr;
		change_point[chgidx++]->pbios = &biosmap[i];
		change_point[chgidx]->addr = biosmap[i].addr + biosmap[i].size;
		change_point[chgidx++]->pbios = &biosmap[i];
	}

	/* отсортировать список точек изменения по адресам памяти (от низких к высоким) */
	still_changing = 1;
	while (still_changing)	{
		still_changing = 0;
		for (i=1; i < 2*old_nr; i++)  {
			/* если <текущий_адрес> < <предыдущий_адрес>, поменять местами */
			/* или, если текущий=<начальный_адрес> и предыдущий=<конечный_адрес>, поменять местами */
			if ((change_point[i]->addr < change_point[i-1]->addr) ||
				((change_point[i]->addr == change_point[i-1]->addr) &&
				 (change_point[i]->addr == change_point[i]->pbios->addr) &&
				 (change_point[i-1]->addr != change_point[i-1]->pbios->addr))
			   )
			{
				change_tmp = change_point[i];
				change_point[i] = change_point[i-1];
				change_point[i-1] = change_tmp;
				still_changing=1;
			}
		}
	}

	/* создать новую карту памяти BIOS, удалив перекрытия */
	overlap_entries=0;	 /* количество записей в таблице перекрытий */
	new_bios_entry=0;	 /* индекс для создания новых записей карты BIOS */
	last_type = 0;		 /* начать с неопределенного типа памяти */
	last_addr = 0;		 /* начать с 0 в качестве последнего начального адреса */
	/* цикл по точкам изменения, определяющий влияние на новую карту BIOS */
	for (chgidx=0; chgidx < 2*old_nr; chgidx++)
	{
		/* отслеживать все перекрывающиеся записи BIOS */
		if (change_point[chgidx]->addr == change_point[chgidx]->pbios->addr)
		{
			/* добавить запись карты в список перекрытий (> 1 записи означает перекрытие) */
			overlap_list[overlap_entries++]=change_point[chgidx]->pbios;
		}
		else
		{
			/* удалить запись из списка (независимо от порядка, поэтому меняем местами с последней) */
			for (i=0; i<overlap_entries; i++)
			{
				if (overlap_list[i] == change_point[chgidx]->pbios)
					overlap_list[i] = overlap_list[overlap_entries-1];
			}
			overlap_entries--;
		}
		/* если есть перекрывающиеся записи, решить, какой «тип» использовать */
		/* (большее значение имеет приоритет — 1=используемая, 2,3,4,4+=неиспользуемая) */
		current_type = 0;
		for (i=0; i<overlap_entries; i++)
			if (overlap_list[i]->type > current_type)
				current_type = overlap_list[i]->type;
		/* продолжить построение новой карты BIOS на основе этой информации */
		if (current_type != last_type)	{
			if (last_type != 0)	 {
				new_bios[new_bios_entry].size =
					change_point[chgidx]->addr - last_addr;
				/* продвигаться вперед, только если новый размер был ненулевым */
				if (new_bios[new_bios_entry].size != 0)
					if (++new_bios_entry >= E820MAX)
						break; 	/* больше нет места для новых записей BIOS */
			}
			if (current_type != 0)	{
				new_bios[new_bios_entry].addr = change_point[chgidx]->addr;
				new_bios[new_bios_entry].type = current_type;
				last_addr=change_point[chgidx]->addr;
			}
			last_type = current_type;
		}
	}
	return(new_bios_entry);
}
