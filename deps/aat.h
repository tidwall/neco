// https://github.com/tidwall/aatree
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// Single header file for generating aat binary search trees.

#ifndef AAT_H
#define AAT_H

#define AAT_DEF(specifiers, prefix, type)                                      \
specifiers type *prefix##_insert(type **root, type *item);                     \
specifiers type *prefix##_delete(type **root, type *key);                      \
specifiers type *prefix##_search(type **root, type *key);                      \
specifiers type *prefix##_delete_first(type **root);                           \
specifiers type *prefix##_delete_last(type **root);                            \
specifiers type *prefix##_first(type **root);                                  \
specifiers type *prefix##_last(type **root);                                   \
specifiers type *prefix##_iter(type **root, type *key);                        \
specifiers type *prefix##_prev(type **root, type *item);                       \
specifiers type *prefix##_next(type **root, type *item);                       \

#define AAT_FIELDS(type, left, right, level)                                   \
type *left;                                                                    \
type *right;                                                                   \
int level;                                                                     \

#define AAT_IMPL(prefix, type, left, right, level, compare)                    \
static void prefix##_clear(type *node) {                                       \
    if (node) {                                                                \
        node->left = 0;                                                        \
        node->right = 0;                                                       \
        node->level = 0;                                                       \
    }                                                                          \
}                                                                              \
                                                                               \
