/*
 * Специфичный код MemTest86+ V5 (GPL V2.0)
 * Автор: Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 * ------------------------------------------------
 * smp.c - MemTest-86 Версия 3.5
 *
 * Выпущено под версией 2 лицензии Gnu Public License.
 * Автор: Chris Brady
 */

#include <stddef.h>
#include <stdint.h>

#include "arch.h"
#include "smp.h"
#include "test.h"

#define DELAY_FACTOR 1

extern void memcpy(void *dst, void *src , int len);
extern void test_start(void);
extern int maxcpus;
extern char cpu_mask[];
extern struct cpu_ident cpu_id;

void smp_find_cpus();

typedef struct {
   bool started;
} ap_info_t;

volatile apic_register_t *APIC = NULL;
/* Таблица отображения номера процессора на APIC ID. Процессор 0 — это BSP. */
static unsigned cpu_num_to_apic_id[MAX_CPUS];
volatile ap_info_t AP[MAX_CPUS];

void PUT_MEM16(uintptr_t addr, uint16_t val)
{
   *((volatile uint16_t *)addr) = val;
}

void PUT_MEM32(uintptr_t addr, uint32_t val)
{
   *((volatile uint32_t *)addr) = val;
}

static void inline 
APIC_WRITE(unsigned reg, uint32_t val)
{
   APIC[reg][0] = val;
}

static inline uint32_t 
APIC_READ(unsigned reg)
{
   return APIC[reg][0];
}


static void 
SEND_IPI(unsigned apic_id, unsigned trigger, unsigned level, unsigned mode,
	    uint8_t vector)
{
   uint32_t v;

   v = APIC_READ(APICR_ICRHI) & 0x00ffffff;
   APIC_WRITE(APICR_ICRHI, v | (apic_id << 24));

   v = APIC_READ(APICR_ICRLO) & ~0xcdfff;
   v |= (APIC_DEST_DEST << APIC_ICRLO_DEST_OFFSET) 
      | (trigger << APIC_ICRLO_TRIGGER_OFFSET)
      | (level << APIC_ICRLO_LEVEL_OFFSET)
      | (mode << APIC_ICRLO_DELMODE_OFFSET)
      | (vector);
   APIC_WRITE(APICR_ICRLO, v);
}

// Примитивный способ ожидания (busywaiting), так как у нас нет таймера
void delay(unsigned us) 
{
   unsigned freq = 1000; // в МГц, предполагаем частоту процессора 1 ГГц
   uint64_t cycles = us * freq;
   uint64_t t0 = RDTSC();
   uint64_t t1;
   volatile unsigned k;

   do {
      for (k = 0; k < 1000; k++) continue;
      t1 = RDTSC();
   } while (t1 - t0 < cycles);
}

static inline void
memset (void *dst,
        char  value,
        int   len)
{
   int i;
   for (i = 0 ; i < len ; i++ ) { 
      *((char *) dst + i) = value;
   }
}
void initialise_cpus(void)
{
	int i;

	act_cpus = 0;
	if (maxcpus > 1) {
		smp_find_cpus();
		/* Общее количество процессоров может быть ограничено */
		if (num_cpus > maxcpus) {
			num_cpus = maxcpus;
		}
		/* Определить, сколько процессоров было выбрано */
		for(i = 0; i < num_cpus; i++) {
			if (cpu_mask[i]) {
				act_cpus++;
			}
		}
	} else {
		act_cpus = found_cpus = num_cpus = 1;
	}

	/* Инициализировать барьер перед запуском AP */
	barrier_init(act_cpus);
	/* пусть BSP инициализирует AP. */
	for(i = 1; i < num_cpus; i++) {
	    /* Запускать этот процессор только если он выбран маской */
	    if (cpu_mask[i]) {
	        smp_boot_ap(i);
	    }
	}
}

