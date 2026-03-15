/*
 * Специфичный код MemTest86+ V5 (GPL V2.0)
 * Автор: Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 * ------------------------------------------------
 * main.c - MemTest-86  Версия 3.5
 *
 * Выпущено под лицензией GNU Public License версии 2.
 * Автор: Chris Brady
 */

#include <stddef.h>
#include "arch.h"
#include "test.h"
#include "smp.h"
#include "config.h"
#undef TEST_TIMES
#define DEFTESTS 9
#define FIRST_DIVISER 3

/* Основной стек выделяется во время загрузки. Размер стека предпочтительно 
 * должен быть кратен размеру страницы (4 КБ).
*/


static int	find_ticks_for_test(int test);
void		find_ticks_for_pass(void);
int		find_chunks(int test);
static void	test_setup(void);
static int	compute_segments(struct pmap map, int cpu);
int		do_test(int ord);

bool	        reloc_pending = FALSE;
uint8_t volatile stacks[MAX_CPUS][STACKSIZE];
int 		nticks;
int 		test_ticks;
ulong 		high_test_adr;
volatile static struct pmap winx;  	/* Структура окна для отображения окон */


/* Функция трассировки загрузки */
short tidx = 25;
void btrace(int me, int line, char *msg, int wait, long v1, long v2)
{
	int y, x;

	/* Включена ли трассировка? */
	if (btflag == 0) return;

	spin_lock(&barr->mutex);
	y = tidx%13;
	x = tidx/13*40;
	cplace(y+11, x+1, ' ');
	if (++tidx > 25) {
		tidx = 0;
	}
	y = tidx%13;
	x = tidx/13*40;

	cplace(y+11, x+1, '>');
	dprint(y+11, x+2, me, 2, 0);
	dprint(y+11, x+5, line, 4, 0);
	cprint(y+11, x+10, msg);
	hprint(y+11, x+22, v1);
	hprint(y+11, x+31, v2);
	if (wait) {
		wait_keyup();
	}
	spin_unlock(&barr->mutex);
}

/* Переместить тест по новому адресу. Будьте осторожны, чтобы избежать перекрытия! */
void run_at(unsigned long addr, int cpu)
{
	ulong *ja = (ulong *)(addr + startup_32 - _start);

	/* Процессор 0, Копирование кода memtest86+ */
	if (cpu == 0) {
		memmove((void *)addr, &_start, _end - _start);
	}

	/* Ожидание завершения копирования */
	barrier();

	/* Мы используем блокировку, чтобы гарантировать, что только один процессор 
	 * за раз переходит к новому коду. Некоторые операции запуска не являются потокобезопасными! */
  spin_lock(&barr->mutex);   

	/* Переход по начальному адресу */
	goto *ja;
}

/* Переключение с загрузочного стека на основной. Сначала выделяется основной стек, 
 * затем копируется содержимое загрузочного стека, после чего ESP настраивается 
 * на указание на новый стек.
 */
void
switch_to_main_stack(unsigned cpu_num)
{
	extern uintptr_t boot_stack;
	extern uintptr_t boot_stack_top; 
	uintptr_t *src, *dst;
	int offs;
	uint8_t * stackAddr, *stackTop;
   
	stackAddr = (uint8_t *) &stacks[cpu_num][0];

	stackTop  = stackAddr + STACKSIZE;
   
	src = (uintptr_t*)&boot_stack_top;
	dst = (uintptr_t*)stackTop;
	do {
		src--; dst--;
		*dst = *src;
	} while ((uintptr_t *)src > (uintptr_t *)&boot_stack);

	offs = (uint8_t *)&boot_stack_top - stackTop;
	ADJUST_STACK(offs);
}

void reloc_internal(int cpu)
{
	/* очистка переменных */
        reloc_pending = FALSE;

	run_at(LOW_TEST_ADR, cpu);
}

void reloc(void)
{
	bail++;
        reloc_pending = TRUE;
}



