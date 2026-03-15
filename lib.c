Вот переведенный файл `memtest86-plus-riscv-master/lib.c`. Весь код, структура и отступы сохранены без изменений, переведены только комментарии.

```c
/* lib.c - MemTest-86  Версия 3.4
 *
 * Выпущено под лицензией GNU Public License версии 2.
 * Автор: Крис Брэди (Chris Brady)
 */

#include <stdint.h>

#include "arch.h"
#include "io.h"
#if defined(__i386__)
#include "serial.h"
#endif
#include "test.h"
#include "config.h"
#include "screen_buffer.h"
#include "smp.h"


int slock = 0, lsr = 0;
short serial_cons = SERIAL_CONSOLE_DEFAULT;
#if SERIAL_TTY != 0 && SERIAL_TTY != 1
#error Неверный SERIAL_TTY. Поддерживаются только ttyS0 и ttyS1.
#endif
short serial_tty = SERIAL_TTY;

#if ((115200%SERIAL_BAUD_RATE) != 0)
#error Неверная скорость передачи данных (baud rate) по умолчанию
#endif
int serial_baud_rate = SERIAL_BAUD_RATE;
unsigned char serial_parity = 0;
unsigned char serial_bits = 8;

struct ascii_map_str {
        int ascii;
        int keycode;
};

int strlen(char * string){
	int i=0;
	while(*string++){i++;};
	return i;
}

int strstr(char *haystack, char * needle)
{
	int i=0,j=0;
	int here=0;
	while(1){
		if(needle[i]==haystack[j])
		{
			if(here==0)
				here=j;
				i++;j++;
				if(i>=strlen(needle))
				{
					return here;
				}
				if(j>=strlen(haystack))
				{
					return -1;
				}
		} else {
			j++;i=0;here=0;
		}
	}
}

int memcmp(const void *s1, const void *s2, ulong count)
{
	const unsigned char *src1 = s1, *src2 = s2;
	int i;
	for(i = 0; i < count; i++) {
		if (src1[i] != src2[i]) {
			return (int)src1[i] - (int)src2[i];
		}
	}
	return 0;
}

int strncmp(const char *s1, const char *s2, ulong n) {
	signed char res = 0;
	while (n) {
		res = *s1 - *s2;
		if (res != 0)
			return res;
		if (*s1 == '\0')
			return 0;
		++s1, ++s2;
		--n;
	}
	return res;
}

void *memmove(void *dest, const void *src, ulong n)
{
	long i;
	char *d = (char *)dest, *s = (char *)src;

	/* Если src == dest, ничего не делать */
	if (dest < src) {
		for(i = 0; i < n; i++) {
			d[i] = s[i];
		}
	}
	else if (dest > src) {
		for(i = n -1; i >= 0; i--) {
			d[i] = s[i];
		}
	}
	return dest;
}

char toupper(char c)
{
	if (c >= 'a' && c <= 'z')
		return c + 'A' -'a';
	else
		return c;
}

int isdigit(char c)
{
	return c >= '0' && c <= '9';
}

int isxdigit(char c)
{
	return isdigit(c) || (toupper(c) >= 'A' && toupper(c) <= 'F'); }

unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base) {
	unsigned long result = 0, value;

	if (!base) {
		base = 10;
		if (*cp == '0') {
			base = 8;
			cp++;
			if (toupper(*cp) == 'X' && isxdigit(cp[1])) {
				cp++;
				base = 16;
			}
		}
	} else if (base == 16) {
		if (cp[0] == '0' && toupper(cp[1]) == 'X')
			cp += 2;
	}
	while (isxdigit(*cp) &&
		(value = isdigit(*cp) ? *cp-'0' : toupper(*cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}
	if (endp)
		*endp = (char *)cp;
	return result;
}

/*
 * Прокрутка области сообщений об ошибках экрана по мере необходимости
 * Начинается на строке LINE_SCROLL и заканчивается на строке 23
 */
void scroll(void) 
{
	int i, j;
	char *s, tmp;

	/* Прокручивать только если находимся внизу экрана */
	if (v->msg_line < 23) {
		v->msg_line++;
	} else {
		/* Если блокировка прокрутки включена, цикл до её снятия */
		while (slock) {
			check_input();
		}
	        for (i=LINE_SCROLL; i<23; i++) {
			s = (char *)(SCREEN_ADR + ((i+1) * 160));
			for (j=0; j<160; j+=2, s+=2) {
				*(s-160) = *s;
                                tmp = get_scrn_buf(i+1, j/2);
                                set_scrn_buf(i, j/2, tmp);
			}
		}
		/* Очистка вновь открытой строки */
		s = (char *)(SCREEN_ADR + (23 * 160));
		for (j=0; j<80; j++) {
			*s = ' ';
                        set_scrn_buf(23, j, ' ');
			s += 2;
		}
                tty_print_region(LINE_SCROLL, 0, 23, 79);
        }
}

/*
 * Очистка области прокрутки
 */
void clear_scroll(void)
{
	int i;
	char *s;

	s = (char*)(SCREEN_ADR+LINE_HEADER*160);
        for(i=0; i<80*(24-LINE_HEADER); i++) {
                *s++ = ' ';
                *s++ = 0x17;
        }
}

/*
 * Размещение одного символа на экране
 */
void cplace(int y, int x, const char c)
{
	char *dptr;

	dptr = (char *)(SCREEN_ADR + (160*y) + (2*x));
	*dptr = c;
}

/*
 * Печать символов на экране
 */
void cprint(int y, int x, const char *text)
{
	register int i;
	char *dptr;

	dptr = (char *)(SCREEN_ADR + (160*y) + (2*x));
	for (i=0; text[i]; i++) {
		*dptr = text[i];
		dptr += 2;
        }
        tty_print_line(y, x, text);
}

void itoa(char s[], int n) 
{
  int i, sign;

  if((sign = n) < 0)
    n = -n;
  i=0;
  do {
    s[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);
  if(sign < 0)
    s[i++] = '-';
  s[i] = '\0';
  reverse(s);
}

void reverse(char s[])
{
  int c, i, j;
  for(j = 0; s[j] != 0; j++)
	  ;

  for(i=0, j = j - 1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}
void memcpy (void *dst, void *src, int len)
{
	char *s = (char*)src;
	char *d = (char*)dst;
	int i;

	if (len <= 0) {
		return;
	}
	for (i = 0 ; i < len; i++) {
		*d++ = *s++;
	} 
}

/*
 * Печать адреса в удобном для человека формате
 */
void aprint(int y, int x, ulong page)
{
	/* page кратна 4K */
	if ((page << 2) < 9999) {
		dprint(y, x, page << 2, 4, 0);
		cprint(y, x+4, "K");
	}
	else if ((page >>8) < 9999) {
		dprint(y, x, (page  + (1 << 7)) >> 8, 4, 0);
		cprint(y, x+4, "M");
	}
	else if ((page >>18) < 9999) {
		dprint(y, x, (page + (1 << 17)) >> 18, 4, 0);
		cprint(y, x+4, "G");
	}
	else {
		dprint(y, x, (page + (1 << 27)) >> 28, 4, 0);
		cprint(y, x+4, "T");
	}
}

/*
 * Печать десятичного числа на экране
 */
void dprint(int y, int x, ulong val, int len, int right)
{
	ulong j, k;
	int i, flag=0;
	char buf[18];

	if (val > 999999999 || len > 9) {
		return;
	}
	for(i=0, j=1; i<len-1; i++) {
		j *= 10;
	}
	if (!right) {
		for (i=0; j>0; j/=10) {
			k = val/j;
			if (k > 9) {
				j *= 100;
				continue;
			}
			if (flag || k || j == 1) {
				buf[i++] = k + '0';
				flag++;
			} else {
				buf[i++] = ' ';
			}
			val -= k * j;
		}
	} else {
		for(i=0; i<len; j/=10) {
			if (j) {
				k = val/j;
					if (k > 9) {
					j *= 100;
					len++;
					continue;
				}
				if (k == 0 && flag == 0) {
					continue;				
				}
				buf[i++] = k + '0';
				val -= k * j;
			} else {
                                if (flag == 0 &&  i < len-1) {
                                        buf[i++] = '0';
                                } else {
                                        buf[i++] = ' ';
                                }
			}
			flag++;
		}
	}
	buf[i] = 0;
	cprint(y,x,buf);
}

/*
 * Печать шестнадцатеричного числа на экране длиной не менее digits
 */
void hprint2(int y,int x, unsigned long val, int digits)
{
	unsigned long j;
	int i, idx, flag = 0;
	char buf[18];

        for (i=0, idx=0; i<8; i++) {
                j = val >> (28 - (4 * i));
		j &= 0xf;
		if (j < 10) {
			if (flag || j || i == 7) {
		                buf[idx++] = j + '0';
				flag++;
			} else {
				buf[idx++] = '0';
			}
		} else {
			buf[idx++] = j + 'a' - 10;
			flag++;
		}
        }
	if (digits > 8) {
		digits = 8;
	}
	if (flag > digits) {
		digits = flag;
	}
        buf[idx] = 0;
	cprint(y,x,buf + (idx - digits));
}

/*
 * Печать шестнадцатеричного числа на экране длиной ровно digits
 */
void hprint3(int y,int x, unsigned long val, int digits)
{
	unsigned long j;
	int i, idx, flag = 0;
	char buf[18];

	for (i=0, idx=0; i<digits; i++) {
		j = 0xf & val;
		val /= 16;

		if (j < 10) {
			if (flag || j || i == 7) {
				buf[digits - ++idx] = j + '0';
				flag++;
			} else {
				buf[digits - ++idx] = '0';
			}
		} else {
			buf[digits - ++idx] = j + 'a' - 10;
			flag++;
		}
	}
	buf[idx] = 0;
	cprint(y,x,buf);
}

/*
 * Печать шестнадцатеричного числа на экране
 */
void hprint(int y, int x, unsigned long val)
{
	return hprint2(y, x, val, 8);
}

/*
 * Печать адреса в нотации 0000m0000k0000
 */
void xprint(int y,int x, ulong val)
{
        ulong j;

	j = (val & 0xffc00000) >> 20;
	dprint(y, x, j, 4, 0);
	cprint(y, x+4, "m");
	j = (val & 0xffc00) >> 10;
	dprint(y, x+5, j, 4, 0);
	cprint(y, x+9, "k");
	j = val & 0x3ff;
	dprint(y, x+10, j, 4, 0);
}

void set_cache(int val)
{
	switch(val) {
	case 0:
		cache_off();
		break;
	case 1:
		cache_on();
		break;
	}
}

void check_input(void)
{
	unsigned char c;

	if ((c = get_key())) {
		switch(c & 0x7f) {
		case 1:	
			/* Была нажата клавиша "ESC", выход.  */
			cprint(LINE_RANGE, COL_MID+23, "Halting... ");
			reboot();
			break;
		case 46:
			/* c - Настройка */
			get_config();
			break;
		case 28:
			/* CR - снять блокировку прокрутки */
			slock = 0;
			footer();
			break;
		case 57:
			/* SP - установить блокировку прокрутки */
			slock = 1;
			footer();
			break;
		case 0x26:
			/* ^L/L - перерисовать экран */
			tty_print_screen();
			break;
		}
	}
}

void footer()
{
	cprint(24, 0, "(ESC)exit  (c)configuration  (SP)scroll_lock  (CR)scroll_unlock");
	if (slock) {
		cprint(24, 74, "Locked");
	} else {
		cprint(24, 74, "      ");
	}
}

ulong getval(int x, int y, int result_shift)
{
	unsigned long val;
	int done;
	int c;
	int i, n;
	int base;
	int shift;
	char buf[16];

	for(i = 0; i < sizeof(buf)/sizeof(buf[0]); i++ ) {
		buf[i] = ' ';
	}
	buf[sizeof(buf)/sizeof(buf[0]) -1] = '\0';
	
	wait_keyup();
	done = 0;
	n = 0;
	base = 10;
	while(!done) {
		/* Прочитать новый символ и обработать его */
		c = get_key();
		switch(c) {
		case 0x26: /* ^L/L - перерисовать экран */
			tty_print_screen();
			break;
		case 0x1c: /* CR */
			/* Если что-то было введено, мы закончили */
			if(n) done = 1;
			break;
		case 0x19: /* p */ buf[n] = 'p'; break;
		case 0x22: /* g */ buf[n] = 'g'; break;
		case 0x32: /* m */ buf[n] = 'm'; break;
		case 0x25: /* k */ buf[n] = 'k'; break;
		case 0x2d: /* x */
			/* Разрешать 'x' только после начального 0 */
			if (n == 1 && (buf[0] == '0')) {
				buf[n] = 'x';
			}
			break;
		case 0x0e: /* BS */
			if (n > 0) {
				n -= 1;
				buf[n] = ' ';
			}
			break;
		/* Не позволять вводить число, не входящее в нашу текущую систему счисления */
		case 0x0B: if (base >= 1) buf[n] = '0'; break;
		case 0x02: if (base >= 2) buf[n] = '1'; break;
		case 0x03: if (base >= 3) buf[n] = '2'; break;
		case 0x04: if (base >= 4) buf[n] = '3'; break;
		case 0x05: if (base >= 5) buf[n] = '4'; break;
		case 0x06: if (base >= 6) buf[n] = '5'; break;
		case 0x07: if (base >= 7) buf[n] = '6'; break;
		case 0x08: if (base >= 8) buf[n] = '7'; break;
		case 0x09: if (base >= 9) buf[n] = '8'; break;
		case 0x0A: if (base >= 10) buf[n] = '9'; break;
		case 0x1e: if (base >= 11) buf[n] = 'a'; break;
		case 0x30: if (base >= 12) buf[n] = 'b'; break;
		case 0x2e: if (base >= 13) buf[n] = 'c'; break;
		case 0x20: if (base >= 14) buf[n] = 'd'; break;
		case 0x12: if (base >= 15) buf[n] = 'e'; break;
		case 0x21: if (base >= 16) buf[n] = 'f'; break;
		default:
			break;
		}
		/* Не позволять вводить что-либо после суффикса */
		if (n > 0 && (
			(buf[n-1] == 'p') || (buf[n-1] == 'g') || 
			(buf[n-1] == 'm') || (buf[n-1] == 'k'))) {
			buf[n] = ' ';
		}
		/* Если мы ввели символ, увеличиваем n */
		if (buf[n] != ' ') {
			n++;
		}
		buf[n] = ' ';
		/* Печать текущего числа */
		cprint(x, y, buf);

		/* Определить систему счисления, в которой мы вводим числа */
		base = 10;
		if ((buf[0] == '0') && (buf[1] == 'x')) {
			base = 16;
		}
		else if (buf[0] == '0') {
			base = 8;
		}
	}
	/* Вычислить наш текущий сдвиг */
	shift = 0;
	switch(buf[n-1]) {
	case 'g': /* гига */ shift = 30; break;
	case 'm': /* мега */ shift = 20; break;
	case 'p': /* страница */ shift = 12; break;
	case 'k': /* кило */ shift = 10; break;
	}
	shift -= result_shift;

	/* Вычислить наше текущее значение */
	val = simple_strtoul(buf, 0, base);
	if (shift > 0) {
		if (shift >= 32) {
			val = 0xffffffff;
		} else {
			val <<= shift;
		}
	} else {
		if (-shift >= 32) {
			val = 0;
		}
		else {
			val >>= -shift;
		}
	}
	return val;
}

void ttyprint(int y, int x, const char *p)
{
	static char sx[3];
	static char sy[3];
	
	sx[0]='\0';
	sy[0]='\0';
	x++; y++;
	itoa(sx, x);
	itoa(sy, y);
	serial_echo_print(" [");
	serial_echo_print(sy);
	serial_echo_print(";");
	serial_echo_print(sx);
	serial_echo_print("H");
	serial_echo_print(p);
}


/*
 * Получить количество цифр
 */
int getnum(ulong val)
{
	int len = 0;
	int i = 1;
	
	while(i <= val)
	{
		len++;
		i *= 10;
	}

	return len;
		
}


/* За исключением многосимвольных последовательностей клавиш, эта таблица
 * сопоставления является полной. Поэтому ее не нужно обновлять
 * при поиске новых клавиш. Однако обработку клавиш действительно
 * следовало бы перевернуть, и только в get_key нам следовало бы
 * беспокоиться о точном коде нажатой клавиши. Везде в остальных
 * местах мы должны переключаться по самому символу...
 */
struct ascii_map_str ser_map[] =
/*ascii код_клавиши   ascii  код_клавиши*/
{ 
  /* Особые случаи идут первыми, чтобы я мог оставить их
   * «обычное» сопоставление в таблице без его активации.
   */
  {  27,   0x01}, /* ^[/ESC -> ESC  */
  { 127,   0x0e}, /*    DEL -> BS   */
  {   8,   0x0e}, /* ^H/BS  -> BS   */
  {  10,   0x1c}, /* ^L/NL  -> CR   */
  {  13,   0x1c}, /* ^M/CR  -> CR   */
  {   9,   0x0f}, /* ^I/TAB -> TAB  */
  {  19,   0x39}, /* ^S     -> SP   */
  {  17,     28}, /* ^Q     -> CR   */

  { ' ',   0x39}, /*     SP -> SP   */
  { 'a',   0x1e},
  { 'A',   0x1e},
  {   1,   0x1e}, /* ^A      -> A */
  { 'b',   0x30},
  { 'B',   0x30},
  {   2,   0x30}, /* ^B      -> B */
  { 'c',   0x2e},
  { 'C',   0x2e},
  {   3,   0x2e}, /* ^C      -> C */
  { 'd',   0x20},
  { 'D',   0x20},
  {   4,   0x20}, /* ^D      -> D */
  { 'e',   0x12},
  { 'E',   0x12},
  {   5,   0x12}, /* ^E      -> E */
  { 'f',   0x21},
  { 'F',   0x21},
  {   6,   0x21}, /* ^F      -> F */
  { 'g',   0x22},
  { 'G',   0x22},
  {   7,   0x22}, /* ^G      -> G */
  { 'h',   0x23},
  { 'H',   0x23},
  {   8,   0x23}, /* ^H      -> H */
  { 'i',   0x17},
  { 'I',   0x17},
  {   9,   0x17}, /* ^I      -> I */
  { 'j',   0x24},
  { 'J',   0x24},
  {  10,   0x24}, /* ^J      -> J */
  { 'k',   0x25},
  { 'K',   0x25},
  {  11,   0x25}, /* ^K      -> K */
  { 'l',   0x26},
  { 'L',   0x26},
  {  12,   0x26}, /* ^L      -> L */
  { 'm',   0x32},
  { 'M',   0x32},
  {  13,   0x32}, /* ^M      -> M */
  { 'n',   0x31},
  { 'N',   0x31},
  {  14,   0x31}, /* ^N      -> N */
  { 'o',   0x18},
  { 'O',   0x18},
  {  15,   0x18}, /* ^O      -> O */
  { 'p',   0x19},
  { 'P',   0x19},
  {  16,   0x19}, /* ^P      -> P */
  { 'q',   0x10},
  { 'Q',   0x10},
  {  17,   0x10}, /* ^Q      -> Q */
  { 'r',   0x13},
  { 'R',   0x13},
  {  18,   0x13}, /* ^R      -> R */
  { 's',   0x1f},
  { 'S',   0x1f},
  {  19,   0x1f}, /* ^S      -> S */
  { 't',   0x14},
  { 'T',   0x14},
  {  20,   0x14}, /* ^T      -> T */
  { 'u',   0x16},
  { 'U',   0x16},
  {  21,   0x16}, /* ^U      -> U */
  { 'v',   0x2f},
  { 'V',   0x2f},
  {  22,   0x2f}, /* ^V      -> V */
  { 'w',   0x11},
  { 'W',   0x11},
  {  23,   0x11}, /* ^W      -> W */
  { 'x',   0x2d},
  { 'X',   0x2d},
  {  24,   0x2d}, /* ^X      -> X */
  { 'y',   0x15},
  { 'Y',   0x15},
  {  25,   0x15}, /* ^Y      -> Y */
  { 'z',   0x2c},
  { 'Z',   0x2c},
  {  26,   0x2c}, /* ^Z      -> Z */
  { '-',   0x0c},
  { '_',   0x0c},
  {  31,   0x0c}, /* ^_      -> _ */
  { '=',   0x0c},
  { '+',   0x0c},
  { '[',   0x1a},
  { '{',   0x1a},
  {  27,   0x1a}, /* ^[      -> [ */
  { ']',   0x1b},
  { '}',   0x1b},
  {  29,   0x1b}, /* ^]      -> ] */
  { ';',   0x27},
  { ':',   0x27},
  { '\'',  0x28},
  { '"',   0x28},
  { '`',   0x29},
  { '~',   0x29},
  { '\\',  0x2b},
  { '|',   0x2b},
  {  28,   0x2b}, /* ^\      -> \ */
  { ',',   0x33},
  { '<',   0x33},
  { '.',   0x34},
  { '>',   0x34},
  { '/',   0x35},
  { '?',   0x35},
  { '1',   0x02},
  { '!',   0x02},
  { '2',   0x03},
  { '@',   0x03},
  { '3',   0x04},
  { '#',   0x04},
  { '4',   0x05},
  { '$',   0x05},
  { '5',   0x06},
  { '%',   0x06},
  { '6',   0x07},
  { '^',   0x07},
  {  30,   0x07}, /* ^^      -> 6 */
  { '7',   0x08},
  { '&',   0x08},
  { '8',   0x09},
  { '*',   0x09},
  { '9',   0x0a},
  { '(',   0x0a},
  { '0',   0x0b},
  { ')',   0x0b},
  {   0,      0}
};

