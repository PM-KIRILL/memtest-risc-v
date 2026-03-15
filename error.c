/* error.c - MemTest-86  Версия 4.1
 *
 * Выпущено под версией 2 лицензии Gnu Public License.
 * Крис Брейди (Chris Brady)
 */
#include <stddef.h>
#include <stdint.h>

#include "arch.h"
#include "test.h"
#include "config.h"
#include "smp.h"
#include "dmi.h"
#include "controller.h"

void poll_errors();

static void update_err_counts(void);
static void print_err_counts(void);
static void common_err();
static int syn, chan, len=1;

/*
 * Отобразить сообщение об ошибке данных. Не отображать дублирующиеся ошибки.
 */
void error(ulong *adr, ulong good, ulong bad)
{
	
	ulong xor;

	spin_lock(&barr->mutex);
	
	xor = good ^ bad;

#ifdef USB_WAR
	/* Пропускать любые ошибки, которые, по всей видимости, вызваны тем, что BIOS
	 * использует адрес 0x4e0 для поддержки USB-клавиатуры. Это часто случается
	 * с чипсетами Intel 810, 815 и 820. Существует вероятность, что мы пропустим
	 * реальную ошибку, но шансы очень малы.
	 */
	if ((ulong)adr == 0x4e0 || (ulong)adr == 0x410) {
		return;
	}
#endif

	/* В тесте №6 при включенном SMP существует спорадический баг, который
	 * сообщает о ложных срабатываниях в диапазоне < 65K-0.5MB. Мне не
	 * удалось это исправить. После расследования выяснилось, что это,
	 * по-видимому, связано с проблемой BIOS, аналогичной той, что решена в 
	 * USB_WAR, но для таблицы MP.
	 */
	/* Решено 
	if (test == 6 && (ulong)adr <= 0x07FFFF && num_cpus > 1) 
	{
	  cprint(6,78,"-"); // Отладка
		return;
	}
	*/
	
	common_err(adr, good, bad, xor, 0);
	spin_unlock(&barr->mutex);
}



/*
 * Отобразить сообщение об ошибке адреса.
 * Поскольку это строго тест адреса, попытка создать шаблоны
 * BadRAM не имеет смысла. Просто сообщить об ошибке.
 */
void ad_err1(uint32_t *adr1, uint32_t mask, uint32_t bad, uint32_t good)
{
	spin_lock(&barr->mutex);
	common_err(adr1, good, bad, (ulong)mask, 1);
	spin_unlock(&barr->mutex);
}

/*
 * Отобразить сообщение об ошибке адреса.
 * Поскольку этот тип ошибки адреса также может сообщать об ошибках данных,
 * следует продолжить и сгенерировать шаблоны BadRAM.
 */
void ad_err2(uint32_t *adr, uint32_t bad)
{
	spin_lock(&barr->mutex);
	common_err(adr, (ulong)adr, bad, ((ulong)adr) ^ bad, 0);
	spin_unlock(&barr->mutex);
}

static void update_err_counts(void)
{
	if (beepmode){
		beep(600);
		beep(1000);
	}
	
	if (v->pass && v->ecount == 0) {
		cprint(LINE_MSG, COL_MSG,
			"                                            ");
	}
	++(v->ecount);
	tseq[test].errors++;
		
}

static void print_err_counts(void)
{
	int i;
	char *pp;

	if ((v->ecount > 4096) && (v->ecount % 256 != 0)) return;

	dprint(LINE_INFO, 72, v->ecount, 6, 0);
/*
	dprint(LINE_INFO, 56, v->ecc_ecount, 6, 0);
*/

	/* Окрасить сообщения об ошибках на экране в красный цвет, чтобы наглядно */
	/* указать на то, что произошла ошибка */ 
	if ((v->printmode == PRINTMODE_ADDRESSES ||
			v->printmode == PRINTMODE_PATTERNS) &&
			v->msg_line < 24) {
		for(i=0, pp=(char *)((SCREEN_ADR+v->msg_line*160+1));
				 i<76; i++, pp+=2) {
			*pp = 0x47;
		}
	}
}

