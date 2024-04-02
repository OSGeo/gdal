/******************************************************************************
 *
 * Purpose:  Bulk loading of SQLite R*Tree tables
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2020- 2023 Joshua J Baker
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "sqlite_rtree_bulk_load.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(__cplusplus) && !defined(DISABLE_CPLUSPLUS)
#define USE_CPLUSPLUS
#endif

#ifdef USE_CPLUSPLUS
#include <algorithm>
#else
#include <stdlib.h>
#endif

#ifdef __cplusplus
#define STATIC_CAST(type, value) static_cast<type>(value)
#ifdef NULL
#undef NULL
#endif
#define NULL nullptr
#undef SQLITE_STATIC
#define SQLITE_STATIC STATIC_CAST(sqlite3_destructor_type, nullptr)
#else
#define STATIC_CAST(type, value) (type)(value)
#endif

////////////////////////////////

#define DATATYPE int64_t
#define DIMS 2
#define NUMTYPE float

// Cf https://github.com/sqlite/sqlite/blob/90e4a3b7fcdf63035d6f35eb44d11ff58ff4b068/ext/rtree/rtree.c#L262
#define MAXITEMS 51

#define BYTES_PER_CELL STATIC_CAST(int, sizeof(int64_t) + 4 * sizeof(float))

//#ifndef RTREE_NOPATHHINT
//#define USE_PATHHINT
//#endif

enum kind {
    LEAF = 1,
    BRANCH = 2,
};

struct rect {
    NUMTYPE min[DIMS];
    NUMTYPE max[DIMS];
};

struct item {
    DATATYPE data;
};

struct node {
    enum kind kind;     // LEAF or BRANCH
    int count;          // number of rects
    struct rect rects[MAXITEMS];
    union {
        struct node *nodes[MAXITEMS];
        struct item datas[MAXITEMS];
    };
};

struct node_MAXITEMS_plus_ONE {
    enum kind kind;     // LEAF or BRANCH
    int count;          // number of rects
    struct rect rects[MAXITEMS+1];
    union {
        struct node *nodes[MAXITEMS+1];
        struct item datas[MAXITEMS+1];
    };
};

struct sqlite_rtree_bl {
    struct rect rect;
    struct node *root;
    size_t count;
    size_t mem_usage;
    int height;
    int node_size;
    int node_capacity;
#ifdef USE_PATHHINT
    int path_hint[16];
#endif
    void *(*malloc)(size_t);
    void (*free)(void *);
};

#if defined(__GNUC__) && __GNUC__ >= 4
#define CPL_UNUSED __attribute((__unused__))
#else
#define CPL_UNUSED
#endif

static void CPL_IGNORE_RET_VAL_INT(CPL_UNUSED int v) {
    (void)v;
}

static inline NUMTYPE min0(NUMTYPE x, NUMTYPE y) {
    return x < y ? x : y;
}

static inline NUMTYPE max0(NUMTYPE x, NUMTYPE y) {
    return x > y ? x : y;
}

static struct node *node_new(struct sqlite_rtree_bl *tr, enum kind kind) {
    struct node *node = STATIC_CAST(struct node *, tr->malloc(sizeof(struct node)));
    if (!node) {
        return NULL;
    }
    memset(node, 0, sizeof(struct node));
    node->kind = kind;
    tr->mem_usage += sizeof(struct node);
    return node;
}

static void node_free(struct sqlite_rtree_bl *tr, struct node *node) {
    if (node->kind == BRANCH) {
        for (int i = 0; i < node->count; i++) {
            node_free(tr, node->nodes[i]);
        }
    }
    tr->mem_usage -= sizeof(struct node);
    tr->free(node);
}

static void rect_expand(struct rect *rect, const struct rect *other) {
    for (int i = 0; i < DIMS; i++) {
        rect->min[i] = min0(rect->min[i], other->min[i]);
        rect->max[i] = max0(rect->max[i], other->max[i]);
    }
}

static double rect_area(const struct rect *rect) {
    double result = 1;
    for (int i = 0; i < DIMS; i++) {
        result *= STATIC_CAST(double, rect->max[i]) - rect->min[i];
    }
    return result;
}

// return the area of two rects expanded
static double rect_unioned_area(const struct rect *rect,
                                const struct rect *other) {
    double result = 1;
    for (int i = 0; i < DIMS; i++) {
        result *= STATIC_CAST(double, max0(rect->max[i], other->max[i])) -
                  min0(rect->min[i], other->min[i]);
    }
    return result;
}

static bool rect_contains(const struct rect *rect, const struct rect *other) {
    for (int i = 0; i < DIMS; i++) {
        if (other->min[i] < rect->min[i])
            return false;
        if (other->max[i] > rect->max[i])
            return false;
    }
    return true;
}

static double rect_margin(const struct rect *rect) {
    return (STATIC_CAST(double, rect->max[0]) - rect->min[0]) +
           (STATIC_CAST(double, rect->max[1]) - rect->min[1]);
}

static double rect_overlap(const struct rect *rect1, const struct rect *rect2) {
   double overlap = 1;
   for (int idim = 0; idim < DIMS; ++idim)
   {
       double minv = max0(rect1->min[idim], rect2->min[idim]);
       double maxv = min0(rect1->max[idim], rect2->max[idim]);
       if( maxv < minv ) {
           return 0;
       }
       overlap *= (maxv - minv);
   }
   return overlap;
}

typedef struct SortType
{
    int i;
#ifndef USE_CPLUSPLUS
    struct rect* rects;
#endif
} SortType;

#ifndef USE_CPLUSPLUS
static int CompareAxis0(const void *a, const void *b)
{
    const SortType* sa = STATIC_CAST(const SortType*, a);
    const SortType* sb = STATIC_CAST(const SortType*, b);
    if (sa->rects[sa->i].min[0] < sb->rects[sb->i].min[0])
        return -1;
    if (sa->rects[sa->i].min[0] == sb->rects[sb->i].min[0])
    {
        if (sa->rects[sa->i].max[0] < sb->rects[sb->i].max[0])
            return -1;
        if (sa->rects[sa->i].max[0] == sb->rects[sb->i].max[0])
            return 0;
        return 1;
    }
    return 1;
}

static int CompareAxis1(const void *a, const void *b)
{
    const SortType* sa = STATIC_CAST(const SortType*, a);
    const SortType* sb = STATIC_CAST(const SortType*, b);
    if (sa->rects[sa->i].min[1] < sb->rects[sb->i].min[1])
        return -1;
    if (sa->rects[sa->i].min[1] == sb->rects[sb->i].min[1])
    {
        if (sa->rects[sa->i].max[1] < sb->rects[sb->i].max[1])
            return -1;
        if (sa->rects[sa->i].max[1] == sb->rects[sb->i].max[1])
            return 0;
        return 1;
    }
    return 1;
}
#endif

// Implementation of the R*-tree variant of SplitNode from Beckman[1990].
// Cf https://github.com/sqlite/sqlite/blob/5f53f85e22df1c5e1e36106b5e4d1db5089519aa/ext/rtree/rtree.c#L2418
static bool node_split_rstartree(struct sqlite_rtree_bl *tr,
                                 struct node *node,
                                 const struct rect *extraRect,
                                 struct item extraData,
                                 struct node *extraNode,
                                 struct node **right_out) {
    struct node_MAXITEMS_plus_ONE nodeOri;
    nodeOri.kind = node->kind;
    nodeOri.count = node->count;
    memcpy(nodeOri.rects, node->rects, node->count * sizeof(struct rect));
    nodeOri.rects[node->count] = *extraRect;
    if (nodeOri.kind == LEAF) {
        memcpy(nodeOri.datas, node->datas, node->count * sizeof(struct item));
        nodeOri.datas[nodeOri.count] = extraData;
    } else {
        memcpy(nodeOri.nodes, node->nodes, node->count * sizeof(struct node*));
        nodeOri.nodes[nodeOri.count] = extraNode;
    }
    nodeOri.count ++;
    assert(nodeOri.count == tr->node_capacity + 1);

    SortType aSorted[DIMS][MAXITEMS+1];
    for (int idim = 0; idim < DIMS; ++idim) {
        for (int i = 0; i < tr->node_capacity + 1; ++i) {
            aSorted[idim][i].i = i;
#ifndef USE_CPLUSPLUS
            aSorted[idim][i].rects = nodeOri.rects;
#endif
        }
    }

#ifndef USE_CPLUSPLUS
    qsort(aSorted[0], nodeOri.count, sizeof(SortType), CompareAxis0);
    qsort(aSorted[1], nodeOri.count, sizeof(SortType), CompareAxis1);
#else
    std::sort(aSorted[0], aSorted[0] + nodeOri.count, [&nodeOri](const SortType& a, const SortType& b) {
        return nodeOri.rects[a.i].min[0] < nodeOri.rects[b.i].min[0] ||
               (nodeOri.rects[a.i].min[0] == nodeOri.rects[b.i].min[0] &&
                nodeOri.rects[a.i].max[0] < nodeOri.rects[b.i].max[0]);
    });
    std::sort(aSorted[1], aSorted[1] + nodeOri.count, [&nodeOri](const SortType& a, const SortType& b) {
        return nodeOri.rects[a.i].min[1] < nodeOri.rects[b.i].min[1] ||
               (nodeOri.rects[a.i].min[1] == nodeOri.rects[b.i].min[1] &&
                nodeOri.rects[a.i].max[1] < nodeOri.rects[b.i].max[1]);
    });
#endif

    int iBestDim = 0;
    int iBestSplit = tr->node_capacity / 2;
    double fBestMargin = INFINITY;
    for (int idim = 0; idim < DIMS; ++idim) {
        double margin = 0;
        double fBestOverlap = INFINITY;
        double fBestArea = INFINITY;
        int iBestLeft = 0;
        const int minItems = tr->node_capacity / 3;
        for (int nLeft = minItems; nLeft <= nodeOri.count - minItems; ++nLeft) {
            struct rect rectLeft = nodeOri.rects[aSorted[idim][0].i];
            struct rect rectRight = nodeOri.rects[aSorted[idim][nodeOri.count-1].i];
            for(int kk=1; kk<(nodeOri.count-1); kk++) {
                if( kk<nLeft ){
                  rect_expand(&rectLeft, &nodeOri.rects[aSorted[idim][kk].i]);
                }else{
                  rect_expand(&rectRight, &nodeOri.rects[aSorted[idim][kk].i]);
                }
            }
            margin += rect_margin(&rectLeft);
            margin += rect_margin(&rectRight);
            double overlap = rect_overlap(&rectLeft, &rectRight);
            double area = rect_area(&rectLeft) + rect_area(&rectRight);
            if( overlap<fBestOverlap || (overlap==fBestOverlap && area<fBestArea)) {
                iBestLeft = nLeft;
                fBestOverlap = overlap;
                fBestArea = area;
            }
        }

        if (margin<fBestMargin) {
          iBestDim = idim;
          fBestMargin = margin;
          iBestSplit = iBestLeft;
        }
    }

    struct node *right = node_new(tr, node->kind);
    if (!right) {
        return false;
    }
    node->count = 0;
    int i = 0;
    for (; i < iBestSplit; ++i) {
        struct node* target = node;
        target->rects[target->count] = nodeOri.rects[aSorted[iBestDim][i].i];
        if (nodeOri.kind == LEAF) {
            target->datas[target->count] = nodeOri.datas[aSorted[iBestDim][i].i];
        } else {
            target->nodes[target->count] = nodeOri.nodes[aSorted[iBestDim][i].i];
        }
        ++target->count;
    }
    for (; i < nodeOri.count; ++i) {
        struct node* target = right;
        target->rects[target->count] = nodeOri.rects[aSorted[iBestDim][i].i];
        if (nodeOri.kind == LEAF) {
            target->datas[target->count] = nodeOri.datas[aSorted[iBestDim][i].i];
        } else {
            target->nodes[target->count] = nodeOri.nodes[aSorted[iBestDim][i].i];
        }
        ++target->count;
    }
    *right_out = right;
    return true;
}

static int node_choose_least_enlargement(const struct node *node,
                                         const struct rect *ir) {
    int j = 0;
    double jenlarge = INFINITY;
    double minarea = 0;
    for (int i = 0; i < node->count; i++) {
        // calculate the enlarged area
        double uarea = rect_unioned_area(&node->rects[i], ir);
        double area = rect_area(&node->rects[i]);
        double enlarge = uarea - area;
        if (enlarge < jenlarge || (enlarge == jenlarge && area < minarea)) {
            j = i;
            jenlarge = enlarge;
            minarea = area;
        }
    }
    return j;
}

static int node_choose(struct sqlite_rtree_bl * tr,
                       const struct node *node,
                       const struct rect *rect,
                       int depth)
{
#ifndef USE_PATHHINT
    (void)tr;
    (void)depth;
#endif

#ifdef USE_PATHHINT
    int h = tr->path_hint[depth];
    if (h < node->count) {
        if (rect_contains(&node->rects[h], rect)) {
            return h;
        }
    }
#endif

    // Take a quick look for the first node that contain the rect.
    int iBestIndex = -1;
    double minArea = INFINITY;
    for (int i = 0; i < node->count; i++) {
        if (rect_contains(&node->rects[i], rect)) {
            double area = rect_area(&node->rects[i]);
            if( area < minArea )
            {
                iBestIndex = i;
                minArea = area;
            }
        }
    }
    if (iBestIndex >= 0) {
#ifdef USE_PATHHINT
        tr->path_hint[depth] = iBestIndex;
#endif
        return iBestIndex;
    }

    // Fallback to using che "choose least enlargement" algorithm.
    int i = node_choose_least_enlargement(node, rect);
#ifdef USE_PATHHINT
    tr->path_hint[depth] = i;
#endif
    return i;
}

static struct rect node_rect_calc(const struct node *node) {
    struct rect rect = node->rects[0];
    for (int i = 1; i < node->count; i++) {
        rect_expand(&rect, &node->rects[i]);
    }
    return rect;
}

// node_insert returns false if out of memory
static bool node_insert(struct sqlite_rtree_bl *tr,
                        struct node *node,
                        const struct rect *ir,
                        struct item item,
                        int depth,
                        bool *split,
                        struct rect *rectToInsert,
                        struct item *itemToInsert,
                        struct node** nodeToInsert)
{
    if (node->kind == LEAF) {
        if (node->count == tr->node_capacity) {
            *split = true;
            *rectToInsert = *ir;
            *itemToInsert = item;
            *nodeToInsert = NULL;
            return true;
        }
        int index = node->count;
        node->rects[index] = *ir;
        node->datas[index] = item;
        node->count++;
        *split = false;
        return true;
    }
    // Choose a subtree for inserting the rectangle.
    const int i = node_choose(tr, node, ir, depth);
    if (!node_insert(tr, node->nodes[i], ir, item, depth+1,
                     split, rectToInsert, itemToInsert, nodeToInsert))
    {
        return false;
    }
    if (!*split) {
        rect_expand(&node->rects[i], ir);
        *split = false;
        return true;
    }

    struct node *right;
    if (!node_split_rstartree(tr, node->nodes[i], rectToInsert,
                              *itemToInsert, *nodeToInsert, &right)) {
        return false;
    }
    node->rects[i] = node_rect_calc(node->nodes[i]);

    // split the child node
    if (node->count == tr->node_capacity) {
        *split = true;
        *rectToInsert = node_rect_calc(right);
        *nodeToInsert = right;
        itemToInsert->data = -1;
        return true;
    }

    *split = false;
    node->rects[node->count] = node_rect_calc(right);
    node->nodes[node->count] = right;
    node->count++;
    return true;
}

static
struct sqlite_rtree_bl *sqlite_rtree_bl_new_with_allocator(int sqlite_page_size,
                                              void *(*pfnMalloc)(size_t),
                                              void (*pfnFree)(void*)) {
    if (!pfnMalloc)
        pfnMalloc = malloc;
    if (!pfnFree)
        pfnFree = free;
    struct sqlite_rtree_bl *tr = STATIC_CAST(struct sqlite_rtree_bl *, pfnMalloc(sizeof(struct sqlite_rtree_bl)));
    if (!tr) return NULL;
    memset(tr, 0, sizeof(struct sqlite_rtree_bl));
    tr->malloc = pfnMalloc;
    tr->free = pfnFree;

    // Cf https://github.com/sqlite/sqlite/blob/90e4a3b7fcdf63035d6f35eb44d11ff58ff4b068/ext/rtree/rtree.c#L3541
    tr->node_size = sqlite_page_size-64;
    if( tr->node_size > 4+BYTES_PER_CELL*MAXITEMS ){
        tr->node_size = 4+BYTES_PER_CELL*MAXITEMS;
    }
    tr->node_capacity = (tr->node_size-4)/BYTES_PER_CELL;
    tr->mem_usage = sizeof(struct sqlite_rtree_bl);

    return tr;
}

struct sqlite_rtree_bl *SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_new)(int sqlite_page_size) {
    return sqlite_rtree_bl_new_with_allocator(sqlite_page_size, NULL, NULL);
}

// Cf https://github.com/sqlite/sqlite/blob/90e4a3b7fcdf63035d6f35eb44d11ff58ff4b068/ext/rtree/rtree.c#L2993C1-L2995C3
/*
** Rounding constants for float->double conversion.
*/
#define RNDTOWARDS  (1.0 - 1.0/8388608.0)  /* Round towards zero */
#define RNDAWAY     (1.0 + 1.0/8388608.0)  /* Round away from zero */

