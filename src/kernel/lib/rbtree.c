#include <xjos/rbtree.h>

/*
 * Red-Black Tree Properties (Rules):
 *
 * 1. Color Rule: Every node is either RED or BLACK.
 * (Insertion Practice: New nodes are typically colored RED initially).
 *
 * 2. Root Rule: The root node MUST be BLACK.
 *
 * 3. Leaf Rule: All NULL leaves (NIL nodes) are considered BLACK.
 *
 * 4. Red Rule: No two consecutive RED nodes are allowed on any simple path
 * from the root to a leaf. (Simply put: A RED node cannot have a RED child).
 *
 * 5. Black-Height Rule: Every simple path from a given node to any of its
 * descendant NULL leaves contains the same number of BLACK nodes.
 */

// ===================================
//      Internal Auxilary Function(rotation)
// ===================================
/**
 * @brief left rotation
 * @param node current root, which will move down.
 * @param root rb_root
 */
static void __rb_rotate_left(struct rb_node *node, struct rb_root *root) {
    struct rb_node *right = node->rb_right;
    struct rb_node *parent = rb_parent(node);

    if ((node->rb_right = right->rb_left)) {    // insert node exist brother
        rb_set_parent(right->rb_left, node);
    }
    right->rb_left = node;
    rb_set_parent(right, parent);

    if (parent) {                               // gparent root_node exist
        if (node == parent->rb_left)
            parent->rb_left = right;
        else
            parent->rb_right = right;
    } else {
        // G is root, now p is new_root
        root->rb_node = right;
    }

    // update G.parent -> P
    rb_set_parent(node, right);
}


/**
 * @brief right rotation
 * @param node current root, which will move down.
 * @param root rb_root
 */
static void __rb_rotate_right(struct rb_node *node, struct rb_root *root) {
    struct rb_node *left = node->rb_left;
    struct rb_node *parent = rb_parent(node);

    if ((node->rb_left = left->rb_right)) {
        rb_set_parent(left->rb_right, node);
    }
    left->rb_right = node; // G -> P.right
    rb_set_parent(left, parent); // left.parent -> gparent.parent

    if (parent) {
        if (node == parent->rb_right) // G is right child
            parent->rb_right = left;
        else // G is left child
            parent->rb_left = left;
    } else {
        // G is root, now p is new_root
        root->rb_node = left;
    }

    // update G.parent -> P
    rb_set_parent(node, left);
}


/**
 * @brief (Helper) Fix the tree after deleting a BLACK node.
 * @param node The node that replaced the deleted black node (can be NULL, represents "double black")
 * @param parent The parent of 'node'
 * @param root The root of the tree
 */
