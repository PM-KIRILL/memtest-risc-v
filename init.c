/*
 * Специфичный код MemTest86+ V5 (GPL V2.0)
 * Автор: Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 * ------------------------------------------------
 * init.c - MemTest-86  Версия 3.6
 *
 * Выпущено под версией 2 лицензии Gnu Public License.
 * Автор: Chris Brady
 */
 

#include <stdint.h>
#include <stddef.h>
#include "arch.h"
#include "test.h"
#include "config.h"
#include "smp.h"
#include "io.h"
#include "spd.h"
#include "pci.h"
#include "controller.h"

extern void initialise_cpus();

/* Здесь мы храним все данные cpuid */
extern struct cpu_ident cpu_id;

int l1_cache=0, l2_cache=0, l3_cache=0;
ulong extclock;

int beepmode;

/* Функция безопасного режима (failsafe) */
/* msec: количество мс для ожидания - scs: ожидаемый скан-код для остановки */
/* биты: 0 = расширенное обнаружение - 1: SMP - 2: Проверка температуры */
/* 3: MP SMP - 4-7: ЗАРЕЗЕРВИРОВАНО */
void failsafe(int msec, int scs)
{
	int i;
	uint64_t deadline;
	unsigned char c;
	volatile char *pp;
	
	for(i=0, pp=(char *)(SCREEN_ADR+(18*160)+(18*2)+1); i<40; i++, pp+=2) {
		*pp = 0x1E;
	}	
	for(i=0, pp=(char *)(SCREEN_ADR+(18*160)+(18*2)+1); i<3; i++, pp+=2) {
		*pp = 0x9E;
	}	
	for(i=0, pp=(char *)(SCREEN_ADR+(18*160)+(55*2)+1); i<3; i++, pp+=2) {
		*pp = 0x9E;
	}	
	
	cprint(18, 18, "==> Press F1 to enter Fail-Safe Mode <==");
	
	if(v->fail_safe & 2)
	{
	cprint(19, 15, "==> Press F2 to force Multi-Threading (SMP) <==");				
	}

	deadline = RDTSC() + ((uint64_t)v->clks_msec) * msec;

	/* цикл на n секунд */
	while (1) {
		/* Время вышло? */
		if (RDTSC() >= deadline) { break;	}
		
		/* Нажат ли ожидаемый скан-код? */
		c = get_key();
		c &= 0x7f;
		
		/* F1 */
		if(c == scs) { v->fail_safe |= 1;	break; }
					
		/* F2 */
		if(c == scs+1) 
		{ 
			v->fail_safe ^= 2;
			break;

		}
		
		/* F3 */
		if(c == scs+2) 
		{ 
			if(v->fail_safe & 2) { v->fail_safe ^= 2; }
			v->fail_safe |= 8;
			break;
		}				
			
	}
	
	cprint(18, 18, "                                          ");
	cprint(19, 15, "                                                ");

	for(i=0, pp=(char *)(SCREEN_ADR+(18*160)+(18*2)+1); i<40; i++, pp+=2) {
		*pp = 0x17;
	}		
}



static void display_init(void)
{
	int i;
	volatile char *pp;

	serial_echo_init();
  serial_echo_print(" [LINE_SCROLL;24r"); /* Установить область прокрутки, строки 7-23 */
  serial_echo_print(" [H [2J");   /* Очистить экран */
  serial_echo_print(" [37m [44m");
  serial_echo_print(" [0m");
  serial_echo_print(" [37m [44m");

	/* Очистить экран и установить синий фон */
	for(i=0, pp=(char *)(SCREEN_ADR); i<80*24; i++) {
		*pp++ = ' ';
		*pp++ = 0x17;
	}

	/* Сделать фон названия зеленым */
	for(i=0, pp=(char *)(SCREEN_ADR+1); i<TITLE_WIDTH; i++, pp+=2) {
		*pp = 0x20;
	}
	cprint(0, 0, "      Memtest86  5.01        ");

	/* Установить мигающий "+" */
	for(i=0, pp=(char *)(SCREEN_ADR+1); i<2; i++, pp+=30) {
		*pp = 0xA4;
	}
	cprint(0, 15, "+");

	/* Инвертировать цвета для нижней строки дисплея */
	for(i=0, pp=(char *)(SCREEN_ADR+1+(24 * 160)); i<80; i++, pp+=2) {
		*pp = 0x71;
	}
   serial_echo_print(" [0m");
}