/*
 * Вывести отдельную ошибку
 */
void common_err( ulong *adr, ulong good, ulong bad, ulong xor, int type) 
{
	int i, j, n, x, flag=0;
	ulong page, offset;
	int patnchg;
	ulong mb;

	update_err_counts();

	switch(v->printmode) {
	case PRINTMODE_SUMMARY:
		/* Ничего не делать для ошибки четности. */
		if (type == 3) {
			return;
		}

		/* Ошибка адреса */
		if (type == 1) {
			xor = good ^ bad;
		}

		/* Исправляемые ошибки ECC */
		if (type == 2) {
			/* ошибочное значение является флагом исправления */
			if (bad) {
				v->erri.cor_err++;
			}
			page = (ulong)adr;
			offset = good;
		} else {
			page = page_of(adr);
			offset = (ulong)adr & 0xFFF;
		}
			
		/* Вычислить верхний и нижний адреса ошибок */
		if (v->erri.low_addr.page > page) {
			v->erri.low_addr.page = page;
			v->erri.low_addr.offset = offset;
			flag++;
		} else if (v->erri.low_addr.page == page &&
				v->erri.low_addr.offset > offset) {
			v->erri.low_addr.offset = offset;
			v->erri.high_addr.offset = offset;
			flag++;
		} else if (v->erri.high_addr.page < page) {
			v->erri.high_addr.page = page;
			flag++;
		}
		if (v->erri.high_addr.page == page &&
				v->erri.high_addr.offset < offset) {
			v->erri.high_addr.offset = offset;
			flag++;
		}

		/* Вычислить биты в ошибке */
		for (i=0, n=0; i<32; i++) {
			if (xor>>i & 1) {
				n++;
			}
		}
		v->erri.tbits += n;
		if (n > v->erri.max_bits) {
			v->erri.max_bits = n;
			flag++;
		}
		if (n < v->erri.min_bits) {
			v->erri.min_bits = n;
			flag++;
		}
		if (v->erri.ebits ^ xor) {
			flag++;
		}
		v->erri.ebits |= xor;

	 	/* Вычислить максимальное количество идущих подряд ошибок */
		len = 1;
		if ((ulong)adr == (ulong)v->erri.eadr+4 ||
				(ulong)adr == (ulong)v->erri.eadr-4 ) {
			len++;
		}
		if (len > v->erri.maxl) {
			v->erri.maxl = len;
			flag++;
		}
		v->erri.eadr = (ulong)adr;

		if (v->erri.hdr_flag == 0) {
			clear_scroll();
			cprint(LINE_HEADER+0, 1,  "Error Confidence Value:");
			cprint(LINE_HEADER+1, 1,  "  Lowest Error Address:");
			cprint(LINE_HEADER+2, 1,  " Highest Error Address:");
			cprint(LINE_HEADER+3, 1,  "    Bits in Error Mask:");
			cprint(LINE_HEADER+4, 1,  " Bits in Error - Total:");
			cprint(LINE_HEADER+4, 29,  "Min:    Max:    Avg:");
			cprint(LINE_HEADER+5, 1,  " Max Contiguous Errors:");
			x = 24;
			if (dmi_initialized) {
			  for ( i=0; i < MAX_DMI_MEMDEVS;){
			    n = LINE_HEADER+7;
			    for (j=0; j<4; j++) {
				if (dmi_err_cnts[i] >= 0) {
					dprint(n, x, i, 2, 0);
					cprint(n, x+2, ": 0");
				}
				i++;
				n++;
			    }
			    x += 10;
			  }
			}
			
			cprint(LINE_HEADER+0, 64,   "Test  Errors");
			v->erri.hdr_flag++;
		}
		if (flag) {
		  /* Вычислить биты в ошибке */
		  for (i=0, n=0; i<32; i++) {
			if (v->erri.ebits>>i & 1) {
				n++;
			}
		  }
		  page = v->erri.low_addr.page;
		  offset = v->erri.low_addr.offset;
		  mb = page >> 8;
		  hprint(LINE_HEADER+1, 25, page);
		  hprint2(LINE_HEADER+1, 33, offset, 3);
		  cprint(LINE_HEADER+1, 36, " -      . MB");
		  dprint(LINE_HEADER+1, 39, mb, 5, 0);
		  dprint(LINE_HEADER+1, 45, ((page & 0xF)*10)/16, 1, 0);
		  page = v->erri.high_addr.page;
		  offset = v->erri.high_addr.offset;
		  mb = page >> 8;
		  hprint(LINE_HEADER+2, 25, page);
		  hprint2(LINE_HEADER+2, 33, offset, 3);
		  cprint(LINE_HEADER+2, 36, " -      . MB");
		  dprint(LINE_HEADER+2, 39, mb, 5, 0);
		  dprint(LINE_HEADER+2, 45, ((page & 0xF)*10)/16, 1, 0);
		  hprint(LINE_HEADER+3, 25, v->erri.ebits);
		  dprint(LINE_HEADER+4, 25, n, 2, 1);
		  dprint(LINE_HEADER+4, 34, v->erri.min_bits, 2, 1);
		  dprint(LINE_HEADER+4, 42, v->erri.max_bits, 2, 1);
		  dprint(LINE_HEADER+4, 50, v->erri.tbits/v->ecount, 2, 1);
		  dprint(LINE_HEADER+5, 25, v->erri.maxl, 7, 1);
		  x = 28;
		  for ( i=0; i < MAX_DMI_MEMDEVS;){
		  	n = LINE_HEADER+7;
			for (j=0; j<4; j++) {
				if (dmi_err_cnts[i] > 0) {
					dprint (n, x, dmi_err_cnts[i], 7, 1);
				}
				i++;
				n++;
			}
			x += 10;
		  }
		  			
		  for (i=0; tseq[i].msg != NULL; i++) {
			dprint(LINE_HEADER+1+i, 66, i, 2, 0);
			dprint(LINE_HEADER+1+i, 68, tseq[i].errors, 8, 0);
	  	  }
		}
		if (v->erri.cor_err) {
		  dprint(LINE_HEADER+6, 25, v->erri.cor_err, 8, 1);
		}
		break;

	case PRINTMODE_ADDRESSES:
		/* Не отображать дублирующиеся ошибки */
		if ((ulong)adr == (ulong)v->erri.eadr &&
				 xor == v->erri.exor) {
			return;
		}
		if (v->erri.hdr_flag == 0) {
			clear_scroll();
			cprint(LINE_HEADER, 0,
"Tst  Pass   Failing Address          Good       Bad     Err-Bits  Count CPU");
			cprint(LINE_HEADER+1, 0,
"---  ----  -----------------------  --------  --------  --------  ----- ----");
			v->erri.hdr_flag++;
		}
		/* Проверить ввод с клавиатуры */
		check_input();
		scroll();
	
		if ( type == 2 || type == 3) {
			page = (ulong)adr;
			offset = good;
		} else {
			page = page_of(adr);
			offset = ((unsigned long)adr) & 0xFFF;
		}
		mb = page >> 8;
		dprint(v->msg_line, 0, test+1, 3, 0);
		dprint(v->msg_line, 4, v->pass, 5, 0);
		hprint(v->msg_line, 11, page);
		hprint2(v->msg_line, 19, offset, 3);
		cprint(v->msg_line, 22, " -      . MB");
		dprint(v->msg_line, 25, mb, 5, 0);
		dprint(v->msg_line, 31, ((page & 0xF)*10)/16, 1, 0);

		if (type == 3) {
			/* Ошибка ECC */
			cprint(v->msg_line, 36, 
			  bad?"corrected           ": "uncorrected         ");
			hprint2(v->msg_line, 60, syn, 4);
			cprint(v->msg_line, 68, "ECC"); 
			dprint(v->msg_line, 74, chan, 2, 0);
		} else if (type == 2) {
			cprint(v->msg_line, 36, "Parity error detected                ");
		} else {
			hprint(v->msg_line, 36, good);
			hprint(v->msg_line, 46, bad);
			hprint(v->msg_line, 56, xor);
			dprint(v->msg_line, 66, v->ecount, 5, 0);
			dprint(v->msg_line, 74, smp_my_cpu_num(), 2,1);
			v->erri.exor = xor;
		}
		v->erri.eadr = (ulong)adr;
		print_err_counts();
		break;

	case PRINTMODE_PATTERNS:
		if (v->erri.hdr_flag == 0) {
			clear_scroll();
			v->erri.hdr_flag++;
		}
		/* Не создавать шаблоны BadRAM для теста 0 или 5 */
		if (test == 0 || test == 5) {
			return;
		}
		/* Создавать шаблоны только для ошибок данных */
		if ( type != 0) {
			return;
		}
		/* Обработать адрес в управлении шаблонами */
		patnchg=insertaddress ((ulong) adr);
		if (patnchg) { 
			printpatn();
		}
		break;

	case PRINTMODE_NONE:
		if (v->erri.hdr_flag == 0) {
			clear_scroll();
			v->erri.hdr_flag++;
		}
		break;
	}
}