static void __rb_erase_color(struct rb_node *node, struct rb_node *parent, struct rb_root *root) {
    struct rb_node *sibling = NULL;

    // Loop while 'node' has "extra black" and is not the root
    while ((!node || rb_is_black(node)) && node != root->rb_node) {
        // 'parent' must exist because 'node' is not root
        
        // --- case A: node (with extra black) is a left child ---
        if (node == parent->rb_left) {
            sibling = parent->rb_right;

            // case 1: sibling is RED
            // Action: Recolor parent/sibling, rotate parent-left.
            // This transforms into a Case 2, 3, or 4.
            if (sibling && rb_is_red(sibling)) {
                /*
                 * P(b)                S(b)
                 * /   \               /   \
                 * N(db)   S(r)   =>     P(r)  S_R(b)
                 * /   \         /   \
                 * S_L(b) S_R(b)  N(db) S_L(b)
                 */
                rb_set_black(sibling);
                rb_set_red(parent);
                __rb_rotate_left(parent, root);
                sibling = parent->rb_right; // Update sibling (must be black now)
            }

            // --- sibling is now BLACK ---

            // case 2: sibling is black, and both its children are black
            // Action: Recolor sibling to red, move "extra black" up to parent.
            if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
                (!sibling->rb_right || rb_is_black(sibling->rb_right))) {
                /*
                 * P(color)            P(db) <-- extra black moves up
                 * /      \            /      \
                 * N(db)     S(b)   =>  N(b)      S(r)
                 * /   \              /   \
                 * S_L(b) S_R(b)      S_L(b) S_R(b)
                 */
                rb_set_red(sibling);
                node = parent;            // 'node' becomes the new problem
                parent = rb_parent(node); // Update parent
                continue;                 // Continue loop from new 'node'
            }
            
            // --- sibling is black, at least one child is RED ---
            
            // case 3: sibling black, near child (left) is RED, far child (right) is BLACK
            // Action: Recolor sibling/near-child, rotate sibling-right.
            // This transforms into a Case 4 (straight line).
            if (!sibling->rb_right || rb_is_black(sibling->rb_right)) {
                /*
                 * P(color)          P(color)
                 * /      \          /      \
                 * N(db)     S(b)  =>  N(db)   S_L(b)
                 * /   \              \
                 * S_L(r) S_R(b)          S(r)
                 * \
                 * S_R(b)
                 */
                rb_set_black(sibling->rb_left); // S_L(r) must be red
                rb_set_red(sibling);
                __rb_rotate_right(sibling, root);
                sibling = parent->rb_right; // Update sibling
            }

            // case 4: sibling black, far child (right) is RED
            // (Also handles fall-through from Case 3)
            // Action: Recolor all, rotate parent-left. Fixup is complete.
            /*
             * P(color)             S(color)
             * /      \             /      \
             * N(db)     S(b)    =>  P(b)      S_R(b)
             * /   \         /   \
             * S_L(b) S_R(r)  N(b) S_L(b)
             */
            rb_set_color(sibling, rb_color(parent));
            rb_set_black(parent);
            if (sibling->rb_right) // S_R(r) must be red
                rb_set_black(sibling->rb_right);
            __rb_rotate_left(parent, root);
            
            node = root->rb_node; // Fixup complete, set node to root
            break;                // Stop the loop
        }
        
        // --- case B: node (with extra black) is a right child (Symmetric to A) ---
        else {
            sibling = parent->rb_left;

            // case 1: sibling is RED (Symmetric)
            if (sibling && rb_is_red(sibling)) {
                /*
                 * P(b)                S(b)
                 * /   \               /   \
                 * S(r)   N(db)   =>   S_L(b)  P(r)
                 * /   \                   /   \
                 *S_L(b) S_R(b)             S_R(b) N(db)
                 */
                rb_set_black(sibling);
                rb_set_red(parent);
                __rb_rotate_right(parent, root);
                sibling = parent->rb_left; // Update sibling (must be black now)
            }

            // --- sibling is now BLACK ---

            // case 2: sibling is black, and both its children are black (Symmetric)
            // Action: Recolor sibling to red, move "extra black" up to parent.
            if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
                (!sibling->rb_right || rb_is_black(sibling->rb_right))) {

                /*
                 * P(color)            P(db) <-- extra black moves up
                 * /      \            /      \
                 * S(b)     N(db) =>  S(r)      N(b)
                 * /   \              /   \
                 *S_L(b) S_R(b)      S_L(b) S_R(b)
                 */
                rb_set_red(sibling);
                node = parent;            // 'node' becomes the new problem
                parent = rb_parent(node); // Update parent
                continue;                 // Continue loop from new 'node'
            }
            
            // --- sibling is black, at least one child is RED ---

            // case 3: sibling black, near child (right) is RED, far child (left) is BLACK (Symmetric)
            // Action: Recolor sibling/near-child, rotate sibling-left.
            // This transforms into a Case 4 (straight line).
            if (!sibling->rb_left || rb_is_black(sibling->rb_left)) {
                /*
                 * P(color)          P(color)
                 * /      \          /      \
                 * S(b)     N(db) =>  S_R(b)   N(db)
                 * /   \              /
                 *S_L(b) S_R(r)      S(r)
                 * /
                 * S_L(b)
                 */
                rb_set_black(sibling->rb_right); // S_R(r) must be red
                rb_set_red(sibling);
                __rb_rotate_left(sibling, root);
                sibling = parent->rb_left; // Update sibling
            }

            // case 4: sibling black, far child (left) is RED (Symmetric)
            // (Also handles fall-through from Case 3)
            // Action: Recolor all, rotate parent-right. Fixup is complete.
            /*
             * P(color)           S(color)
             * /      \           /      \
             * S(b)     N(db) =>  S_L(b)    P(b)
             * /   \                 /   \
             *S_L(r) S_R(b)           S_R(b) N(b)
             */
            rb_set_color(sibling, rb_color(parent));
            rb_set_black(parent);
            if (sibling->rb_left) // S_L(r) must be red
                rb_set_black(sibling->rb_left);
            __rb_rotate_right(parent, root);
            
            node = root->rb_node; // Fixup complete, set node to root
            break;                // Stop the loop
        }
    }

    // Loop terminates if:
    // 1. We fixed the tree (Case 4, node = root)
    // 2. Extra black reached the root (node == root)
    // 3. Extra black reached a RED node (loop condition failed)
    
    // Absorb the "extra black" by setting the final 'node' to black.
    // This satisfies Property 2 (root is black) or absorbs the extra black
    // into the red node (making it black).
    if (node)
        rb_set_black(node);
}


