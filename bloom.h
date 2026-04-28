#ifndef LSM3_BLOOM_H
#define LSM3_BLOOM_H

#include "postgres.h"
#include <stdint.h>

typedef struct BloomFilter
{
    size_t size_in_bytes;
    int num_hashes;
    uint8_t *bits;
} BloomFilter;

BloomFilter *bloom_create(size_t size_in_bytes, int num_hashes);
void bloom_add(BloomFilter *bf, const void *key, size_t key_len);
bool bloom_check(BloomFilter *bf, const void *key, size_t key_len);

#endif