#include "arch.h"
#include "globals.h"
#include "test.h"
#include "smp.h"

unsigned act_cpus;
unsigned found_cpus = 0;
unsigned num_cpus = 1; // Как минимум один процессор (BSP)
struct barrier_s *barr;

void barrier_init(int max)
{
	/* Установка адреса структуры барьера */
	barr = (struct barrier_s *)BARRIER_ADDR;
        barr->lck.slock = 1;
        barr->mutex.slock = 1;
        barr->maxproc = max;
        barr->count = max;
        barr->st1.slock = 1;
        barr->st2.slock = 0;
}

void s_barrier_init(int max)
{
        barr->s_lck.slock = 1;
        barr->s_maxproc = max;
        barr->s_count = max;
        barr->s_st1.slock = 1;
        barr->s_st2.slock = 0;
}

void barrier()
{
	if (num_cpus == 1 || v->fail_safe & 3) {
		return;
	}
	spin_wait(&barr->st1);     /* Ожидание, если барьер активен */
        spin_lock(&barr->lck);	   /* Получение блокировки для структуры barr */
        if (--barr->count == 0) {  /* Последний процесс? */
                barr->st1.slock = 0;   /* Задержать любые повторно входящие процессы */
                barr->st2.slock = 1;   /* Разблокировать остальные процессы */
                barr->count++;
                spin_unlock(&barr->lck);
        } else {
                spin_unlock(&barr->lck);
                spin_wait(&barr->st2);	/* ожидание прибытия остальных участников */
                spin_lock(&barr->lck);
                if (++barr->count == barr->maxproc) {
                        barr->st1.slock = 1;
                        barr->st2.slock = 0;
                }
                spin_unlock(&barr->lck);
        }
}

void s_barrier()
{
	if (run_cpus == 1 || v->fail_safe & 3) {
		return;
	}
	spin_wait(&barr->s_st1);     /* Ожидание, если барьер активен */
        spin_lock(&barr->s_lck);     /* Получение блокировки для структуры barr */
        if (--barr->s_count == 0) {  /* Последний процесс? */
                barr->s_st1.slock = 0;   /* Задержать любые повторно входящие процессы */
                barr->s_st2.slock = 1;   /* Разблокировать остальные процессы */
                barr->s_count++;
                spin_unlock(&barr->s_lck);
        } else {
                spin_unlock(&barr->s_lck);
                spin_wait(&barr->s_st2);	/* ожидание прибытия остальных участников */
                spin_lock(&barr->s_lck);
                if (++barr->s_count == barr->s_maxproc) {
                        barr->s_st1.slock = 1;
                        barr->s_st2.slock = 0;
                }
                spin_unlock(&barr->s_lck);
        }
}
