#include "arch.h"
#include "serial.h"

static const short serial_base_ports[] = {0x3f8, 0x2f8};

void reboot(void)
{
	/* указание BIOS выполнить холодный старт */
	*((unsigned short *)0x472) = 0x0;
	while(1)
	{
		outb(0xFE, 0x64);
		outb(0x02, 0xcf9); /* сброс, который не зависит от контроллера клавиатуры */
		outb(0x04, 0xcf9);
		outb(0x0E, 0xcf9);
	}
}

int get_key() {
	int c;

	c = inb(0x64);
	if ((c & 1) == 0) {
		if (serial_cons) {
			int comstat;
			comstat = serial_echo_inb(UART_LSR);
			if (comstat & UART_LSR_DR) {
				c = serial_echo_inb(UART_RX);
				/* Нажатие '.' имеет тот же эффект, что и 'c'
				   на клавиатуре.
				   Oct 056   Dec 46   Hex 2E   Ascii .
				*/
				return (ascii_to_keycode(c));
			}
		}
		return(0);
	}
	c = inb(0x60);
	return((c));
}

void serial_echo_init(void)
{
	int comstat, hi, lo, serial_div;
	unsigned char lcr;

	/* чтение защелки делителя (Divisor Latch) */
	comstat = serial_echo_inb(UART_LCR);
	serial_echo_outb(comstat | UART_LCR_DLAB, UART_LCR);
	hi = serial_echo_inb(UART_DLM);
	lo = serial_echo_inb(UART_DLL);
	serial_echo_outb(comstat, UART_LCR);

	/* выполнение аппаратной инициализации */
	lcr = serial_parity | (serial_bits - 5);
	serial_echo_outb(lcr, UART_LCR); /* Без четности, 8 бит данных, 1 стоповый бит */
	serial_div = 115200 / serial_baud_rate;
	serial_echo_outb(0x80|lcr, UART_LCR); /* Доступ к защелке делителя */
	serial_echo_outb(serial_div & 0xff, UART_DLL);  /* делитель скорости передачи */
	serial_echo_outb((serial_div >> 8) & 0xff, UART_DLM);
	serial_echo_outb(lcr, UART_LCR); /* Работа с делителем завершена */

	/* Перед отключением прерываний необходимо прочитать
	 * регистры LSR и RBR */
	comstat = serial_echo_inb(UART_LSR); /* COM? LSR */
	comstat = serial_echo_inb(UART_RX);	/* COM? RBR */
	serial_echo_outb(0x00, UART_IER); /* Отключить все прерывания */

        clear_screen_buf();

	return;
}

void serial_echo_print(const char *p)
{
	if (!serial_cons) {
		return;
	}
	/* Теперь обрабатываем каждый символ */
	while (*p) {
		WAIT_FOR_XMITR;

		/* Отправка символа. */
		serial_echo_outb(*p, UART_TX);
		if(*p==10) {
			WAIT_FOR_XMITR;
			serial_echo_outb(13, UART_TX);
		}
		p++;
	}
}