/* 
Authors:
Zheng Zhong

LRU + Hash cache
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include "my_io.h"

const char* device_path = "/dev/sdb1";

// linked list node for buffer cache
struct CacheNode {
    struct CacheNode* queue_prev; // prev pointer for lru queue
    struct CacheNode* queue_next; // next pointer for lru queue
    struct CacheNode* hash_prev; // next pointer for hash bucket list
    struct CacheNode* hash_next; // next pointer for hash bucket list
    bool dirty; // cache is modified or not
    int block_id; // block id in disk drive
    char* block_ptr; // pointer to cached block data
};

// cache queue (doubly linked list)
struct CacheQueue {
    unsigned count; // number of filled frames
    unsigned cache_capacity; // maximum number of nodes in cache
    struct CacheNode* front;
    struct CacheNode* rear;
};

// hash table (linked list hash bucket)
struct Hash {
    int hash_capacity; // number of hash buckets
    struct CacheNode** buckets; // buckets
};

// create a new cache node
struct CacheNode* newCacheNode(unsigned block_id) {
    // read block
    struct CacheNode* temp = (struct CacheNode*) malloc(sizeof(struct CacheNode));
    int result = posix_memalign((void**) &(temp->block_ptr), block_size, block_size);
    if (result != 0) return NULL;
    int fd = open(device_path, O_RDONLY | O_DIRECT);
    if (fd < 0) return NULL;
    io_read(fd, temp->block_ptr, block_id);
    result = close(fd);
    if (result < 0) return NULL;

    // set values
    temp->dirty = false;
    temp->block_id = block_id;
    temp->queue_prev = temp->queue_next = NULL;
    temp->hash_prev = temp->hash_next = NULL;

    return temp;
} 
  
// create empty cache queue
struct CacheQueue* create_cache_queue(int cache_capacity) {
    struct CacheQueue* queue = (struct CacheQueue*) malloc(sizeof(struct CacheQueue));

    queue->count = 0;
    queue->front = queue->rear = NULL;

    queue->cache_capacity = cache_capacity;

    return queue;
} 
  
// create empty hash table
struct Hash* create_hash_table(int hash_capacity) {
    struct Hash* hash = (struct Hash*) malloc(sizeof(struct Hash));
    hash->hash_capacity = hash_capacity;

    hash->buckets = (struct CacheNode**) malloc(hash->hash_capacity * sizeof(struct CacheNode*));
    for (int i = 0; i < hash->hash_capacity; i++) hash->buckets[i] = NULL;

    return hash;
} 
  
// check if there is slot available in memory 
bool is_queue_full(struct CacheQueue* queue) { 
    return queue->count == queue->cache_capacity; 
}
  
// check if queue is empty
bool is_queue_empty(struct CacheQueue* queue) { 
    return queue->count == 0;
}
  
// delete a cache node from cache
// return 0 on success and negative integer if not success
int dequeue(struct CacheQueue* queue, struct Hash* hash) {
    if (is_queue_empty(queue)) return 0;

    struct CacheNode* temp = queue->rear;

    // handle cache queue
    if (queue->front == queue->rear) queue->front = NULL;
    queue->rear = queue->rear->queue_prev;
    if (queue->rear != NULL) queue->rear->queue_next = NULL;

    // handle hash table
    int hash_key = temp->block_id % hash->hash_capacity;
    if (temp->hash_prev == NULL) hash->buckets[hash_key] = temp->hash_next;
    if (temp->hash_prev != NULL) temp->hash_prev->hash_next = temp->hash_next;
    if (temp->hash_next != NULL) temp->hash_next->hash_prev = temp->hash_prev;

    // write back if dirty
    if (temp->dirty) {
        int fd = open(device_path, O_WRONLY | O_DIRECT);
        if (fd < 0) return fd;
        io_write(fd, temp->block_ptr, temp->block_id);
        int result = close(fd);
        if (result < 0) return result;
    }
    free(temp->block_ptr);
    free(temp);

    queue->count--;

    return 0;
} 
  
// add a cache node to cache
// return 0 on success and negative integer if not success
int enqueue(struct CacheQueue* queue, struct Hash* hash, unsigned block_id) {
    // evict lru node if cache is full
    if (is_queue_full(queue)) dequeue(queue, hash);

    // create new node
    struct CacheNode* temp = newCacheNode(block_id);

    // handle cache queue
    temp->queue_next = queue->front;
    if (is_queue_empty(queue)) {
        queue->front = temp;
        queue->rear = temp;
    }
    else {
        queue->front->queue_prev = temp;
        queue->front = temp;
    }

    // handle hash table
    int hash_key = temp->block_id % hash->hash_capacity;
    temp->hash_next = hash->buckets[hash_key];
    if (hash->buckets[hash_key] != NULL) hash->buckets[hash_key]->hash_prev = temp;
    hash->buckets[hash_key] = temp;

    queue->count++;
    return 0;
} 

// get pointer to the block data cached
// bring the block to cache if not in cache
struct CacheNode* get_block_cache(struct CacheQueue* queue, struct Hash* hash, unsigned block_id) {
    // printf("[CACHE DBUG INFO] get_block_cache: block_id = %d\n", block_id);
    int hash_key = block_id % hash->hash_capacity;
    struct CacheNode* target = hash->buckets[hash_key];
    while (target != NULL) {
        if (target->block_id == block_id) break;
        target = target->hash_next;
    }
    // bring the block to cache
    if (target == NULL || target->block_id != block_id) {
        // printf("[CACHE DBUG INFO] get_block_cache: bring block %d to cache\n", block_id);
        enqueue(queue, hash, block_id);
        return queue->front;
    }
    // move target to front
    else if (target->block_id == block_id && target != queue->front) {
        // change prev and next
        target->queue_prev->queue_next = target->queue_next;
        if (target->queue_next != NULL) target->queue_next->queue_prev = target->queue_prev;

        // change rear
        if (target == queue->rear) {
            queue->rear = target->queue_prev; 
            queue->rear->queue_next = NULL;
        } 

        // move to front
        queue->front->queue_prev = target;
        target->queue_next = queue->front;
        target->queue_prev = NULL;
        queue->front = target;

        return queue->front;
    }
    else if (target->block_id == block_id && target == queue->front) {
        return queue->front;
    }
    else {
        // printf("[CACHE ERROR] get_block_cache: block_id = %d\n", block_id);
        return NULL;
    };
}

int write_dirty_blocks_back(struct CacheQueue* queue) {
    struct CacheNode* cache_node = queue->front;
    while (cache_node != NULL) {
        if (cache_node->dirty) {
            int fd = open(device_path, O_WRONLY | O_DIRECT);
            if (fd < 0) return fd;
            io_write(fd, cache_node->block_ptr, cache_node->block_id);
            int result = close(fd);
            if (result < 0) return result;
            
            cache_node->dirty = false;    
        }
        cache_node = cache_node->queue_next;
    }

    return 0;
}
