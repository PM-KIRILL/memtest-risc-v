/* Расширение шаблонов для memtest86
 *
 * Генерирует шаблоны для расширения BadRAM ядра Linux, которое позволяет
 * избежать выделения неисправных страниц.
 *
 * Выпущено под лицензией GNU Public License версии 2.
 *
 * Автор: Rick van Rein, vanrein@zonnet.nl
 * ----------------------------------------------------
 * Специфичный код для MemTest86+ V1.60 (GPL V2.0)
 * Автор: Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.x86-secret.com - http://www.memtest.org 
 */


#include "test.h"


/*
 * DEFAULT_MASK охватывает длинное слово (longword), так как это является шагом тестирования.
 */
#define DEFAULT_MASK ((~0L) << 2)


/* extern struct vars *v; */


/* Что делает этот код:
 *  - Отслеживает количество шаблонов BadRAM в массиве;
 *  - По возможности объединяет с ними новые неисправные адреса;
 *  - Поддерживает маски максимально избирательными, минимизируя результирующие ошибки;
 *  - Выводит новый шаблон только при изменении массива шаблонов.
 */

#define COMBINE_MASK(a,b,c,d) ((a & b & c & d) | (~a & b & ~c & d))

/* Объединяет две пары адрес/маска в одну пару адрес/маска.
 */
void combine (ulong adr1, ulong mask1, ulong adr2, ulong mask2,
		ulong *adr, ulong *mask) {

	*mask = COMBINE_MASK (adr1, mask1, adr2, mask2);

	*adr  = adr1 | adr2;
	*adr &= *mask;	// Нормализация, фундаментальной необходимости в этом нет
}

/* Подсчитывает количество адресов, охватываемых маской.
 */
ulong addresses (ulong mask) {
	ulong ctr=1;
	int i=32;
	while (i-- > 0) {
		if (! (mask & 1)) {
			ctr += ctr;
		}
		mask >>= 1;
	}
	return ctr;
}

/* Подсчитывает, сколько еще адресов будет охвачено adr1/mask1 при объединении
 * с adr2/mask2.
 */
ulong combicost (ulong adr1, ulong mask1, ulong adr2, ulong mask2) {
	ulong cost1=addresses (mask1);
	ulong tmp, mask;
	combine (adr1, mask1, adr2, mask2, &tmp, &mask);
	return addresses (mask) - cost1;
}

/* Находит индекс массива с наименьшей стоимостью для расширения заданной парой адрес/маска.
 * Возвращает -1, если не найдено ничего ниже заданной минимальной стоимости.
 */
int cheapindex (ulong adr1, ulong mask1, ulong mincost) {
	int i=v->numpatn;
	int idx=-1;
	while (i-- > 0) {
		ulong tmpcost=combicost(v->patn[i].adr, v->patn[i].mask, adr1, mask1);
		if (tmpcost < mincost) {
			mincost=tmpcost;
			idx=i;
		}
	}
	return idx;
}

/* Пытается найти индекс перемещения для idx, если это ничего не стоит.
 * Возвращает -1, если такого индекса не существует.
 */
int relocateidx (int idx) {
	ulong adr =v->patn[idx].adr;
	ulong mask=v->patn[idx].mask;
	int new;
	v->patn[idx].adr ^= ~0L;	// Никогда не выбирать idx
	new=cheapindex (adr, mask, 1+addresses (mask));
	v->patn[idx].adr = adr;
	return new;
}

/* Перемещает заданный индекс idx только если это бесплатно.
 * Это полезно для объединения с «соседними» секциями для интеграции.
 * Основано на принципе аллокатора Buddy в ядре Linux.
 */
void relocateiffree (int idx) {
	int newidx=relocateidx (idx);
	if (newidx>=0) {
		ulong cadr, cmask;
		combine (v->patn [newidx].adr, v->patn[newidx].mask,
		         v->patn [   idx].adr, v->patn[   idx].mask,
			 &cadr, &cmask);
		v->patn[newidx].adr =cadr;
		v->patn[newidx].mask=cmask;
		if (idx < --v->numpatn) {
			v->patn[idx].adr =v->patn[v->numpatn].adr;
			v->patn[idx].mask=v->patn[v->numpatn].mask;
		}
		relocateiffree (newidx);
	}
}

/* Вставляет один неисправный адрес в массив шаблонов.
 * Возвращает 1 только если массив был изменен.
 */
int insertaddress (ulong adr) {
	if (cheapindex (adr, DEFAULT_MASK, 1L) != -1)
		return 0;

	if (v->numpatn < BADRAM_MAXPATNS) {
		v->patn[v->numpatn].adr =adr;
		v->patn[v->numpatn].mask=DEFAULT_MASK;
		v->numpatn++;
		relocateiffree (v->numpatn-1);
	} else {
		int idx=cheapindex (adr, DEFAULT_MASK, ~0L);
		ulong cadr, cmask;
		combine (v->patn [idx].adr, v->patn[idx].mask,
		         adr, DEFAULT_MASK, &cadr, &cmask);
		v->patn[idx].adr =cadr;
		v->patn[idx].mask=cmask;
		relocateiffree (idx);
	}
	return 1;
}