void clear_screen()
{
	int i;
	char *pp;

	/* Очистка экрана и установка синего фона */
	for(i=0, pp=(char *)(SCREEN_ADR); i<80*25; i++) {
		*pp++ = ' ';
		*pp++ = 0x17;
	}
	if (btflag) {
	    cprint(1, 0, "Boot Trace Enabled");
	    cprint(1, 0, "Press any key to advance to next trace point");
	    cprint(9, 1,"CPU Line Message     Param #1 Param #2  CPU Line Message     Param #1 Param #2");
	    cprint(10,1,"--- ---- ----------- -------- --------  --- ---- ----------- -------- --------");
	}
}

void memtest_main(int my_cpu_num, int my_cpu_ord)
{
	int run;
	const ulong zeroth_window = v->plim_lower;
	/* Цикл по всем тестам */
	while (1) {
	    /* Если установлен флаг перезапуска, сбросить все начальные параметры */
	    if (restart_flag) {
		set_defaults();
		continue;
	    }
            /* Пропустить тесты для одного процессора, если используется только один процессор */
            if (tseq[test].cpu_sel == -1 && 
                    (num_cpus == 1 || cpu_mode != CPM_ALL)) {
                test++; 
                continue;
            }

	    test_setup();

	    /* Цикл по всем возможным окнам */
	    while (win_next <= ((ulong)v->pmap[v->msegs-1].end + WIN_SZ)) {

		/* Основной барьер планировщика */
		cprint(8, my_cpu_num+7, "W");
		btrace(my_cpu_num, __LINE__, "Sched_Barr", 1,window,win_next);
		barrier();

		/* Не превышать лимит PAE в 8 ТБ */
		if (win_next > MAX_MEM) {
			break;
		}

		/* Для теста затухания бит №11 мы не можем выполнить перемещение, 
		 * поэтому увеличиваем окно до 1 */
		if (tseq[test].pat == 11 && window == 0) {
			window = 1;
		}

		/* Переместить, если требуется */
		if (window != 0 && (ulong)&_start != LOW_TEST_ADR) {
			btrace(my_cpu_num, __LINE__, "Sched_RelL", 1,0,0);
			run_at(LOW_TEST_ADR, my_cpu_num);
	        }
#warning TODO
#if 0
		if (window == 0 && v->plim_lower >= zeroth_window + win0_start) {
			window++;
		}
#endif
		if (window == 0 && (ulong)&_start == LOW_TEST_ADR) {
			btrace(my_cpu_num, __LINE__, "Sched_RelH", 1,0,0);
			run_at(high_test_adr, my_cpu_num);
		}

		/* Решить, какой процессор(ы) использовать */
		btrace(my_cpu_num, __LINE__, "Sched_CPU0",1,cpu_sel,
			tseq[test].cpu_sel);
		run = 1;
		switch(cpu_mode) {
		case CPM_RROBIN:
		case CPM_SEQ:
			/* Выбрать один процессор */
			if (my_cpu_ord == cpu_sel) {
				mstr_cpu = cpu_sel;
				run_cpus = 1;
	    		} else {
				run = 0;
			}
			break;
		case CPM_ALL:
		    /* Использовать все процессоры */
		    if (tseq[test].cpu_sel == -1) {
			/* Циклический перебор (Round robin) всех процессоров */
			if (my_cpu_ord == cpu_sel) {
				mstr_cpu = cpu_sel;
				run_cpus = 1;
	    		} else {
				run = 0;
			}
		    } else {
			/* Использовать количество процессоров, указанное тестом,
			 * начиная с нуля */
			if (my_cpu_ord >= tseq[test].cpu_sel) {
				run = 0;
			}
			/* Установить ведущий процессор на наибольший номер выбранного процессора */
			if (act_cpus < tseq[test].cpu_sel) {
				mstr_cpu = act_cpus-1;
				run_cpus = act_cpus;
			} else {
				mstr_cpu = tseq[test].cpu_sel-1;
				run_cpus = tseq[test].cpu_sel;
			}
		    }
		}
		btrace(my_cpu_num, __LINE__, "Sched_CPU1",1,run_cpus,run);
		barrier();
		dprint(9, 7, run_cpus, 2, 0);

		/* Настроить дополнительный барьер только для выбранных процессоров */
		if (my_cpu_ord == mstr_cpu) {
			s_barrier_init(run_cpus);
		}

		/* Убедиться, что дополнительный барьер готов, прежде чем продолжить */
		barrier();

		/* Невыбранные процессоры возвращаются к барьеру планировщика */
		if (run == 0 ) {
			continue;
		}
		cprint(8, my_cpu_num+7, "-");
		btrace(my_cpu_num, __LINE__, "Sched_Win0",1,window,win_next);

		/* Нужно ли выйти? */
		if(reloc_pending) {
		    reloc_internal(my_cpu_num);
	 	}

		if (my_cpu_ord == mstr_cpu) {
		    switch (window) {
		    /* Особый случай для перемещения */
		    case 0:
			winx.start = 0;
			winx.end = win1_end - zeroth_window;
			window++;
			break;
		    /* Особый случай для первого сегмента */
		    case 1:
			winx.start = win0_start;
			winx.end = WIN_SZ;
			win_next += WIN_SZ;
			window++;
			break;
		    /* Для всех остальных окон */
		    default:
			winx.start = win_next;
			win_next += WIN_SZ;
			winx.end = win_next;
		    }
		    btrace(my_cpu_num,__LINE__,"Sched_Win1",1,winx.start,
				winx.end);

	            /* Найти области памяти для тестирования */
	            segs = compute_segments(winx, my_cpu_num);
		}
		s_barrier();
		btrace(my_cpu_num,__LINE__,"Sched_Win2",1,segs,
			v->map[0].pbase_addr);

	        if (segs == 0) {
		/* В этом окне нет памяти, поэтому пропускаем его */
		    continue;
	        }

		/* отображение в окне... */
		if (map_page(v->map[0].pbase_addr) < 0) {
		    /* Либо PAE отсутствует, либо достигнут предел PAE */
		    break;
		}

		btrace(my_cpu_num, __LINE__, "Strt_Test ",1,my_cpu_num,
			my_cpu_ord);
		do_test(my_cpu_ord);
		btrace(my_cpu_num, __LINE__, "End_Test  ",1,my_cpu_num,
			my_cpu_ord);

            	paging_off();

	    } /* Конец цикла окон */

	    s_barrier();
	    btrace(my_cpu_num, __LINE__, "End_Win   ",1,test, window);

	    /* Подготовка к следующему набору окон */
	    win_next = 0;
	    window = 0;
	    bail = 0;

	    /* Только ведущий процессор выполняет завершающие действия теста */
	    if (my_cpu_ord != mstr_cpu) {
		continue;
	    }

	    /* Специальная обработка для теста затухания бит №11 */
	    if (tseq[test].pat == 11 && bitf_seq != 6) {
		/* Продолжать, пока последовательность не будет завершена. */
		bitf_seq++;
		continue;
	    } else {
		bitf_seq = 0;
	    }

	    /* Выбор перехода процессоров и следующего теста */
	    switch(cpu_mode) {
	    case CPM_RROBIN:
		if (++cpu_sel >= act_cpus) {
		    cpu_sel = 0;
		}
		next_test();
		break;
	    case CPM_SEQ:
		if (++cpu_sel >= act_cpus) {
		    cpu_sel = 0;
		    next_test();
		}
		break;
	    case CPM_ALL:
	      if (tseq[test].cpu_sel == -1) 
	      	{
			    /* Выполнить тот же тест для каждого процессора */
			    if (++cpu_sel >= act_cpus) 
			    	{
	            cpu_sel = 0;
			        next_test();
			    	} else {
			        continue;
			    	}
	        } else {
		    		next_test();
					}
	    } //????
	    btrace(my_cpu_num, __LINE__, "Next_CPU  ",1,cpu_sel,test);

	    /* Если это был последний тест, значит проход завершен */
	  if (pass_flag) 
	  	{
			pass_flag = 0;
			
			v->pass++;
			
			dprint(LINE_INFO, 49, v->pass, 5, 0);
			find_ticks_for_pass();
			ltest = -1;
			
			if (v->ecount == 0) 
				{
			    /* Если включен режим одного прохода и ошибок не было,
			     * перезагрузиться для выхода из теста */
			    if (onepass) {	reboot();   }
			    if (!btflag) cprint(LINE_MSG, COL_MSG-8, "** Pass complete, no errors, press Esc to exit **");
					if(BEEP_END_NO_ERROR) 
						{
							beep(1000);
							beep(2000);
							beep(1000);
							beep(2000);
						}
				}
	    }

	    bail=0;
	} /* Конец цикла тестов */
}


