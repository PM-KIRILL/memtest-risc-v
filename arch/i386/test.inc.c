#define HAS_OPT_ADDR_TST2  1
#define HAS_OPT_MOVINVR    1
#define HAS_OPT_MOVINV1    1
#define HAS_OPT_MOVINV32   1
#define HAS_OPT_MODTST     1
#define HAS_OPT_BLOCK_MOVE 1

OPTIMIZED_SNIPPET void addr_tst2_snippet1(uint32_t *p, uint32_t *pe)
{
	asm __volatile__ (
		"jmp L91\n\t"
		".p2align 4,,7\n\t"
		"L90:\n\t"
		"addl $4,%%edi\n\t"
		"L91:\n\t"
		"movl %%edi,(%%edi)\n\t"
		"cmpl %%edx,%%edi\n\t"
		"jb L90\n\t"
		: : "D" (p), "d" (pe)
	);
}

OPTIMIZED_SNIPPET void addr_tst2_snippet2(uint32_t *p, uint32_t *pe)
{
	asm __volatile__ (
		"jmp L95\n\t"
		".p2align 4,,7\n\t"
		"L99:\n\t"
		"addl $4,%%edi\n\t"
		"L95:\n\t"
		"movl (%%edi),%%ecx\n\t"
		"cmpl %%edi,%%ecx\n\t"
		"jne L97\n\t"
		"L96:\n\t"
		"cmpl %%edx,%%edi\n\t"
		"jb L99\n\t"
		"jmp L98\n\t"

		"L97:\n\t"
		"pushl %%edx\n\t"
		"pushl %%ecx\n\t"
		"pushl %%edi\n\t"
		"call ad_err2\n\t"
		"popl %%edi\n\t"
		"popl %%ecx\n\t"
		"popl %%edx\n\t"
		"jmp L96\n\t"

		"L98:\n\t"
		: : "D" (p), "d" (pe)
		: "ecx"
	);
}

OPTIMIZED_SNIPPET void movinvr_snippet1(uint32_t *p, uint32_t *pe, unsigned me)
{
	asm __volatile__ (
		"jmp L200\n\t"
		".p2align 4,,7\n\t"
		"L201:\n\t"
		"addl $4,%%edi\n\t"
		"L200:\n\t"
		"pushl %%ecx\n\t" \
		"call memtest_rand\n\t"
		"popl %%ecx\n\t" \
		"movl %%eax,(%%edi)\n\t"
		"cmpl %%ebx,%%edi\n\t"
		"jb L201\n\t"
		: : "D" (p), "b" (pe), "c" (me)
		: "eax"
	);
}

OPTIMIZED_SNIPPET void movinvr_snippet2(uint32_t *p, uint32_t *pe, uint32_t xorVal, unsigned me)
{
	asm __volatile__ (
		"pushl %%ebp\n\t"

		// Пропустить первое приращение
		"jmp L26\n\t"
		".p2align 4,,7\n\t"

		// увеличить на 4 байта (32 бита)
		"L27:\n\t"
		"addl $4,%%edi\n\t"

		// Проверить этот байт
		"L26:\n\t"

		// Получить следующее случайное число, передать me(edx), случайное значение возвращается в num(eax)
		// num = memtest_rand(me);
		// вызов cdecl сохраняет все регистры, кроме eax, ecx и edx
		// Мы сохраняем edx с помощью push и pop, используя его также в качестве входного параметра
		// нам не нужно текущее значение eax, и мы хотим, чтобы оно изменилось на возвращаемое значение
		// мы перезаписываем ecx вскоре после этого, отбрасывая его текущее значение
		"pushl %%edx\n\t" // Поместить входные данные функции в стек
		"call memtest_rand\n\t"
		"popl %%edx\n\t" // Удалить входные данные функции из стека

		// Выполнить XOR случайного числа с xorVal(ebx), которое равно либо 0xffffffff, либо 0, в зависимости от внешнего цикла
		// if (i) { num = ~num; }
		"xorl %%ebx,%%eax\n\t"

		// Переместить текущее значение текущей позиции p(edi) в bad(ecx)
		// (bad=*p)
		"movl (%%edi),%%ecx\n\t"

		// Сравнить bad(ecx) с num(eax)
		"cmpl %%eax,%%ecx\n\t"

		// Если не равно, перейти к обработке ошибки
		"jne L23\n\t"

		// Установить новое значение (или не num(eax)) в текущей позиции p(edi)
		// *p = ~num;
		"L25:\n\t"
		"movl $0xffffffff,%%ebp\n\t"
		"xorl %%ebp,%%eax\n\t"
		"movl %%eax,(%%edi)\n\t"

		// Цикл до тех пор, пока текущая позиция p(edi) не станет равной конечной позиции pe(esi)
		"cmpl %%esi,%%edi\n\t"
		"jb L27\n\t"
		"jmp L24\n"

		// Случай ошибки
		"L23:\n\t"
		// Необходимо вручную сохранять eax, ecx и edx в рамках соглашения о вызовах cdecl
		"pushl %%edx\n\t"
		"pushl %%ecx\n\t" // Следующие три push — это входные данные функции
		"pushl %%eax\n\t"
		"pushl %%edi\n\t"
		"call error\n\t"
		"popl %%edi\n\t" // Удалить входные данные функции из стека и восстановить значения регистров
		"popl %%eax\n\t"
		"popl %%ecx\n\t"
		"popl %%edx\n\t"
		"jmp L25\n"

		"L24:\n\t"
		"popl %%ebp\n\t"
		:: "D" (p), "S" (pe), "b" (xorVal),
				"d" (me)
		: "eax", "ecx"
	);
}

