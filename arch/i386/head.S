/*
 *  linux/boot/head.S
 *  Copyright (C) 1991, 1992  Линус Торвальдс
 */

/*
 *  head.S содержит 32-битный код запуска.
 *
 *  1 января 1996 г. Модифицировано Крисом Брэди для использования в качестве загрузчика для MemTest-86.
 *  Настройка управления памятью для плоской нестраничной линейной адресации.
 *  17 мая 2004 г.: Добавлена поддержка X86_PWRCAP для AMD64 (Memtest86+ - Самуэль Д.)
 */

.text
#include "defs.h"
#include "config.h"

	.code32
	.globl startup_32
startup_32:
	cld
	cli

	/* Убедиться в наличии указателя стека загрузки (boot_stack) */
	testl	%esp, %esp
	jnz 0f
	movl	$(LOW_TEST_ADR + _GLOBAL_OFFSET_TABLE_), %esp
	leal	boot_stack_top@GOTOFF(%esp), %esp
0:

	/* Загрузить указатель GOT */
	call	0f
0:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-0b], %ebx

	/* Выбрать соответствующий адрес boot_stack */
	leal	boot_stack_top@GOTOFF(%ebx), %esp

	/* Перезагрузить все сегментные регистры */
	leal	gdt@GOTOFF(%ebx), %eax
	movl	%eax, 2 + gdt_descr@GOTOFF(%ebx)
	lgdt	gdt_descr@GOTOFF(%ebx)
	leal	flush@GOTOFF(%ebx), %eax
	pushl	$KERNEL_CS
	pushl	%eax
	lret
flush:	movl	$KERNEL_DS, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

/*
 *  Обнуление BSS
 */
	cmpl	$1, zerobss@GOTOFF(%ebx)
	jnz	zerobss_done
	xorl	%eax, %eax
	leal	_bss@GOTOFF(%ebx), %edi
	leal	_end@GOTOFF(%ebx), %ecx
	subl	%edi, %ecx
1:	movl	%eax, (%edi)
	addl	$4, %edi
	subl	$4, %ecx
	jnz	1b
	movl	$0, zerobss@GOTOFF(%ebx)
zerobss_done:

/*
 * Настройка обработчика исключений
 */
	leal	idt@GOTOFF(%ebx), %edi

	leal	vec0@GOTOFF(%ebx), %edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx, %ax	/* селектор = 0x0010 = cs */
	movw	$0x8E00, %dx	/* шлюз прерывания - dpl=0, присутствует */
	movl	%eax, (%edi)
	movl	%edx, 4(%edi)
	addl	$8, %edi

	leal	vec1@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec2@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec3@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec4@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec5@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec6@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec7@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec8@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec9@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec10@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec11@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec12@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec13@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec14@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec15@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec16@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec17@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec18@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	leal	vec19@GOTOFF(%ebx),%edx
	movl	$(KERNEL_CS << 16),%eax
	movw	%dx,%ax		   /* селектор = 0x0010 = cs */
	movw	$0x8E00,%dx	   /* шлюз прерывания - dpl=0, присутствует */
	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi

	/* Теперь, когда всё инициализировано, загрузить таблицу дескрипторов прерываний */
	leal	idt@GOTOFF(%ebx), %eax
	movl	%eax, 2 + idt_descr@GOTOFF(%ebx)
	lidt	idt_descr@GOTOFF(%ebx)

	leal	_dl_start@GOTOFF(%ebx), %eax
	call	*%eax

	/* Никогда не забывайте инициализировать FPU ... Никогда! */ 
	finit

	call	test_start

	/* В случае возврата симулировать исключение */
	pushfl
	pushl	%cs
	call	0f
0:	pushl	$0 /* код ошибки */
	pushl	$257 /* вектор */
	jmp	int_hand

vec0:
	pushl	$0 /* код ошибки */
	pushl	$0 /* вектор */
	jmp int_hand
vec1:
	pushl	$0 /* код ошибки */
	pushl	$1 /* вектор */
	jmp int_hand

vec2:
	pushl	$0 /* код ошибки */
	pushl	$2 /* вектор */
	jmp int_hand

vec3:
	pushl	$0 /* код ошибки */
	pushl	$3 /* вектор */
	jmp	int_hand

