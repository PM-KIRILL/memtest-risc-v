/* test.c - MemTest-86  Версия 3.4
 *
 * Выпущено под лицензией Gnu Public License версии 2.
 * Автор: Крис Брейди (Chris Brady)
 * ----------------------------------------------------
 * Специфичный код MemTest86+ V5 (GPL V2.0)
 * Автор: Самуэль ДЕМЮЛЬМЕСТЕР (Samuel DEMEULEMEESTER), sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 * Благодарность Passmark за calculate_chunk() и различные комментарии!
 */

#include "arch.h"
#include "test.h"
#include "config.h"
#include "stdint.h"
#include "smp.h"
#include "io.h"

#define OPTIMIZED_SNIPPET static inline __attribute__((always_inline))

// Установите в 0, чтобы отключить все оптимизации
#ifndef OPTIMIZED
#   define OPTIMIZED 1
#endif

// Выборочное отключение фрагментов для проверки оптимизированного кода на соответствие обычному C
#ifndef OPTIMIZED_FIRST_SNIPPET
#   define OPTIMIZED_FIRST_SNIPPET 1
#endif
#ifndef OPTIMIZED_SECOND_SNIPPET
#   define OPTIMIZED_SECOND_SNIPPET 1
#endif
#ifndef OPTIMIZED_THIRD_SNIPPET
#   define OPTIMIZED_THIRD_SNIPPET 1
#endif

OPTIMIZED_SNIPPET void addr_tst2_snippet1(uint32_t *p, uint32_t *pe);
OPTIMIZED_SNIPPET void addr_tst2_snippet2(uint32_t *p, uint32_t *pe);
OPTIMIZED_SNIPPET void movinvr_snippet1(uint32_t *p, uint32_t *pe, unsigned me);
OPTIMIZED_SNIPPET void movinvr_snippet2(uint32_t *p, uint32_t *pe, uint32_t xorVal, unsigned me);
OPTIMIZED_SNIPPET void movinv1_snippet1(uint32_t *p, uint32_t *pe, uint32_t p1);
OPTIMIZED_SNIPPET void movinv1_snippet2(uint32_t *p, uint32_t *pe, uint32_t p1, uint32_t p2);
OPTIMIZED_SNIPPET void movinv1_snippet3(uint32_t *p, uint32_t *pe, uint32_t p1, uint32_t p2);
OPTIMIZED_SNIPPET void movinv32_snippet1(
	int *p_k, uint32_t *p_pat, // входные-выходные параметры
	uint32_t *p, uint32_t *pe, uint32_t sval, uint32_t lb // только входные параметры
);
OPTIMIZED_SNIPPET void movinv32_snippet2(
	int *p_k, uint32_t *p_pat, // входные-выходные параметры
	uint32_t *p, uint32_t *pe, uint32_t sval, uint32_t lb // только входные параметры
);
OPTIMIZED_SNIPPET void movinv32_snippet3(
	int *p_k, uint32_t *p_pat, // входные-выходные параметры
	uint32_t *p, uint32_t *pe, uint32_t p3, uint32_t hb // только входные параметры
);
OPTIMIZED_SNIPPET void modtst_snippet1(uint32_t **p_p, uint32_t *pe, uint32_t p1);
OPTIMIZED_SNIPPET void modtst_snippet2(int *p_k, uint32_t *p, uint32_t *pe, uint32_t p2, int offset);
OPTIMIZED_SNIPPET void modtst_snippet3(uint32_t **p_p,	uint32_t *pe, uint32_t p1);
OPTIMIZED_SNIPPET void block_move_snippet1(uint32_t **p_p, ulong len);
OPTIMIZED_SNIPPET void block_move_snippet2(uint32_t *p, ulong pp, ulong len);
OPTIMIZED_SNIPPET void block_move_snippet3(uint32_t **p_p, uint32_t *pe);

#include "test.inc.c"

extern volatile int    mstr_cpu;
extern volatile int    run_cpus;
extern volatile int    test;
extern volatile int segs, bail;
extern int test_ticks, nticks;
extern struct tseq tseq[];
extern void update_err_counts(void);
extern void print_err_counts(void);
void rand_seed( unsigned int seed1, unsigned int seed2, int me);
void poll_errors();

