#include "arch.h"
#include "test.h"
#include "cpuid.h"
#include "globals.h"
#include "io.h"

unsigned long imc_type = 0;
int tsc_invariable = 0;

void arch_init_early(void)
{
	/* Остановить мотор дисковода */
	outb(0x8, 0x3f2);

	/* Установить аппаратный курсор за границы экрана */
	__outb(0x0F, 0x03D4);
	__outb(0xFF, 0x03D5);

	__outb(0x0E, 0x03D4);
	__outb(0xFF, 0x03D5);

	/* Включить кэш */
	set_cache(1);
}

void arch_init(void)
{
	beepmode = BEEP_MODE;
	get_cpuid();
	pci_init();
	get_cache_size();
	cpu_type();
	cpu_cache_speed();
}

/*
 * Найти тип IMC и соответствующим образом установить глобальные переменные
 */
void detect_imc(void)
{
	// Проверить AMD IMC
	if(cpu_id.vend_id.char_array[0] == 'A' && cpu_id.vers.bits.family == 0xF)
		{
			switch(cpu_id.vers.bits.extendedFamily)
					{
						case 0x0:
							imc_type = 0x0100; // Старый K8
							break;
						case 0x1:
						case 0x2:
							imc_type = 0x0101; // K10 (Семейство 10h и 11h)
							break;
						case 0x3:
							imc_type = 0x0102; // APU серии A (Семейство 12h)
							break;
						case 0x5:
							imc_type = 0x0103; // APU серий C- / E- / Z- (Семейство 14h)
							break;
						case 0x6:
							imc_type = 0x0104; // Серия FX (Семейство 15h)
							break;
						case 0x7:
							imc_type = 0x0105; // Kabini и родственные (Семейство 16h)
							break;
					}
			return;
		}

	// Проверить Intel IMC
	if(cpu_id.vend_id.char_array[0] == 'G' && cpu_id.vers.bits.family == 6 && cpu_id.vers.bits.extendedModel)
		{
			switch(cpu_id.vers.bits.model)
			{
				case 0x5:
					if(cpu_id.vers.bits.extendedModel == 2) { imc_type = 0x0003; } // Core i3/i5 1-го поколения 45 нм (NHM)
					if(cpu_id.vers.bits.extendedModel == 3) { v->fail_safe |= 4; } // Atom Clover Trail
					if(cpu_id.vers.bits.extendedModel == 4) { imc_type = 0x0007; } // HSW-ULT
					break;
				case 0x6:
					if(cpu_id.vers.bits.extendedModel == 3) {
						imc_type = 0x0009;  // Atom Cedar Trail
						v->fail_safe |= 4; // Отключить температуру ядра
					}
					break;
				case 0xA:
					switch(cpu_id.vers.bits.extendedModel)
					{
						case 0x1:
							imc_type = 0x0001; // Core i7 1-го поколения 45 нм (NHME)
							break;
						case 0x2:
							imc_type = 0x0004; // Core 2-го поколения (SNB)
							break;
						case 0x3:
							imc_type = 0x0006; // Core 3-го поколения (IVB)
							break;
					}
					break;
				case 0xC:
					switch(cpu_id.vers.bits.extendedModel)
					{
						case 0x1:
							if(cpu_id.vers.bits.stepping > 9) { imc_type = 0x0008; } // Atom PineView
							v->fail_safe |= 4; // Отключить температуру ядра
							break;
						case 0x2:
							imc_type = 0x0002; // Core i7 1-го поколения 32 нм (WMR)
							break;
						case 0x3:
							imc_type = 0x0007; // Core 4-го поколения (HSW)
							break;
					}
					break;
				case 0xD:
					imc_type = 0x0005; // SNB-E
					break;
				case 0xE:
					imc_type = 0x0001; // Core i7 1-го поколения 45 нм (NHM)
					break;
			}

		if(imc_type) { tsc_invariable = 1; }
		return;
		}
}