vec4:
	pushl	$0 /* код ошибки */
	pushl	$4 /* вектор */
	jmp	int_hand

vec5:
	pushl	$0 /* код ошибки */
	pushl	$5 /* вектор */
	jmp	int_hand

vec6:
	pushl	$0 /* код ошибки */
	pushl	$6 /* вектор */
	jmp	int_hand

vec7:
	pushl	$0 /* код ошибки */
	pushl	$7 /* вектор */
	jmp	int_hand

vec8:
	/* код ошибки */
	pushl	$8 /* вектор */
	jmp	int_hand

vec9:
	pushl	$0 /* код ошибки */
	pushl	$9 /* вектор */
	jmp int_hand

vec10:
	/* код ошибки */
	pushl	$10 /* вектор */
	jmp	int_hand

vec11:
	/* код ошибки */
	pushl	$11 /* вектор */
	jmp	int_hand

vec12:
	/* код ошибки */
	pushl	$12 /* вектор */
	jmp	int_hand

vec13:
	/* код ошибки */
	pushl	$13 /* вектор */
	jmp	int_hand

vec14:
	/* код ошибки */
	pushl	$14 /* вектор */
	jmp	int_hand

vec15:
	pushl	$0 /* код ошибки */
	pushl	$15 /* вектор */
	jmp	int_hand

vec16:
	pushl	$0 /* код ошибки */
	pushl	$16 /* вектор */
	jmp	int_hand

vec17:
	/* код ошибки */
	pushl	$17 /* вектор */
	jmp	int_hand

vec18:
	pushl	$0 /* код ошибки */
	pushl	$18 /* вектор */
	jmp	int_hand

vec19:
	pushl	$0 /* код ошибки */
	pushl	$19 /* вектор */
	jmp	int_hand

int_hand:
	pushl	%eax
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%edi
	pushl	%esi
	pushl	%ebp

	/* оригинальный указатель boot_stack */
	leal	48(%esp), %eax
	pushl	%eax
	pushl	%ds  
	pushl	%ss 
	pushl	%esp /* указатель на структуру trap regs в boot_stack */
	call	inter
	addl	$8, %esp

	popl	%ebp
	popl	%esi
	popl	%edi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	iret

/*
 * Таблица дескрипторов прерываний имеет место для 32 записей (IDT)
 */
.align 4
.word 0
idt_descr:
	.word 20*8-1	       # idt содержит 32 записи
	.long 0

idt:
	.fill 20,8,0	       # idt не инициализирована

gdt_descr:
	.word gdt_end - gdt - 1
	.long 0

.align 4
.globl gdt, gdt_end
gdt:
	.quad 0x0000000000000000	/* NULL-дескриптор */
	.quad 0x0000000000000000	/* не используется */
	.quad 0x00cf9b000000ffff	/* 0x10 основной код 4 ГБ по адресу 0x000000 */
	.quad 0x00cf93000000ffff	/* 0x18 основные данные 4 ГБ по адресу 0x000000 */

	.word	0xFFFF				# 16-бит 64 КБ - (0x10000*1 = 64 КБ)
	.word	0				# базовый адрес = SETUPSEG
	.byte	0x00, 0x9b			# код чтение/выполнение/доступ
	.byte	0x00, 0x00			# гранулярность = байты


	.word	0xFFFF				# 16-бит 64 КБ - (0x10000*1 = 64 КБ)
	.word	0				# базовый адрес = SETUPSEG
	.byte	0x00, 0x93			# данные чтение/запись/доступ
	.byte	0x00, 0x00			# гранулярность = байты

gdt_end:

.data

.macro ptes64 start, count=64
.quad \start + 0x0000000 + 0xE3
.quad \start + 0x0200000 + 0xE3
.quad \start + 0x0400000 + 0xE3
.quad \start + 0x0600000 + 0xE3
.quad \start + 0x0800000 + 0xE3
.quad \start + 0x0A00000 + 0xE3
.quad \start + 0x0C00000 + 0xE3
.quad \start + 0x0E00000 + 0xE3
.if \count-1
ptes64 "(\start+0x01000000)",\count-1
.endif
.endm

.macro maxdepth depth=1
.if \depth-1
maxdepth \depth-1
.endif
.endm

maxdepth

