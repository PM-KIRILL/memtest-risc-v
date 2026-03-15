/*
 * setup.s отвечает за получение системных данных из BIOS
 * и их размещение в соответствующих местах системной памяти.
 * и setup.s, и сама система были загружены загрузочным блоком (bootblock).
 *
 * 1-янв-96 Изменено Крисом Брэди для использования в качестве загрузчика memtest-86.
 */

#include "defs.h"

.code16
.section ".setup", "ax", @progbits
.globl start
start:
# Итак, чтение прошло успешно 
# теперь мы хотим перейти в защищенный режим ...


	cli			# прерывания запрещены #
	movb	$0x80, %al	# отключить NMI для последовательности загрузки
	outb	%al, $0x70

# Система переместит себя на положенное ей место.
# перезагружаем сегментные регистры и стек, так как 
# AP-процессоры также выполняют этот код
#ljmp	$INITSEG, $(reload - start + 0x200)
reload:
	movw	$INITSEG, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %ss	# сброс стека в INITSEG:0x4000-12.
	movw	%dx, %sp
	push	%cs
	pop	%ds
	lidt	idt_48 - start	# загрузка idt значениями 0,0
	lgdt	gdt_48 - start	# загрузка gdt соответствующими значениями

# это было безболезненно, теперь включаем A20
# начало из grub-a20.patch
     	/*
     	* пытаемся переключить линию A20 через PORT92, регистр
     	* "Fast A20 and Init"
     	*/
     	mov	$0x92, %dx
     	inb	%dx, %al
     	/* пропустить код port92, если он не реализован (чтение возвращает 0xff) */
     	cmpb	$0xff, %al
     	jz	alt_a20_done
     	
     	/* установить или очистить бит 1, бит ALT_A20_GATE */
     	movb	4(%esp), %ah
     	testb	%ah, %ah
     	jz	alt_a20_cont1
     	orb	$2, %al
     	jmp	alt_a20_cont2
alt_a20_cont1:
	and	$0xfd, %al

	/* очистить бит INIT_NOW; чтобы случайно не перезагрузить машину */
alt_a20_cont2:
	and	$0xfe, %al
	outb	%al, %dx

alt_a20_done:
# конец из grub-a20.patch

	call    empty_8042
	
	movb	$0xD1, %al	# запись команды
	outb	%al, $0x64
	call    empty_8042	

	movb	$0xDF, %al	# A20 включена
	outb	%al, $0x60
	call	empty_8042

/*
 * Обратите внимание, что короткий переход (short jump) не является строго обязательным,
 * хотя есть причины, по которым это может быть хорошей идеей. В любом случае это не повредит.
 */
	movw	$0x0001, %ax	# бит защищенного режима (PE)
	lmsw	%ax		# Вот оно! #
	jmp	flush_instr
flush_instr:
	movw	$KERNEL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	%ax, %fs
	movw	%ax, %gs

data32	ljmp	$KERNEL_CS, $(TSTLOAD <<4)	# переход на смещение 2000 сегмента 0x10 (cs)

/*
 * Эта подпрограмма проверяет, пуста ли очередь команд клавиатуры
 * (после очистки выходных буферов)
 *
 * Тайм-аут не используется — если выполнение зависнет здесь, значит с машиной
 * что-то не так, и мы, вероятно, все равно не смогли бы продолжить.
 */
empty_8042:
	call	delay
	inb	$0x64, %al	# порт состояния 8042
	cmpb	$0xff, %al	# из grub-a20-patch, пропустить если не реализовано
	jz	empty_8042_ret
	testb	$1, %al		# есть данные в выходном буфере?
	jz	no_output
	call	delay
	inb	$0x60, %al	# прочитать их
	jmp	empty_8042
	
no_output:
	testb	$2, %al		# входной буфер полон?
	jnz	empty_8042	# да — цикл
empty_8042_ret:
	ret
#
# Задержка необходима после выполнения операций ввода-вывода
#
delay:
	.word	0x00eb			# jmp $+2
	ret

gdt:
	.word	0,0,0,0		# пустая запись

	.word	0,0,0,0		# не используется

	.word	0x7FFF		# лимит 128мб
	.word	0x0000		# базовый адрес=0
	.word	0x9A00		# код чтение/выполнение
	.word	0x00C0		# гранулярность=4096, 386

	.word	0x7FFF		# лимит 128мб
	.word	0x0000		# базовый адрес=0
	.word	0x9200		# данные чтение/запись
	.word	0x00C0		# гранулярность=4096, 386

idt_48:
	.word	0			# лимит idt=0
	.long	0			# база idt=0L

gdt_48:
	.word	0x800		# лимит gdt=2048, 256 записей GDT
	.word	512+gdt - start,0x9	# база gdt = 0X9xxxx

msg1:
	.asciz "Setup.S\r\n"

	/* Дополнение setup до нужного размера */
	.org	(SETUPSECS*512)