void smp_default_mode(void)
{
	int i, result;
	char *cpupsn = cpu_id.brand_id.char_array;
  char *disabledcpu[] = { "Opteron", "Xeon", "Genuine Intel" };

  for(i = 0; i < 3; i++)
  {
  	result = strstr(cpupsn , disabledcpu[i]);
  	if(result != -1) { v->fail_safe |= 0b10; }
  }

  // Для релиза 5.01 SMP отключен по умолчанию через переключатель в config.h
  if(CONSERVATIVE_SMP) { v->fail_safe |= 0b10; }

}

/*
 * Определить тип процессора
 */
void cpu_type(void)
{
	/* Если мы можем получить строку бренда, используем её и на этом всё */
	if (cpu_id.max_xcpuid >= 0x80000004) {
		cprint(0, COL_MID, cpu_id.brand_id.char_array);
		// Если есть строка бренда, возможно, есть IMC. Проверяем это.
		detect_imc();
		smp_default_mode();
		return;
	}

	/* Строка бренда недоступна, поэтому нам нужно выяснить,
	 * какой именно у нас процессор */
	switch(cpu_id.vend_id.char_array[0]) {
	/* Процессоры AMD */
	case 'A':
		switch(cpu_id.vers.bits.family) {
		case 4:
			switch(cpu_id.vers.bits.model) {
			case 3:
				cprint(0, COL_MID, "AMD 486DX2");
				break;
			case 7:
				cprint(0, COL_MID, "AMD 486DX2-WB");
				break;
			case 8:
				cprint(0, COL_MID, "AMD 486DX4");
				break;
			case 9:
				cprint(0, COL_MID, "AMD 486DX4-WB");
				break;
			case 14:
				cprint(0, COL_MID, "AMD 5x86-WT");
				break;
			case 15:
				cprint(0, COL_MID, "AMD 5x86-WB");
				break;
			}
			/* Так как мы не можем получить скорость процессора или инфо о кэше, выходим */
			return;
		case 5:
			switch(cpu_id.vers.bits.model) {
			case 0:
			case 1:
			case 2:
			case 3:
				cprint(0, COL_MID, "AMD K5");
				l1_cache = 8;
				break;
			case 6:
			case 7:
				cprint(0, COL_MID, "AMD K6");
				break;
			case 8:
				cprint(0, COL_MID, "AMD K6-2");
				break;
			case 9:
				cprint(0, COL_MID, "AMD K6-III");
				break;
			case 13:
				cprint(0, COL_MID, "AMD K6-III+");
				break;
			}
			break;
		case 6:

			switch(cpu_id.vers.bits.model) {
			case 1:
				cprint(0, COL_MID, "AMD Athlon (0.25)");
				break;
			case 2:
			case 4:
				cprint(0, COL_MID, "AMD Athlon (0.18)");
				break;
			case 6:
				if (l2_cache == 64) {
					cprint(0, COL_MID, "AMD Duron (0.18)");
				} else {
					cprint(0, COL_MID, "Athlon XP (0.18)");
				}
				break;
			case 8:
			case 10:
				if (l2_cache == 64) {
					cprint(0, COL_MID, "AMD Duron (0.13)");
				} else {
					cprint(0, COL_MID, "Athlon XP (0.13)");
				}
				break;
			case 3:
			case 7:
				cprint(0, COL_MID, "AMD Duron");
				/* CPUID для L2 в Duron степпинга 0 сломан */
				/* (AMD errata T13)*/
				if (cpu_id.vers.bits.stepping == 0) { /* степпинг 0 */
					/* Жестко задаем правильный размер L2 */
					l2_cache = 64;
				} else {
				}
				break;
			}
			break;

			/* Все значения семейства AMD >= 10 имеют функцию Brand ID,
			 * поэтому нам не нужно искать тип процессора */
		}
		break;

	/* Процессоры Intel или Transmeta */
	case 'G':
		if ( cpu_id.vend_id.char_array[7] == 'T' ) { /* GenuineTMx86 */
			if (cpu_id.vers.bits.family == 5) {
				cprint(0, COL_MID, "TM 5x00");
			} else if (cpu_id.vers.bits.family == 15) {
				cprint(0, COL_MID, "TM 8x00");
			}
			l1_cache = cpu_id.cache_info.ch[3] + cpu_id.cache_info.ch[7];
			l2_cache = (cpu_id.cache_info.ch[11]*256) + cpu_id.cache_info.ch[10];
		} else {				/* GenuineIntel */
			if (cpu_id.vers.bits.family == 4) {
			switch(cpu_id.vers.bits.model) {
			case 0:
			case 1:
				cprint(0, COL_MID, "Intel 486DX");
				break;
			case 2:
				cprint(0, COL_MID, "Intel 486SX");
				break;
			case 3:
				cprint(0, COL_MID, "Intel 486DX2");
				break;
			case 4:
				cprint(0, COL_MID, "Intel 486SL");
				break;
			case 5:
				cprint(0, COL_MID, "Intel 486SX2");
				break;
			case 7:
				cprint(0, COL_MID, "Intel 486DX2-WB");
				break;
			case 8:
				cprint(0, COL_MID, "Intel 486DX4");
				break;
			case 9:
				cprint(0, COL_MID, "Intel 486DX4-WB");
				break;
			}
			/* Так как мы не можем получить скорость процессора или инфо о кэше, выходим */
			return;
		}


		switch(cpu_id.vers.bits.family) {
		case 5:
			switch(cpu_id.vers.bits.model) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 7:
				cprint(0, COL_MID, "Pentium");
				if (l1_cache == 0) {
					l1_cache = 8;
				}
				break;
			case 4:
			case 8:
				cprint(0, COL_MID, "Pentium-MMX");
				if (l1_cache == 0) {
					l1_cache = 16;
				}
				break;
			}
			break;
		case 6:
			switch(cpu_id.vers.bits.model) {
			case 0:
			case 1:
				cprint(0, COL_MID, "Pentium Pro");
				break;
			case 3:
			case 4:
				cprint(0, COL_MID, "Pentium II");
				break;
			case 5:
				if (l2_cache == 0) {
					cprint(0, COL_MID, "Celeron");
				} else {
					cprint(0, COL_MID, "Pentium II");
				}
				break;
			case 6:
				  if (l2_cache == 128) {
					cprint(0, COL_MID, "Celeron");
				  } else {
					cprint(0, COL_MID, "Pentium II");
				  }
				}
				break;
			case 7:
			case 8:
			case 11:
				if (l2_cache == 128) {
					cprint(0, COL_MID, "Celeron");
				} else {
					cprint(0, COL_MID, "Pentium III");
				}
				break;
			case 9:
				if (l2_cache == 512) {
					cprint(0, COL_MID, "Celeron M (0.13)");
				} else {
					cprint(0, COL_MID, "Pentium M (0.13)");
				}
				break;
     			case 10:
				cprint(0, COL_MID, "Pentium III Xeon");
				break;
			case 12:
				l1_cache = 24;
				cprint(0, COL_MID, "Atom (0.045)");
				break;
			case 13:
				if (l2_cache == 1024) {
					cprint(0, COL_MID, "Celeron M (0.09)");
				} else {
					cprint(0, COL_MID, "Pentium M (0.09)");
				}
				break;
			case 14:
				cprint(0, COL_MID, "Intel Core");
				break;
			case 15:
				if (l2_cache == 1024) {
					cprint(0, COL_MID, "Pentium E");
				} else {
					cprint(0, COL_MID, "Intel Core 2");
				}
				break;
			}
			break;
		case 15:
			switch(cpu_id.vers.bits.model) {
			case 0:
			case 1:
			case 2:
				if (l2_cache == 128) {
					cprint(0, COL_MID, "Celeron");
				} else {
					cprint(0, COL_MID, "Pentium 4");
				}
				break;
			case 3:
			case 4:
				if (l2_cache == 256) {
					cprint(0, COL_MID, "Celeron (0.09)");
				} else {
					cprint(0, COL_MID, "Pentium 4 (0.09)");
				}
				break;
			case 6:
				cprint(0, COL_MID, "Pentium D (65nm)");
				break;
			default:
				cprint(0, COL_MID, "Unknown Intel");
 				break;
			break;
		    }

		}
		break;

	/* Процессоры VIA/Cyrix/Centaur с поддержкой CPUID */
	case 'C':
		if ( cpu_id.vend_id.char_array[1] == 'e' ) { /* CentaurHauls */
			l1_cache = cpu_id.cache_info.ch[3] + cpu_id.cache_info.ch[7];
			l2_cache = cpu_id.cache_info.ch[11];
			switch(cpu_id.vers.bits.family){
			case 5:
				cprint(0, COL_MID, "Centaur 5x86");
				break;
			case 6: // VIA C3
				switch(cpu_id.vers.bits.model){
				default:
				    if (cpu_id.vers.bits.stepping < 8) {
					cprint(0, COL_MID, "VIA C3 Samuel2");
				    } else {
					cprint(0, COL_MID, "VIA C3 Eden");
				    }
				break;
				case 10:
					cprint(0, COL_MID, "VIA C7 (C5J)");
					l1_cache = 64;
					l2_cache = 128;
					break;
				case 13:
					cprint(0, COL_MID, "VIA C7 (C5R)");
					l1_cache = 64;
					l2_cache = 128;
					break;
				case 15:
					cprint(0, COL_MID, "VIA Isaiah (CN)");
					l1_cache = 64;
					l2_cache = 128;
					break;
				}
			}
		} else {				/* CyrixInstead */
			switch(cpu_id.vers.bits.family) {
			case 5:
				switch(cpu_id.vers.bits.model) {
				case 0:
					cprint(0, COL_MID, "Cyrix 6x86MX/MII");
					break;
				case 4:
					cprint(0, COL_MID, "Cyrix GXm");
					break;
				}
				return;

			case 6: // VIA C3
				switch(cpu_id.vers.bits.model) {
				case 6:
					cprint(0, COL_MID, "Cyrix III");
					break;
				case 7:
					if (cpu_id.vers.bits.stepping < 8) {
						cprint(0, COL_MID, "VIA C3 Samuel2");
					} else {
						cprint(0, COL_MID, "VIA C3 Ezra-T");
					}
					break;
				case 8:
					cprint(0, COL_MID, "VIA C3 Ezra-T");
					break;
				case 9:
					cprint(0, COL_MID, "VIA C3 Nehemiah");
					break;
				}
				// L1 = L2 = 64 КБ от Cyrix III до Nehemiah
				l1_cache = 64;
				l2_cache = 64;
				break;
			}
		}
		break;
	/* Неизвестный процессор */
	default:
		/* Попробуем угадать семейство */
		switch(cpu_id.vers.bits.family) {
		case 5:
			cprint(0, COL_MID, "586");
		case 6:
			cprint(0, COL_MID, "686");
		default:
			cprint(0, COL_MID, "Unidentified Processor");
		}
	}
}