/*
** Convert an sqlite3_value into an RtreeValue (presumably a float)
** while taking care to round toward negative or positive, respectively.
*/
static float rtreeValueDown(double d){
  float f = STATIC_CAST(float, d);
  if( f>d ){
    f = STATIC_CAST(float, d*(d<0 ? RNDAWAY : RNDTOWARDS));
  }
  return f;
}
static float rtreeValueUp(double d){
  float f = STATIC_CAST(float, d);
  if( f<d ){
    f = STATIC_CAST(float, d*(d<0 ? RNDTOWARDS : RNDAWAY));
  }
  return f;
}

bool SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_insert)(struct sqlite_rtree_bl *tr,
                         int64_t fid,
                         double minx, double miny,
                         double maxx, double maxy)
{
    if( !(minx <= maxx) || !(miny <= maxy) )
        return false;

    // copy input rect
    struct rect rect;
    rect.min[0] = rtreeValueDown(minx);
    rect.min[1] = rtreeValueDown(miny);
    rect.max[0] = rtreeValueUp(maxx);
    rect.max[1] = rtreeValueUp(maxy);

    // copy input data
    struct item item;
    item.data = fid;

    struct rect rectToInsert;
    struct item itemToInsert;
    struct node* nodeToInsert;

    if (!tr->root) {
        struct node *new_root = node_new(tr, LEAF);
        if (!new_root) {
            return false;
        }
        tr->root = new_root;
        tr->rect = rect;
        tr->height = 1;
    }
    bool split = false;
    if (!node_insert(tr, tr->root, &rect, item, 0,
                     &split, &rectToInsert, &itemToInsert, &nodeToInsert)) {
        return false;
    }
    if (!split) {
        rect_expand(&tr->rect, &rect);
        tr->count++;
        return true;
    }
    struct node *new_root = node_new(tr, BRANCH);
    if (!new_root) {
        return false;
    }
    struct node *right;
    if (!node_split_rstartree(tr, tr->root, &rectToInsert, itemToInsert, nodeToInsert, &right)) {
        tr->free(new_root);
        return false;
    }
    new_root->rects[0] = node_rect_calc(tr->root);
    new_root->rects[1] = node_rect_calc(right);
    new_root->nodes[0] = tr->root;
    new_root->nodes[1] = right;
    tr->root = new_root;
    tr->root->count = 2;
    tr->height++;

    return true;
}

