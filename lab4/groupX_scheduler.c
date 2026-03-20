/*
 * Lab 4 - Scheduler
 *
 * IMPORTANT:
 * - Do NOT change print_log() format (autograder / TA diff expects exact output).
 * - Do NOT change the order of operations in the main tick loop.
 * - You may change internal implementations of the TODO functions freely,
 *   as long as behavior matches the lab requirements.
 * - compile: $make
 *   run testcase: $./groupX_scheduler < test_input.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "queue.h"

/*
 * Assumptions / Lab rules:
 * - user priority values are 0,1,2,3 (0 = Real-Time, 1..3 = user queues)
 * - all processes have mem_req > 0
 * - RT process memory is ALWAYS 64MB (reserved); user processes share 960MB
 * - user memory allocation range is [64, 1023], integer MB, contiguous blocks
 * - continuous allocation policy: First Fit (per modified handout)
 * - Processes are sorted by arrival_time in the test files
 */

/* ----------------------------
 * Global "hardware resources"
 * ---------------------------- */
int printers  = 2;
int scanners  = 1;
int modems    = 1;
int cd_drives = 2;

/* Total user-available memory (excluding RT reserved region) */
int memory           = 960;
int memory_real_time = 64;

/* ----------------------------
 * Ready queues (provided by queue.h / queue.c)
 * ---------------------------- */
queue_t rt_queue;       /* real-time queue */
queue_t sub_queue;      /* submission queue (user processes wait here until admitted) */
queue_t user_queue[3];  /* user queues: index 0->PR1, 1->PR2, 2->PR3 */

/* ----------------------------
 * Free-block list for user memory (First Fit, addresses 64..1023)
 * ---------------------------- */
typedef struct free_block free_block_t;

typedef struct free_block {
    int start;
    int size;
    struct free_block *next;
} free_block_t;

free_block_t *freelist; /* head of sorted free-block list */

#define MAX_PROCESSES 128

/* ----------------------------
 * Process state and process struct
 * ---------------------------- */
typedef enum {
    NEW,
    SUBMITTED,
    READY,
    RUNNING,
    TERMINATED
} proc_state_t;

typedef struct process {
    int pid;

    int arrival_time;
    int init_prio;
    int cpu_total;
    int mem_req;
    int printers;
    int scanners;
    int modems;
    int cds;

    int cpu_remain;
    int current_prio;
    proc_state_t state;

    int mem_start;
} process_t;

/* =========================================================
 * FUNCTION PROTOTYPES
 * ========================================================= */
void memory_init(void);
void admit_process(void);
process_t *dispatch(process_t **cur_running_rt);
void run_process(process_t *p);
void post_run(process_t *p, process_t **cur_running_rt);
int  termination_check(int processNo, int process_count, process_t *cur_running_rt);

int  memory_can_allocate(int req_size);
int  memory_allocate(process_t *p);
int  memory_free(process_t *p);

int  resource_available(process_t *p);
void resource_occupy(process_t *p);
void resource_free(process_t *p);

void arrival(process_t *p);

/* =========================================================
 * LOG OUTPUT (DO NOT MODIFY)
 * ========================================================= */
void print_log(process_t *ready_process, int time) {
    if (ready_process == NULL) {
        printf("[t=%d] IDLE\n", time);
    } else {
        printf(
            "[t=%d] RUN PID=%d PR=%d CPU=%d MEM_ST=%d MEM=%d P=%d S=%d M=%d C=%d\n",
            time,
            ready_process->pid,
            ready_process->current_prio,
            ready_process->cpu_remain,
            ready_process->mem_start,
            ready_process->mem_req,
            ready_process->printers,
            ready_process->scanners,
            ready_process->modems,
            ready_process->cds
        );
    }
}

/* =========================================================
 * MEMORY MANAGER
 * =========================================================
 * User memory spans addresses 64..1023 (960 MB).
 * We maintain a linked list of free blocks sorted by start address.
 * First Fit: pick the first block with size >= req_size.
 * ========================================================= */

void memory_init(void) {
    freelist = (free_block_t *)malloc(sizeof(free_block_t));
    freelist->start = 64;       /* user memory starts at 64 */
    freelist->size  = 960;      /* 960 MB total */
    freelist->next  = NULL;
}

/* Returns 1 if req_size MB can be satisfied by the free list */
int memory_can_allocate(int req_size) {
    for (free_block_t *b = freelist; b != NULL; b = b->next) {
        if (b->size >= req_size) return 1;
    }
    return 0;
}