static void test_setup()
{
	static int ltest = -1;

	/* Проверить, выбран ли конкретный тест */
	if (v->testsel >= 0) {
                test = v->testsel;
        }

	/* Выполнять настройку только если это новый тест */
	if (test == ltest) {
		return;
	}
	ltest = test;

	/* Теперь настроить параметры теста на основе текущего номера теста */
	if (v->pass == 0) {
		/* Уменьшить количество итераций для первого прохода */
		c_iter = tseq[test].iter/FIRST_DIVISER;
	} else {
		c_iter = tseq[test].iter;
	}

	/* Установить количество итераций. На первом проходе выполняется 
	 * только половина итераций */
	//dprint(LINE_INFO, 28, c_iter, 3, 0);
	test_ticks = find_ticks_for_test(test);
	nticks = 0;
	v->tptr = 0;

	cprint(LINE_PAT, COL_PAT, "            ");
	cprint(LINE_PAT, COL_PAT-3, "   ");
	dprint(LINE_TST, COL_MID+6, tseq[test].pat, 2, 1);
	cprint(LINE_TST, COL_MID+9, tseq[test].msg);
	cprint(2, COL_MID+8, "                                         ");
}

int do_test(int my_ord)
{
	int i=0, j=0;
	unsigned long p0=0, p1=0;

	if (my_ord == mstr_cpu) {
	  if ((ulong)&_start > LOW_TEST_ADR) {
		/* Перемещено, поэтому необходимо протестировать всю выбранную нижнюю память */
		v->map[0].start = mapping(v->plim_lower);
		
#ifdef __i386__
		/* Старый добрый Legacy USB_WAR */
		if (v->map[0].start < (ulong*)0x500) 
		{
    	v->map[0].start = (ulong*)0x500;
		}
#else
#warning TODO зарезервированные области
#endif

		cprint(LINE_PAT, COL_MID+25, " R");
	    } else {
		cprint(LINE_PAT, COL_MID+25, "  ");
	    }

	    /* Обновить отображение тестируемых сегментов памяти */
	    p0 = page_of(v->map[0].start);
	    p1 = page_of(v->map[segs-1].end);
	    aprint(LINE_RANGE, COL_MID+9, p0);
	    cprint(LINE_RANGE, COL_MID+14, " - ");
	    aprint(LINE_RANGE, COL_MID+17, p1);
	    aprint(LINE_RANGE, COL_MID+25, p1-p0);
	    cprint(LINE_RANGE, COL_MID+30, " of ");
	    aprint(LINE_RANGE, COL_MID+34, v->selected_pages);
	}

	return invoke_test(my_ord);
}