size_t SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_ram_usage)(const sqlite_rtree_bl* tr) {
    return tr->mem_usage;
}

void SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_free)(struct sqlite_rtree_bl *tr) {
    if (tr) {
        if (tr->root) {
            node_free(tr, tr->root);
        }
        tr->free(tr);
    }
}

static void write_be_uint16(uint8_t* dest, uint16_t n) {
    dest[0] = STATIC_CAST(uint8_t, n >> 8);
    dest[1] = STATIC_CAST(uint8_t, n);
}

static void write_be_int64(uint8_t* dest, int64_t i) {
    const uint64_t n = i;
    dest[0] = STATIC_CAST(uint8_t, n >> 56);
    dest[1] = STATIC_CAST(uint8_t, n >> 48);
    dest[2] = STATIC_CAST(uint8_t, n >> 40);
    dest[3] = STATIC_CAST(uint8_t, n >> 32);
    dest[4] = STATIC_CAST(uint8_t, n >> 24);
    dest[5] = STATIC_CAST(uint8_t, n >> 16);
    dest[6] = STATIC_CAST(uint8_t, n >> 8);
    dest[7] = STATIC_CAST(uint8_t, n);
}

static void write_be_float(uint8_t* dest, float f) {
    uint32_t n;
    memcpy(&n, &f, sizeof(float));
    dest[0] = STATIC_CAST(uint8_t, n >> 24);
    dest[1] = STATIC_CAST(uint8_t, n >> 16);
    dest[2] = STATIC_CAST(uint8_t, n >> 8);
    dest[3] = STATIC_CAST(uint8_t, n);
}