void kick_cpu(unsigned cpu_num)
{
   unsigned num_sipi, apic_id;
   apic_id = cpu_num_to_apic_id[cpu_num];

   // очистить регистр APIC ESR
   APIC_WRITE(APICR_ESR, 0);
   APIC_READ(APICR_ESR);

   // активация INIT IPI
   SEND_IPI(apic_id, APIC_TRIGGER_LEVEL, 1, APIC_DELMODE_INIT, 0);
   delay(100000 / DELAY_FACTOR);

   // деактивация INIT IPI
   SEND_IPI(apic_id, APIC_TRIGGER_LEVEL, 0, APIC_DELMODE_INIT, 0);

   for (num_sipi = 0; num_sipi < 2; num_sipi++) {
      unsigned timeout;
      bool send_pending;
      unsigned err;

      APIC_WRITE(APICR_ESR, 0);

      SEND_IPI(apic_id, 0, 0, APIC_DELMODE_STARTUP, (unsigned)startup_32 >> 12);

      timeout = 0;
      do {
	 delay(10);
	 timeout++;
	 send_pending = (APIC_READ(APICR_ICRLO) & APIC_ICRLO_STATUS_MASK) != 0;
      } while (send_pending && timeout < 1000);

      if (send_pending) {
	 cprint(LINE_STATUS+3, 0, "SMP: STARTUP IPI was never sent");
      }
      
      delay(100000 / DELAY_FACTOR);

      err = APIC_READ(APICR_ESR) & 0xef;
      if (err) {
	 cprint(LINE_STATUS+3, 0, "SMP: After STARTUP IPI: err = 0x");
         hprint(LINE_STATUS+3, COL_MID, err);
      }
   }
}

// Эти области памяти используются для кода и данных трамплина (trampoline).

#define BOOTCODESTART 0x9000
#define GDTPOINTERADDR 0x9100
#define GDTADDR 0x9110

void boot_ap(unsigned cpu_num)
{
   unsigned num_sipi, apic_id;
   extern uint8_t gdt; 
   extern uint8_t _ap_trampoline_start;
   extern uint8_t _ap_trampoline_protmode;
   unsigned len = &_ap_trampoline_protmode - &_ap_trampoline_start;
   apic_id = cpu_num_to_apic_id[cpu_num];


   memcpy((uint8_t*)BOOTCODESTART, &_ap_trampoline_start, len);

   // Настройка инструкции LGDT, чтобы она указывала на указатель GDT.
   PUT_MEM16(BOOTCODESTART + 3, GDTPOINTERADDR);

   // Копирование указателя на временную GDT по адресу GDTPOINTERADDR.
   // Временная GDT находится по адресу GDTADDR
   PUT_MEM16(GDTPOINTERADDR, 4 * 8);
   PUT_MEM32(GDTPOINTERADDR + 2, GDTADDR);

   // Копирование первых 4 записей GDT из текущей GDT во
   // временную GDT.
   memcpy((uint8_t *)GDTADDR, &gdt, 32);

   // очистить регистр APIC ESR
   APIC_WRITE(APICR_ESR, 0);
   APIC_READ(APICR_ESR);

   // активация INIT IPI
   SEND_IPI(apic_id, APIC_TRIGGER_LEVEL, 1, APIC_DELMODE_INIT, 0);
   delay(100000 / DELAY_FACTOR);

   // деактивация INIT IPI
   SEND_IPI(apic_id, APIC_TRIGGER_LEVEL, 0, APIC_DELMODE_INIT, 0);

   for (num_sipi = 0; num_sipi < 2; num_sipi++) {
      unsigned timeout;
      bool send_pending;
      unsigned err;

      APIC_WRITE(APICR_ESR, 0);

      SEND_IPI(apic_id, 0, 0, APIC_DELMODE_STARTUP, BOOTCODESTART >> 12);

      timeout = 0;
      do {
	 delay(10);
	 timeout++;
	 send_pending = (APIC_READ(APICR_ICRLO) & APIC_ICRLO_STATUS_MASK) != 0;
      } while (send_pending && timeout < 1000);

      if (send_pending) {
	 cprint(LINE_STATUS+3, 0, "SMP: STARTUP IPI was never sent");
      }
      
      delay(100000 / DELAY_FACTOR);

      err = APIC_READ(APICR_ESR) & 0xef;
      if (err) {
	 cprint(LINE_STATUS+3, 0, "SMP: After STARTUP IPI: err = 0x");
         hprint(LINE_STATUS+3, COL_MID, err);
      }
   }
}