OPTIMIZED_SNIPPET void movinv1_snippet1(uint32_t *p, uint32_t *pe, uint32_t p1)
{
	uintptr_t len = pe - p + 1;

	asm __volatile__ (
		"rep\n\t" \
		"stosl\n\t"
		: : "c" (len), "D" (p), "a" (p1)
	);
}

OPTIMIZED_SNIPPET void movinv1_snippet2(uint32_t *p, uint32_t *pe, uint32_t p1, uint32_t p2)
{
	asm __volatile__ (
		"jmp L2\n\t" \
		".p2align 4,,7\n\t" \
		"L0:\n\t" \
		"addl $4,%%edi\n\t" \
		"L2:\n\t" \
		"movl (%%edi),%%ecx\n\t" \
		"cmpl %%eax,%%ecx\n\t" \
		"jne L3\n\t" \
		"L5:\n\t" \
		"movl %%ebx,(%%edi)\n\t" \
		"cmpl %%edx,%%edi\n\t" \
		"jb L0\n\t" \
		"jmp L4\n" \

		"L3:\n\t" \
		"pushl %%edx\n\t" \
		"pushl %%ebx\n\t" \
		"pushl %%ecx\n\t" \
		"pushl %%eax\n\t" \
		"pushl %%edi\n\t" \
		"call error\n\t" \
		"popl %%edi\n\t" \
		"popl %%eax\n\t" \
		"popl %%ecx\n\t" \
		"popl %%ebx\n\t" \
		"popl %%edx\n\t" \
		"jmp L5\n" \

		"L4:\n\t" \
		:: "a" (p1), "D" (p), "d" (pe), "b" (p2)
		: "ecx"
	);
}

OPTIMIZED_SNIPPET void movinv1_snippet3(uint32_t *p, uint32_t *pe, uint32_t p1, uint32_t p2)
{
	asm __volatile__ (
		"jmp L9\n\t"
		".p2align 4,,7\n\t"
		"L11:\n\t"
		"subl $4, %%edi\n\t"
		"L9:\n\t"
		"movl (%%edi),%%ecx\n\t"
		"cmpl %%ebx,%%ecx\n\t"
		"jne L6\n\t"
		"L10:\n\t"
		"movl %%eax,(%%edi)\n\t"
		"cmpl %%edi, %%edx\n\t"
		"jne L11\n\t"
		"jmp L7\n\t"

		"L6:\n\t"
		"pushl %%edx\n\t"
		"pushl %%eax\n\t"
		"pushl %%ecx\n\t"
		"pushl %%ebx\n\t"
		"pushl %%edi\n\t"
		"call error\n\t"
		"popl %%edi\n\t"
		"popl %%ebx\n\t"
		"popl %%ecx\n\t"
		"popl %%eax\n\t"
		"popl %%edx\n\t"
		"jmp L10\n"

		"L7:\n\t"
		:: "a" (p1), "D" (p), "d" (pe), "b" (p2)
		: "ecx"
	);
}