/*
 * По заданному символу ascii возвращает код клавиши
 *
 * Использует определение ser_map выше.
 *
 * Было бы эффективнее использовать массив из 255 символов
 * и напрямую индексировать его.
 */
int ascii_to_keycode (int in)
{
	struct ascii_map_str *p;
	for (p = ser_map; p->ascii; p++) {
		if (in ==p->ascii)
			return p->keycode;
	}
	return 0;
}

/*
 * Вызывайте это, когда хотите подождать, пока пользователь уберет
 * палец с клавиши. Это пустая операция, если вы используете
 * последовательную консоль.
 */
void wait_keyup( void ) {
	/* Проверить, отпустил ли кто-то клавишу на клавиатуре */
	while (1) {
		if ((get_key() & 0x80) != 0) {
			return;
		}
		/* Попытка имитировать ожидание отпускания клавиши через
		 * последовательный порт слишком ужасна, чтобы оставлять её.
		 * В частности, некоторые меню даже не отображаются до тех пор,
		 * пока вы не отпустите клавишу, которая привела вас туда.
		 * С последовательным портом это приводит к двойному нажатию
		 * или чему-то худшему почти для каждой клавиши.
		 */
		if (serial_cons) {
			return;
		}
	}
}

/*
 * Обрабатывает параметр командной строки "console=<параметр>"
 *
 * Примеры принимаемых параметров:
 *   ttyS0
 *   ttyS1
 *   ttyS0,115200
 *   ttyS0,9600e8
 */