static type *prefix##_skew(type *node) {                                       \
    if (node && node->left &&                                                  \
        node->left->level == node->level)                                      \
    {                                                                          \
        type *left_node = node->left;                                          \
        node->left = left_node->right;                                         \
        left_node->right = node;                                               \
        node = left_node;                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_split(type *node) {                                      \
    if (node && node->right && node->right->right &&                           \
        node->right->right->level == node->level)                              \
    {                                                                          \
        type *right_node = node->right;                                        \
        node->right = right_node->left;                                        \
        right_node->left = node;                                               \
        right_node->level++;                                                   \
        node = right_node;                                                     \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_insert0(type *node, type *item, type **replaced) {       \
    if (!node) {                                                               \
        item->left = 0;                                                        \
        item->right = 0;                                                       \
        item->level = 1;                                                       \
        node = item;                                                           \
    } else {                                                                   \
        int cmp = compare(item, node);                                         \
        if (cmp < 0) {                                                         \
            node->left = prefix##_insert0(node->left, item, replaced);         \
        } else if (cmp > 0) {                                                  \
            node->right = prefix##_insert0(node->right, item, replaced);       \
        } else {                                                               \
            *replaced = node;                                                  \
            item->left = node->left;                                           \
            item->right = node->right;                                         \
            item->level = node->level;                                         \
            node = item;                                                       \
        }                                                                      \
    }                                                                          \
    node = prefix##_skew(node);                                                \
    node = prefix##_split(node);                                               \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_insert(type **root, type *item) {                               \
    type *replaced = 0;                                                        \
    *root = prefix##_insert0(*root, item, &replaced);                          \
    if (replaced != item) {                                                    \
        prefix##_clear(replaced);                                              \
    }                                                                          \
    return replaced;                                                           \
}                                                                              \
                                                                               \
static type *prefix##_decrease_level(type *node) {                             \
    if (node->left || node->right) {                                           \
        int new_level = 0;                                                     \
        if (node->left && node->right) {                                       \
            if (node->left->level < node->right->level) {                      \
                new_level = node->left->level;                                 \
            } else {                                                           \
                new_level = node->right->level;                                \
            }                                                                  \
        }                                                                      \
        new_level++;                                                           \
        if (new_level < node->level) {                                         \
            node->level = new_level;                                           \
            if (node->right && new_level < node->right->level) {               \
                node->right->level = new_level;                                \
            }                                                                  \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_delete_fixup(type *node) {                               \
    node = prefix##_decrease_level(node);                                      \
    node = prefix##_skew(node);                                                \
    node->right = prefix##_skew(node->right);                                  \
    if (node->right && node->right->right) {                                   \
        node->right->right = prefix##_skew(node->right->right);                \
    }                                                                          \
    node = prefix##_split(node);                                               \
    node->right = prefix##_split(node->right);                                 \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_delete_first0(type *node,                                \
    type **deleted)                                                            \
{                                                                              \
    if (node) {                                                                \
        if (!node->left) {                                                     \
            *deleted = node;                                                   \
            if (node->right) {                                                 \
                node = node->right;                                            \
            } else {                                                           \
                node = 0;                                                      \
            }                                                                  \
        } else {                                                               \
            node->left = prefix##_delete_first0(node->left, deleted);          \
            node = prefix##_delete_fixup(node);                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_delete_last0(type *node,                                 \
    type **deleted)                                                            \
{                                                                              \
    if (node) {                                                                \
        if (!node->right) {                                                    \
            *deleted = node;                                                   \
            if (node->left) {                                                  \
                node = node->left;                                             \
            } else {                                                           \
                node = 0;                                                      \
            }                                                                  \
        } else {                                                               \
            node->right = prefix##_delete_last0(node->right, deleted);         \
            node = prefix##_delete_fixup(node);                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_delete_first(type **root) {                                     \
    type *deleted = 0;                                                         \
    *root = prefix##_delete_first0(*root, &deleted);                           \
    prefix##_clear(deleted);                                                   \
    return deleted;                                                            \
}                                                                              \
                                                                               \
type *prefix##_delete_last(type **root) {                                      \
    type *deleted = 0;                                                         \
    *root = prefix##_delete_last0(*root, &deleted);                            \
    prefix##_clear(deleted);                                                   \
    return deleted;                                                            \
}                                                                              \
                                                                               \
static type *prefix##_delete0(type *node,                                      \
    type *key, type **deleted)                                                 \
{                                                                              \
    if (node) {                                                                \
        int cmp = compare(key, node);                                          \
        if (cmp < 0) {                                                         \
            node->left = prefix##_delete0(node->left, key, deleted);           \
        } else if (cmp > 0) {                                                  \
            node->right = prefix##_delete0(node->right, key, deleted);         \
        } else {                                                               \
            *deleted = node;                                                   \
            if (!node->left && !node->right) {                                 \
                node = 0;                                                      \
            } else {                                                           \
                type *leaf_deleted = 0;                                        \
                if (!node->left) {                                             \
                    node->right = prefix##_delete_first0(node->right,          \
                        &leaf_deleted);                                        \
                } else {                                                       \
                    node->left = prefix##_delete_last0(node->left,             \
                        &leaf_deleted);                                        \
                }                                                              \
                leaf_deleted->left = node->left;                               \
                leaf_deleted->right = node->right;                             \
                leaf_deleted->level = node->level;                             \
                node = leaf_deleted;                                           \
            }                                                                  \
        }                                                                      \
        if (node) {                                                            \
            node = prefix##_delete_fixup(node);                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_delete(type **root, type *key) {                                \
    type *deleted = 0;                                                         \
    *root = prefix##_delete0(*root, key, &deleted);                            \
    prefix##_clear(deleted);                                                   \
    return deleted;                                                            \
}                                                                              \
                                                                               \
type *prefix##_search(type **root, type *key) {                                \
    type *found = 0;                                                           \
    type *node = *root;                                                        \
    while (node) {                                                             \
        int cmp = compare(key, node);                                          \
        if (cmp < 0) {                                                         \
            node = node->left;                                                 \
        } else if (cmp > 0) {                                                  \
            node = node->right;                                                \
        } else {                                                               \
            found = node;                                                      \
            node = 0;                                                          \
        }                                                                      \
    }                                                                          \
    return found;                                                              \
}                                                                              \
                                                                               \
type *prefix##_first(type **root) {                                            \
    type *node = *root;                                                        \
    if (node) {                                                                \
        while (node->left) {                                                   \
            node = node->left;                                                 \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_last(type **root) {                                             \
    type *node = *root;                                                        \
    if (node) {                                                                \
        while (node->right) {                                                  \
            node = node->right;                                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_iter(type **root, type *key) {                                  \
    type *found = 0;                                                           \
    type *node = *root;                                                        \
    while (node) {                                                             \
        int cmp = compare(key, node);                                          \
        if (cmp < 0) {                                                         \
            found = node;                                                      \
            node = node->left;                                                 \
        } else if (cmp > 0) {                                                  \
            node = node->right;                                                \
        } else {                                                               \
            found = node;                                                      \
            node = 0;                                                          \
        }                                                                      \
    }                                                                          \
    return found;                                                              \
}                                                                              \
                                                                               \
static type *prefix##_parent(type **root,                                      \
    type *item)                                                                \
{                                                                              \
    type *parent = 0;                                                          \
    type *node = *root;                                                        \
    while (node) {                                                             \
        int cmp = compare(item, node);                                         \
        if (cmp < 0) {                                                         \
            parent = node;                                                     \
            node = node->left;                                                 \
        } else if (cmp > 0) {                                                  \
            parent = node;                                                     \
            node = node->right;                                                \
        } else {                                                               \
            node = 0;                                                          \
        }                                                                      \
    }                                                                          \
    return parent;                                                             \
}                                                                              \
                                                                               \
type *prefix##_next(type **root, type *node) {                                 \
    if (node) {                                                                \
        if (node->right) {                                                     \
            node = node->right;                                                \
            while (node->left) {                                               \
                node = node->left;                                             \
            }                                                                  \
        } else {                                                               \
            type *parent = prefix##_parent(root, node);                        \
            while (parent && parent->left != node) {                           \
                node = parent;                                                 \
                parent = prefix##_parent(root, parent);                        \
            }                                                                  \
            node = parent;                                                     \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_prev(type **root, type *node) {                                 \
    if (node) {                                                                \
        if (node->left) {                                                      \
            node = node->left;                                                 \
            while (node->right) {                                              \
                node = node->right;                                            \
            }                                                                  \
        } else {                                                               \
            type *parent = prefix##_parent(root, node);                        \
            while (parent && parent->right != node) {                          \
                node = parent;                                                 \
                parent = prefix##_parent(root, parent);                        \
            }                                                                  \
            node = parent;                                                     \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \

#endif // AAT_H