/* First Fit: find first block that fits, carve out req bytes, set p->mem_start */
int memory_allocate(process_t *p) {
    for (free_block_t *b = freelist; b != NULL; b = b->next) {
        if (b->size >= p->mem_req) {
            p->mem_start = b->start;
            b->start    += p->mem_req;
            b->size     -= p->mem_req;
            /* Remove the block from the list if it's now empty */
            if (b->size == 0) {
                /* Find and remove b from the list */
                if (freelist == b) {
                    freelist = b->next;
                    free(b);
                } else {
                    free_block_t *prev = freelist;
                    while (prev->next != b) prev = prev->next;
                    prev->next = b->next;
                    free(b);
                }
            }
            return 0; /* success */
        }
    }
    return -1; /* no space */
}

/* Return p's memory block back to the free list, then merge adjacent blocks */
int memory_free(process_t *p) {
    free_block_t *newb = (free_block_t *)malloc(sizeof(free_block_t));
    newb->start = p->mem_start;
    newb->size  = p->mem_req;
    newb->next  = NULL;

    /* Insert in sorted order by start address */
    if (freelist == NULL || newb->start < freelist->start) {
        newb->next = freelist;
        freelist   = newb;
    } else {
        free_block_t *cur = freelist;
        while (cur->next != NULL && cur->next->start < newb->start) {
            cur = cur->next;
        }
        newb->next = cur->next;
        cur->next  = newb;
    }

    /* Merge adjacent / overlapping blocks */
    free_block_t *cur = freelist;
    while (cur != NULL && cur->next != NULL) {
        if (cur->start + cur->size >= cur->next->start) {
            /* Merge cur and cur->next */
            free_block_t *nxt = cur->next;
            int end = cur->start + cur->size;
            int nxt_end = nxt->start + nxt->size;
            if (nxt_end > end) cur->size = nxt_end - cur->start;
            cur->next = nxt->next;
            free(nxt);
        } else {
            cur = cur->next;
        }
    }
    return 0;
}

/* =========================================================
 * RESOURCE HELPERS
 * ========================================================= */

int resource_available(process_t *p) {
    return (printers  >= p->printers  &&
            scanners  >= p->scanners  &&
            modems    >= p->modems    &&
            cd_drives >= p->cds);
}

void resource_occupy(process_t *p) {
    printers  -= p->printers;
    scanners  -= p->scanners;
    modems    -= p->modems;
    cd_drives -= p->cds;
}

void resource_free(process_t *p) {
    printers  += p->printers;
    scanners  += p->scanners;
    modems    += p->modems;
    cd_drives += p->cds;
}

/* =========================================================
 * ARRIVAL
 * =========================================================
 * RT processes go straight into rt_queue (no admission needed).
 * User processes go into sub_queue to wait for admission.
 * ========================================================= */

void arrival(process_t *p) {
    if (p->init_prio == 0) {
        /* RT process: memory is the fixed 0..63 region, no resource check */
        p->mem_start   = 0;
        p->state       = READY;
        queue_push(&rt_queue, p);
    } else {
        p->state = SUBMITTED;
        queue_push(&sub_queue, p);
    }
}

/* =========================================================
 * ADMIT
 * =========================================================
 * Scan sub_queue from front to back.
 * For each process, if resources AND memory are available, admit it:
 *   - allocate memory
 *   - reserve resources
 *   - move to the appropriate user_queue (index = init_prio - 1)
 *
 * We do a full scan (not just the head) so that if the front
 * process can't fit, a later one that fits can still be admitted.
 * NOTE: the test output suggests we admit in FIFO order only —
 * we only admit as many as we can in one pass.
 * ========================================================= */

void admit_process(void) {
    process_t *held[MAX_PROCESSES];
    int held_count = 0;

    while (!queue_empty(&sub_queue)) {
        process_t *p = queue_pop(&sub_queue);
        if (p == NULL) break;

        if (resource_available(p) && memory_can_allocate(p->mem_req)) {
            memory_allocate(p);
            resource_occupy(p);
            p->state        = READY;
            p->current_prio = p->init_prio;
            queue_push(&user_queue[p->current_prio - 1], p);
        } else {
            held[held_count++] = p;
        }
    }

    for (int i = 0; i < held_count; i++) {
        queue_push(&sub_queue, held[i]);
    }
}
/* =========================================================
 * DISPATCH
 * =========================================================
 * Priority order: RT > user_queue[0] (PR=1) > [1] (PR=2) > [2] (PR=3)
 *
 * RT is non-preemptive once running: if cur_running_rt is set,
 * keep running it. Otherwise grab the front of rt_queue.
 *
 * If no RT is running/waiting, pick from user queues highest-first.
 * ========================================================= */