/*
 * Инициализировать тест, настроить экран и выяснить объем памяти.
 */
void init(void)
{
	int i;

	arch_init_early();

	display_init();

	cprint(5, 60, "| Time:   0:00:00");
	cprint(1, COL_MID,"Pass   %");
	cprint(2, COL_MID,"Test   %");
	cprint(3, COL_MID,"Test #");
	cprint(4, COL_MID,"Testing: ");
	cprint(5, COL_MID,"Pattern: ");
	cprint(1, 0, "CLK:           (32b Mode)");
	cprint(2, 0, "L1 Cache: Unknown ");
	cprint(3, 0, "L2 Cache: Unknown ");
  cprint(4, 0, "L3 Cache:  None    ");
  cprint(5, 0, "Memory  :         ");
  cprint(6, 0, "------------------------------------------------------------------------------");
	cprint(7, 0, "Core#:");
	cprint(8, 0, "State:");
	cprint(9, 0, "Cores:    Active /    Total (Run: All) | Pass:       0        Errors:      0  ");
	cprint(10, 0, "------------------------------------------------------------------------------");

	/*	
	for(i=0, pp=(char *)(SCREEN_ADR+(5*160)+(53*2)+1); i<20; i++, pp+=2) {
		*pp = 0x92;
	}

	for(i=0, pp=(char *)(SCREEN_ADR+0*160+1); i<80; i++, pp+=2) {
		*pp = 0x47;
	}
	*/
	
	cprint(7, 39, "| Chipset : Unknown");
	cprint(8, 39, "| Memory Type : Unknown");
	

	for(i=0; i < 6; i++) {
		cprint(i, COL_MID-2, "| ");
	}
	
	footer();

  aprint(5, 10, v->test_pages);

  v->pass = 0;
  v->msg_line = 0;
  v->ecount = 0;
  v->ecc_ecount = 0;
	v->testsel = -1;
	v->msg_line = LINE_SCROLL-1;
	v->scroll_start = v->msg_line * 160;
	v->erri.low_addr.page = 0x7fffffff;
	v->erri.low_addr.offset = 0xfff;
	v->erri.high_addr.page = 0;
	v->erri.high_addr.offset = 0;
	v->erri.min_bits = 32;
	v->erri.max_bits = 0;
	v->erri.min_bits = 32;
	v->erri.max_bits = 0;
	v->erri.maxl = 0;
	v->erri.cor_err = 0;
	v->erri.ebits = 0;
	v->erri.hdr_flag = 0;
	v->erri.tbits = 0;
	for (i=0; tseq[i].msg != NULL; i++) {
		tseq[i].errors = 0;
	}
	if (dmi_initialized) {
		for (i=0; i < MAX_DMI_MEMDEVS; i++){
			if (dmi_err_cnts[i] > 0) {
				dmi_err_cnts[i] = 0;
			}
		}
	}
	
	arch_init();

	/* Проверка безопасного режима */
	failsafe(5000, 0x3B);

	/* Инициализировать SMP */
	initialise_cpus();
	
	for (i = 0; i <num_cpus; i++) {
		dprint(7, i+7, i%10, 1, 0);
		cprint(8, i+7, "S");
	}

	dprint(9, 19, num_cpus, 2, 0);
	
	if((v->fail_safe & 3) == 2)
	{
			cprint(LINE_CPU,9, "(SMP: Disabled)");
			cprint(LINE_RAM,9, "Running...");
	}
	// dprint(10, 5, found_cpus, 2, 0); 

	/* Найти характеристики памяти */
	if(v->fail_safe & 1) 
		{ 	
			cprint(LINE_CPU, COL_SPEC, " **** FAIL SAFE **** FAIL SAFE **** ");
			cprint(LINE_RAM, COL_SPEC, "   No detection, same reliability   ");
		} else {
			find_controller();
			get_spd_spec();
			if(num_cpus <= 16 && !(v->fail_safe & 4)) { coretemp(); }
		}
	
	if(v->check_temp > 0 && !(v->fail_safe & 4))
	{
		cprint(LINE_CPU, 26, "|  CPU Temp");
		cprint(LINE_CPU+1, 26, "|      
	}
		beep(600);
		beep(1000);
	
	/* Записать время начала */
	v->startt = RDTSC();
	v->snapt = v->startt;
	if (l1_cache == 0) { l1_cache = 64; }
	if (l2_cache == 0) { l1_cache = 512; }
	v->printmode=PRINTMODE_ADDRESSES;
	v->numpatn=0;
}


#define STEST_ADDR 0x100000	/* Измерить скорость памяти, начиная с 1МБ */

/* Измерить и отобразить размеры и скорости процессора и кэша */
void cpu_cache_speed()
{
	int i, off = 4;
	ulong speed;


	/* Вывести скорость процессора */
	if ((speed = cpuspeed()) > 0) {
		if (speed < 999499) {
			speed += 50; /* для округления */
			cprint(1, off, "    . MHz");
			dprint(1, off+1, speed/1000, 3, 1);
			dprint(1, off+5, (speed/100)%10, 1, 0);
		} else {
			speed += 500; /* для округления */
			cprint(1, off, "      MHz");
			dprint(1, off, speed/1000, 5, 0);
		}
		extclock = speed;
	}

	/* Вывести информацию о кэше L1 */
	/* Для измерения скорости кэша L1 мы используем размер блока, равный 1/4 */
	/* от общего размера кэша L1, так как половина его предназначена для инструкций */
	if (l1_cache) {
		cprint(2, 0, "L1 Cache:     K  ");
		dprint(2, 11, l1_cache, 3, 0);
		if ((speed=memspeed(STEST_ADDR, (l1_cache/2)*1024, 200))) {
			cprint(2, 16, "       MB/s");
			dprint(2, 16, speed, 6, 0);
		}
	}

	/* Вывести информацию о кэше L2 */
	/* Мы измеряем скорость кэша L2, используя размер блока, который */
	/* равен размеру кэша L1. Нам приходится подстраиваться, если кэш L1 */
	/* больше, чем кэш L2 */
	if (l2_cache) {
		cprint(3, 0, "L2 Cache:     K  ");
		dprint(3, 10, l2_cache, 4, 0);

		if (l2_cache < l1_cache) {
			i = l1_cache / 4 + l2_cache / 4;
		} else {
			i = l1_cache;
		}
		if ((speed=memspeed(STEST_ADDR, i*1024, 200))) {
			cprint(3, 16, "       MB/s");
			dprint(3, 16, speed, 6, 0);
		}
	}
	/* Вывести информацию о кэше L3 */
	/* Мы измеряем скорость кэша L3, используя размер блока, который */
	/* в 2 раза больше размера кэша L2. */

	if (l3_cache) 
	{
		cprint(4, 0, "L3 Cache:     K  ");
   	aprint(4, 10, l3_cache/4);
    //dprint(4, 10, l3_cache, 4, 0);
    
    		i = l2_cache*2;
    
    		if ((speed=memspeed(STEST_ADDR, i*1024, 150))) {
    			cprint(4, 16, "       MB/s");
    			dprint(4, 16, speed, 6, 0);
    		}
   }
}

/* Измерить и отобразить скорость памяти, многопоточно используя все процессоры */
ulong spd[MAX_CPUS];
void get_mem_speed(int me, int ncpus)
{
	int i;
	ulong speed=0;

   /* Определить скорость памяти. Чтобы найти скорость памяти, мы используем 
   * размер блока, который является суммой всех кэшей L1, L2 и L3
	 * во всех процессорах, умноженный на 6 */
   i = (l3_cache + l2_cache + l1_cache) * 4;

	/* Убедиться, что памяти достаточно для проведения теста */
	/* Если нет, использовать всё, что есть */
	if ((1 + (i * 2)) > (v->plim_upper << 2)) {
		i = ((v->plim_upper <<2) - 1) / 2;
	}
	
	speed = memspeed(STEST_ADDR, i * 1024, 100);
	cprint(5, 16, "       MB/s");
	dprint(5, 16, speed, 6, 0);
	
}