static char* my_sqlite3_strdup(const char* s) {
    if( !s )
        return NULL;
    const int n = STATIC_CAST(int, strlen(s));
    char* new_s = STATIC_CAST(char*, sqlite3_malloc(n+1));
    memcpy(new_s, s, n+1);
    return new_s;
}

typedef struct rtree_insert_context
{
    sqlite3* hDB;
    sqlite3_stmt* hStmtNode;
    sqlite3_stmt* hStmtParent;
    sqlite3_stmt* hStmtRowid;
    int node_capacity;
    int tree_height;
    char** p_error_msg;
} rtree_insert_context;

typedef enum
{
    PASS_ALL,
    PASS_NODE,
    PASS_PARENT,
    PASS_ROWID,
} PassType;

static bool insert_into_db(const struct rtree_insert_context* ctxt,
                           const struct node* node,
                           int64_t* p_cur_nodeno,
                           int64_t parent_nodeno,
                           PassType pass) {
    const int64_t this_cur_nodeno = (*p_cur_nodeno);
    uint8_t blob[4 + MAXITEMS * BYTES_PER_CELL] = {0};
    size_t offset = 4;

    if (node->kind == BRANCH) {
        for (int i = 0; i < node->count; ++i) {
            ++(*p_cur_nodeno);

            if (pass == PASS_ALL || pass == PASS_NODE) {
                write_be_int64(blob + offset, (*p_cur_nodeno));
                offset += sizeof(int64_t);

                const float minx = node->rects[i].min[0];
                write_be_float(blob + offset, minx);
                offset += sizeof(float);

                const float maxx = node->rects[i].max[0];
                write_be_float(blob + offset, maxx);
                offset += sizeof(float);

                const float miny = node->rects[i].min[1];
                write_be_float(blob + offset, miny);
                offset += sizeof(float);

                const float maxy = node->rects[i].max[1];
                write_be_float(blob + offset, maxy);
                offset += sizeof(float);
            }

            if (!insert_into_db(ctxt, node->nodes[i], p_cur_nodeno,
                                this_cur_nodeno, pass)) {
                return false;
            }
        }
    }
    else if (pass == PASS_ALL || pass == PASS_NODE || pass == PASS_ROWID) {
        for (int i = 0; i < node->count; ++i ) {
            const int64_t fid = node->datas[i].data;
            if( pass == PASS_ALL || pass == PASS_NODE )
            {
                write_be_int64(blob + offset, fid);
                offset += sizeof(int64_t);

                const float minx = node->rects[i].min[0];
                write_be_float(blob + offset, minx);
                offset += sizeof(float);

                const float maxx = node->rects[i].max[0];
                write_be_float(blob + offset, maxx);
                offset += sizeof(float);

                const float miny = node->rects[i].min[1];
                write_be_float(blob + offset, miny);
                offset += sizeof(float);

                const float maxy = node->rects[i].max[1];
                write_be_float(blob + offset, maxy);
                offset += sizeof(float);
            }

            if (pass == PASS_ALL || pass == PASS_ROWID) {
                sqlite3_reset(ctxt->hStmtRowid);
                sqlite3_bind_int64(ctxt->hStmtRowid, 1, fid);
                sqlite3_bind_int64(ctxt->hStmtRowid, 2, this_cur_nodeno);
                int ret = sqlite3_step(ctxt->hStmtRowid);
                if (ret != SQLITE_OK && ret != SQLITE_DONE) {
                    if (ctxt->p_error_msg) {
                        *ctxt->p_error_msg = my_sqlite3_strdup(sqlite3_errmsg(ctxt->hDB));
                    }
                    return false;
                }
            }
        }
    }

    if (pass == PASS_ALL || pass == PASS_NODE) {
        write_be_uint16(blob, STATIC_CAST(uint16_t, parent_nodeno == 0 ? ctxt->tree_height - 1 : 0));
        write_be_uint16(blob + 2, STATIC_CAST(uint16_t, node->count));

        sqlite3_reset(ctxt->hStmtNode);
        sqlite3_bind_int64(ctxt->hStmtNode, 1, this_cur_nodeno);
        sqlite3_bind_blob(ctxt->hStmtNode, 2, blob,
                          4 + ctxt->node_capacity * BYTES_PER_CELL,
                          SQLITE_STATIC);
        int ret = sqlite3_step(ctxt->hStmtNode);
        if (ret != SQLITE_OK && ret != SQLITE_DONE) {
            if (ctxt->p_error_msg) {
                *ctxt->p_error_msg = my_sqlite3_strdup(sqlite3_errmsg(ctxt->hDB));
            }
            return false;
        }
    }

    if ((pass == PASS_ALL || pass == PASS_PARENT) && parent_nodeno > 0) {
        sqlite3_reset(ctxt->hStmtParent);
        sqlite3_bind_int64(ctxt->hStmtParent, 1, this_cur_nodeno);
        sqlite3_bind_int64(ctxt->hStmtParent, 2, parent_nodeno);
        int ret = sqlite3_step(ctxt->hStmtParent);
        if (ret != SQLITE_OK && ret != SQLITE_DONE) {
            if (ctxt->p_error_msg) {
                *ctxt->p_error_msg = my_sqlite3_strdup(sqlite3_errmsg(ctxt->hDB));
            }
            return false;
        }
    }

    return true;
}