static int checksum(unsigned char *mp, int len)
{
   int sum = 0;

   while (len--) {
       sum += *mp++;
   }
   return (sum & 0xFF);
}

/* Разбор таблицы конфигурации MP для получения информации о процессорах */
bool read_mp_config_table(uintptr_t addr)
{
   mp_config_table_header_t *mpc = (mp_config_table_header_t*)addr;
   uint8_t *tab_entry_ptr;
   uint8_t *mpc_table_end;

   if (mpc->signature != MPCSignature) {
      return FALSE;
   }
   if (checksum((unsigned char*)mpc, mpc->length) != 0) {
      return FALSE;
   }

   /* FIXME: приведение к uintptr_t здесь обходит проблему компиляции на
    * AMD64, но игнорирует реальную проблему — lapic_addr
    * занимает всего 32 бита. Возможно, это нормально, но стоит проверить.
    */
   APIC = (volatile apic_register_t*)(uintptr_t)mpc->lapic_addr;

   tab_entry_ptr = ((uint8_t*)mpc) + sizeof(mp_config_table_header_t);
   mpc_table_end = ((uint8_t*)mpc) + mpc->length;
      
   while (tab_entry_ptr < mpc_table_end) {
      switch (*tab_entry_ptr) {
	      case MP_PROCESSOR: {
		 			mp_processor_entry_t *pe = (mp_processor_entry_t*)tab_entry_ptr;
	
					 if (pe->cpu_flag & CPU_BOOTPROCESSOR) {
					    // BSP — это процессор 0
					    cpu_num_to_apic_id[0] = pe->apic_id;
					 } else if (num_cpus < MAX_CPUS) {
					    cpu_num_to_apic_id[num_cpus] = pe->apic_id;
					    num_cpus++;
					 }
					 found_cpus++;
					    
					 // мы не можем обрабатывать внешние (не локальные) APIC 82489DX
					 if ((pe->apic_ver & 0xf0) != 0x10) {
					    return 0;
					 }
				
					 tab_entry_ptr += sizeof(mp_processor_entry_t);
					 break;
				}
	      case MP_BUS: {
					 tab_entry_ptr += sizeof(mp_bus_entry_t);
					 break;
				}
	      case MP_IOAPIC: {
					 tab_entry_ptr += sizeof(mp_io_apic_entry_t);
					 break;
				}
	      case MP_INTSRC:
					 tab_entry_ptr += sizeof(mp_interrupt_entry_t);
					 break;
	      case MP_LINTSRC:
					 tab_entry_ptr += sizeof(mp_local_interrupt_entry_t);
					 break;
	      default: 
		 			 return FALSE;
      }
   }
   return TRUE;
}

/* Поиск структуры Floating Pointer */
floating_pointer_struct_t *
scan_for_floating_ptr_struct(uintptr_t addr, uint32_t length)
{
   floating_pointer_struct_t *fp;
   uintptr_t end = addr + length;


   while ((uintptr_t)addr < end) {
      fp = (floating_pointer_struct_t*)addr;
      if (*(unsigned int *)addr == FPSignature && fp->length == 1 && checksum((unsigned char*)addr, 16) == 0 &&	((fp->spec_rev == 1) || (fp->spec_rev == 4))) {
	   		return fp;
      }
      addr += 4;
   }
   return NULL;
}

/* Поиск RSDP (Root System Descriptor Pointer) */
rsdp_t *scan_for_rsdp(uintptr_t addr, uint32_t length)
{
   rsdp_t *rp;
   uintptr_t end = addr + length;


   while ((uintptr_t)addr < end) {
      rp = (rsdp_t*)addr;
      if (*(unsigned int *)addr == RSDPSignature && 
		checksum((unsigned char*)addr, rp->length) == 0) {
	   return rp;
      }
      addr += 4;
   }
   return NULL;
}