/* Вычислить количество тестируемых фрагментов размера SPINSZ */
int find_chunks(int tst) 
{
	int i, j, sg, wmax, ch;
	struct pmap twin={0,0};
	unsigned long wnxt = WIN_SZ;
	unsigned long len;

	wmax = MAX_MEM/WIN_SZ+2;  /* Количество сегментов по 2 ГБ + 2 */
	/* Вычислить количество сегментов памяти размера SPINSZ */
	ch = 0;
	for(j = 0; j < wmax; j++) {
		/* особый случай для перемещения */
		if (j == 0) {
			twin.start = 0;
			twin.end = win1_end;
		}

		/* особый случай для первых 2 ГБ */
		if (j == 1) {
			twin.start = win0_start;
			twin.end = WIN_SZ;
		}

		/* Для всех остальных окон */
		if (j > 1) {
			twin.start = wnxt;
			wnxt += WIN_SZ;
			twin.end = wnxt;
		}

	        /* Найти области памяти, которые будут тестироваться */
		sg = compute_segments(twin, -1);
		for(i = 0; i < sg; i++) {
			len = v->map[i].end - v->map[i].start;

			if (cpu_mode == CPM_ALL && num_cpus > 1) {
				switch(tseq[tst].pat) {
				case 2:
				case 4:
				case 5:
				case 6:
				case 9:
				case 10:
				    len /= act_cpus;
				    break;
				case 7:
				case 8:
				    len /= act_cpus;
				    break;
				}
			}
			ch += (len + SPINSZ -1)/SPINSZ;
		}
	}
	return(ch);
}