/*
 * Вывести ошибку ECC
 */
void print_ecc_err(unsigned long page, unsigned long offset, 
	int corrected, unsigned short syndrome, int channel)
{
	++(v->ecc_ecount);
	syn = syndrome;
	chan = channel;
	common_err((ulong *)page, offset, corrected, 0, 2);
}

#ifdef PARITY_MEM
/*
 * Вывести сообщение об ошибке четности
 */
void parity_err( unsigned long edi, unsigned long esi) 
{
	unsigned long addr;

	if (test == 5) {
		addr = esi;
	} else {
		addr = edi;
	}
	common_err((ulong *)addr, addr & 0xFFF, 0, 0, 3);
}
#endif

/*
 * Вывести массив шаблонов как опцию загрузки LILO для поддержки BadRAM.
 */
void printpatn (void)
{
       int idx=0;
       int x;

	/* Проверить ввод с клавиатуры */
	check_input();

       if (v->numpatn == 0)
               return;

       scroll();

       cprint (v->msg_line, 0, "badram=");
       x=7;

       for (idx = 0; idx < v->numpatn; idx++) {

               if (x > 80-22) {
                       scroll();
                       x=7;
               }
               cprint (v->msg_line, x, "0x");
               hprint (v->msg_line, x+2,  v->patn[idx].adr );
               cprint (v->msg_line, x+10, ",0x");
               hprint (v->msg_line, x+13, v->patn[idx].mask);
               if (idx+1 < v->numpatn)
                       cprint (v->msg_line, x+21, ",");
               x+=22;
       }
}
	