static inline ulong roundup(ulong value, ulong mask)
{
	return (value + mask) & ~mask;
}

// start / end - возвращаемые значения для диапазона тестирования
// me - номер CPU текущего потока
// j - индекс в v->map для текущего тестируемого сегмента
// align - количество байт для выравнивания каждого блока
void calculate_chunk(uint32_t** start, uint32_t** end, int me, int j, int makeMultipleOf)
{
	ulong chunk;


	// Если запущен только 1 CPU, тестируем весь блок
	if (run_cpus == 1) {
		*start = v->map[j].start;
		*end = v->map[j].end;
	} 
	else{

		// Разделяем текущий сегмент на количество CPU
		chunk = (ulong)v->map[j].end-(ulong)v->map[j].start;
		chunk /= run_cpus;
		
		// Округляем вниз до ближайшего кратного требуемой длине битов
		chunk = (chunk + (makeMultipleOf-1)) &  ~(makeMultipleOf-1);

		// Определяем границы фрагмента (чанка)
		*start = (uint32_t*)((uintptr_t)v->map[j].start+(chunk*me));
		/* Устанавливаем конечные адреса для CPU с наибольшим номером до
			* конца сегмента во избежание ошибок округления */
		// Также округляем вниз до границы, если необходимо; может быть пропущена часть RAM, 
		// но это лучше, чем крах системы или ложные ошибки.
		// Это округление, вероятно, никогда не произойдет, так как сегменты должны быть
		// выровнены по страницам 4096 байт, если я правильно понимаю.
		if (me == mstr_cpu) {
			*end = (uint32_t*)(v->map[j].end);
		} else {
			*end = (uint32_t*)((uintptr_t)(*start) + chunk);
			(*end)--;
		}
	}
}

/*
 * Тест адреса памяти, "бегущая единица"
 */
void addr_tst1(int me)
{
	int i, j, k;
	volatile uint32_t *p, *pt, *end;
	uint32_t bad, mask, p1;
	ulong bank;

	/* Тестирование глобальных битов адреса */
	for (p1=0, j=0; j<2; j++) {
        	hprint(LINE_PAT, COL_PAT, p1);

		/* Устанавливаем шаблон в нашем наименьшем кратном 0x20000 */
		p = (uint32_t *)roundup((ulong)v->map[0].start, 0x1ffff);
		*p = p1;
	
		/* Теперь записываем инверсию шаблона */
		p1 = ~p1;
		end = v->map[segs-1].end;
		for (i=0; i<100; i++) {
			mask = 4;
			do {
				pt = (uint32_t *)((ulong)p | mask);
				if (pt == p) {
					mask = mask << 1;
					continue;
				}
				if (pt >= end) {
					break;
				}
				*pt = p1;
				if ((bad = *p) != ~p1) {
					ad_err1((uint32_t *)p, mask,
						bad, ~p1);
					i = 1000;
				}
				mask = mask << 1;
			} while(mask);
		}
		do_tick(me);
		BAILR
	}

	/* Теперь проверяем биты адреса в каждом банке */
	/* Если памяти больше 8 МБ, то размер банка должен быть */
	/* больше 256 КБ. В таком случае используем 1 МБ для размера банка. */
	if (v->pmap[v->msegs - 1].end > (0x800000 >> 12)) {
		bank = 0x100000;
	} else {
		bank = 0x40000;
	}
	for (p1=0, k=0; k<2; k++) {
        	hprint(LINE_PAT, COL_PAT, p1);

		for (j=0; j<segs; j++) {
			p = v->map[j].start;
			/* Принудительно устанавливаем начальный адрес кратным 256 КБ */
			p = (uint32_t *)roundup((ulong)p, bank - 1);
			end = v->map[j].end;
			/* Избыточные проверки на переполнение */
                        while (p < end && p > v->map[j].start && p != 0) {
				*p = p1;

				p1 = ~p1;
				for (i=0; i<50; i++) {
					mask = 4;
					do {
						pt = (uint32_t *)
						    ((ulong)p | mask);
						if (pt == p) {
							mask = mask << 1;
							continue;
						}
						if (pt >= end) {
							break;
						}
						*pt = p1;
						if ((bad = *p) != ~p1) {
							ad_err1((uint32_t *)p,
							    mask,bad,~p1);
							i = 200;
						}
						mask = mask << 1;
					} while(mask);
				}
				if (p + bank > p) {
					p += bank;
				} else {
					p = end;
				}
				p1 = ~p1;
			}
		}
		do_tick(me);
		BAILR
		p1 = ~p1;
	}
}