OPTIMIZED_SNIPPET void movinv32_snippet1(
	int *p_k, uint32_t *p_pat, // входы-выходы
	uint32_t *p, uint32_t *pe, uint32_t sval, uint32_t lb // только входы
)
{
	int k = *p_k;
	uint32_t pat = *p_pat;
	asm __volatile__ (
		"jmp L20\n\t"
		".p2align 4,,7\n\t"
		"L923:\n\t"
		"addl $4,%%edi\n\t"
		"L20:\n\t"
		"movl %%ecx,(%%edi)\n\t"
		"addl $1,%%ebx\n\t"
		"cmpl $32,%%ebx\n\t"
		"jne L21\n\t"
		"movl %%esi,%%ecx\n\t"
		"xorl %%ebx,%%ebx\n\t"
		"jmp L22\n"
		"L21:\n\t"
		"shll $1,%%ecx\n\t"
		"orl %%eax,%%ecx\n\t"
		"L22:\n\t"
		"cmpl %%edx,%%edi\n\t"
		"jb L923\n\t"
		: "=b" (k), "=c" (pat)
		: "D" (p),"d" (pe),"b" (k),"c" (pat),
			"a" (sval), "S" (lb)
	);
	*p_k = k;
	*p_pat = pat;
}

OPTIMIZED_SNIPPET void movinv32_snippet2(
	int *p_k, uint32_t *p_pat, // входы-выходы
	uint32_t *p, uint32_t *pe, uint32_t sval, uint32_t lb // только входы
)
{
	int k = *p_k;
	uint32_t pat = *p_pat;
	asm __volatile__ (
		"pushl %%ebp\n\t"
		"jmp L30\n\t"
		".p2align 4,,7\n\t"
		"L930:\n\t"
		"addl $4,%%edi\n\t"
		"L30:\n\t"
		"movl (%%edi),%%ebp\n\t"
		"cmpl %%ecx,%%ebp\n\t"
		"jne L34\n\t"

		"L35:\n\t"
		"notl %%ecx\n\t"
		"movl %%ecx,(%%edi)\n\t"
		"notl %%ecx\n\t"
		"incl %%ebx\n\t"
		"cmpl $32,%%ebx\n\t"
		"jne L31\n\t"
		"movl %%esi,%%ecx\n\t"
		"xorl %%ebx,%%ebx\n\t"
		"jmp L32\n"
		"L31:\n\t"
		"shll $1,%%ecx\n\t"
		"orl %%eax,%%ecx\n\t"
		"L32:\n\t"
		"cmpl %%edx,%%edi\n\t"
		"jb L930\n\t"
		"jmp L33\n\t"

		"L34:\n\t" \
		"pushl %%esi\n\t"
		"pushl %%eax\n\t"
		"pushl %%ebx\n\t"
		"pushl %%edx\n\t"
		"pushl %%ebp\n\t"
		"pushl %%ecx\n\t"
		"pushl %%edi\n\t"
		"call error\n\t"
		"popl %%edi\n\t"
		"popl %%ecx\n\t"
		"popl %%ebp\n\t"
		"popl %%edx\n\t"
		"popl %%ebx\n\t"
		"popl %%eax\n\t"
		"popl %%esi\n\t"
		"jmp L35\n"

		"L33:\n\t"
		"popl %%ebp\n\t"
		: "=b" (k),"=c" (pat)
		: "D" (p),"d" (pe),"b" (k),"c" (pat),
			"a" (sval), "S" (lb)
	);
	*p_k = k;
	*p_pat = pat;
}

OPTIMIZED_SNIPPET void movinv32_snippet3(
	int *p_k, uint32_t *p_pat, // входы-выходы
	uint32_t *p, uint32_t *pe, uint32_t p3, uint32_t hb // только входы
)
{
	int k = *p_k;
	uint32_t pat = *p_pat;

	asm __volatile__ (
		"pushl %%ebp\n\t"
		"jmp L40\n\t"
		".p2align 4,,7\n\t"
		"L49:\n\t"
		"subl $4,%%edi\n\t"
		"L40:\n\t"
		"movl (%%edi),%%ebp\n\t"
		"notl %%ecx\n\t"
		"cmpl %%ecx,%%ebp\n\t"
		"jne L44\n\t"

		"L45:\n\t"
		"notl %%ecx\n\t"
		"movl %%ecx,(%%edi)\n\t"
		"decl %%ebx\n\t"
		"cmpl $0,%%ebx\n\t"
		"jg L41\n\t"
		"movl %%esi,%%ecx\n\t"
		"movl $32,%%ebx\n\t"
		"jmp L42\n"
		"L41:\n\t"
		"shrl $1,%%ecx\n\t"
		"orl %%eax,%%ecx\n\t"
		"L42:\n\t"
		"cmpl %%edx,%%edi\n\t"
		"ja L49\n\t"
		"jmp L43\n\t"

		"L44:\n\t" \
		"pushl %%esi\n\t"
		"pushl %%eax\n\t"
		"pushl %%ebx\n\t"
		"pushl %%edx\n\t"
		"pushl %%ebp\n\t"
		"pushl %%ecx\n\t"
		"pushl %%edi\n\t"
		"call error\n\t"
		"popl %%edi\n\t"
		"popl %%ecx\n\t"
		"popl %%ebp\n\t"
		"popl %%edx\n\t"
		"popl %%ebx\n\t"
		"popl %%eax\n\t"
		"popl %%esi\n\t"
		"jmp L45\n"

		"L43:\n\t"
		"popl %%ebp\n\t"
		: "=b" (k), "=c" (pat)
		: "D" (p),"d" (pe),"b" (k),"c" (pat),
			"a" (p3), "S" (hb)
	);

	*p_k = k;
	*p_pat = pat;
}

