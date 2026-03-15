#include <sys/io.h>

#include "arch.h"
#include "globals.h"
#include "smp.h"
#include "test.h"
#include "cpuid.h"

volatile int    cpu_ord=0;

/* Это точка входа теста. Мы попадаем сюда при запуске, а также при каждом 
 * перемещении (relocation). */
void test_start(void)
{
	const char *cmdline;
	int my_cpu_num, my_cpu_ord;
	/* Если это первый раз здесь, то мы — CPU 0 */
	if (start_seq == 0) {
		my_cpu_num = 0;
	} else {
		my_cpu_num = smp_my_cpu_num();
	}
	/* Первым делом переключаемся на основной стек */
	switch_to_main_stack(my_cpu_num);

	/* Инициализация при первом запуске (для этого процессора) */
	if (start_seq < 2) {

	    /* Эти шаги выполняются только загрузочным процессором (boot cpu) */
	    if (my_cpu_num == 0) {
		my_cpu_ord = cpu_ord++;
		smp_set_ordinal(my_cpu_num, my_cpu_ord);
		if (*OLD_CL_MAGIC_ADDR == OLD_CL_MAGIC) {
			unsigned short offset = *OLD_CL_OFFSET_ADDR;
			cmdline = MK_PTR(INITSEG, offset);
			parse_command_line(cmdline);
		}
		clear_screen();
		/* Инициализируем барьер, чтобы блокировка в btrace работала.
		 * Это будет переделано позже, когда мы узнаем, сколько у нас процессоров */
		barrier_init(1);
		btrace(my_cpu_num, __LINE__, "Begin     ", 1, 0, 0);
		/* Определяем размер памяти */
		mem_size();	/* должно быть вызвано перед initialise_cpus(); */
		/* Заполняем таблицу CPUID */
		get_cpuid();
		/* Запускаем остальные процессоры */
		start_seq = 1;
		//initialise_cpus();
		btrace(my_cpu_num, __LINE__, "BeforeInit", 1, 0, 0);
		/* Рисуем экран и получаем системную информацию */
	  init();

		/* Устанавливаем значения по умолчанию и инициализируем переменные */
		set_defaults();

		/* Устанавливаем базовый адрес для тестирования, 1 МБ */
		win0_start = 0x100;

		/* Устанавливаем адрес перемещения на 32 МБ, если памяти достаточно.
		 * В противном случае устанавливаем его на 3 МБ */
		/* Большой адрес релокации позволяет большее перекрытие при тестировании */
	        if ((ulong)v->pmap[v->msegs-1].end > 0x2f00) {
			high_test_adr = 0x2000000;
	        } else {
			high_test_adr = 0x300000;
		}
		win1_end = (high_test_adr >> 12);
		/* Корректируем карту, чтобы не тестировать страницу на 939 КБ,
		 * зарезервированную для блокировок */
		v->pmap[0].end--;

		find_ticks_for_pass();
       	    } else {
		/* Только для AP (Application Processors), регистрируем процессоры */
		btrace(my_cpu_num, __LINE__, "AP_Start  ", 0, my_cpu_num,
			cpu_ord);
		smp_ap_booted(my_cpu_num);
		/* Назначаем порядковый номер CPU каждому активному процессору */
		spin_lock(&barr->mutex);
		my_cpu_ord = cpu_ord++;
		smp_set_ordinal(my_cpu_num, my_cpu_ord);
		spin_unlock(&barr->mutex);
		btrace(my_cpu_num, __LINE__, "AP_Done   ", 0, my_cpu_num,
			my_cpu_ord);
	    }

	} else {
	    /* Разблокировка после перемещения */
	    spin_unlock(&barr->mutex);
	    /* Получаем порядковый номер CPU, так как он теряется при перемещении */
	    my_cpu_ord = smp_my_ord_num(my_cpu_num);
	    btrace(my_cpu_num, __LINE__, "Reloc_Done",0,my_cpu_num,my_cpu_ord);
	}

	/* Барьер для гарантии того, что все процессоры завершили запуск */
	barrier();
	btrace(my_cpu_num, __LINE__, "1st Barr  ", 1, my_cpu_num, my_cpu_ord);


	/* Настройка управления памятью и измерение скорости памяти, мы делаем это здесь,
	 * так как нам нужны все доступные процессоры */
	if (start_seq < 2) {

		/* Включаем обработку чисел с плавающей запятой */
	   if (cpu_id.fid.bits.fpu)
        	__asm__ __volatile__ (
		    "movl %%cr0, %%eax\n\t"
		    "andl $0x7, %%eax\n\t"
		    "movl %%eax, %%cr0\n\t"
                    : :
                    : "ax"
                );
	   if (cpu_id.fid.bits.sse)
        	__asm__ __volatile__ (
                    "movl %%cr4, %%eax\n\t"
                    "orl $0x00000200, %%eax\n\t"
                    "movl %%eax, %%cr4\n\t"
                    : :
                    : "ax"
                );

	    btrace(my_cpu_num, __LINE__, "Mem Mgmnt ", 1, cpu_id.fid.bits.pae, cpu_id.fid.bits.lm);
	   /* Настройка режимов управления памятью */
	    /* Если есть поддержка PAE, включаем её */
	    if (cpu_id.fid.bits.pae == 1) {
		__asm__ __volatile__(
                    "movl %%cr4, %%eax\n\t"
                    "orl $0x00000020, %%eax\n\t"
                    "movl %%eax, %%cr4\n\t"
                    : :
                    : "ax"
                );
        cprint(LINE_TITLE+1, COL_MODE, "(PAE Mode)");
       	    }
	    /* Если это 64-битный CPU, включаем long mode */
	    if (cpu_id.fid.bits.lm == 1) {
		__asm__ __volatile__(
		    "movl $0xc0000080, %%ecx\n\t"
		    "rdmsr\n\t"
		    "orl $0x00000100, %%eax\n\t"
		    "wrmsr\n\t"
		    : :
		    : "ax", "cx"
		);
		cprint(LINE_TITLE+1, COL_MODE, "(X64 Mode)");
            }
	    /* Определяем скорость памяти при работе всех процессоров */
	    get_mem_speed(my_cpu_num, num_cpus);
	}

	/* Устанавливаем флаг инициализации только после того, как все процессоры
	 * достигли барьера. Это гарантирует, что перемещение было завершено
	 * для каждого процессора. */
	btrace(my_cpu_num, __LINE__, "Start Done", 1, 0, 0);
	start_seq = 2;
	memtest_main(my_cpu_num, my_cpu_ord);
}

