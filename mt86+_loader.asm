; Загрузчик для образов www.memtest.org, Эрик Ауэр (Eric Auer), 2003.
; Предполагается, что образ начинается с бут-сектора,
; размер которого (в секторах) для setup.S указан в байте по смещению
; 1f1h (497). Далее предполагается, что setup.S идет сразу за бут-сектором,
; а основной head.S memtest-а — после setup.S ...

; Эта версия основана на загрузчике memtestL, который загружает
; memtest.bin из отдельного файла. Данная версия предназначена для
; использования следующим образом (варианты для DOS / Unix):
; copy /b memteste.bin + memtest.bin memtest.exe
; cat memteste.bin memtest.bin > memtest.exe
; Преимущество в том, что вы получаете один файл, который можно
; сжать, например, с помощью http://upx.sf.net/ (UPX).

%define fullsize (150024 + buffer - exeh)
	; 150024 — это размер memtest86+ V5.01, подкорректируйте при необходимости!

%define stacksize 2048
%define stackpara ((stacksize + 15) / 16)

	; хитрость в том, что NASM считает, что заголовок будет частью
	; загруженного образа, поэтому мы используем "org на 20h байт раньше", чтобы это исправить:
	org 0e0h	; NASM думает, что после заголовка будет 100h
			; чего мы и добиваемся.

exeh:	db "MZ"
	dw fullsize % 512		; сколько загружать из
	dw (fullsize + 511) / 512	;      .exe в ОЗУ
	dw 0		; релокации не используются
	dw 2		; размер заголовка: 2 * 16 байт
	dw stackpara	; минимальная куча 128 * 16 байт, для стека
	dw stackpara	; больше кучи нам тоже не нужно
	dw (fullsize + 15) / 16		; SS находится после файла
			; смещения сегментов относительны PSPseg+10h
			; начальные DS и ES указывают на PSPseg, а файл
			; за исключением заголовков загружается в PSPseg+10h.

	dw stacksize-4	; начальное значение SP
	dw 0		; контрольная сумма отсутствует
	dw 100h		; начальный IP
	dw -10h		; начальный CS относительно PSPseg+10h
	dw 0		; таблицы релокаций нет, "смещение 0 в файле"
	dw 0		; это не оверлей
	db "MEMT"	; выравнивание до кратности 16 байтам

	; загружаемая часть начинается здесь (CS установлен так, что IP здесь равен 100h)

start:	; точка входа	; если используется obj + линковщик, используйте "..start:"
  mov    ah, 01h      
  mov    bh, 00h
  mov   cx, 2000h
  int    10h

	mov ax,cs	; ***
	mov ds,ax	; ***
	mov es,ax	; ***

		; проверка: процессор 386 или лучше:
	pushf   ; сохранить флаги
	xor ax,ax
	push ax
	popf    ; попытка сбросить все биты
	pushf
	pop ax
	and ax,0f000h
	cmp ax,0f000h
	jz noinst1      ; 4 старших бита зафиксированы в 1: 808x или 80186
	mov ax,0f000h
	push ax
	popf    ; попытка установить 4 старших бита
	pushf
	pop ax
	test ax,0f000h
	jz noinst1      ; 4 старших бита зафиксированы в 0: 80286
	popf	; восстановить флаги
	jmp short found386

noinst1:
	popf	; восстановить флаги
	mov dx,need386
	jmp generror


found386:		; теперь проверим, находится ли система в реальном режиме:
	smsw ax		; MSW — это младшая половина CR0
			; (smsw не является привилегированной инструкцией, в отличие от mov eax,cr0)
	test al,1	; включен ли флаг PE (защищенный режим)?
%ifndef DEBUG		; игнорировать результаты в режиме отладки
	jnz foundprotected
%endif
	jmp foundreal

foundprotected:
	mov dx,noreal
	jmp generror

; ------------

need386	db "Sorry, you need at least a 386 CPU to use Memtest86+."
	db 13,10,"$"
noreal	db "You cannot run Memtest86+ if the system already is in"
	db " protected mode.",13,10,"$"

; ------------

generror:	; общий выход по ошибке
	push cs
	pop ds
	push cs
	pop es
	mov ah,9
	int 21h
	mov ax,4c01h
	int 21h

; ------------