OPTIMIZED_SNIPPET void modtst_snippet1(
	uint32_t **p_p, // вход-выход
	uint32_t *pe, uint32_t p1 // только входы
)
{
	uint32_t *p = *p_p;
	asm __volatile__ (
		"jmp L60\n\t" \
		".p2align 4,,7\n\t" \

		"L60:\n\t" \
		"movl %%eax,(%%edi)\n\t" \
		"addl $80,%%edi\n\t" \
		"cmpl %%edx,%%edi\n\t" \
		"jb L60\n\t" \
		: "=D" (p)
		: "D" (p), "d" (pe), "a" (p1)
	);
	*p_p = p;
}


OPTIMIZED_SNIPPET void modtst_snippet2(
	int *p_k, // вход-выход
	uint32_t *p, uint32_t *pe, uint32_t p2, int offset // только входы
)
{
	int k = *p_k;
	asm __volatile__ (
		"jmp L50\n\t" \
		".p2align 4,,7\n\t" \

		"L54:\n\t" \
		"addl $4,%%edi\n\t" \
		"L50:\n\t" \
		"cmpl %%ebx,%%ecx\n\t" \
		"je L52\n\t" \
			"movl %%eax,(%%edi)\n\t" \
		"L52:\n\t" \
		"incl %%ebx\n\t" \
		"cmpl $19,%%ebx\n\t" \
		"jle L53\n\t" \
			"xorl %%ebx,%%ebx\n\t" \
		"L53:\n\t" \
		"cmpl %%edx,%%edi\n\t" \
		"jb L54\n\t" \
		: "=b" (k)
		: "D" (p), "d" (pe), "a" (p2),
			"b" (k), "c" (offset)
	);
	*p_k = k;
}

OPTIMIZED_SNIPPET void modtst_snippet3(
	uint32_t **p_p, // вход-выход
	uint32_t *pe, uint32_t p1 // только входы
)
{
	uint32_t *p = *p_p;

	asm __volatile__ (
		"jmp L70\n\t" \
		".p2align 4,,7\n\t" \

		"L70:\n\t" \
		"movl (%%edi),%%ecx\n\t" \
		"cmpl %%eax,%%ecx\n\t" \
		"jne L71\n\t" \
		"L72:\n\t" \
		"addl $80,%%edi\n\t" \
		"cmpl %%edx,%%edi\n\t" \
		"jb L70\n\t" \
		"jmp L73\n\t" \

		"L71:\n\t" \
		"pushl %%edx\n\t"
		"pushl %%ecx\n\t"
		"pushl %%eax\n\t"
		"pushl %%edi\n\t"
		"call error\n\t"
		"popl %%edi\n\t"
		"popl %%eax\n\t"
		"popl %%ecx\n\t"
		"popl %%edx\n\t"
		"jmp L72\n"

		"L73:\n\t" \
		: "=D" (p)
		: "D" (p), "d" (pe), "a" (p1)
		: "ecx"
	);

	*p_p = p;
}

