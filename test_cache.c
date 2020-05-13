#include "cache.h"

void display(struct CacheQueue* queue, struct Hash* hash) {
    printf("lru queue id in order / reverse order):\n");
    struct CacheNode* traverse = queue->front;
    while(traverse != NULL) {
        printf("%d ", traverse->block_id);
        traverse = traverse->queue_next;
    }
    printf("\n");
    traverse = queue->rear;
    while(traverse != NULL) {
        printf("%d ", traverse->block_id);
        traverse = traverse->queue_prev;
    }
    printf("\n"); 
    for (int i = 0; i < hash->hash_capacity; i++) {
        printf("hash bucket %d id in order / reverse order:\n", i);
        traverse = hash->buckets[i];
        while(traverse != NULL) {
            printf("%d ", traverse->block_id);
            traverse = traverse->hash_next;
        }
        printf("\n");
        traverse = hash->buckets[i];
        while(traverse != NULL && traverse->hash_next != NULL) {
            traverse = traverse->hash_next;
        }
        while(traverse != NULL) {
            printf("%d ", traverse->block_id);
            traverse = traverse->hash_prev;
        }
        printf("\n");
    }
}

int main() {
    struct CacheQueue* queue = create_cache_queue(4);

    struct Hash* hash = create_hash_table(10);

    get_block_cache(queue, hash, 0);
    get_block_cache(queue, hash, 0);
    get_block_cache(queue, hash, 1);
    get_block_cache(queue, hash, 2);
    get_block_cache(queue, hash, 3);
    get_block_cache(queue, hash, 0);
    get_block_cache(queue, hash, 12);
    get_block_cache(queue, hash, 13);
    get_block_cache(queue, hash, 3);
    get_block_cache(queue, hash, 12);
    get_block_cache(queue, hash, 0);
    get_block_cache(queue, hash, 22);
    // get_block_cache(queue, hash, 103);

    display(queue, hash);

    return 0; 
}