# Таблицы каталогов страниц (Page Directory Tables):
# Существует 4 таблицы, первые две отображают первые 2 ГБ памяти. Последние две используются с
# PAE для отображения остальной памяти сегментами по 2 ГБ. Последние две таблицы изменяются в vmem.c
# для отображения каждого сегмента. Мы используем страницы размером 2 МБ, поэтому используется 
# только таблица каталогов страниц (без таблиц страниц). 
.balign 4096
.globl pd0
pd0:
	ptes64 0x0000000000000000

.balign 4096
.globl pd1
pd1:
	ptes64 0x0000000040000000

.balign 4096
.globl pd2
pd2:
	ptes64 0x0000000080000000

.balign 4096
.globl pd3
pd3:
	ptes64 0x00000000C0000000

# Таблица указателей каталогов страниц устаревшего режима (Legacy Mode):
# 4 записи, указывающие на таблицы каталогов страниц
.balign 4096
.globl pdp
pdp:
	.long pd0 + 1
	.long 0
	.long pd1 + 1
	.long 0
	.long pd2 + 1
	.long 0
	.long pd3 + 1
	.long 0

# Таблица указателей каталогов страниц длинного режима (Long Mode):
# 4 записи, указывающие на таблицы каталогов страниц
.balign 4096
lpdp:
	.long pd0 + 3
	.long 0
	.long pd1 + 3
	.long 0
	.long pd2 + 3
	.long 0
	.long pd3 + 3
	.long 0


# Таблица отображения страниц 4-го уровня для длинного режима (pml4)
.balign 4096
.globl pml4
pml4:
	.long lpdp + 3
	.long 0
.previous

#define RSTART startup_32

	.globl query_pcbios
query_pcbios:
	/* Сохранить регистры, сохраняемые вызывающей стороной */
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx

	/* Вычислить адрес релокации */
	leal	RSTART@GOTOFF(%ebx), %esi

	/* Исправить указатель на код реального режима */
	movl	%esi, %eax
	shrl	$4, %eax
	movw	%ax, 2 + realptr@GOTOFF(%ebx)

	/* Исправить указатель на код защищенного режима */
	leal	prot@GOTOFF(%ebx), %eax
	movl	%eax, protptr@GOTOFF(%ebx)

	/* Вычислить исправление (fixup) для GDT */
	movl	%esi, %eax
	shll	$16, %eax	# Младшая часть базы

	movl	%esi, %ecx
	shrl	$16, %ecx
	andl	$0xff, %ecx

	movl	%esi, %edx
	andl	$0xff000000, %edx
	orl	%edx, %ecx

	/* Исправить GDT */
	andl	$0x0000ffff, REAL_CS + 0 + gdt@GOTOFF(%ebx)
	orl	%eax,        REAL_CS + 0 + gdt@GOTOFF(%ebx)
	andl	$0x00ffff00, REAL_CS + 4 + gdt@GOTOFF(%ebx)
	orl	%ecx,        REAL_CS + 4 + gdt@GOTOFF(%ebx)
	andl	$0x0000ffff, REAL_DS + 0 + gdt@GOTOFF(%ebx)
	orl	%eax,        REAL_DS + 0 + gdt@GOTOFF(%ebx)
	andl	$0x00ffff00, REAL_DS + 4 + gdt@GOTOFF(%ebx)
	orl	%ecx,        REAL_DS + 4 + gdt@GOTOFF(%ebx)

	/* Исправить gdt_descr */
	leal	gdt@GOTOFF(%ebx), %eax
	movl	%eax, 2 + gdt_descr@GOTOFF(%ebx)

	lidt	idt_real@GOTOFF(%ebx)

	/* Не отключать линию A20 */

	/* Загрузить 16-битные сегменты данных, чтобы обеспечить установку лимитов сегментов */
	movl	$REAL_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	/* Вычислить базу boot_stack */
	leal	boot_stack@GOTOFF(%ebx), %ecx
	/* Вычислить адрес meminfo */
	leal	mem_info@GOTOFF(%ebx), %edi

	/* переключиться в 16-битный режим */
	ljmp	$REAL_CS, $1f - RSTART
1:
	.code16
	/* Отключить страничную организацию и защищенный режим */
	/* сбросить биты PG и PE в CR0 */
	movl	%cr0,%eax
	andl	$~((1 << 31)|(1<<0)),%eax
	movl	%eax,%cr0

	/* выполнить межсегментный переход для очистки конвейера процессора
	 * и перезагрузки %cs:%eip (чтобы очистить верхние 16 бит %eip).
	 */
	ljmp	*(realptr - RSTART)