/* Разбор таблицы MADT для записей о процессорах */
int parse_madt(uintptr_t addr) {

   mp_config_table_header_t *mpc = (mp_config_table_header_t*)addr;
   uint8_t *tab_entry_ptr;
   uint8_t *mpc_table_end;

   if (checksum((unsigned char*)mpc, mpc->length) != 0) {
      return FALSE;
   }

   APIC = (volatile apic_register_t*)(uintptr_t)mpc->lapic_addr;

   tab_entry_ptr = ((uint8_t*)mpc) + sizeof(mp_config_table_header_t);
   mpc_table_end = ((uint8_t*)mpc) + mpc->length;
   	while (tab_entry_ptr < mpc_table_end) {
		
			madt_processor_entry_t *pe = (madt_processor_entry_t*)tab_entry_ptr;
			if (pe->type == MP_PROCESSOR) {
				if (pe->enabled) {
					if (num_cpus < MAX_CPUS) {
						cpu_num_to_apic_id[num_cpus] = pe->apic_id;
		
						/* первый процессор — это BSP, не инкрементировать */
						if (found_cpus) {
							num_cpus++;
						}
					}
					found_cpus++;
				}
			}
			tab_entry_ptr += pe->length;
		}
   return TRUE;
}

/* Здесь мы ищем информацию о SMP в следующем порядке:
 * поиск указателя Floating MP
 *   найден:
 *     проверка конфигурации по умолчанию
 * 	 найден:
 *	   настройка конфигурации, возврат
 *     проверка таблицы конфигурации MP
 *	 найден:
 *	   проверка валидности:
 *           успешно:
 *	        разбор таблицы конфигурации MP
 *		  успешно:
 *		    настройка конфигурации, возврат
 *
 * поиск и проверка ACPI RSDP (Root System Descriptor Pointer)
 *   найден:
 *     поиск и проверка RSDT (Root System Descriptor Table)
 *       найден:
 *         поиск и проверка MSDT
 *	     найден:
 *             разбор таблицы MADT
 *               успешно:
 *		   настройка конфигурации, возврат
 */