/* Получение размеров кэша для большинства процессоров AMD и Intel; исключения 
 * для старых процессоров обрабатываются при определении типа процессора */
void get_cache_size()
{
	int i, j, n, size;
	unsigned int v[4];
	unsigned char *dp = (unsigned char *)v;
	struct cpuid4_eax *eax = (struct cpuid4_eax *)&v[0];
	struct cpuid4_ebx *ebx = (struct cpuid4_ebx *)&v[1];
	struct cpuid4_ecx *ecx = (struct cpuid4_ecx *)&v[2];

	switch(cpu_id.vend_id.char_array[0]) {
	/* Процессоры AMD */
	case 'A':
		//l1_cache = cpu_id.cache_info.amd.l1_i_sz;
		l1_cache = cpu_id.cache_info.amd.l1_d_sz;
		l2_cache = cpu_id.cache_info.amd.l2_sz;
		l3_cache = cpu_id.cache_info.amd.l3_sz;
    l3_cache *= 512;
		break;
	case 'G':
		/* Процессоры Intel */
		l1_cache = 0;
		l2_cache = 0;
		l3_cache = 0;

		/* Использовать CPUID(4), если доступно */
		if (cpu_id.max_cpuid > 3) {

		   /* выяснить количество уровней (leaves) кэша */
		    n = -1;
		    do
		    {
					++n;
					/* Цикл cpuid(4) для определения num_cache_leaves */
					cpuid_count(4, n, &v[0], &v[1], &v[2], &v[3]);
		    } while ((eax->ctype) != 0);

		    /* цикл по всем уровням */
		    for (i=0; i<n; i++)
		    {
					cpuid_count(4, i, &v[0], &v[1], &v[2], &v[3]);

					/* Проверка на допустимый тип кэша */
					if (eax->ctype == 1 || eax->ctype == 3)
					{

			    	/* Вычислить размер кэша */
			    	size = (ecx->number_of_sets + 1) *
            	              	  (ebx->coherency_line_size + 1) *
              	            	  (ebx->physical_line_partition + 1) *
                	          	  (ebx->ways_of_associativity + 1);
			    	size /= 1024;

				    switch (eax->level)
				    {
					  	case 1:
								l1_cache += size;
								break;
					    case 2:
								l2_cache += size;
								break;
					    case 3:
								l3_cache += size;
								break;
					  }
					}
		    }
		    return;
		}

		/* CPUID(4) отсутствует, используем старый метод CPUID(2) */
		/* Получить количество итераций */
		cpuid(2, &v[0], &v[1], &v[2], &v[3]);
		n = v[0] & 0xff;
                for (i=0 ; i<n ; i++) {
                    cpuid(2, &v[0], &v[1], &v[2], &v[3]);

                    /* Если бит 31 установлен, это неизвестный формат */
                    for (j=0 ; j<3 ; j++) {
                            if (v[j] & (1 << 31)) {
                                    v[j] = 0;
			    }
		    }

                    /* Байт 0 — это количество уровней, а не дескриптор */
                    for (j = 1 ; j < 16 ; j++) {
			switch(dp[j]) {
			case 0x6:
			case 0xa:
			case 0x66:
				l1_cache += 8;
				break;
			case 0x8:
			case 0xc:
			case 0xd:
			case 0x60:
			case 0x67:
				l1_cache += 16;
				break;
			case 0xe:
				l1_cache += 24;
				break;
			case 0x9:
			case 0x2c:
			case 0x30:
			case 0x68:
				l1_cache += 32;
				break;
			case 0x39:
			case 0x3b:
			case 0x41:
			case 0x79:
				l2_cache += 128;
				break;
			case 0x3a:
				l2_cache += 192;
				break;
			case 0x21:
			case 0x3c:
			case 0x3f:
			case 0x42:
			case 0x7a:
			case 0x82:
				l2_cache += 256;
				break;
			case 0x3d:
				l2_cache += 384;
				break;
			case 0x3e:
			case 0x43:
			case 0x7b:
			case 0x7f:
			case 0x80:
			case 0x83:
			case 0x86:
				l2_cache += 512;
				break;
			case 0x44:
			case 0x78:
			case 0x7c:
			case 0x84:
			case 0x87:
				l2_cache += 1024;
				break;
			case 0x45:
			case 0x7d:
			case 0x85:
				l2_cache += 2048;
				break;
			case 0x48:
				l2_cache += 3072;
				break;
			case 0x4e:
				l2_cache += 6144;
				break;
			case 0x23:
			case 0xd0:
				l3_cache += 512;
				break;
			case 0xd1:
			case 0xd6:
				l3_cache += 1024;
				break;
			case 0x25:
			case 0xd2:
			case 0xd7:
			case 0xdc:
			case 0xe2:
				l3_cache += 2048;
				break;
			case 0x29:
			case 0x46:
			case 0x49:
			case 0xd8:
			case 0xdd:
			case 0xe3:
				l3_cache += 4096;
				break;
			case 0x4a:
				l3_cache += 6144;
				break;
			case 0x47:
			case 0x4b:
			case 0xde:
			case 0xe4:
				l3_cache += 8192;
				break;
			case 0x4c:
			case 0xea:
				l3_cache += 12288;
				break;
			case 0x4d:
				l3_cache += 16384;
				break;
			case 0xeb:
				l3_cache += 18432;
				break;
			case 0xec:
				l3_cache += 24576;
				break;
			} /* конец switch */
		    } /* конец цикла 1-16 */
		} /* конец цикла 0 - n */
	}
}

