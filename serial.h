/*
 * include/linux/serial.h
 *
 * Авторские права (C) 1992, 1994, Теодор Цо (Theodore Ts'o).
 * 
 * Перераспространение этого файла разрешено в соответствии с условиями 
 * GNU Public License (GPL)
 * 
 * Это назначения портов UART, выраженные в виде смещений от базового
 * регистра. Эти назначения должны быть действительны для любого
 * последовательного порта на базе 8250, 16450 или 16550(A).
 */

#ifndef _LINUX_SERIAL_REG_H
#define _LINUX_SERIAL_REG_H

#define UART_RX		0	/* Вход: Буфер приема (DLAB=0) */
#define UART_TX		0	/* Выход: Буфер передачи (DLAB=0) */
#define UART_DLL	0	/* Выход: Младший байт делителя (DLAB=1) */
#define UART_DLM	1	/* Выход: Старший байт делителя (DLAB=1) */
#define UART_IER	1	/* Выход: Регистр разрешения прерываний */
#define UART_IIR	2	/* Вход: Регистр идентификации прерывания */
#define UART_FCR	2	/* Выход: Регистр управления FIFO */
#define UART_EFR	2	/* Ввод-вывод: Регистр расширенных функций */
				/* (только DLAB=1, 16C660) */
#define UART_LCR	3	/* Выход: Регистр управления линией */
#define UART_MCR	4	/* Выход: Регистр управления модемом */
#define UART_LSR	5	/* Вход: Регистр состояния линии */
#define UART_MSR	6	/* Вход: Регистр состояния модема */
#define UART_SCR	7	/* Ввод-вывод: Вспомогательный регистр */



/*
 * Это определения для регистра управления FIFO
 * (только 16650)
 */
#define UART_FCR_ENABLE_FIFO	0x01 /* Включить FIFO */
#define UART_FCR_CLEAR_RCVR	0x02 /* Очистить FIFO приемника */
#define UART_FCR_CLEAR_XMIT	0x04 /* Очистить FIFO передатчика */
#define UART_FCR_DMA_SELECT	0x08 /* Для приложений DMA */
#define UART_FCR_TRIGGER_MASK	0xC0 /* Маска для диапазона триггера FIFO */
#define UART_FCR_TRIGGER_1	0x00 /* Маска для триггера, установленного на 1 */
#define UART_FCR_TRIGGER_4	0x40 /* Маска для триггера, установленного на 4 */
#define UART_FCR_TRIGGER_8	0x80 /* Маска для триггера, установленного на 8 */
#define UART_FCR_TRIGGER_14	0xC0 /* Маска для триггера, установленного на 14 */
/* Переопределения для 16650 */
#define UART_FCR6_R_TRIGGER_8	0x00 /* Маска для триггера приема, установленного на 1 */
#define UART_FCR6_R_TRIGGER_16	0x40 /* Маска для триггера приема, установленного на 4 */
#define UART_FCR6_R_TRIGGER_24  0x80 /* Маска для триггера приема, установленного на 8 */
#define UART_FCR6_R_TRIGGER_28	0xC0 /* Маска для триггера приема, установленного на 14 */
#define UART_FCR6_T_TRIGGER_16	0x00 /* Маска для триггера передачи, установленного на 16 */
#define UART_FCR6_T_TRIGGER_8	0x10 /* Маска для триггера передачи, установленного на 8 */
#define UART_FCR6_T_TRIGGER_24  0x20 /* Маска для триггера передачи, установленного на 24 */
#define UART_FCR6_T_TRIGGER_30	0x30 /* Маска для триггера передачи, установленного на 30 */

/*
 * Это определения для регистра управления линией
 * 
 * Примечание: если длина слова составляет 5 бит (UART_LCR_WLEN5), то установка 
 * UART_LCR_STOP выберет 1,5 стоп-бита, а не 2 стоп-бита.
 */
#define UART_LCR_DLAB	0x80	/* Бит доступа к защелке делителя */
#define UART_LCR_SBC	0x40	/* Установка управления прерыванием (break) */
#define UART_LCR_SPAR	0x20	/* Фиксированная четность (?) */
#define UART_LCR_EPAR	0x10	/* Выбор четной четности */
#define UART_LCR_PARITY	0x08	/* Разрешение проверки четности */
#define UART_LCR_STOP	0x04	/* Стоп-биты: 0=1 стоп-бит, 1= 2 стоп-бита */
#define UART_LCR_WLEN5  0x00	/* Длина слова: 5 бит */
#define UART_LCR_WLEN6  0x01	/* Длина слова: 6 бит */
#define UART_LCR_WLEN7  0x02	/* Длина слова: 7 бит */
#define UART_LCR_WLEN8  0x03	/* Длина слова: 8 бит */

