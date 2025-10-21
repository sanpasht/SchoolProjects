/**
 * Tony Givargis
 * Copyright (C), 2023-2025
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.c
 */

#undef _FORTIFY_SOURCE

#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include "system.h"
#include "scheduler.h"

#define STACK_SIZE (64 * 1024)

enum thread_status {
    INIT,
    RUNNING,
    SLEEPING,
    TERMINATED
};

struct thread {
    int status;
    void *stack;
    scheduler_fnc_t fnc;
    void *arg;
    jmp_buf ctx;
    struct thread *link;
};

static struct thread *head = 0;
static struct thread *current = 0;
static jmp_buf scheduler_ctx;

/* ---------------------------------------------------- */
/* Create new thread */
int scheduler_create(scheduler_fnc_t fnc, void *arg)
{
    struct thread *t, *p;

    t = malloc(sizeof(*t));
    if (!t) return -1;

    memset(t, 0, sizeof(*t));
    t->stack = malloc(STACK_SIZE);
    if (!t->stack) {
        free(t);
        return -1;
    }

    t->fnc = fnc;
    t->arg = arg;
    t->status = INIT;

    if (!head) {
        head = t;
        t->link = t;
    } else {
        p = head;
        while (p->link != head)
            p = p->link;
        p->link = t;
        t->link = head;
    }

    return 0;
}

/* ---------------------------------------------------- */
/* Pick the next runnable thread */
static struct thread *candidate(void)
{
    struct thread *start = current ? current->link : head;
    struct thread *t = start;

    if (!t) return 0;

    do {
        if (t->status == INIT || t->status == SLEEPING)
            return t;
        t = t->link;
    } while (t != start);

    return 0;
}

/* ---------------------------------------------------- */
/* Yield control back to scheduler */
void scheduler_yield(void)
{
    if (!current) return;

    if (setjmp(current->ctx) == 0) {
        current->status = SLEEPING;
        longjmp(scheduler_ctx, 1);
    }
}

/* ---------------------------------------------------- */
/* The core scheduling logic (one context switch) */
static void schedule(void)
{
    struct thread *t;
    unsigned long rsp;

    t = candidate();
    if (!t) return;

    current = t;

    if (setjmp(scheduler_ctx) == 0) {
        if (current->status == INIT) {
            current->status = RUNNING;

#if defined(__x86_64__)
            rsp = (unsigned long)current->stack + STACK_SIZE;
            rsp &= ~(unsigned long)0xF;  /* 16-byte align */
            __asm__ volatile (
                "mov %0, %%rsp\n"
                "mov %1, %%rdi\n"
                "call *%2\n"
                :
                : "r"(rsp), "r"(current->arg), "r"(current->fnc)
                : "rdi", "memory"
            );
#else
            current->fnc(current->arg);
#endif
            current->status = TERMINATED;
            longjmp(scheduler_ctx, 1);
        } 
        else if (current->status == SLEEPING) {
            current->status = RUNNING;
            longjmp(current->ctx, 1);
        }
    }
}

/* ---------------------------------------------------- */
/* Run until all threads are finished */
void scheduler_execute(void)
{
    
    struct thread *t;
    struct thread *next;
    while (candidate())
        schedule();



    t = head;
    if (t) {
        struct thread *p = head;
        while (p->link != head)
            p = p->link;
        p->link = 0;

        while (t) {
            next = t->link;
            free(t->stack);
            free(t);
            t = next;
        }
    }

    head = 0;
    current = 0;
}

/* ---------------------------------------------------- */
/* Destroy all threads (if needed externally) */
void scheduler_destroy(void)
{
    struct thread *t;
    struct thread *next;

    t = head;
    if (t) {
        struct thread *p = head;
        while (p->link != head)
            p = p->link;
        p->link = 0;

        while (t) {
            next = t->link;
            free(t->stack);
            free(t);
            t = next;
        }
    }

    head = 0;
    current = 0;
}