/* #define TICKS 5 * 11832 (количество = 6376)*/
/* #define TICKS (65536 - 12752) */
#define TICKS 59659	/* 50 мс */

/* Возвращает тактовую частоту процессора в кГц */
ulong stlow, sthigh;
int cpuspeed(void)
{
	int loops;
	ulong end_low, end_high;

	if (cpu_id.fid.bits.rdtsc == 0 ) {
		return(-1);
	}

	/* Настройка таймера */
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);
	outb(0xb0, 0x43);
	outb(TICKS & 0xff, 0x42);
	outb(TICKS >> 8, 0x42);

	asm __volatile__ ("rdtsc":"=a" (stlow),"=d" (sthigh));

	loops = 0;
	do {
		loops++;
	} while ((inb(0x61) & 0x20) == 0);

	asm __volatile__ (
		"rdtsc\n\t" \
		"subl stlow,%%eax\n\t" \
		"sbbl sthigh,%%edx\n\t" \
		:"=a" (end_low), "=d" (end_high)
	);

	/* Убедиться, что результат достоверный */
	if (loops < 4 || end_low < 50000) {
		return(-1);
	}
	v->clks_msec = end_low/50;

	if (tsc_invariable) end_low = correct_tsc(end_low);

	return(v->clks_msec);
}

