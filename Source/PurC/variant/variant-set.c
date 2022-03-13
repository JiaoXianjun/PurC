/**
 * @file variant-set.c
 * @author Xu Xiaohong (freemine)
 * @date 2021/07/09
 * @brief The API for variant.
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of PurC (short for Purring Cat), an HVML interpreter.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE       // qsort_r

#include "config.h"
#include "private/variant.h"
#include "private/list.h"
#include "private/hashtable.h"
#include "private/errors.h"
#include "private/stringbuilder.h"
#include "purc-errors.h"
#include "variant-internals.h"


#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static bool
grow(purc_variant_t set, purc_variant_t value,
        bool check)
{
    if (!check)
        return true;

    purc_variant_t vals[] = { value };

    return pcvariant_on_pre_fired(set, PCVAR_OPERATION_GROW,
            PCA_TABLESIZE(vals), vals);
}

static bool
shrink(purc_variant_t set, purc_variant_t value,
        bool check)
{
    if (!check)
        return true;

    purc_variant_t vals[] = { value };

    return pcvariant_on_pre_fired(set, PCVAR_OPERATION_SHRINK,
            PCA_TABLESIZE(vals), vals);
}

static bool
change(purc_variant_t set,
        purc_variant_t o, purc_variant_t n,
        bool check)
{
    if (!check)
        return true;

    purc_variant_t vals[] = { o, n };

    return pcvariant_on_pre_fired(set, PCVAR_OPERATION_CHANGE,
            PCA_TABLESIZE(vals), vals);
}

static void
grown(purc_variant_t set, purc_variant_t value,
        bool check)
{
    if (!check)
        return;

    purc_variant_t vals[] = { value };

    pcvariant_on_post_fired(set, PCVAR_OPERATION_GROW,
            PCA_TABLESIZE(vals), vals);
}

static void
shrunk(purc_variant_t set, purc_variant_t value,
        bool check)
{
    if (!check)
        return;

    purc_variant_t vals[] = { value };

    pcvariant_on_post_fired(set, PCVAR_OPERATION_SHRINK,
            PCA_TABLESIZE(vals), vals);
}

static void
changed(purc_variant_t set,
        purc_variant_t o, purc_variant_t n,
        bool check)
{
    if (!check)
        return;

    purc_variant_t vals[] = { o, n };

    pcvariant_on_post_fired(set, PCVAR_OPERATION_CHANGE,
            PCA_TABLESIZE(vals), vals);
}

static variant_set_t
pcv_set_get_data(purc_variant_t set)
{
    return (variant_set_t)set->sz_ptr[1];
}

static size_t
variant_set_get_extra_size(variant_set_t set)
{
    size_t extra = 0;
    if (set->unique_key) {
        extra += strlen(set->unique_key) + 1;
        extra += sizeof(*set->keynames) * set->nr_keynames;
    }
    size_t sz_record = sizeof(struct set_node) +
        sizeof(purc_variant_t) * set->nr_keynames;
    size_t count = pcutils_arrlist_length(set->arr);
    extra += sz_record * count;
    extra += sizeof(*set->arr);
    extra += sizeof(struct set_node*)*(set->arr->size);

    return extra;
}

static void
pcv_set_set_data(purc_variant_t set, variant_set_t data)
{
    set->sz_ptr[1]     = (uintptr_t)data;
}

static int
variant_set_init(variant_set_t set, const char *unique_key)
{
    set->elems = RB_ROOT;

    size_t initial_size = ARRAY_LIST_DEFAULT_SIZE;
    set->arr = pcutils_arrlist_new_ex(NULL, initial_size);
    if (!set->arr) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    if (!unique_key || !*unique_key) {
        // empty key
        set->nr_keynames = 1;
        return 0;
    }

    set->unique_key = strdup(unique_key);
    if (!set->unique_key) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    size_t n = strlen(set->unique_key);
    set->keynames = (const char**)calloc(n, sizeof(*set->keynames));
    if (!set->keynames) {
        free(set->unique_key);
        set->unique_key = NULL;
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    strcpy(set->unique_key, unique_key);
    char *ctx = set->unique_key;
    char *tok = strtok_r(ctx, " ", &ctx);
    size_t idx = 0;
    while (tok) {
        set->keynames[idx++] = tok;
        tok = strtok_r(ctx, " ", &ctx);
    }

    if (idx==0) {
        // no content in key
        free(set->unique_key);
        set->unique_key = NULL;
        set->nr_keynames = 1;
        return 0;
    }

    PC_ASSERT(idx>0);
    set->nr_keynames = idx;

    return 0;
}

static purc_variant_t
variant_set_kvs_from_val(variant_set_t set,
    purc_variant_t value)
{
    PC_ASSERT(value != PURC_VARIANT_INVALID);
    PC_ASSERT(purc_variant_is_object(value));
    PC_ASSERT(set->nr_keynames);

    if (set->unique_key) {
        purc_variant_t kvs;
        kvs = purc_variant_make_object(0,
                PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
        if (kvs == PURC_VARIANT_INVALID)
            return PURC_VARIANT_INVALID;

        for (size_t i=0; i<set->nr_keynames; ++i) {
            const char *sk = set->keynames[i];
            purc_variant_t v;
            v = purc_variant_object_get_by_ckey(value, sk, false);
            if (v == PURC_VARIANT_INVALID)
                continue;
            PC_ASSERT(purc_variant_is_undefined(v) == false);
            bool ok;
            ok = purc_variant_object_set_by_static_ckey(kvs, sk, v);
            if (!ok) {
                purc_variant_unref(kvs);
                return PURC_VARIANT_INVALID;
            }
        }

        return kvs;
    } else {
        PC_ASSERT(set->nr_keynames==1);
        purc_variant_ref(value);

        return value;
    }
}

static purc_variant_t
pcv_set_new(void)
{
    purc_variant_t set = pcvariant_get(PVT(_SET));
    if (!set) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return PURC_VARIANT_INVALID;
    }

    set->type          = PVT(_SET);
    set->flags         = PCVARIANT_FLAG_EXTRA_SIZE;

    variant_set_t ptr  = (variant_set_t)calloc(1, sizeof(*ptr));
    pcv_set_set_data(set, ptr);

    if (!ptr) {
        pcvariant_put(set);
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return PURC_VARIANT_INVALID;
    }

    INIT_LIST_HEAD(&ptr->rev_update_chain);
    set->refc          = 1;

    // a valid empty set
    return set;
}

static void
elem_node_revoke_constraints(struct set_node *elem)
{
    bool ok;

    if (elem->constraints) {
        PC_ASSERT(elem->elem);
        ok = purc_variant_revoke_listener(elem->elem, elem->constraints);
        PC_ASSERT(ok);
        elem->constraints = NULL;
    }

    elem->set = PURC_VARIANT_INVALID;
}

static bool
variant_set_constraint_grow_handler(
        purc_variant_t source,  // the source variant.
        void *ctxt,             // the context stored when registering the handler.
        size_t nr_args,         // the number of the relevant child variants.
        purc_variant_t *argv    // the array of all relevant child variants.
        )
{
    PC_ASSERT(source);
    PC_ASSERT(purc_variant_is_object(source));
    purc_variant_t set = (purc_variant_t)ctxt;
    PC_ASSERT(set);
    PC_ASSERT(purc_variant_is_set(set));
    PC_ASSERT(nr_args == 2);
    purc_variant_t k = (purc_variant_t)argv[0];
    PC_ASSERT(k);
    PC_ASSERT(purc_variant_is_string(k));
    purc_variant_t v = (purc_variant_t)argv[1];
    PC_ASSERT(v);

    return true;
}

static bool
variant_set_constraint_shrink_handler(
        purc_variant_t source,  // the source variant.
        void *ctxt,             // the context stored when registering the handler.
        size_t nr_args,         // the number of the relevant child variants.
        purc_variant_t *argv    // the array of all relevant child variants.
        )
{
    PC_ASSERT(source);
    PC_ASSERT(purc_variant_is_object(source));
    purc_variant_t set = (purc_variant_t)ctxt;
    PC_ASSERT(set);
    PC_ASSERT(purc_variant_is_set(set));
    PC_ASSERT(nr_args == 2);
    purc_variant_t k = (purc_variant_t)argv[0];
    PC_ASSERT(k);
    PC_ASSERT(purc_variant_is_string(k));
    purc_variant_t v = (purc_variant_t)argv[1];
    PC_ASSERT(v);

    return true;
}

struct element_rb_node {
    struct rb_node     **pnode;
    struct rb_node      *parent;
    struct rb_node      *entry;
};

static void
find_element_rb_node(struct element_rb_node *node,
        variant_set_t set, purc_variant_t kvs)
{
    struct rb_node **pnode = &set->elems.rb_node;
    struct rb_node *parent = NULL;
    struct rb_node *entry = NULL;
    while (*pnode) {
        struct set_node *on;
        on = container_of(*pnode, struct set_node, node);
        int ret;
        ret = purc_variant_compare_ex(kvs, on->kvs,
                PCVARIANT_COMPARE_OPT_AUTO);

        parent = *pnode;

        if (ret < 0)
            pnode = &parent->rb_left;
        else if (ret > 0)
            pnode = &parent->rb_right;
        else{
            entry = *pnode;
            break;
        }
    }

    node->pnode  = pnode;
    node->parent = parent;
    node->entry  = entry;
}

static struct set_node*
find_element(variant_set_t set, purc_variant_t kvs)
{
    struct element_rb_node node;
    find_element_rb_node(&node, set, kvs);

    if (!node.entry)
        return NULL;

    return container_of(node.entry, struct set_node, node);
}

static bool
variant_set_constraint_change_handler(
        purc_variant_t source,  // the source variant.
        void *ctxt,             // the context stored when registering the handler.
        size_t nr_args,         // the number of the relevant child variants.
        purc_variant_t *argv    // the array of all relevant child variants.
        )
{
    PC_ASSERT(source);
    PC_ASSERT(purc_variant_is_object(source));
    purc_variant_t set = (purc_variant_t)ctxt;
    PC_ASSERT(set);
    PC_ASSERT(purc_variant_is_set(set));
    PC_ASSERT(nr_args == 4);
    purc_variant_t ko = (purc_variant_t)argv[0];
    PC_ASSERT(ko);
    PC_ASSERT(purc_variant_is_string(ko));
    purc_variant_t vo = (purc_variant_t)argv[1];
    PC_ASSERT(vo);
    purc_variant_t kn = (purc_variant_t)argv[2];
    PC_ASSERT(kn);
    PC_ASSERT(purc_variant_is_string(kn));
    purc_variant_t vn = (purc_variant_t)argv[3];
    PC_ASSERT(vn);
    PC_ASSERT(0 == pcvariant_equal(ko, kn));

    purc_variant_t tmp;
    tmp = purc_variant_container_clone(source);
    PC_ASSERT(tmp != PURC_VARIANT_INVALID);
    bool ok;
    ok = purc_variant_object_set(tmp, kn, vn);
    PC_ASSERT(ok);

    variant_set_t data = pcv_set_get_data(set);
    purc_variant_t kvs = variant_set_kvs_from_val(data, tmp);
    if (kvs == PURC_VARIANT_INVALID) {
        PURC_VARIANT_SAFE_CLEAR(tmp);
        // FIXME: better let caller know the root-cause
        return false;
    }

    struct set_node *p;
    p = find_element(data, kvs);
    PURC_VARIANT_SAFE_CLEAR(kvs);
    PURC_VARIANT_SAFE_CLEAR(tmp);
    if (p && p->elem != source)
        return false;

    return true;
}

static bool
variant_set_constraints_handler(
        purc_variant_t source,  // the source variant.
        pcvar_op_t op,          // the operation identifier.
        void *ctxt,             // the context stored when registering the handler.
        size_t nr_args,         // the number of the relevant child variants.
        purc_variant_t *argv    // the array of all relevant child variants.
        )
{
    switch (op) {
        case PCVAR_OPERATION_GROW:
            return variant_set_constraint_grow_handler(source, ctxt,
                    nr_args, argv);
        case PCVAR_OPERATION_SHRINK:
            return variant_set_constraint_shrink_handler(source, ctxt,
                    nr_args, argv);
        case PCVAR_OPERATION_CHANGE:
            return variant_set_constraint_change_handler(source, ctxt,
                    nr_args, argv);
        default:
            PC_ASSERT(0);
            return false;
    }
}

static bool
elem_node_setup_constraints(struct set_node *elem)
{
    PC_ASSERT(elem->set != PURC_VARIANT_INVALID);
    purc_variant_t set = elem->set;

    purc_variant_t child = elem->elem;
    PC_ASSERT(child != PURC_VARIANT_INVALID);
    PC_ASSERT(purc_variant_is_object(child));

    elem->constraints = purc_variant_register_pre_listener(elem->elem,
        PCVAR_OPERATION_ALL, variant_set_constraints_handler, set);

    if (!elem->constraints)
        return false;

    // variant_set_t data = pcv_set_get_data(set);
    // for (size_t i=0; data->keynames && i<data->nr_keynames; ++i) {
    //     const char *sk = data->keynames[i];
    //     purc_variant_t v;
    //     const bool silently = true;
    //     v = purc_variant_object_get_by_ckey(child, sk, silently);
    //     if (v == PURC_VARIANT_INVALID)
    //         continue;
    // }

    return true;
}

static void
elem_node_break_rev_update_edges(purc_variant_t set, struct set_node *elem)
{
    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);
    PC_ASSERT(elem);
    PC_ASSERT(elem->elem != PURC_VARIANT_INVALID);
    PC_ASSERT(purc_variant_is_object(elem->elem));

    variant_obj_t obj_data;
    obj_data = (variant_obj_t)elem->elem->sz_ptr[1];
    struct rb_root *root = &obj_data->kvs;
    struct rb_node *p = pcutils_rbtree_first(root);
    for (; p; p = pcutils_rbtree_next(p)) {
        struct obj_node *node;
        node = container_of(p, struct obj_node, node);
        const char *sk = purc_variant_get_string_const(node->key);
        if (data->keynames) {
            size_t i=0;
            for (i=0; i<data->nr_keynames; ++i) {
                const char *k = data->keynames[i];
                if (strcmp(sk, k) == 0)
                    break;
            }
            if (i>=data->nr_keynames)
                continue;
        }
        struct pcvar_rev_update_edge edge = {
            .parent         = elem->elem,
            .obj_me         = node,
        };
        pcvar_break_edge_to_parent(node->val, &edge);
    }
}

static void
elem_node_release(struct set_node *elem)
{
    if (elem->elem != PURC_VARIANT_INVALID) {
        if (elem->set != PURC_VARIANT_INVALID) {
            elem_node_break_rev_update_edges(elem->set, elem);
        }
        elem_node_revoke_constraints(elem);
        purc_variant_unref(elem->elem);
        elem->elem = PURC_VARIANT_INVALID;
    }
    PURC_VARIANT_SAFE_CLEAR(elem->kvs);
    elem->set = PURC_VARIANT_INVALID;
}

static int
elem_node_replace(struct set_node *node,
        purc_variant_t val, purc_variant_t kvs)
{
    PC_ASSERT(node->set != PURC_VARIANT_INVALID);
    PC_ASSERT(node->elem != PURC_VARIANT_INVALID);

    purc_variant_t set = node->set;

    purc_variant_ref(val);
    purc_variant_ref(kvs);

    elem_node_break_rev_update_edges(node->set, node);
    elem_node_revoke_constraints(node);
    PURC_VARIANT_SAFE_CLEAR(node->elem);
    PURC_VARIANT_SAFE_CLEAR(node->kvs);

    node->elem = val;
    node->kvs  = kvs;
    node->set  = set;

    if (!elem_node_setup_constraints(node))
        return -1;

    return 0;
}

static void
refresh_arr(struct pcutils_arrlist *arr, size_t idx)
{
    if (idx == (size_t)-1)
        return;

    size_t count = pcutils_arrlist_length(arr);
    for (; idx < count; ++idx) {
        struct set_node *p;
        p = (struct set_node*)pcutils_arrlist_get_idx(arr, idx);
        p->idx = idx;
    }
}

static void
variant_set_release_elems(variant_set_t set)
{
    struct rb_node *node, *next;
    for (node=pcutils_rbtree_first(&set->elems);
         ({next = node ? pcutils_rbtree_next(node) : NULL; node;});
         node = next)
    {
        struct set_node *p;
        p = container_of(node, struct set_node, node);
        pcutils_rbtree_erase(node, &set->elems);
        // NOTE: for the sake of performance
        // int r = pcutils_arrlist_del_idx(set->arr, p->idx, 1);
        // PC_ASSERT(r==0);
        // refresh_arr(set->arr, p->idx);
        elem_node_release(p);
        free(p);
    }

    pcutils_arrlist_free(set->arr);
    set->arr = NULL;
}

static void
variant_set_release(variant_set_t data)
{
    variant_set_release_elems(data);

    free(data->keynames);
    data->keynames = NULL;
    data->nr_keynames = 0;
    free(data->unique_key);
    data->unique_key = NULL;
}

static purc_variant_t
variant_set_create_kvs_n(variant_set_t set, purc_variant_t v1, va_list ap)
{
    PC_ASSERT(set->keynames);
    PC_ASSERT(v1 != PURC_VARIANT_INVALID);

    purc_variant_t kvs;
    kvs = purc_variant_make_object(0,
            PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
    if (kvs == PURC_VARIANT_INVALID)
        return PURC_VARIANT_INVALID;

    for (size_t i=0; i<set->nr_keynames; ++i) {
        purc_variant_t v;
        if (i == 0)
            v = v1;
        else
            v = va_arg(ap, purc_variant_t);

        if (v == PURC_VARIANT_INVALID) {
            purc_variant_unref(kvs);
            pcinst_set_error(PURC_ERROR_INVALID_VALUE);
            return PURC_VARIANT_INVALID;
        }
        if (purc_variant_is_undefined(v))
            continue;

        const char *sk = set->keynames[i];
        bool ok;
        ok = purc_variant_object_set_by_static_ckey(kvs, sk, v);
        if (!ok) {
            purc_variant_unref(kvs);
            return PURC_VARIANT_INVALID;
        }
    }

    return kvs;
}

static struct set_node*
variant_set_create_elem_node(purc_variant_t set, purc_variant_t val)
{
    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);

    struct set_node *_new = (struct set_node*)calloc(1, sizeof(*_new));
    if (!_new) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    _new->kvs = variant_set_kvs_from_val(data, val);
    if (!_new->kvs) {
        free(_new);
        return NULL;
    }

    _new->set  = set;
    _new->elem = val;
    purc_variant_ref(val);

    return _new;
}

static int
set_remove(purc_variant_t set, variant_set_t data, struct set_node *node,
        bool check)
{
    if (!shrink(set, node->elem, check)) {
        return -1;
    }

    pcutils_rbtree_erase(&node->node, &data->elems);
    int r = pcutils_arrlist_del_idx(data->arr, node->idx, 1);
    PC_ASSERT(r==0);

    shrunk(set, node->elem, check);

    refresh_arr(data->arr, node->idx);
    node->idx = -1;
    elem_node_release(node);
    free(node);

    return 0;
}

static int
insert(purc_variant_t set, variant_set_t data,
        purc_variant_t val, purc_variant_t kvs,
        struct rb_node *parent, struct rb_node **pnode,
        bool check)
{
    UNUSED_PARAM(kvs);

    if (!grow(set, val, check))
        return -1;

    struct set_node *node;
    node = variant_set_create_elem_node(set, val);
    if (!node)
        return -1;

    int r = pcutils_arrlist_add(data->arr, node);
    if (r) {
        elem_node_release(node);
        free(node);
        return -1;
    }

    size_t count = pcutils_arrlist_length(data->arr);
    node->idx = count - 1;

    struct rb_node *entry = &node->node;

    pcutils_rbtree_link_node(entry, parent, pnode);
    pcutils_rbtree_insert_color(entry, &data->elems);

    if (!elem_node_setup_constraints(node)) {
        bool check = false;
        r = set_remove(set, data, node, check);
        PC_ASSERT(r == 0);
        return -1;
    }

    grown(set, node->elem, check);

    return 0;
}

static purc_variant_t
variant_set_union(variant_set_t data, purc_variant_t old, purc_variant_t _new)
{
    UNUSED_PARAM(data);
    // FIXME: performance, performance, performance!!!
    purc_variant_t output;
    output = purc_variant_make_object(0,
            PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
    if (output == PURC_VARIANT_INVALID)
        return PURC_VARIANT_INVALID;

    purc_variant_t k, v;
    foreach_key_value_in_variant_object(_new, k, v) {
        PC_ASSERT(purc_variant_is_undefined(v) == false);
        bool ok;
        ok = purc_variant_object_set(output, k, v);
        if (!ok) {
            purc_variant_unref(output);
            return PURC_VARIANT_INVALID;
        }
    } end_foreach;

    foreach_key_value_in_variant_object(old, k, v) {
        PC_ASSERT(purc_variant_is_undefined(v) == false);
        bool silently = true;
        purc_variant_t t = purc_variant_object_get(output, k, silently);
        if (t != PURC_VARIANT_INVALID)
            continue;

        bool ok;
        ok = purc_variant_object_set(output, k, v);
        if (!ok) {
            purc_variant_unref(output);
            return PURC_VARIANT_INVALID;
        }
    } end_foreach;

    return output;
}

static int
insert_or_replace(purc_variant_t set,
        variant_set_t data, purc_variant_t val, bool overwrite,
        bool check)
{
    purc_variant_t kvs = variant_set_kvs_from_val(data, val);
    if (kvs == PURC_VARIANT_INVALID)
        return -1;

    struct element_rb_node rbn;
    find_element_rb_node(&rbn, data, kvs);

    if (!rbn.entry) {
        int r = insert(set, data, val, kvs, rbn.parent, rbn.pnode, check);
        purc_variant_unref(kvs);
        return r ? -1 : 0;
    }

    if (!overwrite) {
        purc_variant_unref(kvs);
        purc_set_error(PURC_ERROR_NOT_SUPPORTED);
        return -1;
    }

    struct set_node *curr;
    curr = container_of(rbn.entry, struct set_node, node);

    PC_ASSERT(curr->set != PURC_VARIANT_INVALID);

    if (curr->elem == val) {
        purc_variant_unref(kvs);
        return 0;
    }

    purc_variant_t tmp = variant_set_union(data, curr->elem, val);
    do {
        if (tmp == PURC_VARIANT_INVALID)
            break;

        if (!change(set, curr->elem, tmp, check))
            break;

        if (elem_node_replace(curr, tmp, kvs))
            break;

        changed(set, curr->elem, tmp, check);

        PURC_VARIANT_SAFE_CLEAR(tmp);
        PURC_VARIANT_SAFE_CLEAR(kvs);

        return 0;
    } while (0);

    PURC_VARIANT_SAFE_CLEAR(tmp);
    PURC_VARIANT_SAFE_CLEAR(kvs);

    return -1;
}

static int
variant_set_add_val(purc_variant_t set,
        variant_set_t data, purc_variant_t val, bool overwrite,
        bool check)
{
    if (!val) {
        pcinst_set_error(PURC_ERROR_INVALID_VALUE);
        return -1;
    }

    if (purc_variant_is_object(val) == false) {
        pcinst_set_error(PURC_ERROR_INVALID_VALUE);
        return -1;
    }

    if (insert_or_replace(set, data, val, overwrite, check))
        return -1;

    return 0;
}

static int
variant_set_add_valsn(purc_variant_t set, variant_set_t data, bool overwrite,
    bool check, size_t sz, va_list ap)
{
    size_t i = 0;
    while (i<sz) {
        purc_variant_t v = va_arg(ap, purc_variant_t);
        if (!v) {
            pcinst_set_error(PURC_ERROR_INVALID_VALUE);
            break;
        }

        if (variant_set_add_val(set, data, v, overwrite, check)) {
            break;
        }

        ++i;
    }
    return i<sz ? -1 : 0;
}

static purc_variant_t
make_set_c(bool check, size_t sz, const char *unique_key,
    purc_variant_t value0, va_list ap)
{
    purc_variant_t set = pcv_set_new();
    if (set==PURC_VARIANT_INVALID) {
        return PURC_VARIANT_INVALID;
    }

    do {
        variant_set_t data = pcv_set_get_data(set);
        if (variant_set_init(data, unique_key))
            break;

        if (sz>0) {
            purc_variant_t  v = value0;
            if (variant_set_add_val(set, data, v, true, check))
                break;

            int r = variant_set_add_valsn(set, data, true, check, sz-1, ap);
            if (r)
                break;
        }

        size_t extra = variant_set_get_extra_size(data);
        pcvariant_stat_set_extra_size(set, extra);
        return set;
    } while (0);

    // cleanup
    purc_variant_unref(set);

    return PURC_VARIANT_INVALID;
}

static purc_variant_t
pv_make_set_by_ckey_n(bool check, size_t sz, const char* unique_key,
    purc_variant_t value0, va_list ap)
{
    purc_variant_t v = make_set_c(check, sz, unique_key, value0, ap);

    return v;
}

purc_variant_t
purc_variant_make_set_by_ckey(size_t sz, const char* unique_key,
    purc_variant_t value0, ...)
{
    PCVARIANT_CHECK_FAIL_RET((sz==0 && value0==NULL) || (sz>0 && value0),
        PURC_VARIANT_INVALID);

    bool check = true;
    purc_variant_t v;
    va_list ap;
    va_start(ap, value0);
    v = pv_make_set_by_ckey_n(check, sz, unique_key, value0, ap);
    va_end(ap);

    return v;
}

static purc_variant_t
pv_make_set_n(bool check, size_t sz, purc_variant_t unique_key,
    purc_variant_t value0, va_list ap)
{
    const char *uk = NULL;
    if (unique_key) {
        uk = purc_variant_get_string_const(unique_key);
        PC_ASSERT(uk);
    }

    purc_variant_t v = make_set_c(check, sz, uk, value0, ap);

    return v;
}

purc_variant_t
purc_variant_make_set(size_t sz, purc_variant_t unique_key,
    purc_variant_t value0, ...)
{
    PCVARIANT_CHECK_FAIL_RET((sz==0 && value0==NULL) ||
        (sz>0 && value0),
        PURC_VARIANT_INVALID);

    PCVARIANT_CHECK_FAIL_RET(!unique_key || unique_key->type==PVT(_STRING),
        PURC_VARIANT_INVALID);

    bool check = true;
    purc_variant_t v;
    va_list ap;
    va_start(ap, value0);
    v = pv_make_set_n(check, sz, unique_key, value0, ap);
    va_end(ap);

    return v;
}

bool
purc_variant_set_add(purc_variant_t set, purc_variant_t value, bool overwrite)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) &&
        value && value->type==PVT(_OBJECT),
        PURC_VARIANT_INVALID);

    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);

    bool check = true;
    if (variant_set_add_val(set, data, value, overwrite, check))
        return false;

    size_t extra = variant_set_get_extra_size(data);
    pcvariant_stat_set_extra_size(set, extra);
    return true;
}

bool
purc_variant_set_remove(purc_variant_t set, purc_variant_t value,
        bool silently)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) && value,
        PURC_VARIANT_INVALID);

    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);
    PC_ASSERT(data->nr_keynames);

    purc_variant_t kvs = variant_set_kvs_from_val(data, value);
    if (kvs == PURC_VARIANT_INVALID)
        return false;

    bool check = true;
    int r = 0;
    struct set_node *p;
    p = find_element(data, kvs);
    if (p) {
        r = set_remove(set, data, p, check);
    }

    purc_variant_unref(kvs);

    if (r)
        return false;

    return p ? true : (silently ? true : false);
}

purc_variant_t
purc_variant_set_get_member_by_key_values(purc_variant_t set,
        purc_variant_t v1, ...)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) && v1,
        PURC_VARIANT_INVALID);

    variant_set_t data = pcv_set_get_data(set);
    if (!data || !data->unique_key || data->nr_keynames==0) {
        pcinst_set_error(PURC_ERROR_NOT_SUPPORTED);
        return PURC_VARIANT_INVALID;
    }

    va_list ap;
    va_start(ap, v1);
    purc_variant_t kvs = variant_set_create_kvs_n(data, v1, ap);
    va_end(ap);
    if (kvs == PURC_VARIANT_INVALID)
        return false;

    struct set_node *p;
    p = find_element(data, kvs);
    purc_variant_unref(kvs);

    return p ? p->elem : PURC_VARIANT_INVALID;
}

purc_variant_t
purc_variant_set_remove_member_by_key_values(purc_variant_t set,
        purc_variant_t v1, ...)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) && v1,
        PURC_VARIANT_INVALID);

    variant_set_t data = pcv_set_get_data(set);
    if (!data || !data->unique_key || data->nr_keynames==0) {
        pcinst_set_error(PURC_ERROR_NOT_SUPPORTED);
        return PURC_VARIANT_INVALID;
    }

    va_list ap;
    va_start(ap, v1);
    purc_variant_t kvs = variant_set_create_kvs_n(data, v1, ap);
    va_end(ap);
    if (kvs == PURC_VARIANT_INVALID)
        return PURC_VARIANT_INVALID;

    struct set_node *p;
    p = find_element(data, kvs);
    purc_variant_unref(kvs);

    if (!p) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
        return PURC_VARIANT_INVALID;
    }

    purc_variant_t v = p->elem;
    purc_variant_ref(v);

    bool check = true;
    int r = set_remove(set, data, p, check);
    if (r) {
        purc_variant_unref(v);
        return PURC_VARIANT_INVALID;
    }

    size_t extra = variant_set_get_extra_size(data);
    pcvariant_stat_set_extra_size(set, extra);

    return v;
}

bool
purc_variant_set_size(purc_variant_t set, size_t *sz)
{
    PC_ASSERT(set && sz);

    PCVARIANT_CHECK_FAIL_RET(set->type == PVT(_SET), false);

    variant_set_t data = pcv_set_get_data(set);

    PC_ASSERT(data);
    size_t count = pcutils_arrlist_length(data->arr);
    *sz = count;

    return true;
}

purc_variant_t
purc_variant_set_get_by_index(purc_variant_t set, int idx)
{
    PC_ASSERT(set);

    variant_set_t data = pcv_set_get_data(set);
    size_t count = pcutils_arrlist_length(data->arr);

    if (idx < 0 || (size_t)idx >= count)
        return PURC_VARIANT_INVALID;

    struct set_node *node;
    node = (struct set_node*)pcutils_arrlist_get_idx(data->arr, idx);
    PC_ASSERT(node);
    PC_ASSERT(node->idx == (size_t)idx);
    PC_ASSERT(node->elem != PURC_VARIANT_INVALID);

    return node->elem;
}

PCA_EXPORT purc_variant_t
purc_variant_set_remove_by_index(purc_variant_t set, int idx)
{
    PC_ASSERT(set);

    variant_set_t data = pcv_set_get_data(set);
    size_t count = pcutils_arrlist_length(data->arr);

    if (idx < 0 || (size_t)idx >= count) {
        pcinst_set_error(PCVARIANT_ERROR_OUT_OF_BOUNDS);
        return PURC_VARIANT_INVALID;
    }

    struct set_node *elem;
    elem = (struct set_node*)pcutils_arrlist_get_idx(data->arr, idx);
    PC_ASSERT(elem);
    PC_ASSERT(elem->idx == (size_t)idx);

    purc_variant_t v = elem->elem;
    purc_variant_ref(v);

    bool check = true;
    int r = set_remove(set, data, elem, check);
    if (r) {
        purc_variant_unref(v);
        return PURC_VARIANT_INVALID;
    }

    size_t extra = variant_set_get_extra_size(data);
    pcvariant_stat_set_extra_size(set, extra);

    return v;
}

PCA_EXPORT bool
purc_variant_set_set_by_index(purc_variant_t set, int idx, purc_variant_t val)
{
    PC_ASSERT(set);

    variant_set_t data = pcv_set_get_data(set);
    size_t count = pcutils_arrlist_length(data->arr);

    if (idx < 0 || (size_t)idx >= count) {
        pcinst_set_error(PCVARIANT_ERROR_OUT_OF_BOUNDS);
        return false;
    }

    struct set_node *elem;
    elem = (struct set_node*)pcutils_arrlist_get_idx(data->arr, idx);
    if (elem->elem == val)
        return true;

    purc_variant_t v = purc_variant_set_remove_by_index(set, idx);
    PC_ASSERT(v != PURC_VARIANT_INVALID);
    bool ok = purc_variant_set_add(set, val, true);
    PC_ASSERT(ok);
    purc_variant_unref(v);
    return ok;
}

struct purc_variant_set_iterator {
    purc_variant_t      set;
    struct rb_node     *curr;
    struct rb_node     *prev, *next;
};

static void
iterator_refresh(struct purc_variant_set_iterator *it)
{
    if (it->curr == NULL) {
        it->next = NULL;
        it->prev = NULL;
        return;
    }
    variant_set_t data = pcv_set_get_data(it->set);
    size_t count = pcutils_arrlist_length(data->arr);
    if (count==0) {
        it->next = NULL;
        it->prev = NULL;
        return;
    }
    struct rb_node *first, *last;
    first = pcutils_rbtree_first(&data->elems);
    last  = pcutils_rbtree_last(&data->elems);
    if (it->curr == first) {
        it->prev = NULL;
    } else {
        it->prev = pcutils_rbtree_prev(it->curr);
    }
    if (it->curr == last) {
        it->next = NULL;
    } else {
        it->next = pcutils_rbtree_next(it->curr);
    }
}

struct purc_variant_set_iterator*
purc_variant_set_make_iterator_begin(purc_variant_t set)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET),
        NULL);

    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);

    size_t count = pcutils_arrlist_length(data->arr);
    if (count == 0) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
        return NULL;
    }

    struct purc_variant_set_iterator *it;
    it = (struct purc_variant_set_iterator*)calloc(1, sizeof(*it));
    if (!it) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    it->set = set;

    struct rb_node *p;
    p = pcutils_rbtree_first(&data->elems);
    PC_ASSERT(p);

    it->curr = p;
    iterator_refresh(it);

    return it;
}

struct purc_variant_set_iterator*
purc_variant_set_make_iterator_end(purc_variant_t set)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET),
        NULL);

    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);

    size_t count = pcutils_arrlist_length(data->arr);
    if (count == 0) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
        return NULL;
    }

    struct purc_variant_set_iterator *it;
    it = (struct purc_variant_set_iterator*)calloc(1, sizeof(*it));
    if (!it) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    it->set = set;

    struct rb_node *p;
    p = pcutils_rbtree_last(&data->elems);
    PC_ASSERT(p);

    it->curr = p;
    iterator_refresh(it);

    return it;
}

void
purc_variant_set_release_iterator(struct purc_variant_set_iterator* it)
{
    if (!it)
        return;
    free(it);
}

bool
purc_variant_set_iterator_next(struct purc_variant_set_iterator* it)
{
    PCVARIANT_CHECK_FAIL_RET(it && it->set &&
        it->set->type==PVT(_SET) && it->curr,
        false);

    variant_set_t data = pcv_set_get_data(it->set);
    PC_ASSERT(data);

    it->curr = it->next;
    iterator_refresh(it);

    return it->curr ? true : false;
}

bool
purc_variant_set_iterator_prev(struct purc_variant_set_iterator* it)
{
    PCVARIANT_CHECK_FAIL_RET(it && it->set &&
        it->set->type==PVT(_SET) && it->curr,
        false);

    variant_set_t data = pcv_set_get_data(it->set);
    PC_ASSERT(data);

    it->curr = it->prev;
    iterator_refresh(it);

    return it->curr ? true : false;
}

purc_variant_t
purc_variant_set_iterator_get_value(struct purc_variant_set_iterator* it)
{
    PCVARIANT_CHECK_FAIL_RET(it && it->set &&
        it->set->type==PVT(_SET) && it->curr,
        PURC_VARIANT_INVALID);

    struct set_node *p;
    p = container_of(it->curr, struct set_node, node);
    return p->elem;
}

void
pcvariant_set_release(purc_variant_t value)
{
    variant_set_t data = pcv_set_get_data(value);
    PC_ASSERT(data);

    variant_set_release(data);
    free(data);
    pcv_set_set_data(value, NULL);
    pcvariant_stat_set_extra_size(value, 0);
}

/* VWNOTE: unnecessary
int pcvariant_set_compare(purc_variant_t lv, purc_variant_t rv)
{
    variant_set_t ldata = _pcv_set_get_data(lv);
    variant_set_t rdata = _pcv_set_get_data(rv);
    PC_ASSERT(ldata && rdata);

    struct set_node *ln, *rn;
    ln = avl_first_element(&ldata->objs, ln, avl);
    rn = avl_first_element(&rdata->objs, rn, avl);
    for (; ln && rn;
        ln = avl_next_element(ln, avl),
        rn = avl_next_element(rn, avl))
    {
        int t = purc_variant_compare(ln->obj, rn->obj);
        if (t)
            return t;
    }

    return ln ? 1 : -1;
}
*/

