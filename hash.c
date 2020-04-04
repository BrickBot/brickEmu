/* Emulator for LEGO RCX Brick, Copyright (C) 2003 Jochen Hoenicke.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; see the file COPYING.LESSER.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: hash.c 266 2006-02-26 17:30:02Z hoenicke $
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "hash.h"

typedef struct hash_bucket {
    unsigned int key;
    struct hash_bucket* next;
} hash_bucket;

void hash_init(hash_type *hash, int initsize) {
    hash->size  = initsize;
    hash->elems = 0;
    hash->list  = malloc(sizeof(hash_bucket *) * initsize);
    memset(hash->list, 0, sizeof(hash_bucket *) * initsize);
}

void *hash_get(hash_type *hash, unsigned int key) {
    unsigned int idx = key % hash->size;
    struct hash_bucket *bucket;
    
    for (bucket = hash->list[idx]; bucket; bucket = bucket->next) {
        if (bucket->key == key)
            return (void *) (bucket+1);
    }
    return NULL;
}

void *hash_move(hash_type *hash, unsigned int key, unsigned int newkey) {
    unsigned int idx = key % hash->size;
    struct hash_bucket *bucket, **pbucket;
    
    for (pbucket = &hash->list[idx]; (bucket = *pbucket); pbucket = &bucket->next) {
        if (bucket->key == key) {
            *pbucket = bucket->next;

            idx = newkey % hash->size;
            bucket->key = newkey;
            bucket->next = hash->list[idx];
            hash->list[idx] = bucket;
            return (void *) (bucket+1);
        }
    }
    return NULL;
}

static void hash_grow(hash_type *hash) {
    int nsize = 2*hash->size + 1;
    unsigned int i, idx;
    hash_bucket *bucket, *nbucket;
    hash_bucket **nlist = malloc(sizeof(hash_bucket*) * nsize);
    memset(nlist, 0, sizeof(hash_bucket*) * nsize);
    
    for (i = 0; i < hash->size; i++) {
        for (bucket = hash->list[i]; bucket; bucket = nbucket) {
            nbucket = bucket->next;
            idx = bucket->key % nsize;
            bucket->next = nlist[idx];
            nlist[idx] = bucket;
        }
    }
    hash->list = nlist;
    hash->size = nsize;
}

void *hash_create(hash_type *hash, unsigned int key, size_t elemsize) {
    unsigned int idx;
    struct hash_bucket *bucket;
    
    if (hash->elems * 5 > hash->size * 4)
        hash_grow(hash);

    hash->elems++;

    idx = key % hash->size;
    bucket = malloc(sizeof(hash_bucket) + elemsize);
    bucket->key = key;
    bucket->next = hash->list[idx];
    hash->list[idx] = bucket;

    return (void *) (bucket+1);
}

void *hash_realloc(hash_type *hash, unsigned int key, size_t elemsize) {
    unsigned int idx = key % hash->size;
    struct hash_bucket *bucket, **pbucket;

    for (pbucket = &hash->list[idx]; (bucket = *pbucket); pbucket = &bucket->next) {
        if (bucket->key == key) {
            bucket = realloc(bucket, sizeof(hash_bucket) + elemsize);
            *pbucket = bucket;
            return (void *) (bucket+1);
        }
    }
    
    return hash_create(hash, key, elemsize);
}

int hash_remove(hash_type *hash, unsigned int key) {
    unsigned int idx = key % hash->size;
    struct hash_bucket *bucket, **pbucket;
    
    for (pbucket = &hash->list[idx]; (bucket = *pbucket); pbucket = &bucket->next) {
        if (bucket->key == key) {
            *pbucket = bucket->next;
            free(bucket);
            return 1;
        }
    }
    return 0;
}

void hash_enumerate(hash_type *hash, 
                    void (*func) (unsigned int key, void *data)) {
    int i;
    hash_bucket *bucket;
    for (i = 0; i < hash->size; i++) {
        for (bucket = hash->list[i]; bucket; bucket = bucket->next) {
            func(bucket->key, (bucket+1));
        }
    }
}