char *codes[] = {
	"  Divide",
	"   Debug",
	"     NMI",
	"  Brkpnt",
	"Overflow",
	"   Bound",
	"  Inv_Op",
	" No_Math",
	"Double_Fault",
	"Seg_Over",
	" Inv_TSS",
	"  Seg_NP",
	"Stack_Fault",
	"Gen_Prot",
	"Page_Fault",
	"   Resvd",
	"     FPE",
	"Alignment",
	" Mch_Chk",
	"SIMD FPE"
};

struct eregs {
	ulong ss;
	ulong ds;
	ulong esp;
	ulong ebp;
	ulong esi;
	ulong edi;
	ulong edx;
	ulong ecx;
	ulong ebx;
	ulong eax;
	ulong vect;
	ulong code;
	ulong eip;
	ulong cs;
	ulong eflag;
};


/* Обработка прерывания */
void inter(struct eregs *trap_regs)
{
	int i, line;
	unsigned char *pp;
	ulong address = 0;
	int my_cpu_num = smp_my_cpu_num();

	/* Получаем адрес ошибки страницы (page fault) */
	if (trap_regs->vect == 14) {
		__asm__("movl %%cr2,%0":"=r" (address));
	}
#ifdef PARITY_MEM

	/* Проверка на ошибку четности */
	if (trap_regs->vect == 2) {
		parity_err(trap_regs->edi, trap_regs->esi);
		return;
	}
#endif

	/* очистка области прокрутки */
        pp=(unsigned char *)(SCREEN_ADR+(2*80*(LINE_SCROLL-2)));
        for(i=0; i<2*80*(24-LINE_SCROLL-2); i++, pp+=2) {
                *pp = ' ';
        }
	line = LINE_SCROLL-2;

	cprint(line, 0, "Unexpected Interrupt - Halting CPU");
	dprint(line, COL_MID + 4, my_cpu_num, 2, 1);
	cprint(line+2, 0, " Type: ");
	if (trap_regs->vect <= 19) {
		cprint(line+2, 7, codes[trap_regs->vect]);
	} else {
		hprint(line+2, 7, trap_regs->vect);
	}
	cprint(line+3, 0, "   PC: ");
	hprint(line+3, 7, trap_regs->eip);
	cprint(line+4, 0, "   CS: ");
	hprint(line+4, 7, trap_regs->cs);
	cprint(line+5, 0, "Eflag: ");
	hprint(line+5, 7, trap_regs->eflag);
	cprint(line+6, 0, " Code: ");
	hprint(line+6, 7, trap_regs->code);
	cprint(line+7, 0, "   DS: ");
	hprint(line+7, 7, trap_regs->ds);
	cprint(line+8, 0, "   SS: ");
	hprint(line+8, 7, trap_regs->ss);
	if (trap_regs->vect == 14) {
		/* Адрес ошибки страницы */
		cprint(line+7, 0, " Addr: ");
		hprint(line+7, 7, address);
	}

	cprint(line+2, 20, "eax: ");
	hprint(line+2, 25, trap_regs->eax);
	cprint(line+3, 20, "ebx: ");
	hprint(line+3, 25, trap_regs->ebx);
	cprint(line+4, 20, "ecx: ");
	hprint(line+4, 25, trap_regs->ecx);
	cprint(line+5, 20, "edx: ");
	hprint(line+5, 25, trap_regs->edx);
	cprint(line+6, 20, "edi: ");
	hprint(line+6, 25, trap_regs->edi);
	cprint(line+7, 20, "esi: ");
	hprint(line+7, 25, trap_regs->esi);
	cprint(line+8, 20, "ebp: ");
	hprint(line+8, 25, trap_regs->ebp);
	cprint(line+9, 20, "esp: ");
	hprint(line+9, 25, trap_regs->esp);

	cprint(line+1, 38, "Stack:");
	for (i=0; i<10; i++) {
		hprint(line+2+i, 38, trap_regs->esp+(4*i));
		hprint(line+2+i, 47, *(ulong*)(trap_regs->esp+(4*i)));
		hprint(line+2+i, 57, trap_regs->esp+(4*(i+10)));
		hprint(line+2+i, 66, *(ulong*)(trap_regs->esp+(4*(i+10))));
	}

	cprint(line+11, 0, "CS:EIP:                          ");
	pp = (unsigned char *)trap_regs->eip;
	for(i = 0; i < 9; i++) {
		hprint2(line+11, 8+(3*i), pp[i], 2);
	}
	while(1) {
		check_input();
	}
}

/* Функция звукового сигнала */

void beep(unsigned int frequency)
{
	unsigned int count = 1193180 / frequency;

	// Включаем динамик
	outb_p(inb_p(0x61)|3, 0x61);

	// Устанавливаем команду для счетчика 2, запись 2 байт
	outb_p(0xB6, 0x43);

	// Выбираем желаемую частоту в Гц
	outb_p(count & 0xff, 0x42);
	outb((count >> 8) & 0xff, 0x42);

	// Блокировка на 100 микросекунд
	sleep(100, 0, 0, 1);

	// Выключаем динамик
	outb(inb_p(0x61)&0xFC, 0x61);
}