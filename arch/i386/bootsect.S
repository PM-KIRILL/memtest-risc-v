/*
 *	 bootsect.s		Copyright (C) 1991, 1992 Linus Torvalds
 *
 * bootsect.s загружается по адресу 0x7c00 процедурами запуска BIOS, 
 * перемещает себя по адресу 0x90000 и переходит туда.
 *
 * Затем он загружает 'setup' сразу после себя (0x90200) и систему
 * по адресу 0x10000, используя прерывания BIOS.
 *
 * Загрузчик сделан максимально простым, и непрерывные ошибки чтения
 * приведут к бесконечному циклу. Перезагрузите вручную. Он загружается
 * довольно быстро, считывая целые дорожки за раз, когда это возможно.
 *
 * 1-янв-96 Модифицировано Крисом Брэди для использования в качестве загрузчика MemTest-86.
 */

#include "defs.h"

ROOT_DEV = 0

.code16
.section ".bootsect", "ax", @progbits
_boot:


# ld86 требует символ точки входа. Это вполне может быть обычный символ.
.globl	_main
_main:
	movw	$BOOTSEG, %ax
	movw	%ax, %ds
	movw	$INITSEG, %ax
	movw	%ax, %es
	movw	$256, %cx
	subw	%si, %si
	subw	%di, %di
	cld
	rep
	movsw
	ljmp	$INITSEG, $go - _boot

go:
	movw	%cs, %ax
	movw	$(0x4000-12), %dx	# 0x4000 — произвольное значение >= длина
					# bootsect + длина setup + место для стека
					# 12 — размер параметров диска

# bde - изменено 0xff00 на 0x4000 для использования отладчика с адреса 0x6400 и выше (bde).
# Нам не пришлось бы беспокоиться об этом, если бы мы проверяли вершину памяти. Также
# мой BIOS можно настроить на размещение таблиц дисков wini в верхней памяти
# вместо таблицы векторов. Старый стек мог повредить таблицу дисков.

	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss		# поместить стек в INITSEG:0x4000-12.
	movw	%dx, %sp

/*
 *	Таблицы параметров диска по умолчанию во многих BIOS не
 *	распознают чтение нескольких секторов сверх максимального номера сектора,
 *	указанного в таблицах параметров дискет по умолчанию — в некоторых
 *	случаях это может означать 7 секторов.
 *
 *	Поскольку чтение по одному сектору происходит медленно и не обсуждается,
 *	мы должны позаботиться об этом, создав новые таблицы параметров
 *	(для первого диска) в ОЗУ. Мы установим максимальное количество
 *	секторов равным 18 — максимум, который встречается на HD 1.44.
 *
 *	Высокое значение не вредит. Низкое — вредит.
 *
 *	Сегменты следующие: ds=es=ss=cs - INITSEG,
 *		fs = 0, gs = сегмент таблицы параметров
 */
	pushw	$0
	popw	%fs
	movw	$0x78, %bx		# fs:bx — адрес таблицы параметров
	lgs	%fs:(%bx),%si		# gs:si — источник

	movw	%dx, %di		# es:di — место назначения
	movw	$6, %cx			# скопировать 12 байт
	cld

	rep	movsw %gs:(%si), (%di)

	movw	%dx, %di
	movb	$18, 4(%di)		# пропатчить количество секторов

	movw	%di, %fs:(%bx)
	movw	%es, %fs:2(%bx)

	movw	%cs, %ax
	movw	%ax, %fs
	movw	%ax, %gs

	xorb	%ah, %ah		# сброс FDC
	xorb	%dl, %dl
	int	$0x13

# загрузить setup-секторы сразу после загрузочного блока.
# Обратите внимание, что 'es' уже настроен.

