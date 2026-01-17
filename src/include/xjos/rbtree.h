#ifndef XJOS_RB_TREE_H
#define XJOS_RB_TREE_H


#include <xjos/stddef.h>

#define container_of(ptr, type, member) \
    element_entry(type, member, ptr)

#define RB_RED 0
#define RB_BLACK 1

// ===================================
//      Core Data Structures
// ===================================

struct rb_node {
    u32 rb_parent_color;    // 30bits for parent, 2bits for color
    struct rb_node *rb_left;
    struct rb_node *rb_right;
}__attribute__((aligned(sizeof(long))));

// root
typedef struct rb_root {
    struct rb_node *rb_node;
}rb_root_t;

// ===================================
//      RbTree-Specific Macros
// ===================================

// rbt auxilary marcro
#define RB_ROOT (struct rb_root) { .rb_node = NULL }

#define rb_entry(ptr, type, member) \
    element_entry(type, member, ptr)

// rb_parent_color ~ 3 -> parent
#define rb_parent(r) ((struct rb_node *)((r)->rb_parent_color & ~3UL))
// get color
#define rb_color(r) ((r)->rb_parent_color & 1UL)
// check color
#define rb_is_red(r) (!rb_color(r))
#define rb_is_black(r) (rb_color(r))
// set color
#define rb_set_red(r) do { (r)->rb_parent_color &= ~1UL; } while(0)
#define rb_set_black(r) do { (r)->rb_parent_color |= 1UL; } while(0)

/**
 * @brief Set parent of the node
 * @param rb node
 * @param p parent node pointer
 */
static _inline void rb_set_parent(struct rb_node *rb, struct rb_node *p) {
    // &3UL remain parent color
    rb->rb_parent_color = (rb->rb_parent_color & 3UL) | (u32)p;
}


/**
 * @brief Set color of the node
 * @param rb node
 * @param color 0 for red, 1 for black
 */
static _inline void rb_set_color(struct rb_node *rb, int color) {
    rb->rb_parent_color = (rb->rb_parent_color & ~1UL) | color;
}


// ===================================
//      API Function
// ===================================

/**
 * @brief Insert a new node into the red-black tree, set color
 * @param node new node to be inserted
 * @param root root of the red-black tree
 */
void rb_insert_color(struct rb_node *node, struct rb_root *root);

/**
 * @brief Delete a node from the red-black tree
 * @param node node to be deleted
 * @param root root of the red-black tree
 */
void rb_erase(struct rb_node *node, struct rb_root *root);

/**
 * @brief Find the node with the (Left-most)minimum key in the red-black tree
 * @param root root of the red-black tree
 * @return the node with the minimum key, or NULL if the tree is empty
 */
struct rb_node *rb_first(const struct rb_root *root);

/**
 * @brief Find the node with the (Right-most)maximum key in the red-black tree
 * @param root root of the red-black tree
 * @return the node with the maximum key, or NULL if the tree is empty
 */
struct rb_node *rb_last(const struct rb_root *root);

/**
 * @brief Find the successor node of the specified node (in-order traversal)
 * @param node current node
 * @return pointer to the successor node, or NULL if there is no successor node
 */
struct rb_node *rb_next(const struct rb_node *node);

/**
 * @brief Find the predecessor node of the specified node (in-order traversal)
 * @param node current node
 * @return pointer to the predecessor node, or NULL if there is no predecessor node
 */
struct rb_node *rb_prev(const struct rb_node *node);

/**
 * @brief Replace the victim node in the tree with new_node
 * @note This function only replaces the link relationship and is not responsible for restoring the color
 */
void rb_replace_node(struct rb_node *victim, struct rb_node *new_node,
    struct rb_root *root);

#endif /* XJOS_RB_TREE_H */