/*
 * Это определения для регистра состояния линии
 */
#define UART_LSR_TEMT	0x40	/* Передатчик пуст */
#define UART_LSR_THRE	0x20	/* Регистр хранения передатчика пуст */
#define UART_LSR_BI	0x10	/* Индикатор прерывания линии (break) */
#define UART_LSR_FE	0x08	/* Индикатор ошибки кадра */
#define UART_LSR_PE	0x04	/* Индикатор ошибки четности */
#define UART_LSR_OE	0x02	/* Индикатор ошибки переполнения */
#define UART_LSR_DR	0x01	/* Данные приемника готовы */

/*
 * Это определения для регистра идентификации прерывания
 */
#define UART_IIR_NO_INT	0x01	/* Нет ожидающих прерываний */
#define UART_IIR_ID	0x06	/* Маска для ID прерывания */

#define UART_IIR_MSI	0x00	/* Прерывание состояния модема */
#define UART_IIR_THRI	0x02	/* Регистр хранения передатчика пуст */
#define UART_IIR_RDI	0x04	/* Прерывание по данным приемника */
#define UART_IIR_RLSI	0x06	/* Прерывание по состоянию линии приемника */

/*
 * Это определения для регистра разрешения прерываний
 */
#define UART_IER_MSI	0x08	/* Разрешить прерывание по состоянию модема */
#define UART_IER_RLSI	0x04	/* Разрешить прерывание по состоянию линии приемника */
#define UART_IER_THRI	0x02	/* Разрешить прерывание по опустошению регистра хранения передатчика */
#define UART_IER_RDI	0x01	/* Разрешить прерывание по данным приемника */

/*
 * Это определения для регистра управления модемом
 */
#define UART_MCR_LOOP	0x10	/* Включить режим проверки шлейфа (loopback) */
#define UART_MCR_OUT2	0x08	/* Дополнение Out2 */
#define UART_MCR_OUT1	0x04	/* Дополнение Out1 */
#define UART_MCR_RTS	0x02	/* Дополнение RTS */
#define UART_MCR_DTR	0x01	/* Дополнение DTR */

/*
 * Это определения для регистра состояния модема
 */
#define UART_MSR_DCD	0x80	/* Обнаружение несущей данных (DCD) */
#define UART_MSR_RI	0x40	/* Индикатор вызова (RI) */
#define UART_MSR_DSR	0x20	/* Готовность набора данных (DSR) */
#define UART_MSR_CTS	0x10	/* Готовность к передаче (CTS) */
#define UART_MSR_DDCD	0x08	/* Изменение DCD (Delta DCD) */
#define UART_MSR_TERI	0x04	/* Индикатор спада сигнала вызова (Trailing edge RI) */
#define UART_MSR_DDSR	0x02	/* Изменение DSR (Delta DSR) */
#define UART_MSR_DCTS	0x01	/* Изменение CTS (Delta CTS) */
#define UART_MSR_ANY_DELTA 0x0F	/* Любой из битов изменения! */

/*
 * Это определения для регистра расширенных функций
 * (Только StarTech 16C660, когда DLAB=1)
 */
#define UART_EFR_CTS	0x80	/* Управление потоком CTS */
#define UART_EFR_RTS	0x40	/* Управление потоком RTS */
#define UART_EFR_SCD	0x20	/* Определение спецсимвола */
#define UART_EFR_ENI	0x10	/* Расширенное прерывание */
/*
 * младшие четыре бита управляют программным управлением потоком
 */

#include "io.h"
#define serial_echo_outb(v,a) outb((v),(a)+serial_base_ports[serial_tty])
#define serial_echo_inb(a)    inb((a)+serial_base_ports[serial_tty])
#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)
/* Ожидание освобождения передатчика и регистра хранения */
#define WAIT_FOR_XMITR \
 do { \
       lsr = serial_echo_inb(UART_LSR); \
 } while ((lsr & BOTH_EMPTY) != BOTH_EMPTY)

#if 0
static inline void serial_echo(int ch)
{
	int lsr;
	WAIT_FOR_XMITR;
	serial_echo_outb(ch, UART_TX);
}
static inline void serial_debug(int ch)
{
	serial_echo(ch);
	serial_echo('\r');
	serial_echo('\n');
}
#endif
#endif /* _LINUX_SERIAL_REG_H */