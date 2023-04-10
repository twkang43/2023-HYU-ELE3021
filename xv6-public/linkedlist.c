#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// Add proc to the end of the queue 'queue'
int
addListEnd(struct proc* proc, struct proc* queue)
{
    struct proc* prev = queue;

    while(prev->next)
        prev = prev->next;
    prev->next = proc;
    
    return 0;
}

// Add proc to the front of the queue 'queue'
int
addListFront(struct proc* proc, struct proc* queue)
{
    struct proc* tmp = queue->next;
    proc->next = tmp;
    queue->next = proc;

    return 0;
}

// Return 1 if proc is the last element of the queue 'queue'
int
isLast(struct proc* proc, struct proc* queue)
{
    struct proc* cur = queue;
    while(cur->next)
        cur = cur->next;
    return proc == cur;
}

// Delele proc from the queue 'queue'
int
deleteList(struct proc* proc, struct proc* queue)
{
    struct proc* prev = queue;

    while(prev->next != 0 && prev->next != proc)
        prev = prev->next;

    if(isLast(prev, queue)){
        cprintf("ERROR : process with id '%d' is not in a queue '%d'\n", proc->pid, proc->queue);
        return 0;
    }
    
    struct proc* tmp = prev->next;
    prev->next = tmp->next;

    return 0;
}

// Get the number of processes in the queue 'queue' -> 작동 확인해보기
int
getNumList(struct proc* queue)
{
    int num = 0;
    struct proc* cur = queue;
    
    while(cur->next){
        ++num;
        cur = cur->next;
    }

    return num;
}