/*
 * Показать прогресс, отображая прошедшее время и обновляя графики
 */
short spin_idx[MAX_CPUS];
char spin[4] = {'|','/','-','\\'};

void do_tick(int me)
{
	int i, j, pct;
	ulong h, l, n, t;

	if (++spin_idx[me] > 3) {
		spin_idx[me] = 0;
	}
	cplace(8, me+7, spin[spin_idx[me]]);
	
	
	/* Проверить ввод с клавиатуры */
	if (me == mstr_cpu) {
		check_input();
	}
	/* Барьер здесь удерживает остальные процессоры до завершения
	 * изменений конфигурации */
	s_barrier();

	/* Только первый выбранный процессор выполняет обновление */
	if (me !=  mstr_cpu) {
		return;
	}

	/* FIXME: выводить последовательные сообщения об ошибках только из обработчика тиков */
	if (v->ecount) {
		print_err_counts();
	}
	
	nticks++;
	v->total_ticks++;

	if (test_ticks) {
		pct = 100*nticks/test_ticks;
		if (pct > 100) {
			pct = 100;
		}
	} else {
		pct = 0;
	}
	dprint(2, COL_MID+4, pct, 3, 0);
	i = (BAR_SIZE * pct) / 100;
	while (i > v->tptr) {
		if (v->tptr >= BAR_SIZE) {
			break;
		}
		cprint(2, COL_MID+9+v->tptr, "#");
		v->tptr++;
	}
	
	if (v->pass_ticks) {
		pct = 100*v->total_ticks/v->pass_ticks;
		if (pct > 100) {
			pct = 100;
		}
	} else {
		pct = 0;
        }
	dprint(1, COL_MID+4, pct, 3, 0);
	i = (BAR_SIZE * pct) / 100;
	while (i > v->pptr) {
		if (v->pptr >= BAR_SIZE) {
			break;
		}
		cprint(1, COL_MID+9+v->pptr, "#");
		v->pptr++;
	}

	if (v->ecount && v->printmode == PRINTMODE_SUMMARY) {
		/* Вычислить оценку достоверности */
		pct = 0;

		/* Если нет ошибок в пределах 1 МБ от начального и конечного адресов */
		h = v->pmap[v->msegs - 1].end - 0x100;
		if (v->erri.low_addr.page >  0x100 &&
				 v->erri.high_addr.page < h) {
			pct += 8;
		}

		/* Ошибки только для некоторых тестов */
		if (v->pass) {
			for (i=0, n=0; tseq[i].msg != NULL; i++) {
				if (tseq[i].errors == 0) {
					n++;
				}
			}
			pct += n*3;
		} else {
			for (i=0, n=0; i<test; i++) {
				if (tseq[i].errors == 0) {
					n++;
				}
			}
			pct += n*2;
			
		}

		/* Только некоторые биты в ошибке */
		n = 0;
		if (v->erri.ebits & 0xf) n++;
		if (v->erri.ebits & 0xf0) n++;
		if (v->erri.ebits & 0xf00) n++;
		if (v->erri.ebits & 0xf000) n++;
		if (v->erri.ebits & 0xf0000) n++;
		if (v->erri.ebits & 0xf00000) n++;
		if (v->erri.ebits & 0xf000000) n++;
		if (v->erri.ebits & 0xf0000000) n++;
		pct += (8-n)*2;

		/* Скорректировать оценку */
		pct = pct*100/22;
/*
		if (pct > 100) {
			pct = 100;
		}
*/
		dprint(LINE_HEADER+0, 25, pct, 3, 1);
	}
		
	/* Мы не можем вычислить прошедшее время, если инструкция rdtsc
	 * не поддерживается
	 */
	if (RDTSC_AVAILABLE()) {
		uint64_t elapsed = RDTSC() - v->startt;
		uint64_t divisor = ((uint64_t)v->clks_msec) * 1000;
		{
			// имитация деления, предполагая отсутствие переполнения
			uint64_t shift, l, r, m;
			for (shift = 0; (divisor << shift) < elapsed; shift += 1);
			l = 0;
			r = (1LLu << shift) + 1;
			// elapsed < divisor * r
			// Таким образом, бинарный поиск в [0, r)
			while (l < r - 1) {
				m = l + ((r - l) >> 1);
				if (divisor * m <= elapsed)
					l = m;
				else
					r = m;
			}
			t = l;
		}
		i = t % 60;
		j = i % 10;
	
		if(j != v->each_sec)
		{	
			
			dprint(LINE_TIME, COL_TIME+9, i % 10, 1, 0);
			dprint(LINE_TIME, COL_TIME+8, i / 10, 1, 0);
			t /= 60;
			i = t % 60;
			dprint(LINE_TIME, COL_TIME+6, i % 10, 1, 0);
			dprint(LINE_TIME, COL_TIME+5, i / 10, 1, 0);
			t /= 60;
			dprint(LINE_TIME, COL_TIME, t, 4, 0);
		
			if(v->check_temp > 0 && !(v->fail_safe & 4))
				{
					coretemp();	
				}	
			v->each_sec = j;	
		}
	
	}
	


	/* Опрос на наличие ошибок ECC */
/*
	poll_errors();
*/
}