/*
 * Тест адреса памяти, собственный адрес
 */
void addr_tst2(int me)
{
	int j, done;
	uint32_t *p, *pe, *end, *start, bad;

        cprint(LINE_PAT, COL_PAT, "address ");

	/* Записываем в каждый адрес его собственный адрес */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (uint32_t *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}

#if OPTIMIZED && HAS_OPT_ADDR_TST2 && OPTIMIZED_FIRST_SNIPPET
			addr_tst2_snippet1(p, pe);
#else
			for (; p <= pe; p++) {
				*p = (ulong)p;
			}
#endif
			p = pe + 1;
		} while (!done);
	}

	/* Каждый адрес должен содержать свой собственный адрес */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (uint32_t *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
                                pe += SPINSZ;
                        } else {
                                pe = end;
                        }
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
#if OPTIMIZED && HAS_OPT_ADDR_TST2 && OPTIMIZED_SECOND_SNIPPET
			addr_tst2_snippet2(p, pe);
#else
			for (; p <= pe; p++) {
				if((bad = *p) != (uint32_t)p) {
					ad_err2((uint32_t)p, bad);
				}
			}
#endif
			p = pe + 1;
		} while (!done);
	}
}

/*
 * Тестирование всей памяти с использованием алгоритма "половинных бегущих инверсий" 
 * с использованием случайных чисел и их дополнений в качестве шаблона данных. 
 * Поскольку мы не можем генерировать случайные числа в обратном порядке, 
 * тестирование проводится только в прямом направлении.
 */
void movinvr(int me)
{
	int i, j, done, seed1, seed2;
	uint32_t *p;
	uint32_t *pe;
	uint32_t *start,*end;
	uint32_t bad, num;

	/* Инициализация памяти начальной последовательностью случайных чисел. */
	if (RDTSC_AVAILABLE()) {
		RDTSC_LH(seed1, seed2);
	} else {
		seed1 = 521288629 + v->pass;
		seed2 = 362436069 - v->pass;
	}

	/* Отображение текущего начального значения (seed) */
        if (mstr_cpu == me) hprint(LINE_PAT, COL_PAT, seed1);
	rand_seed(seed1, seed2, me);
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);
		pe = start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
#if OPTIMIZED && HAS_OPT_MOVINVR && OPTIMIZED_FIRST_SNIPPET
			movinvr_snippet1(p, pe, me);
#else
			for (; p <= pe; p++) {
				*p = memtest_rand(me);
			}
#endif
			p = pe + 1;
		} while (!done);
	}

	/* Выполнение теста бегущих инверсий. Проверка начального шаблона, а затем 
	 * запись дополнения для каждой ячейки памяти.
	 */
	for (i=0; i<2; i++) {
		uint32_t xorVal = i ? 0xffffffffu : 0;
		rand_seed(seed1, seed2, me);
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 4);
			pe = start;
			p = start;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Проверка на переполнение */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
#if OPTIMIZED && HAS_OPT_MOVINVR && OPTIMIZED_SECOND_SNIPPET
				movinvr_snippet2(p, pe, xorVal, me);
#else
				for (; p <= pe; p++) {
					num = memtest_rand(me);
					if (i) {
						num = ~num;
					}
					if ((bad=*p) != num) {
						error((uint32_t*)p, num, bad);
					}
					*p = ~num;
				}
#endif
				p = pe + 1;
			} while (!done);
		}
	}
}

/*
 * Тестирование всей памяти с использованием алгоритма "бегущих инверсий", 
 * используя шаблон в p1 и его дополнение в p2.
 */
void movinv1 (int iter, uint32_t p1, uint32_t p2, int me)
{
	int i, j, done;
	uint32_t *p, *pe, *start, *end, bad;
	ulong len;

	/* Отображение текущего шаблона */
        if (mstr_cpu == me) hprint(LINE_PAT, COL_PAT, p1);

	/* Инициализация памяти начальным шаблоном. */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);


		pe = start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			len = pe - p + 1;
			if (p == pe ) {
				break;
			}