struct set_user_data {
    int (*cmp)(size_t nr_keynames,
            purc_variant_t l[], purc_variant_t r[], void *ud);
    void *ud;
    size_t               nr_keynames;
};

#if OS(HURD) || OS(LINUX)
static int cmp_variant(const void *l, const void *r, void *ud)
{
    UNUSED_PARAM(ud);

    struct set_node *nl = *(struct set_node**)l;
    struct set_node *nr = *(struct set_node**)r;
    purc_variant_t vl = nl->kvs;
    purc_variant_t vr = nr->kvs;
    return purc_variant_compare_ex(vl, vr, PCVARIANT_COMPARE_OPT_AUTO);
}
#elif OS(DARWIN) || OS(FREEBSD) || OS(NETBSD) || OS(OPENBSD) || OS(WINDOWS)
static int cmp_variant(void *ud, const void *l, const void *r)
{
    UNUSED_PARAM(ud);
    struct set_node *nl = *(struct set_node**)l;
    struct set_node *nr = *(struct set_node**)r;
    purc_variant_t vl = nl->kvs;
    purc_variant_t vr = nr->kvs;
    return purc_variant_compare_ex(vl, vr, PCVARIANT_COMPARE_OPT_AUTO);
}
#else
#error Unsupported operating system.
#endif

