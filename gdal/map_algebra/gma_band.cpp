#include "gdal_map_algebra_private.h"

gma_block *gma_block_create() {
    gma_block *block;
    block = (gma_block *)CPLMalloc(sizeof(gma_block));
    block->_block = NULL;
    return block;
}

void gma_block_destroy(gma_block *block) {
    if (block->_block) CPLFree(block->_block);
    CPLFree(block);
}

gma_block_cache gma_cache_initialize() {
    gma_block_cache cache;
    cache.n = 0;
    cache.blocks = NULL;
    return cache;
}

void gma_empty_cache(gma_block_cache *cache) {
    if (cache->n == 0)
        return;
    for (int i = 0; i < cache->n; i++)
        gma_block_destroy(cache->blocks[i]);
    CPLFree(cache->blocks);
    cache->n = 0;
    cache->blocks = NULL;
}

void gma_cache_remove(gma_block_cache *cache, int i) {
    if (i < 0 || i >= cache->n) return;
    gma_block **blocks = (gma_block **)CPLMalloc((cache->n-1) * sizeof(gma_block*));
    int d = 0;
    for (int j = 0; j < cache->n; j++) {
        if (j == i) {
            gma_block_destroy(cache->blocks[j]);
            d = 1;
        } else {
            blocks[j-d] = cache->blocks[j];
        }
    }
    CPLFree(cache->blocks);
    cache->n = cache->n-1;
    cache->blocks = blocks;
}

gma_block *gma_cache_retrieve(gma_block_cache cache, gma_block_index index) {
    for (int i = 0; i < cache.n; i++)
        if (cache.blocks[i]->index.x == index.x && cache.blocks[i]->index.y == index.y)
            return cache.blocks[i];
    return NULL;
}

CPLErr gma_cache_add(gma_block_cache *cache, gma_block *block) {
    cache->n++;
    if (cache->n == 1)
        cache->blocks = (gma_block**)CPLMalloc(sizeof(gma_block*));
    else
        cache->blocks = (gma_block**)CPLRealloc(cache->blocks, cache->n * sizeof(gma_block*));
    cache->blocks[cache->n-1] = block;
}

int is_border_cell(gma_block *block, int border_block, gma_cell_index i) {
    if (!border_block)
        return 0;
    if (i.x == 0) {
        if (i.y == 0 && border_block == 1)
            return 8;
        else if (i.y == block->h - 1 && border_block == 6)
            return 6;
        else if (border_block == 8 || border_block == 6 || border_block == 7)
            return 7;
    } else if (i.x == block->w - 1) {
        if (i.y == 0 && border_block == 2)
            return 2;
        else if (i.y == block->h - 1 && border_block == 4)
            return 4;
        else if (border_block == 2 || border_block == 4 || border_block == 3)
            return 3;
    } else if (i.y == 0 && (border_block == 8 || border_block == 2 || border_block == 1))
        return 1;
    else if (i.y == block->h - 1 && (border_block == 6 || border_block == 4 || border_block == 5))
        return 5;
    else
        return 0;
}