real:
	/* теперь мы в реальном режиме
	 * Настройка сегментных регистров реального режима: %ds, %ss, %es, %gs, %fs
	 */
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Настроить указатель boot_stack */
	movl	%ecx, %eax
	shrl	$4, %eax
	movw	%ax, %ss
	subl	%ecx, %esp

	/* Сохранить мой указатель базы */
	pushl	%ebx

	/* Настроить %ds так, чтобы он указывал на мою область данных */
	shrl	$4, %edi
	movl	%edi, %ds

	/* Включить прерывания, иначе BIOS может повести себя непредсказуемо */
	sti

# Получить размер памяти (расширенная память, КБ)

#define SMAP	0x534d4150

	xorl	%eax, %eax
	movl	%eax, (E88)
	movl	%eax, (E801)
	movl	%eax, (E820NR)

# Попробовать три различные схемы обнаружения памяти. Сначала e820h, 
# который позволяет собрать карту памяти, затем e801h,
# который возвращает 32-битный размер памяти, и наконец 88h, который
# возвращает 0-64 МБ.

# метод E820H:
# карта памяти «из ада». e820h возвращает память, классифицированную по
# множеству различных типов, допускает «дыры» в памяти и
# прочее. Мы сканируем эту карту памяти и строим список
# первых 32 областей памяти, который возвращаем в [E820MAP].
# Это документировано по адресу http://www.teleport.com/~acpi/acpihtml/topic245.htm

meme820:
	xorl	%ebx, %ebx			# счетчик продолжения
	movw	$E820MAP, %di			# указать на белый список,
						# чтобы BIOS мог
						# писать напрямую в него.

jmpe820:
	movl	$0x0000e820, %eax		# e820, старшее слово обнулено
	movl	$SMAP, %edx			# ASCII 'SMAP'
	movl	$20, %ecx			# размер записи e820rec
	pushw	%ds				# запись данных.
	popw	%es
	int	$0x15				# выполнить вызов
	jc	bail820				# перейти к e801 в случае неудачи

	cmpl	$SMAP, %eax			# проверить, что возвращено 'SMAP'
	jne	bail820				# перейти к e801 в случае неудачи

#	cmpl	$1, 16(%di)			# это полезная память?
#	jne	again820

	# Если это полезная память, мы сохраняем её, просто сдвигая %di на
	# размер sizeof(e820rec).
	#
good820:
	movb	(E820NR), %al			# до 32 записей
	cmpb	$E820MAX, %al
	jnl	bail820

	incb	(E820NR)
	movw	%di, %ax
	addw	$E820ENTRY_SIZE, %ax
	movw	%ax, %di
again820:
	cmpl	$0, %ebx			# проверить,
	jne	jmpe820				# установлен ли %ebx в EOF
bail820:


# метод E801H:
# размер памяти в блоках по 1 КБ, чтобы не путать loadlin.
# мы сохраняем размер памяти из 0xe801 в совершенно другом месте,
# так как он, скорее всего, будет больше 16 бит.

meme801:
	stc					# исправление для обхода бажных
	xorw	%cx,%cx				# BIOS, которые не очищают/устанавливают
	xorw	%dx,%dx				# перенос (carry) при успехе/ошибке
						# вызова размера памяти e801h
						# или просто передают cx,dx без изменений.
	movw	$0xe801, %ax
	int	$0x15
	jc	mem88

	cmpw	$0x0, %cx			# Заплатка для обработки BIOS,
	jne	e801usecxdx			# которые сообщают расширенную
	cmpw	$0x0, %dx			# память в AX/BX вместо
	jne	e801usecxdx			# CX/DX. Спецификация, которую я читал,
	movw	%ax, %cx			# указывает на то, что AX/BX
	movw	%bx, %dx			# в любом случае более разумны...

e801usecxdx:
	andl	$0xffff, %edx			# очистить знаковое расширение
	shll	$6, %edx			# и перейти от блоков по 64 КБ к блокам по 1 КБ
	movl	%edx, (E801)			# сохранить размер расширенной памяти
	andl	$0xffff, %ecx			# очистить знаковое расширение
 	addl	%ecx, (E801)			# и добавить нижнюю память к
						# общему размеру.