#if OPTIMIZED && HAS_OPT_MOVINV1 && OPTIMIZED_FIRST_SNIPPET
			movinv1_snippet1(p, pe, p1);
#else
			for (; p <= pe; p++) {
				*p = p1;
			}
#endif
			p = pe + 1;
		} while (!done);
	}

	/* Выполнение теста бегущих инверсий. Проверка начального шаблона, а затем 
	 * запись дополнения для каждой ячейки памяти. Тестирование снизу вверх, 
	 * а затем сверху вниз. */
	for (i=0; i<iter; i++) {
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 4);
			pe = start;
			p = start;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Проверка на переполнение */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}

#if OPTIMIZED && HAS_OPT_MOVINV1 && OPTIMIZED_SECOND_SNIPPET
				movinv1_snippet2(p, pe, p1, p2);
#else
				for (; p <= pe; p++) {
					if ((bad = *p) != p1) {
 						error((uint32_t*)p, p1, bad);
 					}
 					*p = p2;
				}
#endif
				p = pe + 1;
			} while (!done);
		}
		for (j=segs-1; j>=0; j--) {
		    calculate_chunk(&start, &end, me, j, 4);
			pe = end;
			p = end;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Проверка на антипереполнение (underflow) */
				if (pe - SPINSZ < pe && pe != 0) {
					pe -= SPINSZ;
				} else {
					pe = start;
					done++;
				}

				/* Поскольку мы используем беззнаковые адреса, 
				 * требуется избыточная проверка */
				if (pe < start || pe > end) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}

#if OPTIMIZED && HAS_OPT_MOVINV1 && OPTIMIZED_THIRD_SNIPPET
				movinv1_snippet3(p, pe, p1, p2);
#else
				do {
					if ((bad = *p) != p2) {
						error((uint32_t*)p, p2, bad);
					}
					*p = p1;
				} while (--p >= pe);
#endif
				p = pe - 1;
			} while (!done);
		}
	}
}

void movinv32(int iter, uint32_t p1, uint32_t lb, uint32_t hb, uint32_t sval, int off,int me)
{
	int i, j, k=0, n=0, done;
	uint32_t *p, *pe, *start, *end, pat = 0, p3, bad;

	p3 = sval << 31;
	/* Отображение текущего шаблона */
	if (mstr_cpu == me) hprint(LINE_PAT, COL_PAT, p1);

	/* Инициализация памяти начальным шаблоном. */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);
		pe = start;
		p = start;
		done = 0;
		k = off;
		pat = p1;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			/* Обработка секции памяти размером SPINSZ */
#if OPTIMIZED && HAS_OPT_MOVINV32 && OPTIMIZED_FIRST_SNIPPET
			movinv32_snippet1(&k, &pat, p, pe, sval, lb);
#else
			for ( ; p <= pe; ++p) {
				*p = pat;
				if (++k >= 32) {
					pat = lb;
					k = 0;
				} else {
					pat = pat << 1;
					pat |= sval;
				}
			}
#endif
			p = pe + 1;
		} while (!done);
	}

	/* Выполнение теста бегущих инверсий. Проверка начального шаблона, а затем 
	 * запись дополнения для каждой ячейки памяти. Тестирование снизу вверх, 
	 * а затем сверху вниз. */
	for (i=0; i<iter; i++) {
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 64);
			pe = start;
			p = start;
			done = 0;
			k = off;
			pat = p1;
			do {
				do_tick(me);
				BAILR

				/* Проверка на переполнение */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
#if OPTIMIZED && HAS_OPT_MOVINV32 && OPTIMIZED_SECOND_SNIPPET
				movinv32_snippet2(&k, &pat, p, pe, sval, lb);
#else
				for ( ; p <= pe; ++p) {
					if ((bad=*p) != pat) {
						error((uint32_t*)p, pat, bad);
					}
					*p = ~pat;

					if (++k >= 32) {
						pat = lb;
						k = 0;
					} else {
						pat = pat << 1;
						pat |= sval;
					}
				}
#endif
				p = pe + 1;
			} while (!done);
		}

                if (--k < 0) {
                        k = 31;
                }
                for (pat = lb, n = 0; n < k; n++) {
                        pat = pat << 1;
                        pat |= sval;
                }
		k++;

		for (j=segs-1; j>=0; j--) {
			calculate_chunk(&start, &end, me, j, 64);
			p = end;
			pe = end;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Проверка на антипереполнение (underflow) */
                                if (pe - SPINSZ < pe && pe != 0) {
                                        pe -= SPINSZ;
                                } else {
                                        pe = start;
					done++;
                                }
				/* Необходима избыточная проверка, так как мы 
				 * используем unsigned long для адреса.
				 */
				if (pe < start || pe > end) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}
#if OPTIMIZED && HAS_OPT_MOVINV32 && OPTIMIZED_THIRD_SNIPPET
				movinv32_snippet3(&k, &pat, p, pe, p3, hb);
#else
				for ( ; p >= pe; --p) {
					if ((bad=*p) != ~pat) {
						error((uint32_t*)p, ~pat, bad);
					}
					*p = pat;
					if (--k <= 0) {
						pat = hb;
						k = 32;
					} else {
						pat = pat >> 1;
						pat |= p3;
					}
					if (p == 0) break; // TODO: не является ли это неопределенным поведением?
				};
#endif
				p = pe - 1;
			} while (!done);
		}
	}
}

/*
 * Тестирование всей памяти с использованием шаблона доступа modulo X.
 */
void modtst(int offset, int iter, uint32_t p1, uint32_t p2, int me)
{
	int j, k, l, done;
	uint32_t *p;
	uint32_t *pe;
	uint32_t *start, *end;
	uint32_t bad;

	/* Отображение текущего шаблона */
        if (mstr_cpu == me) {
		hprint(LINE_PAT, COL_PAT-2, p1);
		cprint(LINE_PAT, COL_PAT+6, "-");
       		dprint(LINE_PAT, COL_PAT+7, offset, 2, 1);
	}

	/* Запись шаблона в каждую n-ную позицию */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);
		end -= MOD_SZ;	/* корректировка конечного адреса */
		pe = (uint32_t *)start;
		p = start+offset;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
#if OPTIMIZED && HAS_OPT_MODTST && OPTIMIZED_FIRST_SNIPPET
			modtst_snippet1(&p, pe, p1);
#else
			for (; p <= pe; p += MOD_SZ) {
				*p = p1;
			}
#endif
		} while (!done);
	}

	/* Запись в остальную память дополнения шаблона "iter" раз */
	for (l=0; l<iter; l++) {
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 4);
			pe = (uint32_t *)start;
			p = start;
			done = 0;
			k = 0;
			do {
				do_tick(me);
				BAILR

				/* Проверка на переполнение */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
#if OPTIMIZED && HAS_OPT_MODTST && OPTIMIZED_SECOND_SNIPPET
				modtst_snippet2(&k, p, pe, p2, offset);
#else
				for (; p <= pe; p++) {
					if (k != offset) {
						*p = p2;
					}
					if (++k > MOD_SZ-1) {
						k = 0;
					}
				}
#endif
				p = pe + 1;
			} while (!done);
		}
	}

	/* Теперь проверка каждой n-ной позиции */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);
		pe = (uint32_t *)start;
		p = start+offset;
		done = 0;
		end -= MOD_SZ;	/* корректировка конечного адреса */
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
#if OPTIMIZED && HAS_OPT_MODTST && OPTIMIZED_THIRD_SNIPPET
			modtst_snippet3(&p, pe, p1);
#else
			for (; p < pe; p += MOD_SZ) {
				if ((bad=*p) != p1) {
					error((uint32_t*)p, p1, bad);
				}
			}
#endif
		} while (!done);
	}
}

#if !(OPTIMIZED_FIRST_SNIPPET && OPTIMIZED_SECOND_SNIPPET && OPTIMIZED_THIRD_SNIPPET)
// неоптимизированные версии отсутствуют
#undef HAS_OPT_BLOCK_MOVE
#define HAS_OPT_BLOCK_MOVE 0
#endif

/*
 * Тестирование памяти с использованием перемещения блоков
 * Адаптировано из теста burnBX Роберта Редельмайера (Robert Redelmeier)
 */
void block_move(int iter, int me)
{
	int i, j, done;
	ulong len, pp;
	uint32_t *p, *pe;
	uint32_t *start, *end;

        cprint(LINE_PAT, COL_PAT-2, "          ");

	/* Инициализация памяти начальным шаблоном. */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);

		// конец всегда xxxxxffc, поэтому инкрементируем, чтобы расчеты длины были верными
		end = end + 1;

		pe = start;
		p = start;

		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			len  = ((ulong)pe - (ulong)p) / 64;
			//len++;
#warning
#if OPTIMIZED && HAS_OPT_BLOCK_MOVE && OPTIMIZED_FIRST_SNIPPET
			block_move_snippet1(&p, len);
#endif
		} while (!done);
	}
	s_barrier();

	/* Теперь перемещаем данные
	 * Сначала перемещаем данные вверх на половину размера тестируемого сегмента
	 * Затем перемещаем данные в исходное местоположение + 32 байта
	 */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);

		// конец всегда xxxxxffc, поэтому инкрементируем, чтобы расчеты длины были верными
		end = end + 1;
		pe = start;
		p = start;
		done = 0;

		do {

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			pp = (ulong)p + (((ulong)pe - (ulong)p) / 2); // Середина этого блока
			len  = ((ulong)pe - (ulong)p) / 8; // Половина размера этого блока в двойных словах (DWORDS)
			for(i=0; i<iter; i++) {
				do_tick(me);
				BAILR
#warning
#if OPTIMIZED && HAS_OPT_BLOCK_MOVE && OPTIMIZED_SECOND_SNIPPET
			block_move_snippet2(p, pp, len);
#endif
			}
			p = pe;
		} while (!done);
	}
	s_barrier();

	/* Теперь проверка данных
	 * Проверка ошибок довольно примитивная. Мы просто проверяем, 
	 * что соседние слова одинаковы.
	 */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);

		// конец всегда xxxxxffc, поэтому инкрементируем, чтобы расчеты длины были верными
		end = end + 1;
		pe = start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			pe-=2;	/* последние тестируемые двойные слова - это pe[0] и pe[1] */
#warning
#if OPTIMIZED && HAS_OPT_BLOCK_MOVE && OPTIMIZED_THIRD_SNIPPET
			block_move_snippet3(&p, pe);
#endif
		} while (!done);
	}
}

/*
 * Тестирование памяти на "угасание" бит, заполнение памяти шаблоном.
 */
void bit_fade_fill(uint32_t p1, int me)
{
	int j, done;
	uint32_t *p, *pe;
	uint32_t *start,*end;

	/* Отображение текущего шаблона */
	hprint(LINE_PAT, COL_PAT, p1);

	/* Инициализация памяти начальным шаблоном. */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (uint32_t *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe;) {
				*p = p1;
				p++;
			}
			p = pe + 1;
		} while (!done);
	}
}

void bit_fade_chk(uint32_t p1, int me)
{
	int j, done;
	uint32_t *p, *pe, bad;
	uint32_t *start,*end;

	/* Убеждаемся, что ничего не изменилось во время "сна" */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (uint32_t *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Проверка на переполнение */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe;) {
 				if ((bad=*p) != p1) {
					error((uint32_t*)p, p1, bad);
				}
				p++;
			}
			p = pe + 1;
		} while (!done);
	}
}




/* Сон на N секунд */
void sleep(long n, int flag, int me, int sms)
{
	uint64_t st;
	ulong t, ip=0;
	/* Сохраняем время начала */
	st = RDTSC();

	/* Цикл на n секунд */
	ulong deadline;
	if (sms) {
		deadline = st + n * v->clks_msec;
	} else {
		deadline = st + n * v->clks_msec * 1000;
	}
	while (1) {
		t = RDTSC();
		
		/* Время истекло? */
		if (t >= deadline) {
			break;
		}

		/* Отображать прошедшее время только если установлен флаг */
		if (flag == 0) {
			continue;
		}

		if (t != ip) {
			do_tick(me);
			BAILR
			ip = t;
		}
	}
}