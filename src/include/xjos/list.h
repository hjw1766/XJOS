#ifndef XJOS_LIST_H
#define XJOS_LIST_H

#include <libc/stddef.h>
#include <libc/assert.h>



// ===================================
//      Core Data Structures
// ===================================

typedef struct list_node_t {
    struct list_node_t *prev;
    struct list_node_t *next;
} list_node_t;

typedef struct {
    list_node_t head;
} list_t;


// ===================================
//      List-Specific Macros
// ===================================

/**
 * @brief Gets the pointer to the containing struct from a pointer to a list node member.
 */
#define list_entry(ptr, type, member) \
    element_entry(type, member, ptr)

/**
 * @brief key - node offset in type
 */
#define list_node_offset(type, node, key) \
    ((int)(&((type *)0)->key) - (int)(&((type *)0)->node))

/**
 * @brief ndoe + offset(key - node) => key
 */
#define list_node_key(node, offset) \
    *(int *)((int)node + offset)

/**
 * @brief Iterates over a list.
 * @param pos  The list_node_t* to use as a loop cursor.
 * @param list The list_t pointer to iterate over.
 */
#define list_for_each(pos, list) \
    for (list_node_t *pos = (list)->head.next; pos != &(list)->head; pos = pos->next)

/**
 * @brief Iterates over a list of structs.
 * @param pos    A pointer to your struct, used as a loop cursor.
 * @param list   The list_t pointer to iterate over.
 * @param member The name of the list_node_t member in your struct.
 */
#define list_for_each_entry(pos, list, member)                                  \
    for (pos = list_entry((list)->head.next, typeof(*(pos)), member);           \
         &pos->member != &(list)->head;                                         \
         pos = list_entry(pos->member.next, typeof(*(pos)), member))


// func declaration
static inline void list_init(list_t *list);
static inline void list_push(list_t *list, list_node_t *node);
static inline void list_pushback(list_t *list, list_node_t *node);
static inline list_node_t *list_pop(list_t *list);
static inline list_node_t *list_popback(list_t *list);
static inline void list_remove(list_node_t *node);
static inline bool list_empty(list_t *list);
static inline bool list_search(list_t *list, list_node_t *node_to_find);
static inline u32 list_len(list_t *list);
static inline void list_insert_sort(list_t *list, list_node_t *node, int offset);


// ===================================
//      API Function Implementations
// ===================================

// Internal helper: inserts new_node between prev and next.
static _inline void __list_add(list_node_t *new_node, list_node_t *prev, list_node_t *next) {
    // one node match one list
    assert(new_node->prev == NULL && new_node->next == NULL);

    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

// Internal helper: removes the node between prev and next.
static _inline void __list_del(list_node_t *prev, list_node_t *next) {
    next->prev = prev;
    prev->next = next;
}

// Initializes a list.
static _inline void list_init(list_t *list) {
    list->head.next = &list->head;
    list->head.prev = &list->head;
}


// Initializes a list node (to a "not in list" state).
static _inline void list_node_init(list_node_t *node) {
    node->prev = NULL;
    node->next = NULL;
}

// Inserts 'node' after 'anchor'.
static _inline void list_insert_after(list_node_t *anchor, list_node_t *node) {
    __list_add(node, anchor, anchor->next);
}

// Inserts 'node' before 'anchor'.
static _inline void list_insert_before(list_node_t *anchor, list_node_t *node) {
    __list_add(node, anchor->prev, anchor);
}

// Adds a node to the front of the list (head).
static _inline void list_push(list_t *list, list_node_t *node) {
    list_insert_after(&list->head, node);
}

// Adds a node to the back of the list (tail).
static _inline void list_pushback(list_t *list, list_node_t *node) {
    list_insert_before(&list->head, node);
}

// Removes a node from the list.
static _inline void list_remove(list_node_t *node) {
    __list_del(node->prev, node->next);
    // Pointer Poisoning: Invalidate pointers to make use-after-free bugs fail fast.
    node->prev = NULL;
    node->next = NULL;
}

// Removes and returns the node from the front of the list.
static _inline list_node_t *list_pop(list_t *list) {
    assert(!list_empty(list));
    list_node_t *node = list->head.next;
    list_remove(node);
    return node;
}

// Removes and returns the node from the back of the list.
static _inline list_node_t *list_popback(list_t *list) {
    assert(!list_empty(list));
    list_node_t *node = list->head.prev;
    list_remove(node);
    return node;
}

// Checks if the list is empty.
static _inline bool list_empty(list_t *list) {
    return list->head.next == &list->head;
}

// Searches for a node in the list.
static _inline bool list_search(list_t *list, list_node_t *node_to_find) {
    list_node_t *pos;
    list_for_each(pos, list) {
        if (pos == node_to_find) {
            return true;
        }
    }
    return false;
}

// Gets the length of the list (O(n) operation, use with caution).
static _inline u32 list_len(list_t *list) {
    u32 len = 0;
    list_node_t *pos;
    list_for_each(pos, list) {
        len++;
    }
    return len;
}


static _inline void list_insert_sort(list_t *list, list_node_t *node, int offset) {
    list_node_t *anchor = &list->head;
    int key = list_node_key(node, offset);

    for (list_node_t *ptr = list->head.next; ptr != &list->head; ptr = ptr->next) {
        int compare = list_node_key(ptr, offset);
        if (compare > key) {    
            anchor = ptr;
            break;
        }
    }

    assert(node->next == NULL);
    assert(node->prev == NULL);

    // insert before anchor
    list_insert_before(anchor, node);
}

#endif /* XJOS_LIST_H */