void serial_console_setup(const char *param)
{
	char *option, *end;
	unsigned long tty;
	unsigned long baud_rate;
	unsigned char parity, bits;

	if (strncmp(param, "ttyS", 4))
		return;   /* не последовательный порт */

	param += 4;

	tty = simple_strtoul(param, &option, 10);

	if (option == param)
		return;   /* цифр не было */

	if (tty > 1)
		return;   /* поддерживаются только ttyS0 и ttyS1 */

	if (*option == '\0' || *option == ' ')
		goto save_tty; /* параметры не указаны, просто ttyS? */

	if (*option != ',')
		return;  /* отсутствует запятая-разделитель */

	/* далее должна следовать скорость передачи данных (baud rate) */
	option++;
	baud_rate = simple_strtoul(option, &end, 10);

	if (end == option)
		return;  /* нет скорости передачи после запятой */

	if (baud_rate == 0 || (115200 % baud_rate) != 0)
		return;  /* неверная скорость передачи */

	if (*end == '\0' || *end == ' ')
		goto save_baud_rate;  /* больше параметры не указаны */

	switch (toupper(*end)) {
#if defined(__i386__)
		case 'N':
			parity = 0;
			break;
		case 'O':
			parity = UART_LCR_PARITY;
			break;
		case 'E':
			parity = UART_LCR_PARITY | UART_LCR_EPAR;
			break;
#endif
		default:
			/* Неизвестная четность */
			return;
	}

	end++;
	if (*end == '\0' || *end == ' ')
		goto save_parity;

	/* длина слова (биты) */
	if (*end < '7' || *end > '8')
		return;  /* неверное количество бит */

	bits = *end - '0';

	end++;

	if (*end != '\0' || *end != ' ')
		return;  /* мусор в конце */

	serial_bits = bits;
	save_parity:
	serial_parity = parity;
	save_baud_rate:
	serial_baud_rate = (int) baud_rate;
  save_tty:
	serial_tty = (short) tty;
	serial_cons = 1;
}

