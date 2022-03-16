/*
 * @file observer.c
 * @author Geng Yue
 * @date 2021/07/02
 * @brief The implementation for variant observer
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

#include "purc-errors.h"
#include "private/debug.h"
#include "private/errors.h"
#include "variant-internals.h"

#include <stdlib.h>

static pcvar_listener*
register_listener(purc_variant_t v, unsigned int flags,
        pcvar_op_t op, pcvar_op_handler handler, void *ctxt)
{
    struct list_head *listeners;
    listeners = &v->listeners;

    struct pcvar_listener *listener;
    listener = (struct pcvar_listener*)calloc(1, sizeof(*listener));
    if (!listener) {
        pcinst_set_error(PCVARIANT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    listener->flags          = flags;
    listener->op             = op;
    listener->ctxt           = ctxt;
    listener->handler        = handler;

    if ((flags & PCVAR_LISTENER_PRE_OR_POST) == PCVAR_LISTENER_PRE) {
        list_add(&listener->list_node, listeners);
    } else {
        list_add_tail(&listener->list_node, listeners);
    }

    return listener;
}

struct pcvar_listener*
purc_variant_register_pre_listener(purc_variant_t v,
        pcvar_op_t op, pcvar_op_handler handler, void *ctxt)
{
    if ((op & PCVAR_OPERATION_ALL) != op) {
        pcinst_set_error(PCVARIANT_ERROR_WRONG_ARGS);
        return NULL;
    }

    if (v == PURC_VARIANT_INVALID || !op || !handler) {
        pcinst_set_error(PCVARIANT_ERROR_WRONG_ARGS);
        return NULL;
    }

    if (!IS_CONTAINER(v->type)) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_SUPPORTED);
        return NULL;
    }

    return register_listener(v, PCVAR_LISTENER_PRE, op, handler, ctxt);
}

struct pcvar_listener*
purc_variant_register_post_listener(purc_variant_t v,
        pcvar_op_t op, pcvar_op_handler handler, void *ctxt)
{
    if ((op & PCVAR_OPERATION_ALL) != op) {
        pcinst_set_error(PCVARIANT_ERROR_WRONG_ARGS);
        return NULL;
    }

    if (v == PURC_VARIANT_INVALID || !op || !handler) {
        pcinst_set_error(PCVARIANT_ERROR_WRONG_ARGS);
        return NULL;
    }

    if (!IS_CONTAINER(v->type)) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_SUPPORTED);
        return NULL;
    }

    return register_listener(v, PCVAR_LISTENER_POST, op, handler, ctxt);
}

bool
purc_variant_revoke_listener(purc_variant_t v,
        struct pcvar_listener *listener)
{
    if (v == PURC_VARIANT_INVALID || !listener) {
        pcinst_set_error(PCVARIANT_ERROR_WRONG_ARGS);
        return false;
    }

    if (!IS_CONTAINER(v->type)) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_SUPPORTED);
        return false;
    }

    struct list_head *listeners;
    listeners = &v->listeners;

    struct list_head *p, *n;
    list_for_each_safe(p, n, listeners) {
        struct pcvar_listener *curr;
        curr = container_of(p, struct pcvar_listener, list_node);
        if (curr != listener)
            continue;

        list_del(p);
        free(curr);
        return true;
    }

    return false;
}

bool pcvariant_on_pre_fired(
        purc_variant_t source,  // the source variant.
        pcvar_op_t op,          // the operation identifier.
        size_t nr_args,         // the number of the relevant child variants.
        purc_variant_t *argv    // the array of all relevant child variants.
        )
{
    op &= PCVAR_OPERATION_ALL;
    PC_ASSERT(op != PCVAR_OPERATION_ALL);

    struct list_head *listeners;
    listeners = &source->listeners;

    struct list_head *p, *n;
    list_for_each_safe(p, n, listeners) {
        struct pcvar_listener *curr;
        curr = container_of(p, struct pcvar_listener, list_node);
        if ((curr->op & op) == 0)
            continue;

        if ((curr->flags & PCVAR_LISTENER_PRE_OR_POST) != PCVAR_LISTENER_PRE)
            break;

        bool ok = curr->handler(source, op, curr->ctxt, nr_args, argv);
        if (!ok)
            return false;
    }

    return true;
}

void pcvariant_on_post_fired(
        purc_variant_t source,  // the source variant.
        pcvar_op_t op,          // the operation identifier.
        size_t nr_args,         // the number of the relevant child variants.
        purc_variant_t *argv    // the array of all relevant child variants.
        )
{
    op &= PCVAR_OPERATION_ALL;
    PC_ASSERT(op != PCVAR_OPERATION_ALL);

    struct list_head *listeners;
    listeners = &source->listeners;

    struct pcvar_listener *p, *n;
    list_for_each_entry_reverse_safe(p, n, listeners, list_node) {
        struct pcvar_listener *curr = p;
        PC_ASSERT(curr);
        if ((curr->op & op) == 0)
            continue;

        if ((curr->flags & PCVAR_LISTENER_PRE_OR_POST) == PCVAR_LISTENER_PRE)
            break;

        bool ok = curr->handler(source, op, curr->ctxt, nr_args, argv);
        PC_ASSERT(ok);
    }
}

void
pcvar_break_rue_downward(purc_variant_t val)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
    switch (val->type) {
        case PURC_VARIANT_TYPE_ARRAY:
            pcvar_array_break_rue_downward(val);
            return;
        case PURC_VARIANT_TYPE_OBJECT:
            pcvar_object_break_rue_downward(val);
            return;
        case PURC_VARIANT_TYPE_SET:
        case PURC_VARIANT_TYPE_NULL:
        case PURC_VARIANT_TYPE_BOOLEAN:
        case PURC_VARIANT_TYPE_EXCEPTION:
        case PURC_VARIANT_TYPE_NUMBER:
        case PURC_VARIANT_TYPE_LONGINT:
        case PURC_VARIANT_TYPE_ULONGINT:
        case PURC_VARIANT_TYPE_LONGDOUBLE:
        case PURC_VARIANT_TYPE_ATOMSTRING:
        case PURC_VARIANT_TYPE_STRING:
        case PURC_VARIANT_TYPE_BSEQUENCE:
        case PURC_VARIANT_TYPE_DYNAMIC:
        case PURC_VARIANT_TYPE_NATIVE:
            return;
        default:
            PC_DEBUGX("%d", val->type);
            PC_ASSERT(0);
    }
}

void
pcvar_break_edge_to_parent(purc_variant_t val,
        struct pcvar_rev_update_edge *edge)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
    if (pcvariant_is_mutable(val) == false)
        return;

    switch (val->type) {
        case PURC_VARIANT_TYPE_ARRAY:
            pcvar_array_break_edge_to_parent(val, edge);
            return;
        case PURC_VARIANT_TYPE_OBJECT:
            pcvar_object_break_edge_to_parent(val, edge);
            return;
        case PURC_VARIANT_TYPE_SET:
            pcvar_set_break_edge_to_parent(val, edge);
            return;
        default:
            PC_ASSERT(0);
    }
}

void
pcvar_break_edge(purc_variant_t val,
        struct pcvar_rev_update_edge *edge_in_val,
        struct pcvar_rev_update_edge *edge)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
    PC_ASSERT(edge_in_val);
    PC_ASSERT(edge);

    if (edge_in_val->parent == PURC_VARIANT_INVALID)
        return;

    PC_ASSERT(edge_in_val->pre_listener);
    PC_ASSERT(edge_in_val->post_listener);

    PC_ASSERT(edge->parent == edge_in_val->parent);

    bool ok;

    switch (edge->parent->type) {
        case PURC_VARIANT_TYPE_ARRAY:
            PC_ASSERT(edge->arr_me->val == val);
            PC_ASSERT(edge->arr_me == edge_in_val->arr_me);
            ok = purc_variant_revoke_listener(val, edge_in_val->pre_listener);
            PC_ASSERT(ok);
            edge_in_val->pre_listener = NULL;
            ok = purc_variant_revoke_listener(val, edge_in_val->post_listener);
            PC_ASSERT(ok);
            edge_in_val->post_listener = NULL;
            edge_in_val->parent = PURC_VARIANT_INVALID;
            edge_in_val->arr_me = NULL;
            return;

        case PURC_VARIANT_TYPE_OBJECT:
            PC_ASSERT(edge->obj_me->val == val);
            PC_ASSERT(edge->obj_me == edge_in_val->obj_me);
            ok = purc_variant_revoke_listener(val, edge_in_val->pre_listener);
            PC_ASSERT(ok);
            edge_in_val->pre_listener = NULL;
            ok = purc_variant_revoke_listener(val, edge_in_val->post_listener);
            PC_ASSERT(ok);
            edge_in_val->post_listener = NULL;
            edge_in_val->parent = PURC_VARIANT_INVALID;
            edge_in_val->obj_me = NULL;
            return;

        case PURC_VARIANT_TYPE_SET:
            PC_ASSERT(edge->set_me->elem == val);
            PC_ASSERT(edge->set_me == edge_in_val->set_me);
            ok = purc_variant_revoke_listener(val, edge_in_val->pre_listener);
            PC_ASSERT(ok);
            edge_in_val->pre_listener = NULL;
            ok = purc_variant_revoke_listener(val, edge_in_val->post_listener);
            PC_ASSERT(ok);
            edge_in_val->post_listener = NULL;
            edge_in_val->parent = PURC_VARIANT_INVALID;
            edge_in_val->set_me = NULL;
            return;

        default:
            PC_ASSERT(0);
    }
}

int
pcvar_build_rue_downward(purc_variant_t val)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
    switch (val->type) {
        case PURC_VARIANT_TYPE_ARRAY:
            return pcvar_array_build_rue_downward(val);
        case PURC_VARIANT_TYPE_OBJECT:
            return pcvar_object_build_rue_downward(val);
        case PURC_VARIANT_TYPE_SET:
        case PURC_VARIANT_TYPE_NULL:
        case PURC_VARIANT_TYPE_BOOLEAN:
        case PURC_VARIANT_TYPE_EXCEPTION:
        case PURC_VARIANT_TYPE_NUMBER:
        case PURC_VARIANT_TYPE_LONGINT:
        case PURC_VARIANT_TYPE_ULONGINT:
        case PURC_VARIANT_TYPE_LONGDOUBLE:
        case PURC_VARIANT_TYPE_ATOMSTRING:
        case PURC_VARIANT_TYPE_STRING:
        case PURC_VARIANT_TYPE_BSEQUENCE:
        case PURC_VARIANT_TYPE_DYNAMIC:
        case PURC_VARIANT_TYPE_NATIVE:
            return 0;
        default:
            PC_DEBUGX("%d", val->type);
            PC_ASSERT(0);
    }
}

int
pcvar_build_edge_to_parent(purc_variant_t val,
        struct pcvar_rev_update_edge *edge)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
    if (pcvariant_is_mutable(val) == false)
        return 0;

    switch (val->type) {
        case PURC_VARIANT_TYPE_ARRAY:
            return pcvar_array_build_edge_to_parent(val, edge);
        case PURC_VARIANT_TYPE_OBJECT:
            return pcvar_object_build_edge_to_parent(val, edge);
        case PURC_VARIANT_TYPE_SET:
            return pcvar_set_build_edge_to_parent(val, edge);
        default:
            PC_ASSERT(0);
    }
}

static bool
rev_update_grow(
        bool pre,
        purc_variant_t src,
        struct pcvar_rev_update_edge *edge,
        size_t nr_args,
        purc_variant_t *argv)
{
    UNUSED_PARAM(pre);
    UNUSED_PARAM(src);
    UNUSED_PARAM(edge);
    UNUSED_PARAM(nr_args);
    UNUSED_PARAM(argv);
    PC_ASSERT(0);
    return true;
}

static bool
rev_update_shrink(
        bool pre,
        purc_variant_t src,
        struct pcvar_rev_update_edge *edge,
        size_t nr_args,
        purc_variant_t *argv)
{
    UNUSED_PARAM(pre);
    UNUSED_PARAM(src);
    UNUSED_PARAM(edge);
    UNUSED_PARAM(nr_args);
    UNUSED_PARAM(argv);
    PC_ASSERT(0);
    return true;
}

static bool
obj_rev_update_change(
        bool pre,
        purc_variant_t obj,
        struct pcvar_rev_update_edge *edge,
        size_t nr_args,
        purc_variant_t *argv)
{
    UNUSED_PARAM(pre);
    UNUSED_PARAM(obj);
    UNUSED_PARAM(edge);
    UNUSED_PARAM(nr_args);
    UNUSED_PARAM(argv);

    variant_obj_t data = pcvar_obj_get_data(obj);
    PC_ASSERT(data);
    PC_ASSERT(&data->rev_update_chain == edge);

    return true;
}


static bool
rev_update_change(
        bool pre,
        purc_variant_t src,
        struct pcvar_rev_update_edge *edge,
        size_t nr_args,
        purc_variant_t *argv)
{
    switch (src->type) {
        case PURC_VARIANT_TYPE_OBJECT:
            return obj_rev_update_change(pre, src, edge, nr_args, argv);
        default:
            PC_DEBUGX("Not supported for `%s` variant",
                    pcvariant_get_typename(src->type));
            PC_ASSERT(0);
    }

    return true;
}

static bool
rev_update(
        bool pre,
        purc_variant_t src,
        pcvar_op_t op,
        struct pcvar_rev_update_edge *edge,
        size_t nr_args,
        purc_variant_t *argv)
{
    UNUSED_PARAM(pre);
    UNUSED_PARAM(src);
    UNUSED_PARAM(op);
    UNUSED_PARAM(edge);
    UNUSED_PARAM(nr_args);
    UNUSED_PARAM(argv);

    switch (op) {
        case PCVAR_OPERATION_GROW:
            return rev_update_grow(pre, src, edge, nr_args, argv);
        case PCVAR_OPERATION_SHRINK:
            return rev_update_shrink(pre, src, edge, nr_args, argv);
        case PCVAR_OPERATION_CHANGE:
            return rev_update_change(pre, src, edge, nr_args, argv);
        default:
            PC_ASSERT(0);
    }
}

static bool
rev_update_chain_pre_handler(
        purc_variant_t src,  // the source variant.
        pcvar_op_t op,       // the operation identifier.
        void *ctxt,          // the context stored when registering the handler.
        size_t nr_args,      // the number of the relevant child variants.
        purc_variant_t *argv // the array of all relevant child variants.
        )
{
    PC_ASSERT(ctxt);
    struct pcvar_rev_update_edge *edge;
    edge = (struct pcvar_rev_update_edge*)ctxt;
    PC_ASSERT(edge->parent);
    PC_ASSERT(edge->parent != src);

    bool pre = true;
    return rev_update(pre, src, op, edge, nr_args, argv);
}

static bool
rev_update_chain_post_handler(
        purc_variant_t src,  // the source variant.
        pcvar_op_t op,       // the operation identifier.
        void *ctxt,          // the context stored when registering the handler.
        size_t nr_args,      // the number of the relevant child variants.
        purc_variant_t *argv // the array of all relevant child variants.
        )
{
    PC_ASSERT(ctxt);
    struct pcvar_rev_update_edge *edge;
    edge = (struct pcvar_rev_update_edge*)ctxt;
    PC_ASSERT(edge->parent);

    bool pre = false;
    return rev_update(pre, src, op, edge, nr_args, argv);
}

int
pcvar_build_edge(purc_variant_t val,
        struct pcvar_rev_update_edge *edge_in_val,
        struct pcvar_rev_update_edge *edge)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
    PC_ASSERT(edge_in_val);
    PC_ASSERT(edge);

    PC_ASSERT(edge->pre_listener == NULL);
    PC_ASSERT(edge->post_listener == NULL);

    bool parent_is_set = purc_variant_is_set(edge->parent);
    if (!parent_is_set &&
            pcvar_container_belongs_to_set(edge->parent) == false)
    {
        PC_ASSERT(0);
    }

    if (edge_in_val->parent != PURC_VARIANT_INVALID)
        PC_ASSERT(0);

    if (edge_in_val->pre_listener)
        PC_ASSERT(0);

    if (edge_in_val->post_listener)
        PC_ASSERT(0);

    struct pcvar_listener *pre_listener, *post_listener;
    pre_listener = purc_variant_register_pre_listener(val,
            PCVAR_OPERATION_ALL, rev_update_chain_pre_handler,
            edge_in_val);
    if (!pre_listener)
        return -1;
    post_listener = purc_variant_register_post_listener(val,
            PCVAR_OPERATION_ALL, rev_update_chain_post_handler,
            edge_in_val);
    if (!post_listener) {
        bool ok = purc_variant_revoke_listener(val, edge_in_val->pre_listener);
        PC_ASSERT(ok);
        return -1;
    }

    *edge_in_val = *edge;
    edge_in_val->pre_listener  = pre_listener;
    edge_in_val->post_listener = post_listener;

    return 0;
}

bool
pcvar_container_belongs_to_set(purc_variant_t val)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
    switch (val->type) {
        case PURC_VARIANT_TYPE_ARRAY:
            {
                variant_arr_t data = pcvar_arr_get_data(val);
                PC_ASSERT(data);
                if (data->rev_update_chain.parent != PURC_VARIANT_INVALID)
                    return true;
                return false;
            }
        case PURC_VARIANT_TYPE_OBJECT:
            {
                variant_obj_t data = pcvar_obj_get_data(val);
                PC_ASSERT(data);
                if (data->rev_update_chain.parent != PURC_VARIANT_INVALID)
                    return true;
                return false;
            }
        case PURC_VARIANT_TYPE_SET:
            {
                variant_set_t data = pcvar_set_get_data(val);
                PC_ASSERT(data);
                if (data->rev_update_chain.parent != PURC_VARIANT_INVALID)
                    return true;
                return false;
            }
        default:
            return false;
    }
}

purc_variant_t
pcvar_top_in_rev_update_chain(purc_variant_t val)
{
    PC_ASSERT(val != PURC_VARIANT_INVALID);
again:
    switch (val->type) {
        case PURC_VARIANT_TYPE_ARRAY:
            {
                variant_arr_t data = pcvar_arr_get_data(val);
                PC_ASSERT(data);
                if (data->rev_update_chain.parent == PURC_VARIANT_INVALID)
                    return val;
                val = data->rev_update_chain.parent;
                break;
            }
        case PURC_VARIANT_TYPE_OBJECT:
            {
                variant_obj_t data = pcvar_obj_get_data(val);
                PC_ASSERT(data);
                if (data->rev_update_chain.parent == PURC_VARIANT_INVALID)
                    return val;
                val = data->rev_update_chain.parent;
                break;
            }
        case PURC_VARIANT_TYPE_SET:
            {
                variant_set_t data = pcvar_set_get_data(val);
                PC_ASSERT(data);
                if (data->rev_update_chain.parent == PURC_VARIANT_INVALID)
                    return val;
                val = data->rev_update_chain.parent;
                break;
            }
        default:
            PC_ASSERT(0);
    }
    goto again;
}