static bool IsLowercaseAlpha(const char* s)
{
    for (; *s != 0; ++s) {
        if (!(*s >= 'a' && *s <= 'z')) {
            return false;
        }
    }
    return true;
}

bool SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_serialize)(
                               const struct sqlite_rtree_bl *tr,
                               sqlite3* hDB,
                               const char* rtree_name,
                               const char* rowid_colname,
                               const char* minx_colname,
                               const char* miny_colname,
                               const char* maxx_colname,
                               const char* maxy_colname,
                               char** p_error_msg) {
    if (p_error_msg) {
        *p_error_msg = NULL;
    }

    char* sql;
    if (IsLowercaseAlpha(rowid_colname) &&
        IsLowercaseAlpha(minx_colname) &&
        IsLowercaseAlpha(maxx_colname) &&
        IsLowercaseAlpha(miny_colname) &&
        IsLowercaseAlpha(maxy_colname)) {
        /* To make OGC GeoPackage compliance test happy... */
        sql = sqlite3_mprintf(
            "CREATE VIRTUAL TABLE \"%w\" USING rtree(%s, %s, %s, %s, %s)",
            rtree_name, rowid_colname, minx_colname, maxx_colname, miny_colname,
            maxy_colname);
    }
    else {
        sql = sqlite3_mprintf(
            "CREATE VIRTUAL TABLE \"%w\" USING rtree(\"%w\", \"%w\", \"%w\", \"%w\", \"%w\")",
            rtree_name, rowid_colname, minx_colname, maxx_colname, miny_colname,
            maxy_colname);
    }
    int ret = sqlite3_exec(hDB, sql, NULL, NULL, p_error_msg);
    sqlite3_free(sql);
    if (ret != SQLITE_OK) {
        return false;
    }

    if (tr->count == 0) {
        return true;
    }

    // Suppress default root node
    sql = sqlite3_mprintf("DELETE FROM \"%w_node\"", rtree_name);
    ret = sqlite3_exec(hDB, sql, NULL, NULL, p_error_msg);
    sqlite3_free(sql);
    if (ret != SQLITE_OK) {
        return false;
    }

    sqlite3_stmt *hStmtNode = NULL;
    sql = sqlite3_mprintf("INSERT INTO \"%w_node\" VALUES (?, ?)", rtree_name);
    CPL_IGNORE_RET_VAL_INT(sqlite3_prepare_v2(hDB, sql, -1, &hStmtNode, NULL));
    sqlite3_free(sql);
    if (!hStmtNode) {
        return false;
    }

    sqlite3_stmt *hStmtParent = NULL;
    sql = sqlite3_mprintf("INSERT INTO \"%w_parent\" VALUES (?, ?)", rtree_name);
    CPL_IGNORE_RET_VAL_INT(sqlite3_prepare_v2(hDB, sql, -1, &hStmtParent, NULL));
    sqlite3_free(sql);
    if (!hStmtParent) {
        sqlite3_finalize(hStmtNode);
        return false;
    }

    sqlite3_stmt *hStmtRowid = NULL;
    sql = sqlite3_mprintf("INSERT INTO \"%w_rowid\" VALUES (?, ?)", rtree_name);
    CPL_IGNORE_RET_VAL_INT(sqlite3_prepare_v2(hDB, sql, -1, &hStmtRowid, NULL));
    sqlite3_free(sql);
    if (!hStmtRowid) {
        sqlite3_finalize(hStmtNode);
        sqlite3_finalize(hStmtParent);
        return false;
    }

    rtree_insert_context ctxt;
    ctxt.hDB = hDB;
    ctxt.hStmtNode = hStmtNode;
    ctxt.hStmtParent = hStmtParent;
    ctxt.hStmtRowid = hStmtRowid;
    ctxt.node_capacity = tr->node_capacity;
    ctxt.tree_height = tr->height;
    ctxt.p_error_msg = p_error_msg;

    int64_t cur_nodeno = 1;
    bool ok = insert_into_db(&ctxt, tr->root, &cur_nodeno, 0, PASS_NODE);
    if (ok) {
        cur_nodeno = 1;
        ok = insert_into_db(&ctxt, tr->root, &cur_nodeno, 0, PASS_PARENT);
    }
    if (ok) {
        cur_nodeno = 1;
        ok = insert_into_db(&ctxt, tr->root, &cur_nodeno, 0, PASS_ROWID);
    }

    sqlite3_finalize(hStmtNode);
    sqlite3_finalize(hStmtParent);
    sqlite3_finalize(hStmtRowid);
    return ok;
}

