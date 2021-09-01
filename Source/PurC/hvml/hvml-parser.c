/*
 * @file hvml-parser.c
 * @author Xu Xiaohong
 * @date 2021/09/01
 * @brief The interfaces for hvml token.
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

#include "private/instance.h"
#include "private/errors.h"
#include "private/debug.h"
#include "private/utils.h"
#include "hvml-parser.h"

#define VTT(x) PCHVML_TOKEN_##x

struct pchvml_token*
pchvml_vdom_next_token(struct pchvml_vdom_tokenizer *tokenizer,
    purc_rwstream_t in)
{
    // TODO: dummy implementation
    UNUSED_PARAM(tokenizer);
    UNUSED_PARAM(in);

    return NULL;
}

static void
_vdom_parser_reset(struct pchvml_vdom_parser *parser)
{
    parser->curr = NULL;

    if (parser->doc) {
        pcvdom_document_destroy(parser->doc);
        parser->doc = NULL;
    }
}

static void
_vdom_parser_destroy(struct pchvml_vdom_parser *parser)
{
    _vdom_parser_reset(parser);
    free(parser);
}

struct pchvml_vdom_parser*
pchvml_vdom_parser_create(struct pchvml_vdom_tokenizer *tokenizer)
{
    if (!tokenizer) {
        pcinst_set_error(PURC_ERROR_INVALID_VALUE);
        return NULL;
    }

    struct pchvml_vdom_parser *parser;
    parser = (struct pchvml_vdom_parser*)calloc(1, sizeof(*parser));
    if (!parser) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    parser->tokenizer = tokenizer;

    return parser;
}

static int
_on_doctype(struct pchvml_vdom_parser *parser, struct pchvml_token *token)
{
    PC_ASSERT(parser && parser->doc==NULL && parser->curr==NULL);
    PC_ASSERT(token && token->data);

    struct pcvdom_document *doc = pcvdom_document_create(token->data);
    if (!doc) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    parser->doc  = doc;
    parser->curr = &doc->node;

    return 0;
}

static int
_on_start_tag(struct pchvml_vdom_parser *parser, struct pchvml_token *token)
{
    UNUSED_PARAM(parser);
    UNUSED_PARAM(token);
    return -1;
}

static int
_on_end_tag(struct pchvml_vdom_parser *parser, struct pchvml_token *token)
{
    UNUSED_PARAM(parser);
    UNUSED_PARAM(token);
    return -1;
}

static int
_on_comment(struct pchvml_vdom_parser *parser, struct pchvml_token *token)
{
    UNUSED_PARAM(parser);
    UNUSED_PARAM(token);
    return -1;
}

static int
_on_character(struct pchvml_vdom_parser *parser, struct pchvml_token *token)
{
    UNUSED_PARAM(parser);
    UNUSED_PARAM(token);
    return -1;
}

static int
_on_vcm(struct pchvml_vdom_parser *parser, struct pchvml_token *token)
{
    UNUSED_PARAM(parser);
    UNUSED_PARAM(token);
    return -1;
}

int
pchvml_vdom_parser_parse(struct pchvml_vdom_parser *parser,
        purc_rwstream_t in)
{
    if (!parser || !in) {
        pcinst_set_error(PURC_ERROR_INVALID_VALUE);
        return -1;
    }

    if (!parser->tokenizer) {
        pcinst_set_error(PURC_ERROR_NOT_EXISTS);
        return-1;
    }

    PC_ASSERT(parser->eof == 0);

    while (1) {
        struct pchvml_token *token = pchvml_vdom_next_token(parser->tokenizer,
            in);
        if (!token) {
            // check error code
            // feof(in) ?
            return 0;
        }
        switch (token->type) {
            case VTT(DOCTYPE):
            {
                return _on_doctype(parser, token);
            } break;
            case VTT(START_TAG):
            {
                return _on_start_tag(parser, token);
            } break;
            case VTT(END_TAG):
            {
                return _on_end_tag(parser, token);
            } break;
            case VTT(COMMENT):
            {
                return _on_comment(parser, token);
            } break;
            case VTT(CHARACTER):
            {
                return _on_character(parser, token);
            } break;
            case VTT(VCM_TREE):
            {
                return _on_vcm(parser, token);
            } break;
            case VTT(EOF):
            {
                parser->eof = 1; 
            } break;
            default:
            {
                PC_ASSERT(0);
            } break;
        }
    }

    return 0;
}

int
pchvml_vdom_parser_parse_fragment(struct pchvml_vdom_parser *parser,
        struct pcvdom_node *node, purc_rwstream_t in);

int
pchvml_vdom_parser_end(struct pchvml_vdom_parser *parser)
{
    PC_ASSERT(parser);

    if (!parser->doc) {
        // not iniialized yet
        pcinst_set_error(PURC_ERROR_NULL_OBJECT);
        return -1;
    }

    if (parser->curr != &parser->doc->root->node) {
        // not full closed content
        pcinst_set_error(PURC_ERROR_NULL_OBJECT);
        return -1;
    }

    parser->curr = NULL;

    return 0;
}

struct pcvdom_document*
pchvml_vdom_parser_reset(struct pchvml_vdom_parser *parser)
{
    PC_ASSERT(parser);
    struct pcvdom_document *doc = parser->doc;
    parser->doc  = NULL;
    parser->curr = NULL;

    return doc;
}

void
pchvml_vdom_parser_destroy(struct pchvml_vdom_parser *parser)
{
    if (!parser)
        return;

    _vdom_parser_destroy(parser);
}

