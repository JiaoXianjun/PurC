/**
 * @file observer.c
 * @author Xue Shuming
 * @date 2022/07/01
 * @brief The impl for observer
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
 *
 */

#include "config.h"

#include "internal.h"
#include "private/instance.h"
#include "private/msg-queue.h"
#include "private/interpreter.h"
#include "private/regex.h"

#define BUILTIN_VAR_CRTN        PURC_PREDEF_VARNAME_CRTN

static void
release_observer(struct pcintr_observer *observer)
{
    if (!observer)
        return;

    list_del(&observer->node);

    if (observer->on_revoke) {
        observer->on_revoke(observer, observer->on_revoke_data);
    }

    if (observer->observed != PURC_VARIANT_INVALID) {
        if (purc_variant_is_native(observer->observed)) {
            struct purc_native_ops *ops = purc_variant_native_get_ops(
                    observer->observed);
            if (ops && ops->on_forget) {
                void *native_entity = purc_variant_native_get_entity(
                        observer->observed);
                ops->on_forget(native_entity,
                        purc_atom_to_string(observer->msg_type_atom),
                        observer->sub_type);
            }
        }

        PURC_VARIANT_SAFE_CLEAR(observer->observed);
    }

    free(observer->sub_type);
    observer->sub_type = NULL;
}


static void
free_observer(struct pcintr_observer *observer)
{
    if (!observer)
        return;

    release_observer(observer);
    free(observer);
}

static void
add_observer_into_list(pcintr_stack_t stack, struct list_head *list,
        struct pcintr_observer* observer)
{
    observer->list = list;
    list_add_tail(&observer->node, list);

    // TODO:
    PC_ASSERT(stack);
    PC_ASSERT(stack->co->waits >= 0);
    stack->co->waits++;
}

static
bool is_variant_match_observe(purc_variant_t observed, purc_variant_t val)
{
    if (observed == val || purc_variant_is_equal_to(observed, val)) {
        return true;
    }
    if (purc_variant_is_native(observed)) {
        struct purc_native_ops *ops = purc_variant_native_get_ops(observed);
        if (ops == NULL || ops->match_observe == NULL) {
            return false;
        }
        return ops->match_observe(purc_variant_native_get_entity(observed),
                val);
    }
    return false;
}



void
pcintr_destroy_observer_list(struct list_head *observer_list)
{
    struct pcintr_observer *p, *n;
    list_for_each_entry_reverse_safe(p, n, observer_list, node) {
        list_del(&p->node);
        list_del(&p->node);
        free_observer(p);
    }
}

struct list_head *
pcintr_get_observer_list(pcintr_stack_t stack, purc_variant_t observed)
{
    PC_ASSERT(observed != PURC_VARIANT_INVALID);

    struct list_head *list = NULL;
    if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_DYNAMIC)) {
        list = &stack->dynamic_observers;
    }
    else if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_NATIVE)) {
        list = &stack->native_observers;
    }
    else if (purc_variant_is_string(observed)) {
        // XXX: optimization
        // CSS selector used string
        // handle by elements.c match_observe
        const char *s = purc_variant_get_string_const(observed);
        if (strlen(s) > 0 && (s[0] == '#' || s[0] == '.')) {
            list = &stack->native_observers;
        }
        else {
            list = &stack->common_observers;
        }
    }
    else if (pcintr_is_named_var_for_event(observed)) {
        list = &stack->native_observers;
    }
    else {
        list = &stack->common_observers;
    }
    return list;
}

bool
pcintr_is_observer_match(struct pcintr_observer *observer,
        purc_variant_t observed, purc_atom_t type_atom, const char *sub_type)
{
    if ((is_variant_match_observe(observer->observed, observed)) &&
                (observer->msg_type_atom == type_atom)) {
        if (observer->sub_type == sub_type ||
                pcregex_is_match(observer->sub_type, sub_type)) {
            return true;
        }
    }
    return false;
}




struct pcintr_observer*
pcintr_register_observer(pcintr_stack_t stack,
        purc_variant_t observed,
        purc_variant_t for_value,
        purc_atom_t msg_type_atom, const char *sub_type,
        pcvdom_element_t scope,
        pcdoc_element_t edom_element,
        pcvdom_element_t pos,
        pcintr_on_revoke_observer on_revoke,
        void *on_revoke_data
        )
{
    UNUSED_PARAM(for_value);

    struct list_head *list = NULL;
    if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_DYNAMIC)) {
        list = &stack->dynamic_observers;
    }
    else if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_NATIVE)) {
        list = &stack->native_observers;
    }
    else {
        list = &stack->common_observers;
    }

    struct pcintr_observer* observer =  (struct pcintr_observer*)calloc(1,
            sizeof(struct pcintr_observer));
    if (!observer) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    observer->stack = stack;
    observer->observed = observed;
    purc_variant_ref(observed);
    observer->scope = scope;
    observer->edom_element = edom_element;
    observer->pos = pos;
    observer->msg_type_atom = msg_type_atom;
    observer->sub_type = sub_type ? strdup(sub_type) : NULL;
    observer->on_revoke = on_revoke;
    observer->on_revoke_data = on_revoke_data;
    add_observer_into_list(stack, list, observer);

    // observe idle
    purc_variant_t hvml = pcintr_get_coroutine_variable(stack->co,
            BUILTIN_VAR_CRTN);
    if (observed == hvml) {
        stack->observe_idle = 1;
    }

    return observer;
}

void
pcintr_revoke_observer(struct pcintr_observer* observer)
{
    if (!observer)
        return;

    // TODO:
    pcintr_stack_t stack = observer->stack;
    PC_ASSERT(stack);
    PC_ASSERT(stack->co->waits >= 1);
    stack->co->waits--;

    // observe idle
    purc_variant_t hvml = pcintr_get_coroutine_variable(stack->co,
            BUILTIN_VAR_CRTN);
    if (observer->observed == hvml) {
        stack->observe_idle = 0;
    }

    free_observer(observer);
}

void
pcintr_revoke_observer_ex(pcintr_stack_t stack, purc_variant_t observed,
        purc_atom_t msg_type_atom, const char *sub_type)
{
    struct list_head* list = pcintr_get_observer_list(stack, observed);
    struct pcintr_observer *p, *n;
    list_for_each_entry_safe(p, n, list, node) {
        if (pcintr_is_observer_match(p, observed, msg_type_atom, sub_type)) {
            pcintr_revoke_observer(p);
            break;
        }
    }
}

