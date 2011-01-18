#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <talloc.h>

static int pool_destructor(void *p)  { printf("Inside [%s] for object at [%p]\n", __FUNCTION__, p); return 0;}

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

struct test_list
{
    char *s;
    struct list_head list;
} __attribute__((aligned(16)));

static LIST_INIT(test_list);

static void test2(void *parent)
{
    struct test_list *t = talloc(parent, struct test_list);
    assert(t);
    talloc_set_name(t, "test_list");
    t->s = talloc_strdup(t, "foo");
    assert(t->s);
    talloc_free(t);
}

/*
 * Steal the child list elements into the new list
 */
static void test5(void *parent, struct list_head *src_list)
{
    void *new_parent = talloc_new(NULL);
    void *new_list = talloc_new(new_parent);
    struct list_head *iter;
    assert(new_parent);
    assert(new_list);
    talloc_set_name(new_parent, "new_parent");
    talloc_set_name(new_list, "new_list");
    list_for_each(iter, src_list)
    {
        struct test_list *s = list_entry(iter, struct test_list, list);
        assert(talloc_steal(new_list, s) == s);
    }
    talloc_free(new_parent);
}

static void test4(struct list_head *src_list)
{
    static LIST_INIT(dst_list);
    struct list_head *iter;
    list_for_each(iter, src_list)
    {
        struct test_list *s = list_entry(iter, struct test_list, list);
        struct test_list *d = talloc_zero(s, struct test_list);
        assert(d);
        d->s = talloc_reference(s, s->s);
        assert(d->s);
        assert(talloc_reference_count(s->s) == 1);
        assert(talloc_unlink(s, s->s) == 0);
        list_add(&d->list, &dst_list);
    }
    while(!LIST_EMPTY(&dst_list))
    {
        struct test_list *d = list_entry(dst_list.next, struct test_list, list);
        list_del(&d->list);
        printf("Duped list [%s], name [%s]\n", talloc_get_name(d), d->s);
        assert(talloc_unlink(talloc_find_parent_byname(d, d->s), d->s) == 0);
    }
}

static void test3(void *parent)
{
    register int i;
    void *list_parent = talloc_size(parent, 0);
    struct test_list *l;
    assert(list_parent);
    for(i = 0; i < 10; ++i)
    {
        l = talloc_zero(list_parent, struct test_list);
        assert(l);
        talloc_set_name(l, "test_list_%d", i+1);
        l->s = talloc_strdup(l, talloc_get_name(l));
        assert(l->s);
        list_add_tail(&l->list, &test_list);
    }
    test4(&test_list);
    while(!LIST_EMPTY(&test_list))
    {
        struct test_list *l = list_entry(test_list.next, struct test_list, list);
        list_del(&l->list);
        printf("list name [%s], dup [%s]\n", talloc_get_name(l), l->s);
    }
    test5(list_parent, &test_list); /*move elements off to a new parent and free*/
    talloc_free(list_parent); 
}

static void test_cache(void)
{
    void *cache;
    void *parent, *parent_cache;
    struct test_list *list = NULL, **listpp = &list;
    int i;
    int frees = 0;
    parent = talloc_new(NULL);
    assert(parent);
    parent_cache = talloc_pool_cache(parent, 4096, sizeof(struct test_list));
    assert(parent_cache);
    cache = talloc_pool_cache(parent_cache, 1024, sizeof(struct test_list));
    assert(cache);
    for(i = 0; i < 1024/sizeof(struct test_list) + 10; ++i)
    {
        struct test_list *p = talloc_cache(cache);
        if(!p)
        {
            printf("Allocation failed for object [%d]\n", i);
            assert(i == 1024/sizeof(struct test_list));
            break;
        }
        *(struct test_list**)p = *listpp;
        *listpp = p;
    }
    while(*listpp && frees < (i >> 1))
    {
        struct test_list *p = *listpp;
        struct test_list *next = *(struct test_list**)p;
        int res ;
        printf("Freeing block [%p]\n", p);
        res = talloc_free(p);
        assert(res == 0);
        *listpp = next;
        ++frees;
    }
    for(i = 0; i < frees; ++i)
    {
        struct test_list *p = talloc_cache(cache);
        assert(p);
        *(struct test_list **)p = *listpp;
        *listpp = p;
    }
    while(*listpp)
    {
        struct test_list *p = *listpp;
        struct test_list *next = *(struct test_list**)p;
        int res;
        printf("Freeing block [%p]\n", p);
        res = talloc_free(p);
        assert(res == 0);
        *listpp = next;
    }
    talloc_free(cache);
    talloc_free(parent_cache);
    talloc_free(parent);
}

int main()
{
    struct foo { int bar; } __attribute__((aligned(8)));
    struct bar { int foo; } __attribute__((aligned(8)));
    register int i;
    {
        talloc_set_log_stderr();
        talloc_enable_leak_report_full();
    }
    void *parent = talloc_new(NULL);
    void *poolfoo = talloc_pool(parent, sizeof(struct foo) * 1024);
    void *poolbar = talloc_pool(poolfoo, sizeof(struct bar) * 100); /*subpool of foo*/
    void *arr[100];
    assert(parent);
    assert(poolfoo);
    assert(poolbar);
    talloc_set_destructor(poolbar, pool_destructor);
    talloc_set_destructor(poolfoo, pool_destructor);
    talloc_set_name(poolbar, "bar");
    talloc_set_name(poolfoo, "foo");
    talloc_set_name(parent, "parent");
    for(i = 0; i < 100; ++i)
    {
        void *p = talloc_size(poolbar, sizeof(struct bar));
        assert(p);
        memset(p, 0, sizeof(struct bar));
        arr[i] = p;
        assert(talloc_reference(poolbar, p) == arr[i]);
        assert(talloc_reference_count(p) == 1);
        assert(talloc_unlink(poolbar, p) == 0);
        assert(talloc_reference_count(p) == 0);
    }
    for(i = 0; i < 100; ++i)
        assert(talloc_free(arr[i]) == 0);
    assert(talloc_free(poolbar) == 0);
    assert(talloc_free(poolfoo) == 0);
    test2(parent);
    test3(parent);
    test_cache();
    assert(talloc_free(parent) == 0);
    return 0;
}


/*
 * Local variables:
 * c-file-style: "linux"
 * compile-command: "gcc -Wall -g -o talloc_test talloc_test.c -ltalloc"
 * tab-width: 4
 * End:
 */