/* Измерить скорость кэша путем копирования блока памяти. */
/* Возвращаемое значение в Кбайт/сек */
ulong memspeed(ulong src, ulong len, int iter)
{
	int i;
	ulong dst, wlen;
	ulong st_low, st_high;
	ulong end_low, end_high;
	ulong cal_low, cal_high;

	if (cpu_id.fid.bits.rdtsc == 0 ) {
		return(-1);
	}
	if (len == 0) return(-2);

	dst = src + len;
	wlen = len / 4;  /* Длина в байтах */

	/* Калибровка накладных расходов при копировании нулевого слова */
	asm __volatile__ ("rdtsc":"=a" (st_low),"=d" (st_high));
	for (i=0; i<iter; i++) {
		asm __volatile__ (
			"movl %0,%%esi\n\t" \
 		 	"movl %1,%%edi\n\t" \
 		 	"movl %2,%%ecx\n\t" \
 		 	"cld\n\t" \
 		 	"rep\n\t" \
 		 	"movsl\n\t" \
			:: "g" (src), "g" (dst), "g" (0)
			: "esi", "edi", "ecx"
		);
	}
	asm __volatile__ ("rdtsc":"=a" (cal_low),"=d" (cal_high));

	/* Вычислить время накладных расходов */
	asm __volatile__ (
		"subl %2,%0\n\t"
		"sbbl %3,%1"
		:"=a" (cal_low), "=d" (cal_high)
		:"g" (st_low), "g" (st_high),
		"0" (cal_low), "1" (cal_high)
	);


	/* Теперь измерить скорость */
	/* Выполнить первое копирование для прогрева кэша */
	asm __volatile__ (
		"movl %0,%%esi\n\t" \
		"movl %1,%%edi\n\t" \
 	 	"movl %2,%%ecx\n\t" \
 	 	"cld\n\t" \
 	 	"rep\n\t" \
 	 	"movsl\n\t" \
		:: "g" (src), "g" (dst), "g" (wlen)
		: "esi", "edi", "ecx"
	);
	asm __volatile__ ("rdtsc":"=a" (st_low),"=d" (st_high));
	for (i=0; i<iter; i++) {
	        asm __volatile__ (
			"movl %0,%%esi\n\t" \
			"movl %1,%%edi\n\t" \
 		 	"movl %2,%%ecx\n\t" \
 		 	"cld\n\t" \
 		 	"rep\n\t" \
 		 	"movsl\n\t" \
			:: "g" (src), "g" (dst), "g" (wlen)
			: "esi", "edi", "ecx"
		);
	}
	asm __volatile__ ("rdtsc":"=a" (end_low),"=d" (end_high));

	/* Вычислить прошедшее время */
	asm __volatile__ (
		"subl %2,%0\n\t"
		"sbbl %3,%1"
		:"=a" (end_low), "=d" (end_high)
		:"g" (st_low), "g" (st_high),
		"0" (end_low), "1" (end_high)
	);
	/* Вычесть время накладных расходов */
	asm __volatile__ (
		"subl %2,%0\n\t"
		"sbbl %3,%1"
		:"=a" (end_low), "=d" (end_high)
		:"g" (cal_low), "g" (cal_high),
		"0" (end_low), "1" (end_high)
	);

	/* Убедиться, что результат помещается в 32 бита */
	//hprint(11,40,end_high);
	if (end_high) {
		return(-3);
	}
	end_low /= 2;

	/* Преобразовать в такты/КБ */
	end_low /= len;
	end_low *= 1024;
	end_low /= iter;
	if (end_low == 0) {
		return(-4);
	}

	/* Преобразовать в Кбайт/сек */

	if (tsc_invariable) end_low = correct_tsc(end_low);

	return((v->clks_msec)/end_low);
}