# Старый традиционный метод. Возвращает размер памяти (до 16 МБ или
# 64 МБ, в зависимости от BIOS) в ax.
mem88:

	movb	$0x88, %ah
	int	$0x15
	movw	%ax, (E88)

#ifdef APM_OFF
# проверка наличия APM BIOS
	movw	$0x5300, %ax    # проверка установки APM BIOS
	xorw	%bx, %bx
	int	$0x15
	jc	done_apm_bios   # ошибка -> нет APM BIOS

	cmpw	$0x504d, %bx    # проверка подписи "PM"
	jne	done_apm_bios   # нет подписи -> нет APM BIOS

	movw	$0x5304, %ax    # Сначала отключиться на всякий случай
	xorw	%bx, %bx
	int	$0x15           # игнорировать код возврата

	movw	$0x5301, %ax    # Подключение в реальном режиме
	xorw	%bx, %bx
	int	$0x15
	jc	done_apm_bios   # ошибка

	movw	$0x5308, %ax    # Отключить APM
	mov	$0xffff, %bx
	xorw	%cx, %cx
	int	$0x15

done_apm_bios:
#endif

	/* Окей, опрос BIOS завершен, возвращаемся в защищенный режим */
	cli

	/* Восстановить сохраненные переменные */
	popl	%ebx

	/* Получить подходящий %ds */
	movw	%cs, %ax
	movw	%ax, %ds

	/* Загрузить глобальную таблицу дескрипторов (GDT) */
	addr32 lgdt	gdt_descr - RSTART

	/* Включить защищенный режим */
	/* Установить бит PE в CR0 */
	movl	%cr0,%eax
	orl	$(1<<0),%eax
	movl	%eax,%cr0

	/* очистить очередь предвыборки и перезагрузить %cs:%eip */
	data32 ljmp	*(protptr - RSTART)
prot:
	.code32
	/* Перезагрузить остальные сегментные регистры */
	movl	$KERNEL_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss

	/* Настроить указатель boot_stack */
	leal	boot_stack@GOTOFF(%ebx), %eax
	addl	%eax, %esp

	/* Восстановить регистры, сохраненные вызывающей стороной */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	movl	$1, %eax
	ret

realptr:
	.word	real - RSTART
	.word	0x0000
protptr:
	.long	0
	.long	KERNEL_CS

idt_real:
	.word	0x400 - 1			# лимит idt (256 записей)
	.word	0, 0				# база idt = 0L

/* _ap_trampoline_start — это точка входа для процессоров, отличных от 
 * основного (bootstrap). Код между _ap_trampoline_start и 
 * _ap_trampoline_protmode копируется в BootCodeStart (0x9000). 
 * Инструкция ljmp после включения CR0.PE выполнит переход на 
 * релоцируемый код, который обычно находится по адресу 0x10000 + _ap_trampoline_protmode.
 *
 * Код трамплина использует временную GDT. Записи этой временной 
 * GDT должны соответствовать первым нескольким записям GDT, используемой 
 * релоцируемым кодом memtest (см. символ 'gdt' в этом файле).
 *
 */
	.globl _ap_trampoline_start
	.globl _ap_trampoline_protmode
	.code16
_ap_trampoline_start:
	lgdt    0x0 /* будет исправлено позже, см. smp.c:BootAP()*/
	movl	%cr0, %eax
	orl	$1, %eax
	movl	%eax, %cr0
	data32 ljmp    $KERNEL_CS, $_ap_trampoline_protmode
_ap_trampoline_protmode:
	.code32
	movw	$KERNEL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
	movl	$(LOW_TEST_ADR + _GLOBAL_OFFSET_TABLE_), %esp
	leal	boot_stack_top@GOTOFF(%esp), %esp
	pushl   $0
	popf
	call    startup_32
	/* если мы когда-нибудь вернемся, просто зациклимся навсегда */
	cli
2:	hlt
	jmp 2b	
.data
zerobss:	.long	1
.previous
.data
.balign 16
	.globl mem_info
mem_info:
	. = . + MEMINFO_SIZE
.previous
.bss
.balign 16
boot_stack:
	.globl boot_stack
	. = . + 4096
boot_stack_top:
	.globl boot_stack_top
.previous