load_setup:
	xorw	%dx, %dx			# диск 0, головка 0
	movw	$0x0002, %cx			# сектор 2, дорожка 0
	movw	$0x0200, %bx			# адрес = 512, в INITSEG
	movw	$(0x0200 + SETUPSECS), %ax	# сервис 2, кол-во секторов
						# (предполагаем, что все на головке 0, дорожке 0)
	int	$0x13				# прочитать его
	jnc	ok_load_setup			# ок - продолжить

	pushw	%ax			# дамп кода ошибки
	call	print_nl
	movw	%sp, %bp
	call	print_hex
	popw	%ax

	xorb	%dl, %dl		# сброс FDC
	xorb	%ah, %ah
	int	$0x13
	jmp	load_setup

ok_load_setup:

# Получить параметры дисковода, в частности кол-во секторов на дорожку


/* Похоже, что вызова BIOS для получения количества секторов не существует. 
 * Предполагаем 18 секторов, если сектор 18 может быть прочитан, 15, если 
 * может быть прочитан сектор 15. В противном случае предполагаем 9.
 */

	xorw	%dx, %dx			# диск 0, головка 0
	movw	$0x0012, %cx			# сектор 18, дорожка 0
	movw	$(0x200+(SETUPSECS*0x200)), %bx	# адрес после setup (es = cs)
	movw	$0x0201, %ax			# сервис 2, 1 сектор
	int	$0x13
	jnc	got_sectors
	movb	$0x0f, %cl			# сектор 15
	movw	$0x0201, %ax			# сервис 2, 1 сектор
	int	$0x13
	jnc	got_sectors
	movb	$0x09, %cl

got_sectors:
	movw	%cx, %cs:sectors - _boot
	movw	$INITSEG, %ax
	movw	%ax, %es

# Напечатать какое-нибудь бессмысленное сообщение

	movb	$0x03, %ah		# прочитать позицию курсора
	xorb	%bh, %bh
	int	$0x10

	movw	$9, %cx
	movw	$0x0007, %bx		# страница 0, атрибут 7 (нормальный)
	movw	$msg1 - _boot, %bp
	movw	$0x1301, %ax		# вывести строку, переместить курсор
	int	$0x10

# хорошо, мы вывели сообщение, теперь
# мы хотим загрузить систему (по адресу 0x10000)

	movw	$TSTLOAD, %ax
	movw	%ax, %es		# сегмент 0x010000
	call	read_it
	call	kill_motor
	call  turnoffcursor
	call	print_nl

# после этого (когда всё загружено), мы переходим к
# процедуре setup, загруженной сразу после
# загрузочного блока:

	ljmp	$SETUPSEG,$0

# Эта процедура загружает систему по адресу 0x10000, гарантируя, что
# границы 64 КБ не будут пересечены. Мы стараемся загружать её как можно быстрее,
# считывая целые дорожки всякий раз, когда можем.
#
# вход:	es - сегмент начального адреса (обычно 0x1000)
#
sread:	.word 1+SETUPSECS	# прочитано секторов текущей дорожки
head:	.word 0			# текущая головка
track:	.word 0			# текущая дорожка

read_it:
	movw	%es, %ax
	testw	$0x0fff, %ax
die:
	jne	die		# es должен быть на границе 64 КБ
	xorw	%bx,%bx		# bx — начальный адрес внутри сегмента
rp_read:
	movw	%es, %ax
	subw	$TSTLOAD, %ax	# загрузили ли мы уже всё?
	cmpw	syssize - _boot, %ax
	jbe	ok1_read
	ret
ok1_read:
	movw	%cs:sectors - _boot, %ax
	subw	sread - _boot, %ax
	movw	%ax, %cx
	shlw	$9, %cx
	addw	%bx, %cx
	jnc	ok2_read
	je	ok2_read
	xorw	%ax, %ax
	subw	%bx, %ax
	shrw	$9, %ax
ok2_read:
	call	read_track
	movw	%ax, %cx
	add	sread - _boot, %ax
	cmpw	%cs:sectors - _boot, %ax
	jne	ok3_read
	movw	$1, %ax
	subw	head - _boot, %ax
	jne	ok4_read
	incw	track - _boot