// ===================================
//      API Function Implementations
// ===================================


void rb_insert_color(struct rb_node *node, struct rb_root *root) {
    struct rb_node *parent, *gparent;

    // parent exists and is red (red - red conflict)
    while ((parent = rb_parent(node)) && rb_is_red(parent)) {
        gparent = rb_parent(parent);

        // case A: parent is a left child of its grandparent
        if (parent == gparent->rb_left) {
            struct rb_node *uncle = gparent->rb_right;

            // case 1: uncle is red
            if (uncle && rb_is_red(uncle)) {
                /*
                        G(b)          G(r)
                        / \           / \
                    p(r)  u(r) =>  p(b)  u(b)
                    /                \
                    C(r)            C(r)
                */
                rb_set_black(parent);
                rb_set_black(uncle);
                rb_set_red(gparent);
                node = gparent;     // up^
                continue;
            }

            // case 2: uncle is black(or null) and node is a right child(LR)
            if (node == parent->rb_right) {
                /*
                        G(b)          G(b)
                        /   \         /   \
                    p(r)  u(b) =>  c(r)  u(b)
                        \           /
                        C(r)      p(r)
                */
                __rb_rotate_left(parent, root);
                // swap node and parent
                struct rb_node *tmp = parent;
                parent = node;
                node = tmp;
            } 

            // case 3: uncle is black(or null) and node is a left child(LL)

            /*
                    G(b)          p(b)  
                   /   \          /   \
                p(r)  u(b) =>  C(r)  G(r)
                /                       \
            C(r)                        u(b)
            */
            rb_set_black(parent);
            rb_set_red(gparent);
            __rb_rotate_right(gparent, root);
            break;  // stop the loop
        } else {        // case B: parent is a right child of its grandparent
            struct rb_node *uncle = gparent->rb_left;

            // case 1: uncle is red (Symmetric)
            if (uncle && rb_is_red(uncle)) {
                /*
                                G(b)          G(r)
                               / \           / \
                            u(r)  p(r) =>  u(b)  p(b)
                                   \             \
                                    C(r)          C(r)
                */
                rb_set_black(parent);
                rb_set_black(uncle);
                rb_set_red(gparent);
                node = gparent;     // up^
                continue;
            }

            // case 2: uncle is black(or null) and node is a left child(RL) (Symmetric)
            if (node == parent->rb_left) {
                /*
                                G(b)          G(b)
                               /   \         /   \
                            u(b)  p(r) =>  u(b)  C(r)
                                 /               \
                                C(r)              p(r)
                */
                __rb_rotate_right(parent, root);
                // swap node and parent
                struct rb_node *tmp = parent;
                parent = node;
                node = tmp;
            }

            // case 3: uncle is black(or null) and node is a right child(RR) (Symmetric)
            /*
                    G(b)          p(b)  
                   /   \          /   \
                u(b)  p(r) =>  G(r)  C(r)
                         \      /
                          C(r) u(b)
            */
            rb_set_black(parent);
            rb_set_red(gparent);
            __rb_rotate_left(gparent, root);
            break; // stop the loop
        }
    }

    // case B: root must be black
    rb_set_black(root->rb_node);
}


