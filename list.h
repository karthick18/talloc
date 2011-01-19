/* just a simple generic double list for talloc_test.c usage example */
#ifndef _LIST_H_
#define _LIST_H_

struct list_head
{
    struct list_head *next, *prev;
};

#define LIST_INIT(list) \
    struct list_head list = { .next = &(list), .prev = &(list) }

#define list_init(list) do { (list)->next = (list), (list)->prev = (list); } while(0)

#define LIST_ADD(element, before, after) do {   \
    (element)->prev = (before);                 \
    (element)->next = (after);                  \
    (before)->next = (element);                 \
    (after)->prev = (element);                  \
}while(0)

#define LIST_DEL(before, after) do {            \
    (before)->next = (after);                   \
    (after)->prev =  (before);                  \
}while(0)

#define LIST_EMPTY(list) ( (list)->next == (list) )

#define list_entry(element, cast, field) \
    (cast*) ( (unsigned char *)element - (unsigned long)&((cast*)0)->field )

#define list_for_each(iter, list) \
    for(iter = (list)->next ; iter != (list); iter = (iter)->next)

static __inline__ void list_add(struct list_head *element, struct list_head *head)
{
    if(!element->next && !element->prev)
    {
        struct list_head *after = head->next;
        LIST_ADD(element, head, after);
    }
}

static __inline__ void list_add_tail(struct list_head *element, struct list_head *head)
{
    if(!element->next && !element->prev)
    {
        struct list_head *before = head->prev;
        LIST_ADD(element, before, head);
    }
}

static __inline__ void list_del(struct list_head *element)
{
    if(element->next && element->prev)
    {
        struct list_head *before = element->prev;
        struct list_head *after = element->next;
        LIST_DEL(before, after);
        element->next = element->prev = NULL;
    }
}

static __inline__ void list_del_init(struct list_head *element)
{
    list_del(element);
    list_init(element);
}

#endif