ok4_read:
	movw	%ax, head - _boot
	xorw	%ax, %ax
ok3_read:
	movw	%ax, sread - _boot
	shlw	$9, %cx
	addw	%cx, %bx
	jnc	rp_read
	movw	%es, %ax
	addb	$0x10, %ah
	movw	%ax, %es
	xorw	%bx, %bx
	jmp	rp_read

read_track:
	pusha
	pusha
	movw	$0xe2e, %ax 	# сообщение loading... 2e = .
	movw	$7, %bx
	int	$0x10
	popa

	movw	track - _boot, %dx
	movw	sread - _boot, %cx
	incw	%cx
	movb	%dl, %ch
	movw	head - _boot, %dx
	movb	%dl, %dh
	andw	$0x0100, %dx
	movb	$2, %ah

	pushw	%dx				# сохранить для дампа ошибок
	pushw	%cx
	pushw	%bx
	pushw	%ax

	int	$0x13
	jc	bad_rt
	addw	$8, %sp
	popa
	ret

bad_rt:
	pushw	%ax				# сохранить код ошибки
	call	print_all			# ah = ошибка, al = чтение

	xorb	%ah, %ah
	xorb	%dl, %dl
	int	$0x13

	addw	$10, %sp
	popa
	jmp read_track

/*
 *	print_all предназначена для целей отладки.
 *	Она выводит все регистры. Предполагается, что она вызывается из
 *	процедуры со структурой стека:
 *	dx
 *	cx
 *	bx
 *	ax
 *	error
 *	ret <- sp
 *
*/

print_all:
	movw	$5, %cx		# код ошибки + 4 регистра
	movw	%sp, %bp

print_loop:
	pushw	%cx		# сохранить оставшийся счетчик
	call	print_nl	# nl для читаемости

	cmpb	5, %cl		# проверить, нужно ли имя регистра
	jae	no_reg

	movw	$(0xe05	+ 'A' - 1), %ax
	subb	%cl, %al
	int	$0x10
	movb	$'X', %al
	int	$0x10
	movb	$':', %al
	int	$0x10

no_reg:
	addw	$2, %bp		# следующий регистр
	call	print_hex	# напечатать его
	popw	%cx
	loop	print_loop
	ret

print_nl:
	movw	$0xe0d, %ax	# CR
	int	$0x10
	movb	$0x0a, %al	# LF
	int	$0x10
	ret

/*
 *	print_hex предназначена для целей отладки и выводит слово,
 *	на которое указывает ss:bp, в шестнадцатеричном виде.
 */

print_hex:
	movw	$4, %cx		# 4 шестнадцатеричные цифры
	movw	(%bp), %dx	# загрузить слово в dx

print_digit:
	rolw	$4, %dx		# циклический сдвиг, чтобы использовать младшие 4 бита
	movb	$0xe, %ah
	movb	%dl, %al	# маскируем, чтобы получить только следующий ниббл
	andb	$0xf, %al
	addb	$'0', %al	# преобразовать в цифру на базе 0
	cmpb	$'9', %al	# проверка на переполнение
	jbe	good_digit
	addb	$('A' - '0' - 10), %al

good_digit:
	int	$0x10
	loop	print_digit
	ret


/*
 * Эта процедура выключает мотор дисковода гибких дисков, чтобы
 * мы входили в ядро в известном состоянии и нам не
 * пришлось беспокоиться об этом позже.
 */
kill_motor:
	pushw	%dx
	movw	$0x3f2, %dx
	xorb	%al, %al
	outb	%al, %dx
	popw	%dx
	ret

turnoffcursor:
  movb  $0x01, %ah      # выключить курсор
  movb  $0x00, %bh
  movw  $0x2000, %cx
  int   $0x10 
	ret
	
sectors:
	.word 0

msg1:
	.byte 13,10
	.ascii "Loading"

.org 497
setup_sects:
	.byte SETUPSECS
.org 500
syssize:
	.word _syssize
.org 508
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55
_eboot: