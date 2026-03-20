#ifndef QUEUE_H
#define QUEUE_H

/* forward declaration only */
typedef struct process process_t;

/* queue node (internal, but type must be known for queue_t) */
typedef struct qnode {
    process_t *p;
    struct qnode *next;
} qnode_t;

/* queue structure */
typedef struct {
    qnode_t *head;
    qnode_t *tail;
} queue_t;

/* queue operations */
void queue_init(queue_t *q);
int  queue_empty(queue_t *q);
void queue_push(queue_t *q, process_t *p);
process_t *queue_pop(queue_t *q);
process_t *queue_peek(queue_t *q);
void queue_clear(queue_t *q);

#endif /* QUEUE_H */