#define rdmsr(msr,val1,val2) \
	__asm__ __volatile__("rdmsr" \
		  : "=a" (val1), "=d" (val2) \
		  : "c" (msr))


ulong correct_tsc(ulong el_org)
{
	float coef_now, coef_max;
	int msr_lo, msr_hi, is_xe;

	rdmsr(0x198, msr_lo, msr_hi);
	is_xe = (msr_lo >> 31) & 0x1;

	if(is_xe){
		rdmsr(0x198, msr_lo, msr_hi);
		coef_max = ((msr_hi >> 8) & 0x1F);
		if ((msr_hi >> 14) & 0x1) { coef_max = coef_max + 0.5f; }
	} else {
		rdmsr(0x17, msr_lo, msr_hi);
		coef_max = ((msr_lo >> 8) & 0x1F);
		if ((msr_lo >> 14) & 0x1) { coef_max = coef_max + 0.5f; }
	}

	if(cpu_id.fid.bits.eist) {
		rdmsr(0x198, msr_lo, msr_hi);
		coef_now = ((msr_lo >> 8) & 0x1F);
		if ((msr_lo >> 14) & 0x1) { coef_now = coef_now + 0.5f; }
	} else {
		rdmsr(0x2A, msr_lo, msr_hi);
		coef_now = (msr_lo >> 22) & 0x1F;
	}
	if(coef_max && coef_now) {
		el_org = (ulong)(el_org * coef_now / coef_max);
	}
	return el_org;
}