OPTIMIZED_SNIPPET void block_move_snippet1(uint32_t **p_p, ulong len)
{
	uint32_t *p = *p_p;
	asm __volatile__ (
		"jmp L100\n\t"

		".p2align 4,,7\n\t"
		"L100:\n\t"

		// В первом цикле eax равен 0x00000001, edx равен 0xfffffffe
		"movl %%eax, %%edx\n\t"
		"notl %%edx\n\t"

		// Установить блок размером 64 байта	// В первом цикле значения DWORD равны
		"movl %%eax,0(%%edi)\n\t"	// 0x00000001
		"movl %%eax,4(%%edi)\n\t"	// 0x00000001
		"movl %%eax,8(%%edi)\n\t"	// 0x00000001
		"movl %%eax,12(%%edi)\n\t"	// 0x00000001
		"movl %%edx,16(%%edi)\n\t"	// 0xfffffffe
		"movl %%edx,20(%%edi)\n\t"	// 0xfffffffe
		"movl %%eax,24(%%edi)\n\t"	// 0x00000001
		"movl %%eax,28(%%edi)\n\t"	// 0x00000001
		"movl %%eax,32(%%edi)\n\t"	// 0x00000001
		"movl %%eax,36(%%edi)\n\t"	// 0x00000001
		"movl %%edx,40(%%edi)\n\t"	// 0xfffffffe
		"movl %%edx,44(%%edi)\n\t"	// 0xfffffffe
		"movl %%eax,48(%%edi)\n\t"	// 0x00000001
		"movl %%eax,52(%%edi)\n\t"	// 0x00000001
		"movl %%edx,56(%%edi)\n\t"	// 0xfffffffe
		"movl %%edx,60(%%edi)\n\t"	// 0xfffffffe

		// циклический сдвиг влево с переносом,
		// во втором цикле eax равен		 0x00000002
		// во втором цикле edx равен (~eax) 0xfffffffd
		"rcll $1, %%eax\n\t"

		// Переместить текущую позицию вперед на 64 байта (к началу следующего блока)
		"leal 64(%%edi), %%edi\n\t"

		// Цикл до конца
		"decl %%ecx\n\t"
		"jnz  L100\n\t"

		: "=D" (p)
		: "D" (p), "c" (len), "a" (1)
		: "edx"
	);
	*p_p = p;
}

OPTIMIZED_SNIPPET void block_move_snippet2(uint32_t *p, ulong pp, ulong len)
{
	asm __volatile__ (
		"cld\n"
		"jmp L110\n\t"

		".p2align 4,,7\n\t"
		"L110:\n\t"

		//
		// В конце всего этого
		// - вторая половина равна начальному значению первой половины
		// - первая половина смещена вправо на 32 байта (с циклическим переносом)
		//

		// Переместить первую половину во вторую половину
		"movl %1,%%edi\n\t" // Цель, pp (средняя точка)
		"movl %0,%%esi\n\t" // Источник, p (начальная точка)
		"movl %2,%%ecx\n\t" // Длина, len (размер половины в DWORD)
		"rep\n\t"
		"movsl\n\t"

		// Переместить вторую половину без последних 32 байт в первую половину со смещением 32 байта
		"movl %0,%%edi\n\t"
		"addl $32,%%edi\n\t"	// Цель, p (начальная точка) плюс 32 байта
		"movl %1,%%esi\n\t"		// Источник, pp (средняя точка)
		"movl %2,%%ecx\n\t"
		"subl $8,%%ecx\n\t"		// Длина, len (размер половины в DWORD) минус 8 DWORD (32 байта)
		"rep\n\t"
		"movsl\n\t"

		// Переместить последние 8 DWORD (32 байта) второй половины в начало первой половины
		"movl %0,%%edi\n\t"		// Цель, p (начальная точка)
								// Источник, 8 DWORD с конца второй половины, оставшиеся после последней команды rep/movsl
		"movl $8,%%ecx\n\t"		// Длина, 8 DWORD (32 байта)
		"rep\n\t"
		"movsl\n\t"

		:: "g" (p), "g" (pp), "g" (len)
		: "edi", "esi", "ecx"
	);
}


OPTIMIZED_SNIPPET void block_move_snippet3(uint32_t **p_p, uint32_t *pe)
{
	uint32_t *p = *p_p;

	asm __volatile__ (
		"jmp L120\n\t"

		".p2align 4,,7\n\t"
		"L124:\n\t"
		"addl $8,%%edi\n\t" // Следующий QWORD
		"L120:\n\t"

		// Сравнить соседние DWORD
		"movl (%%edi),%%ecx\n\t"
		"cmpl 4(%%edi),%%ecx\n\t"
		"jnz L121\n\t" // Вывести ошибку, если они не совпадают

		// Цикл до конца блока
		"L122:\n\t"
		"cmpl %%edx,%%edi\n\t"
		"jb L124\n"
		"jmp L123\n\t"

		"L121:\n\t"
		// eax не используется, поэтому нам не нужно сохранять его согласно cdecl
		// ecx используется, но не восстанавливается, однако его значение нам больше не понадобится после этого момента
		"pushl %%edx\n\t"
		"pushl 4(%%edi)\n\t"
		"pushl %%ecx\n\t"
		"pushl %%edi\n\t"
		"call error\n\t"
		"popl %%edi\n\t"
		"addl $8,%%esp\n\t"
		"popl %%edx\n\t"
		"jmp L122\n"
		"L123:\n\t"
		: "=D" (p)
		: "D" (p), "d" (pe)
		: "ecx"
	);

	*p_p = p;
}