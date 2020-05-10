#include "cache.h"

int main() {
    struct CacheQueue* q = create_cache_queue(4);

    struct Hash* hash = create_hash_table(10);

    get_block_cache(q, hash, 1);
    get_block_cache(q, hash, 2);
    get_block_cache(q, hash, 3);
    get_block_cache(q, hash, 1);
    get_block_cache(q, hash, 4);
    get_block_cache(q, hash, 5);

    printf("%d ", q->front->block_id);
    printf("%d ", q->front->queue_next->block_id);
    printf("%d ", q->front->queue_next->queue_next->block_id);
    printf("%d ", q->front->queue_next->queue_next->queue_next->block_id);

    return 0; 
}