/* Получить список чисел, разделенных запятыми */
void get_list(int x, int y, int len, char *buf)
{
	int c, n = 0;

	len--;
	wait_keyup();
	while(1) {
		/* Прочитать новый символ и обработать его */
		c = get_key();
		switch(c) {
		    case 0x1c: /* CR */
			/* Если что-то было введено, мы закончили */
			if(n) {
			    buf[n] = 0;
			    return;
			}
			break;
		    case 0x0e: /* BS */
			if (n > 0) {
				n -= 1;
				buf[n] = ' ';
			}
			break;
		    case 0x0B: buf[n++] = '0'; break;
		    case 0x02: buf[n++] = '1'; break;
		    case 0x03: buf[n++] = '2'; break;
		    case 0x04: buf[n++] = '3'; break;
		    case 0x05: buf[n++] = '4'; break;
		    case 0x06: buf[n++] = '5'; break;
		    case 0x07: buf[n++] = '6'; break;
		    case 0x08: buf[n++] = '7'; break;
		    case 0x09: buf[n++] = '8'; break;
		    case 0x0a: buf[n++] = '9'; break;
		    case 0x33: buf[n++] = ','; break;
		}
		cprint(x, y, buf);
		if (n >= len) {
		   buf[n] = 0;
		   return;
		}
	}
}