void smp_find_cpus()
{
   floating_pointer_struct_t *fp;
   rsdp_t *rp;
   rsdt_t *rt;
   uint8_t *tab_ptr, *tab_end;
   unsigned int *ptr;
   unsigned int uiptr;

   if(v->fail_safe & 3) { return; }

   memset(&AP, 0, sizeof AP);

	if(v->fail_safe & 8)
	{		
	   // Поиск указателя на структуру Floating MP
	   fp = scan_for_floating_ptr_struct(0x0, 0x400);
	   if (fp == NULL) {
	      fp = scan_for_floating_ptr_struct(639*0x400, 0x400);
	   }
	   if (fp == NULL) {
	      fp = scan_for_floating_ptr_struct(0xf0000, 0x10000);
	   }
	   if (fp == NULL) {
	        // Поиск в области BIOS ESDS
	        unsigned int address = *(unsigned short *)0x40E;
	        address <<= 4;
					if (address) {
	       		fp = scan_for_floating_ptr_struct(address, 0x400);
	        }
	   }
	
	   if (fp != NULL) {
				// Найден указатель Floating MP
				// Это конфигурация по умолчанию?
				
				if (fp->feature[0] > 0 && fp->feature[0] <=7) {
				    // Это конфигурация по умолчанию, подставляем значения
				    num_cpus = 2;
				    APIC = (volatile apic_register_t*)0xFEE00000;
				    cpu_num_to_apic_id[0] = 0;
				    cpu_num_to_apic_id[1] = 1;
				    return;
				}
				
				// Есть ли указатель на таблицу конфигурации MP?
				if ( fp->phys_addr != 0) {
				    if (read_mp_config_table(fp->phys_addr)) {
							// Найдена корректная таблица MP, готово
							return;
				    }
				}
	   }
	}


   /* Таблица MP не найдена, пытаемся найти таблицу ACPI MADT.
    * Мы сначала пробуем таблицу MP, так как в MADT нет способа
    * отличить реальные ядра от потоков Hyper-Threading. */

   /* Поиск RSDP */
   rp = scan_for_rsdp(0xE0000, 0x20000);
   if (rp == NULL) {
        /* Поиск в области BIOS ESDS */
        unsigned int address = *(unsigned short *)0x40E;
        address <<= 4;
				if (address) {
       		rp = scan_for_rsdp(address, 0x400);
        }
   }
    
   if (rp == NULL) {
		/* RSDP не найден, прекращаем поиск */
		return;
   }

   /* RSDP найден, теперь получаем RSDT или XSDT */
   if (rp->revision >= 2) {
			rt = (rsdt_t *)rp->xrsdt[0];
			
			if (rt == 0) {
				return;
			}
			// Проверка валидности XSDT 
			if (*(unsigned int *)rt != XSDTSignature) {
				return;
			}
			if ( checksum((unsigned char*)rt, rt->length) != 0) {
				return;
			}
			
    } else {
			rt = (rsdt_t *)rp->rsdt;
			if (rt == 0) {
				return;
			}
			/* Проверка валидности RSDT */
			if (*(unsigned int *)rt != RSDTSignature) {
				return;
			}
			if ( checksum((unsigned char*)rt, rt->length) != 0) {
				return;
			}
    }

    /* Сканирование RSDT или XSDT для поиска указателя на MADT */
    tab_ptr = ((uint8_t*)rt) + sizeof(rsdt_t);
    tab_end = ((uint8_t*)rt) + rt->length;

    while (tab_ptr < tab_end) {

		uiptr = *((unsigned int *)tab_ptr);
		ptr = (unsigned int *)uiptr;

			/* Проверка сигнатуры MADT */
			if (ptr && *ptr == MADTSignature) {
		    /* Найдено, выполняем разбор */
		    if (parse_madt((uintptr_t)ptr)) {
					return;
		    }
			}
      tab_ptr += 4;
    }
}
	
unsigned my_apic_id()
{
   return (APIC[APICR_ID][0]) >> 24;
}

void smp_ap_booted(unsigned cpu_num) 
{
   AP[cpu_num].started = TRUE;
}

void smp_boot_ap(unsigned cpu_num)
{
   unsigned timeout;

   boot_ap(cpu_num);
   timeout = 0;
   do {
      delay(1000 / DELAY_FACTOR);
      timeout++;
   } while (!AP[cpu_num].started && timeout < 100000 / DELAY_FACTOR);

   if (!AP[cpu_num].started) {
      cprint(LINE_STATUS+3, 0, "SMP: Boot timeout for");
      dprint(LINE_STATUS+3, COL_MID, cpu_num,2,1);
      cprint(LINE_STATUS+3, 26, "Turning off SMP");
   }
}

unsigned smp_my_cpu_num()
{
   unsigned apicid = my_apic_id();
   unsigned i;

   for (i = 0; i < MAX_CPUS; i++) {
      if (apicid == cpu_num_to_apic_id[i]) {
	 break;
      }
   }
   if (i == MAX_CPUS) {
      i = 0;
   }
   return i;
}

/* Набор простых функций, используемых для сохранения назначенных порядковых номеров 
 * процессоров, так как они теряются после релокации (стек перезагружается).
 */
int num_to_ord[MAX_CPUS];
void smp_set_ordinal(int me, int ord)
{
	num_to_ord[me] = ord;
}

int smp_my_ord_num(int me)
{
	return num_to_ord[me];
}

int smp_ord_to_cpu(int me)
{
	int i;
	for (i=0; i<MAX_CPUS; i++) {
		if (num_to_ord[i] == me) return i;
	}
	return -1;
}