int pcvariant_set_sort(purc_variant_t value)
{
    if (!value || value->type != PURC_VARIANT_TYPE_SET)
        return -1;

    variant_set_t data = pcv_set_get_data(value);
    struct pcutils_arrlist *al = data->arr;
    if (!al)
        return -1;
    void *arr = al->array;

#if OS(HURD) || OS(LINUX)
    qsort_r(arr, al->length, sizeof(struct set_node*), cmp_variant, NULL);
#elif OS(DARWIN) || OS(FREEBSD) || OS(NETBSD) || OS(OPENBSD)
    qsort_r(arr, al->length, sizeof(struct set_node*), NULL, cmp_variant);
#elif OS(WINDOWS)
    qsort_s(arr, al->length, sizeof(struct set_node*), cmp_variant, NULL);
#endif

    refresh_arr(al, 0);

    return 0;
}

purc_variant_t
pcvariant_set_find(purc_variant_t set, purc_variant_t value)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) && value,
        PURC_VARIANT_INVALID);

    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);
    PC_ASSERT(data->nr_keynames);

    purc_variant_t kvs = variant_set_kvs_from_val(data, value);
    if (kvs == PURC_VARIANT_INVALID)
        return false;

    struct set_node *p;
    p = find_element(data, kvs);
    purc_variant_unref(kvs);

    return p ? p->elem : PURC_VARIANT_INVALID;
}

