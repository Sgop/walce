#include <string.h>
#include "table.h"

#define ClusterSize 4

typedef struct {
    tentry_t data[ClusterSize];
} tcluster_t;

typedef struct {
    tcluster_t* entries;
    uint32_t size;
    uint8_t age;
} ttable_t;





static ttable_t* TT = NULL;


static ttable_t* ttable_new()
{
    ttable_t* tt = malloc(sizeof(ttable_t));
    tt->entries = NULL;
    tt->size = 0;
    tt->age = 0;
    return tt;
}

static void ttable_destroy(ttable_t* tt)
{
    if (tt->entries)
        free(tt->entries);
}

static void ttable_clear(ttable_t* tt)
{
    memset(tt->entries, 0, tt->size * sizeof(tcluster_t));
    tt->age = 0;
}

static int ttable_set_size(ttable_t* tt, size_t mb)
{
    size_t newSize;
    size_t entries = (mb << 20) / sizeof(tcluster_t);

//    log_line("entries %x %d", entries, entries);
    while (entries)
    {
        newSize = entries;
        entries &= (entries-1);
    }
//    log_line("newSize %x %d", newSize, newSize);

    if (newSize == tt->size)
        return 1;

    tt->size = newSize;
    if (tt->entries)
        free(tt->entries);
    tt->entries = malloc(tt->size * sizeof(tcluster_t));

    if (!tt->entries)
        return 0;

    ttable_clear(tt);
    return 1;
}

static tentry_t* ttable_find(ttable_t* tt, pkey_t key)
{
    return tt->entries[(pkey32_t)key & (tt->size - 1)].data;
}

void TT_store(pkey_t key, int16_t value, uint8_t bound, int16_t depth, move_t move,
        int16_t sValue)
{
    tentry_t* tte;
    tentry_t* replace;
    int i1;
    pkey32_t ckey = key >> 32;
    
    tte = replace = ttable_find(TT, key);
    
    for (i1 = 0; i1 < ClusterSize; i1++, tte++)
    {
        int c1, c2, c3;
        
        if (!tte->key || tte->key == ckey)
        {
            replace = tte;
            if (move == MOVE_NONE)
                move = tte->move;
            break;
        }
        c1 = replace->age == TT->age ? 2 : 0;
        c2 = tte->age == TT->age || tte->bound == B_EXACT ? -2 : 0;
        c3 = tte->depth < replace->depth ? 1 : 0;
        if (c1 + c2 + c3 > 0)
            replace = tte;
    }

    replace->key = ckey;
    replace->value = value;
    replace->age = TT->age;
    replace->bound = bound;
    replace->move = move;
    replace->depth = depth;
    replace->sValue = sValue;
}

tentry_t* TT_probe(pkey_t key)
{
    tentry_t* tte;
    int i1;
    pkey32_t ckey = key >> 32;
    
    tte = ttable_find(TT, key);
    
    for (i1 = 0; i1 < ClusterSize; i1++, tte++)
    {
        if (tte->key == ckey)
            return tte;
    }
    return NULL;
}

void TT_advance()
{
    TT->age++;
}

void TT_touch(tentry_t* tte)
{
    tte->age = TT->age;
}

void TT_clear()
{
    ttable_clear(TT);
}

void tables_init()
{
    TT = ttable_new();
    ttable_set_size(TT, 64);
}

void tables_exit()
{
    ttable_destroy(TT);
}