void rb_erase(struct rb_node *node, struct rb_root *root) {
    struct rb_node *child = NULL, *parent = NULL;
    int color;

    // 1. find delete node(reap)
    // if reap have 0 or 1 child, reap is the node to be deleted
    // if reap have 2 child, reap = node next
    struct rb_node *reap = node;
    
    if (node->rb_left && node->rb_right) {
        // case: node have 2 child
        reap = rb_next(node);
    }

    // 2. replace reap with its child(or null)
    if (reap->rb_left)
        child = reap->rb_left;
    else
        child = reap->rb_right;

    // 3. save reap's color and parent
    parent = rb_parent(reap);
    color = rb_color(reap);

    // 4. remove reap
    rb_replace_node(reap, child, root); 

    // 5. handle reap's child(two)
    if (reap != node) {
        //5a. reap Inheritance node
        reap->rb_parent_color = node->rb_parent_color;
        //5b. Inheritance node's child
        reap->rb_left = node->rb_left;
        reap->rb_right = node->rb_right;

        // 5c. set child's parent
        if (reap->rb_left)
            rb_set_parent(reap->rb_left, reap);
        if (reap->rb_right)
            rb_set_parent(reap->rb_right, reap);

        // 5d. handler node parent
        if (rb_parent(node)) {
            if (node == rb_parent(node)->rb_left) {
                rb_parent(node)->rb_left = reap;
            } else {
                rb_parent(node)->rb_right = reap;
            }
        } else {
            root->rb_node = reap;
        }

        // 5e. fix parent pointer
        if (parent == node)
            parent = reap;
    }

    // 6. fix color
    if (color == RB_BLACK) {
        __rb_erase_color(child, parent, root);
    }
}


struct rb_node *rb_first(const struct rb_root *root) {
    struct rb_node *node = root->rb_node;

    if (!node)
        return NULL;
    
    while (node->rb_left)
        node = node->rb_left;
    
    return node;    // return the smallest node
}


struct rb_node *rb_last(const struct rb_root *root) {
    struct rb_node *node = root->rb_node;

    if (!node)
        return NULL;
    
    while (node->rb_right)
        node = node->rb_right;
    
    return node;    // return the largest node
}


struct rb_node *rb_next(const struct rb_node *node) {
    struct rb_node *parent;

    // case 1: exist right child
    if (node->rb_right) {
        node = node->rb_right;
        while (node->rb_left)
            node = node->rb_left;
        
        return (struct rb_node *)node;
    }

    // case 2: no right child, go up to the parent
    parent = rb_parent(node);
    while (parent && node == parent->rb_right) {
        node = parent;
        parent = rb_parent(node);
    }

    return parent;    // return the parent node
}


struct rb_node *rb_prev(const struct rb_node *node) {
    struct rb_node *parent;

    // case 1: exist left child
    if (node->rb_left) {
        node = node->rb_left;
        while (node->rb_right)
            node = node->rb_right;
        
        return (struct rb_node *)node;
    }

    // case 2: no left child, go up to the parent
    parent = rb_parent(node);
    while (parent && node == parent->rb_left) {
        node = parent;
        parent = rb_parent(node);
    }

    return parent;    // return the parent node
}


void rb_replace_node(struct rb_node *victim, struct rb_node *new_node,
    struct rb_root *root) {
    struct rb_node *parent = rb_parent(victim);

    if (parent) {
        // case a: victim is not root
        if (victim == parent->rb_left)
            parent->rb_left = new_node;
        else
            parent->rb_right = new_node;
    } else {
        // case b: victim is root
        root->rb_node = new_node;
    }

    // new_node is not null, update its parent
    if (new_node) {
        rb_set_parent(new_node, parent);
    }
}