#define NOTIFICATION_INTERVAL (500 * 1000)

bool SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_from_feature_table)(
                               sqlite3* hDB,
                               const char* feature_table_name,
                               const char* feature_table_fid_colname,
                               const char* feature_table_geom_colname,
                               const char* rtree_name,
                               const char* rowid_colname,
                               const char* minx_colname,
                               const char* miny_colname,
                               const char* maxx_colname,
                               const char* maxy_colname,
                               size_t max_ram_usage,
                               char** p_error_msg,
                               sqlite_rtree_progress_callback progress_cbk,
                               void* progress_cbk_user_data)
{
    char** papszResult = NULL;
    sqlite3_get_table(hDB, "PRAGMA page_size", &papszResult, NULL, NULL, NULL);
    const int page_size = atoi(papszResult[1]);
    sqlite3_free_table(papszResult);

    struct sqlite_rtree_bl* t = SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_new)(page_size);
    if (!t) {
        if (p_error_msg)
            *p_error_msg = my_sqlite3_strdup("not enough memory");
        return false;
    }

    sqlite3_stmt *hStmt = NULL;
    char *pszSQL =
            sqlite3_mprintf("SELECT \"%w\", ST_MinX(\"%w\"), ST_MaxX(\"%w\"), "
                            "ST_MinY(\"%w\"), ST_MaxY(\"%w\") FROM \"%w\" "
                            "WHERE \"%w\" NOT NULL AND NOT ST_IsEmpty(\"%w\")",
                            feature_table_fid_colname,
                            feature_table_geom_colname,
                            feature_table_geom_colname,
                            feature_table_geom_colname,
                            feature_table_geom_colname,
                            feature_table_name,
                            feature_table_geom_colname,
                            feature_table_geom_colname);
    CPL_IGNORE_RET_VAL_INT(sqlite3_prepare_v2(hDB, pszSQL, -1, &hStmt, NULL));
    sqlite3_free(pszSQL);
    if (!hStmt) {
        if (p_error_msg)
            *p_error_msg = my_sqlite3_strdup(sqlite3_errmsg(hDB));
        SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_free)(t);
        return false;
    }

    bool bMaxMemReached = false;
    uint64_t nRows = 0;
    while (sqlite3_step(hStmt) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(hStmt, 0);
        const double minx = sqlite3_column_double(hStmt, 1);
        const double maxx = sqlite3_column_double(hStmt, 2);
        const double miny = sqlite3_column_double(hStmt, 3);
        const double maxy = sqlite3_column_double(hStmt, 4);
        if (!SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_insert)(t, id, minx, miny, maxx, maxy)) {
            bMaxMemReached = true;
            break;
        }
        if (max_ram_usage != 0 &&
            SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_ram_usage)(t) > max_ram_usage) {
            bMaxMemReached = true;
            break;
        }
        if (progress_cbk && ((++nRows) % NOTIFICATION_INTERVAL) == 0) {
            char szMsg[256];
            snprintf(szMsg, sizeof(szMsg),
                     "%" PRIu64 " rows inserted in %s (in RAM)",
                     nRows, rtree_name);
            if (!progress_cbk(szMsg, progress_cbk_user_data)) {
                SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_free)(t);
                sqlite3_finalize(hStmt);
                if (p_error_msg)
                    *p_error_msg = my_sqlite3_strdup("Processing interrupted");
                return false;
            }
        }
    }

    bool bOK = SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_serialize)(
                               t, hDB,
                               rtree_name,
                               rowid_colname,
                               minx_colname,
                               miny_colname,
                               maxx_colname,
                               maxy_colname,
                               p_error_msg);

    SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_free)(t);

    if (bOK && bMaxMemReached) {
        if (progress_cbk) {
            CPL_IGNORE_RET_VAL_INT(progress_cbk(
                "Max RAM reached. Falling back to slower "
                "insertion method", progress_cbk_user_data));
        }

        sqlite3_stmt *hStmtInsert = NULL;
        pszSQL =
                sqlite3_mprintf("INSERT INTO \"%w\" VALUES (?,?,?,?,?)",
                                rtree_name);
        CPL_IGNORE_RET_VAL_INT(sqlite3_prepare_v2(hDB, pszSQL, -1, &hStmtInsert, NULL));
        sqlite3_free(pszSQL);
        if (!hStmtInsert) {
            if (p_error_msg)
                *p_error_msg = my_sqlite3_strdup(sqlite3_errmsg(hDB));
            sqlite3_finalize(hStmt);
            return false;
        }
        while (sqlite3_step(hStmt) == SQLITE_ROW) {
            int64_t id = sqlite3_column_int64(hStmt, 0);
            const double minx = sqlite3_column_double(hStmt, 1);
            const double maxx = sqlite3_column_double(hStmt, 2);
            const double miny = sqlite3_column_double(hStmt, 3);
            const double maxy = sqlite3_column_double(hStmt, 4);

            sqlite3_reset(hStmtInsert);
            sqlite3_bind_int64(hStmtInsert, 1, id);
            sqlite3_bind_double(hStmtInsert, 2, minx);
            sqlite3_bind_double(hStmtInsert, 3, maxx);
            sqlite3_bind_double(hStmtInsert, 4, miny);
            sqlite3_bind_double(hStmtInsert, 5, maxy);
            int ret = sqlite3_step(hStmtInsert);
            if (ret != SQLITE_OK && ret != SQLITE_DONE) {
                if (p_error_msg)
                    *p_error_msg = my_sqlite3_strdup(sqlite3_errmsg(hDB));
                bOK = false;
                break;
            }
            if (progress_cbk && ((++nRows) % NOTIFICATION_INTERVAL) == 0) {
                char szMsg[256];
                snprintf(szMsg, sizeof(szMsg),
                         "%" PRIu64 " rows inserted in %s", nRows, rtree_name);
                if (!progress_cbk(szMsg, progress_cbk_user_data)) {
                    bOK = false;
                    if (p_error_msg)
                        *p_error_msg = my_sqlite3_strdup("Processing interrupted");
                    break;
                }
            }
        }
        sqlite3_finalize(hStmtInsert);
    }

    if (bOK && progress_cbk && (nRows % NOTIFICATION_INTERVAL) != 0)
    {
        char szMsg[256];
        snprintf(szMsg, sizeof(szMsg), "%" PRIu64 " rows inserted in %s",
                 nRows, rtree_name);
        CPL_IGNORE_RET_VAL_INT(progress_cbk(szMsg, progress_cbk_user_data));
    }

    sqlite3_finalize(hStmt);
    return bOK;
}