foundreal:
	mov cx,buffer+15
	shr cx,4		; смещение буфера в параграфах
	mov ax,cs
	add ax,cx		; смещение буфера в параграфах
				; теперь AX — сегмент буфера
	mov [cs:bufsetup+2],ax	; теперь это дальний указатель на бут-сектор
	mov cx,20h		; размер бут-сектора в параграфах
	add [cs:bufsetup+2],cx	; теперь это дальний указатель на setup
	movzx eax,ax
	shl eax,4		; линейное смещение буфера
	mov [cs:buflinear],eax

findpoint:		; теперь пропатчим загрузчик!
	mov al,[buffer+1f1h]	; размер setup.S в секторах
	; должно быть 4 ...
	inc al			; сам бут-сектор
	movzx eax,al
	shl eax,9		; логарифм 2 от размера сектора (размер сектора 512)
	add [cs:buflinear],eax	; теперь это линейный адрес head.S
	mov ax,[buffer+251h]	; должно быть jmp far dword (смещение, сегмент)
	cmp ax,0ea66h
	jz foundpatch
patchbug:			; не удалось пропатчить переход
	mov dx,nopatch
	jmp generror

gdtbug:
	mov dx,nogdt
	jmp generror

foundpatch:
	mov eax,[cs:buflinear]
	mov [buffer+253h],eax	; пропатчить переход в защищенный режим
	; (только смещение - селектор сегмента не меняется: плоский линейный CS)

findgdt:
	mov eax,[cs:buffer+20ch]	; должно быть смещение lgdt
	and eax,00ffffffh
	cmp eax,0016010fh	; lgdt ...
	jnz gdtbug

	mov ax,[cs:buffer+20fh]		; указатель на содержимое GDTR
	mov bx,ax
	mov eax,[cs:buffer+200h+bx+2]	; линейное смещение GDT
	and eax,1ffh	; предполагаем, что GDT в первом секторе setup.S
	; *** ПРЕДУПРЕЖДЕНИЕ: это необходимо, так как setup.S содержит
	; *** ЖЕСТКО ЗАКОДИРОВАННОЕ смещение setup.S по линейному адресу 90200h, которое
	; *** равно 90000h + bootsect.S ... недостаток Memtest86!

	mov cx,[cs:bufsetup+2]		; сегмент setup.S
	movzx ecx,cx
	shl ecx,4			; линейный адрес setup.S
	add eax,ecx			; исправленное линейное смещение GDT
	mov [cs:buffer+200h+bx+2],eax	; пропатчить

	;mov dx,trying
	;mov ah,9
	;int 21h

	;xor ax,ax
	;int 16h		; ждать нажатия клавиши пользователем

	mov ax,[cs:bufsetup+2]	; сегмент setup
	mov ds,ax	; установить подходящие сегменты данных для setup.S ...
	mov es,ax
	xor ax,ax
	mov fs,ax
	mov gs,ax

	cli
	lss sp,[cs:newstack]	; теперь стек в первых 64к!
	movzx esp,sp		; гарантировать 16-битный указатель стека
	; head.S Memtest86 предполагает, что может просто перевести SS в
	; линейный режим. Это поместило бы стек по адресу 0:200h или около того,
	; если мы не переместим стек...

%ifdef DEBUG
	mov ebp,[cs:buflinear]	; отобразится в логах отладки
	mov esi,[cs:bufsetup]	; отобразится в логах отладки
%endif

	jmp far [cs:bufsetup]
	; setup.S включит линию A20 (игнорируя HIMEM, просто используя
	; классический трюк с программированием 8042) и перейдет в защищенный
	; режим. Затем произойдет переход в head.S, который, к счастью, может
	; работать по любому смещению внутри линейного 4 ГБ сегмента CS ...

; ------------

buflinear	dd 0	; линейный адрес точки входа head.S
bufsetup	dw 0,0	; дальний указатель на точку входа setup.S

newstack	dw 03fch,0	; осторожно, стек перезапишет IDT.

; ------------

nopatch	db "jmp far dword not found at setup.S offset 37h,",13,10
	db "(file offset 237h is not 66h, 0eah)",13,10
	db "please adjust and recompile memtestl...",13,10,"$"

nogdt	db "lgdt [...] not found at setup.S offset 0ch,",13,10
	db "(file offset 20ch is not 0fh, 01h, 16h)",13,10
	db "please adjust and recompile memtestl...",13,10,"$"

trying	db "Now trying to start Memtest86...",13,10
	db "You have to reboot to leave Memtest86 again.",13,10
	db "Press a key to go on.",13,10,"$"

; ------------

	align 16
buffer:	; метка, указывающая на место в файле, где будет memtest.bin.