/* Вычислить общее количество тиков за проход */
void find_ticks_for_pass(void)
{
	int i;

	v->pptr = 0;
	v->pass_ticks = 0;
	v->total_ticks = 0;
	cprint(1, COL_MID+8, "                                         ");
	i = 0;
	while (tseq[i].cpu_sel != 0) {
		/* Пропустить тесты 2 и 4, если используется 1 процессор */
		if (act_cpus == 1 && (i == 2 || i == 4)) { 
		    i++;
		    continue;
		}
		v->pass_ticks += find_ticks_for_test(i);
		i++;
	}
}

static int find_ticks_for_test(int tst)
{
	int ticks=0, c, ch;

	if (tseq[tst].sel == 0) {
		return(0);
	}

	/* Определить количество фрагментов для этого теста */
	ch = find_chunks(tst);

	/* Установить количество итераций. На первом проходе выполняется 
	 * только 1/2 итераций */
	if (v->pass == 0) {
		c = tseq[tst].iter/FIRST_DIVISER;
	} else {
		c = tseq[tst].iter;
	}

	switch(tseq[tst].pat) {
	case 0: /* Тест адреса, "бегущая единица" */
		ticks = 2;
		break;
	case 1: /* Тест адреса, собственный адрес */
	case 2:
		ticks = 2;
		break;
	case 3: /* Инверсии со сдвигом, все единицы и нули */
	case 4:
		ticks = 2 + 4 * c;
		break;
	case 5: /* Инверсии со сдвигом, 8-битные "бегущие единицы и нули" */
		ticks = 24 + 24 * c;
		break;
	case 6: /* Случайные данные */
		ticks = c + 4 * c;
		break;
	case 7: /* Перемещение блока */
		ticks = (ch + ch/act_cpus + c*ch);
		break;
	case 8: /* Инверсии со сдвигом, 32-битный паттерн сдвига */
		ticks = (1 + c * 2) * 64;
		break;
	case 9: /* Последовательность случайных данных */
		ticks = 3 * c;
		break;
	case 10: /* Проверка по модулю 20, случайный паттерн */
		ticks = 4 * 40 * c;
		break;
	case 11: /* Тест затухания бит */
		ticks = c * 2 + 4 * ch;
		break;
	case 90: /* Проверка по модулю 20, все единицы и нули (не используется) */
		ticks = (2 + c) * 40;
		break;
	case 91: /* Проверка по модулю 20, 8-битный паттерн (не используется) */
		ticks = (2 + c) * 40 * 8;
		break;
	}
	if (cpu_mode == CPM_SEQ || tseq[tst].cpu_sel == -1) {
		ticks *= act_cpus;
	}
	if (tseq[tst].pat == 7 || tseq[tst].pat == 11) {
		return ticks;
	}
	return ticks*ch;
}

static int compute_segments(struct pmap win, int me)
{
	unsigned long wstart, wend;
	int i, sg;

	/* Вычислить окно, в котором тестируется память */
	wstart = win.start + v->plim_lower;
	wend = win.end + v->plim_lower;
	sg = 0;

	/* Теперь сузить окно до области памяти, которую нужно протестировать */
	if (wstart < v->plim_lower) {
		wstart = v->plim_lower;
	}
	if (wend > v->plim_upper) {
		wend = v->plim_upper;
	}
	if (wstart >= wend) {
		return(0);
	}
	/* Список тестируемых сегментов */
	for (i=0; i< v->msegs; i++) {
		unsigned long start, end;
		start = v->pmap[i].start;
		end = v->pmap[i].end;
		if (start <= wstart) {
			start = wstart;
		}
		if (end >= wend) {
			end = wend;
		}
#if 0
		cprint(LINE_SCROLL+(2*i), 0, " (");
		hprint(LINE_SCROLL+(2*i), 2, start);
		cprint(LINE_SCROLL+(2*i), 10, ", ");
		hprint(LINE_SCROLL+(2*i), 12, end);
		cprint(LINE_SCROLL+(2*i), 20, ") ");

		cprint(LINE_SCROLL+(2*i), 22, "r(");
		hprint(LINE_SCROLL+(2*i), 24, wstart);
		cprint(LINE_SCROLL+(2*i), 32, ", ");
		hprint(LINE_SCROLL+(2*i), 34, wend);
		cprint(LINE_SCROLL+(2*i), 42, ") ");

		cprint(LINE_SCROLL+(2*i), 44, "p(");
		hprint(LINE_SCROLL+(2*i), 46, v->plim_lower);
		cprint(LINE_SCROLL+(2*i), 54, ", ");
		hprint(LINE_SCROLL+(2*i), 56, v->plim_upper);
		cprint(LINE_SCROLL+(2*i), 64, ") ");

		cprint(LINE_SCROLL+(2*i+1),  0, "w(");
		hprint(LINE_SCROLL+(2*i+1),  2, win.start);
		cprint(LINE_SCROLL+(2*i+1), 10, ", ");
		hprint(LINE_SCROLL+(2*i+1), 12, win.end);
		cprint(LINE_SCROLL+(2*i+1), 20, ") ");

		cprint(LINE_SCROLL+(2*i+1), 22, "m(");
		hprint(LINE_SCROLL+(2*i+1), 24, v->pmap[i].start);
		cprint(LINE_SCROLL+(2*i+1), 32, ", ");
		hprint(LINE_SCROLL+(2*i+1), 34, v->pmap[i].end);
		cprint(LINE_SCROLL+(2*i+1), 42, ") ");

		cprint(LINE_SCROLL+(2*i+1), 44, "i=");
		hprint(LINE_SCROLL+(2*i+1), 46, i);
		
		cprint(LINE_SCROLL+(2*i+2), 0, 
			"                                        "
			"                                        ");
		cprint(LINE_SCROLL+(2*i+3), 0, 
			"                                        "
			"                                        ");
#endif
		if ((start < end) && (start < wend) && (end > wstart)) {
			v->map[sg].pbase_addr = start;
			v->map[sg].start = mapping(start);
			v->map[sg].end = emapping(end);
#if 0
		hprint(LINE_SCROLL+(sg+1), 0, sg);
		hprint(LINE_SCROLL+(sg+1), 12, v->map[sg].pbase_addr);
		hprint(LINE_SCROLL+(sg+1), 22, start);
		hprint(LINE_SCROLL+(sg+1), 32, end);
		hprint(LINE_SCROLL+(sg+1), 42, mapping(start));
		hprint(LINE_SCROLL+(sg+1), 52, emapping(end));
		cprint(LINE_SCROLL+(sg+2), 0, 
			"                                        "
			"                                        ");
#endif
#if 0
		cprint(LINE_SCROLL+(2*i+1), 54, ", sg=");
		hprint(LINE_SCROLL+(2*i+1), 59, sg);
#endif
			sg++;
		}
	}
	return (sg);
}