process_t *dispatch(process_t **cur_running_rt) {
    /* If an RT job is already running, keep it */
    if (*cur_running_rt != NULL) {
        return *cur_running_rt;
    }

    /* Check if a new RT job arrived */
    if (!queue_empty(&rt_queue)) {
        *cur_running_rt = queue_pop(&rt_queue);
        (*cur_running_rt)->state = RUNNING;
        return *cur_running_rt;
    }

    /* No RT job — pick highest-priority user queue */
    for (int i = 0; i < 3; i++) {
        if (!queue_empty(&user_queue[i])) {
            process_t *p = queue_pop(&user_queue[i]);
            p->state = RUNNING;
            return p;
        }
    }

    return NULL; /* idle */
}

/* =========================================================
 * RUN PROCESS
 * =========================================================
 * Execute exactly 1 tick: decrement cpu_remain.
 * print_log() is called AFTER this, so the log shows the
 * decremented value.
 * ========================================================= */

void run_process(process_t *p) {
    if (p == NULL) return;
    p->cpu_remain--;
}

/* =========================================================
 * POST RUN
 * =========================================================
 * After each tick:
 *   - If cpu_remain == 0: terminate, free memory & resources
 *   - RT job that finished: clear cur_running_rt
 *   - User job that didn't finish: demote priority (1->2->3, stays at 3),
 *     re-enqueue in new priority queue
 * ========================================================= */

void post_run(process_t *p, process_t **cur_running_rt) {
    if (p == NULL) return;

    if (p->cpu_remain == 0) {
        /* Process finished */
        p->state = TERMINATED;
        if (p->init_prio == 0) {
            /* RT process */
            *cur_running_rt = NULL;
            /* RT memory (0..63) is reserved and never freed to user pool */
        } else {
            /* User process: free memory and resources */
            memory_free(p);
            resource_free(p);
        }
    } else {
        /* Process still has work to do */
        if (p->init_prio == 0) {
            /* RT: stays as cur_running_rt, nothing to do */
        } else {
            /* User: demote priority (max PR=3), re-enqueue */
            p->state = READY;
            if (p->current_prio < 3) {
                p->current_prio++;
            }
            queue_push(&user_queue[p->current_prio - 1], p);
        }
    }
}

/* =========================================================
 * TERMINATION CHECK (provided logic — kept as-is)
 * ========================================================= */

int termination_check(int processNo, int process_count, process_t *cur_running_rt) {
    return  processNo == process_count  &&
            cur_running_rt == NULL      &&
            queue_empty(&rt_queue)      &&
            queue_empty(&sub_queue)     &&
            queue_empty(&user_queue[0]) &&
            queue_empty(&user_queue[1]) &&
            queue_empty(&user_queue[2]);
}

/* =========================================================
 * MAIN (DO NOT CHANGE LOOP ORDER)
 * ========================================================= */
int main(void) {
    queue_init(&rt_queue);
    queue_init(&sub_queue);
    for (int i = 0; i < 3; i++) {
        queue_init(&user_queue[i]);
    }

    memory_init();

    process_t processes[MAX_PROCESSES];
    int process_count = 0;

    while (process_count < MAX_PROCESSES) {
        int a, p, cpu, mem, pr, sc, mo, cd;
        if (scanf("%d %d %d %d %d %d %d %d",
                  &a, &p, &cpu, &mem, &pr, &sc, &mo, &cd) != 8) {
            break;
        }

        processes[process_count].arrival_time = a;
        processes[process_count].init_prio    = p;
        processes[process_count].cpu_total    = cpu;
        processes[process_count].mem_req      = mem;
        processes[process_count].printers     = pr;
        processes[process_count].scanners     = sc;
        processes[process_count].modems       = mo;
        processes[process_count].cds          = cd;

        processes[process_count].pid          = process_count;
        processes[process_count].cpu_remain   = cpu;
        processes[process_count].current_prio = p;
        processes[process_count].state        = NEW;
        processes[process_count].mem_start    = 0;

        process_count++;
    }

    int processNo          = 0;
    process_t *cur_running_rt = NULL;

    for (int time = 0; ; time++) {
        /* 1) ARRIVAL */
        for (; processNo < process_count; processNo++) {
            if (processes[processNo].arrival_time == time) {
                arrival(&processes[processNo]);
            } else {
                break;
            }
        }

        /* 2) ADMIT */
        admit_process();

        /* 3) DISPATCH */
        process_t *ready_process = dispatch(&cur_running_rt);

        /* 4) RUN */
        run_process(ready_process);

        /* 5) PRINT */
        print_log(ready_process, time);

        /* 6) POST-RUN */
        post_run(ready_process, &cur_running_rt);

        /* Terminate when all work is done */
        if (termination_check(processNo, process_count, cur_running_rt)) {
            break;
        }
    }

    return 0;
}