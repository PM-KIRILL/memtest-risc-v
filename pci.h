#ifndef MEMTEST_PCI_H
#define MEMTEST_PCI_H

int pci_conf_read(unsigned bus, unsigned dev, unsigned fn, unsigned reg, 
	unsigned len, unsigned long *value);
int pci_conf_write(unsigned bus, unsigned dev, unsigned fn, unsigned reg, 
	unsigned len, unsigned long value);
int pci_init(void);

#define MAKE_PCIE_ADDRESS(bus, device, function) (((bus) & 0xFF)<<20) | (((device) & 0x1F)<<15) | (((function) & 0x7)<<12)

/*
 * В архитектуре PCI каждое устройство имеет 256 байт конфигурационного адресного пространства,
 * из которых первые 64 байта стандартизированы следующим образом:
 */
#define PCI_VENDOR_ID		0x00	/* 16 бит */
#define PCI_DEVICE_ID		0x02	/* 16 бит */
#define PCI_COMMAND		0x04	/* 16 бит */
#define  PCI_COMMAND_IO		0x1	/* Разрешить ответ в пространстве ввода-вывода (I/O) */
#define  PCI_COMMAND_MEMORY	0x2	/* Разрешить ответ в пространстве памяти */
#define  PCI_COMMAND_MASTER	0x4	/* Разрешить управление шиной (bus mastering) */
#define  PCI_COMMAND_SPECIAL	0x8	/* Разрешить ответ на специальные циклы */
#define  PCI_COMMAND_INVALIDATE	0x10	/* Использовать запись и аннулирование памяти */
#define  PCI_COMMAND_VGA_PALETTE 0x20	/* Разрешить отслеживание палитры (palette snooping) */
#define  PCI_COMMAND_PARITY	0x40	/* Разрешить проверку четности */
#define  PCI_COMMAND_WAIT 	0x80	/* Разрешить пошаговую передачу адреса/данных (stepping) */
#define  PCI_COMMAND_SERR	0x100	/* Разрешить SERR */
#define  PCI_COMMAND_FAST_BACK	0x200	/* Разрешить быстрые последовательные записи */

#define PCI_STATUS		0x06	/* 16 бит */
#define  PCI_STATUS_CAP_LIST	0x10	/* Поддержка списка возможностей (Capability List) */
#define  PCI_STATUS_66MHZ	0x20	/* Поддержка шины PCI 2.1 66 МГц */
#define  PCI_STATUS_UDF		0x40	/* Поддержка пользовательских функций (UDF) [устарело] */
#define  PCI_STATUS_FAST_BACK	0x80	/* Принимать быстрые последовательные циклы */
#define  PCI_STATUS_PARITY	0x100	/* Обнаружена ошибка четности */
#define  PCI_STATUS_DEVSEL_MASK	0x600	/* Тайминг DEVSEL */
#define  PCI_STATUS_DEVSEL_FAST	0x000	
#define  PCI_STATUS_DEVSEL_MEDIUM 0x200
#define  PCI_STATUS_DEVSEL_SLOW 0x400
#define  PCI_STATUS_SIG_TARGET_ABORT 0x800 /* Устанавливается при аварийном завершении целевым устройством */
#define  PCI_STATUS_REC_TARGET_ABORT 0x1000 /* Подтверждение мастером того же самого */
#define  PCI_STATUS_REC_MASTER_ABORT 0x2000 /* Устанавливается при аварийном завершении мастером */
#define  PCI_STATUS_SIG_SYSTEM_ERROR 0x4000 /* Устанавливается при инициировании SERR */
#define  PCI_STATUS_DETECTED_PARITY 0x8000 /* Устанавливается при ошибке четности */

#define PCI_CLASS_REVISION	0x08	/* Старшие 24 бита — класс, младшие 8 —
					   ревизия */
#define PCI_REVISION_ID         0x08    /* ID ревизии */
#define PCI_CLASS_PROG          0x09    /* Программный интерфейс уровня регистров */
#define PCI_CLASS_DEVICE        0x0a    /* Класс устройства */

#define PCI_CACHE_LINE_SIZE	0x0c	/* 8 бит */
#define PCI_LATENCY_TIMER	0x0d	/* 8 бит */
#define PCI_HEADER_TYPE		0x0e	/* 8 бит */
#define  PCI_HEADER_TYPE_NORMAL	0
#define  PCI_HEADER_TYPE_BRIDGE 1
#define  PCI_HEADER_TYPE_CARDBUS 2

#define PCI_BIST		0x0f	/* 8 бит */
#define PCI_BIST_CODE_MASK	0x0f	/* Возвращаемый результат */
#define PCI_BIST_START		0x40	/* 1 для запуска BIST, 2 сек или меньше */
#define PCI_BIST_CAPABLE	0x80	/* 1, если поддерживает BIST */

/*
 * Базовые адреса определяют местоположение в памяти или пространстве ввода-вывода.
 * Декодируемый размер можно определить, записав значение 
 * 0xffffffff в регистр и считав его обратно. Декодируются 
 * только установленные биты (1).
 */
#define PCI_BASE_ADDRESS_0	0x10	/* 32 бита */
#define PCI_BASE_ADDRESS_1	0x14	/* 32 бита [только htype 0,1] */
#define PCI_BASE_ADDRESS_2	0x18	/* 32 бита [только htype 0] */
#define PCI_BASE_ADDRESS_3	0x1c	/* 32 бита */
#define PCI_BASE_ADDRESS_4	0x20	/* 32 бита */
#define PCI_BASE_ADDRESS_5	0x24	/* 32 бита */
#define  PCI_BASE_ADDRESS_SPACE	0x01	/* 0 = память, 1 = ввод-вывод */
#define  PCI_BASE_ADDRESS_SPACE_IO 0x01
#define  PCI_BASE_ADDRESS_SPACE_MEMORY 0x00
#define  PCI_BASE_ADDRESS_MEM_TYPE_MASK 0x06
#define  PCI_BASE_ADDRESS_MEM_TYPE_32	0x00	/* 32-битный адрес */
#define  PCI_BASE_ADDRESS_MEM_TYPE_1M	0x02	/* Ниже 1М [устарело] */
#define  PCI_BASE_ADDRESS_MEM_TYPE_64	0x04	/* 64-битный адрес */
#define  PCI_BASE_ADDRESS_MEM_PREFETCH	0x08	/* поддерживает предвыборку? */
#define  PCI_BASE_ADDRESS_MEM_MASK	(~0x0fUL)
#define  PCI_BASE_ADDRESS_IO_MASK	(~0x03UL)
/* бит 1 зарезервирован, если address_space = 1 */


/* Классы и подклассы устройств */
#define PCI_CLASS_NOT_DEFINED		0x0000
#define PCI_CLASS_NOT_DEFINED_VGA	0x0001

#define PCI_BASE_CLASS_BRIDGE		0x06
#define PCI_CLASS_BRIDGE_HOST		0x0600

#endif /* MEMTEST_PCI_H */
