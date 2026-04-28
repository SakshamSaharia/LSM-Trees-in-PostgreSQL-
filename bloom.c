#include "bloom.h"
#include "utils/memutils.h"

static uint64_t fnv1a_hash(const void *key, size_t len)
{
    uint64_t hash = 14695981039346656037ULL;
    const unsigned char *p = (const unsigned char *)key;
    for (size_t i = 0; i < len; i++)
    {
        hash ^= p[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

BloomFilter *bloom_create(size_t size_in_bytes, int num_hashes)
{
    /* Allocate in TopMemoryContext so it survives until you close the terminal! */
    BloomFilter *bf = (BloomFilter *)MemoryContextAllocZero(TopMemoryContext, sizeof(BloomFilter));
    bf->size_in_bytes = size_in_bytes;
    bf->num_hashes = num_hashes;
    bf->bits = (uint8_t *)MemoryContextAllocZero(TopMemoryContext, size_in_bytes);
    return bf;
}

void bloom_add(BloomFilter *bf, const void *key, size_t key_len)
{
    uint64_t base_hash = fnv1a_hash(key, key_len);
    size_t total_bits = bf->size_in_bytes * 8;
    for (int i = 0; i < bf->num_hashes; i++)
    {
        uint64_t combined_hash = base_hash + (i * 0x9E3779B97F4A7C15ULL);
        size_t bit_index = combined_hash % total_bits;
        bf->bits[bit_index / 8] |= (1 << (bit_index % 8));
    }
}

bool bloom_check(BloomFilter *bf, const void *key, size_t key_len)
{
    uint64_t base_hash = fnv1a_hash(key, key_len);
    size_t total_bits = bf->size_in_bytes * 8;
    for (int i = 0; i < bf->num_hashes; i++)
    {
        uint64_t combined_hash = base_hash + (i * 0x9E3779B97F4A7C15ULL);
        size_t bit_index = combined_hash % total_bits;
        if ((bf->bits[bit_index / 8] & (1 << (bit_index % 8))) == 0)
        {
            return false;
        }
    }
    return true;
}