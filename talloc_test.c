/*
 * Simple talloc usage. for guys trying to wrap their mallocs with the spicy talloc topping.
 * To compile : 
 * make talloc_test
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <talloc.h>
#include "list.h"

static int pool_destructor(void *p)  { printf("Inside [%s] for object at [%p]\n", __FUNCTION__, p); return 0;}

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
    talloc_free(t); /* will automatically free dup'ed t->s */
}

/*
 * Just steal the child list elements into the new list. 
 * But after the free here, the child elements hanging off the last parent list shouldn't be used.
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
        assert(talloc_steal(new_list, s) == s); /* move the list element contexts */
    }
    list_init(src_list); /* empty the list */
    talloc_free(new_parent); /* will automatically free all the list elements stolen from src_list */
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
        list_add(&d->list, &dst_list);
    }
    while(!LIST_EMPTY(&dst_list))
    {
        struct test_list *d = list_entry(dst_list.next, struct test_list, list);
        list_del(&d->list);
        printf("Duped list [%s], name [%s]\n", talloc_get_name(d), d->s);
        /* will drop the reference for the pointer grabbed from the parent */
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
    test5(list_parent, &test_list); /*move elements off to a new parent and free*/
    while(!LIST_EMPTY(&test_list))
    {
        struct test_list *l = list_entry(test_list.next, struct test_list, list);
        list_del(&l->list);
        printf("list name [%s], dup [%s]\n", talloc_get_name(l), l->s);
    }
    talloc_free(list_parent); 
}

/*
 * Test the newly added talloc static object cache.
 */
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
        talloc_enable_leak_report_full(); /* this would register an atexit for a leak report */
    }
    void *parent = talloc_new(NULL);
    void *poolfoo = talloc_pool(parent, sizeof(struct foo) * 1024);
    /*
     *subpool or nested pool of foo. Without my talloc changes, it would crash default talloc 2.0.1 
     * on a subpool free of poolbar 
     */
    void *poolbar = talloc_pool(poolfoo, sizeof(struct bar) * 100); 
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