int pcvariant_set_get_uniqkeys(purc_variant_t set, size_t *nr_keynames,
        const char ***keynames)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) &&
            nr_keynames && keynames, -1);

    variant_set_t data = pcv_set_get_data(set);
    PC_ASSERT(data);
    PC_ASSERT(data->nr_keynames);

    *nr_keynames = data->nr_keynames;
    *keynames = data->keynames;

    return 0;
}

purc_variant_t
pcvariant_set_clone(purc_variant_t set, bool recursively)
{
    int r;
    struct pcutils_string str;
    pcutils_string_init(&str, 32);
    variant_set_t data = pcv_set_get_data(set);
    if (data->keynames) {
        r = 0;
        for (size_t i=0; i<data->nr_keynames; ++i) {
            if (i) {
                r = pcutils_string_append_chunk(&str, " ");
                if (r) {
                    purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
                    break;
                }
            }
            r = pcutils_string_append_chunk(&str, data->keynames[i]);
            if (r) {
                purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
                break;
            }
        }
        if (r) {
            pcutils_string_reset(&str);
            return PURC_VARIANT_INVALID;
        }
    }

    purc_variant_t var;
    var = purc_variant_make_set_by_ckey(0, str.curr, PURC_VARIANT_INVALID);
    pcutils_string_reset(&str);
    if (var == PURC_VARIANT_INVALID)
        return PURC_VARIANT_INVALID;

    purc_variant_t v;
    // NOTE: keep document-order
    foreach_value_in_variant_set(set, v) {
        purc_variant_t val;
        if (recursively) {
            val = pcvariant_container_clone(v, recursively);
        }
        else {
            val = purc_variant_ref(v);
        }
        if (val == PURC_VARIANT_INVALID) {
            purc_variant_unref(var);
            return PURC_VARIANT_INVALID;
        }
        bool ok;
        ok = purc_variant_set_add(var, val, false);
        purc_variant_unref(val);
        if (!ok) {
            purc_variant_unref(var);
            return PURC_VARIANT_INVALID;
        }
    } end_foreach;

    PC_ASSERT(var != set);
    return var;
}

void
pcvar_set_break_edge_to_parent(purc_variant_t set,
        struct pcvar_rev_update_edge *edge)
{
    PC_ASSERT(purc_variant_is_set(set));
    variant_set_t data = pcv_set_get_data(set);
    if (!data)
        return;

    pcvar_break_edge(set, &data->rev_update_chain, edge);
}

int
pcvar_set_build_edge_to_parent(purc_variant_t set,
        struct pcvar_rev_update_edge *edge)
{
    PC_ASSERT(purc_variant_is_set(set));
    variant_set_t data = pcv_set_get_data(set);
    if (!data)
        return 0;

    return pcvar_build_edge(set, &data->rev_update_chain, edge);
}

