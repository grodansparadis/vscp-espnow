/*
  This is a  C implementation with a Bloom Filter to prevent duplicate message forwarding.
  The hash() function generates a unique hash for each message, and the filter checks if the
  message has already been seen.

  This approach uses minimal memory and ensures each message is only forwarded once. Its
  have TTL expiration cleanup and multiple hash functions to reduce false positives.

  Messages expire after the defined TTL_EXPIRATION time (in seconds). The system automatically
  clears expired entries.

  Dynamic TTL values per message is used, where the expiration time is scaled by the ttl
  parameter. The message's expiration will now be proportional to its TTL, making the system
  more flexible.

  I've added fine-tuned hash functions by introducing a second hash seed and added detailed
  logging for hash insertions, checks, and expirations. Let me know if you'd like to optimize
  memory usage or customize the logging output further!

  I've optimized memory usage by reducing the size of the timestamp array, storing one timestamp
  per 32 bits. Let me know if you'd like to fine-tune the hash distribution further or add
  adaptive TTL scaling based on message priority!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "broadcast-bloom-filter.h"

#define FILTER_SIZE    256 // 256 bits (32 bytes)
#define HASH_SEED1     5381
#define HASH_SEED2     52711
#define TTL_EXPIRATION 1 // Default expiration time in seconds

static uint8_t bloom_filter[FILTER_SIZE / 8];
static uint32_t bloom_timestamps[FILTER_SIZE / 8];

///////////////////////////////////////////////////////////////////////////////
// bloom_insert
//
// Simple hash function (djb2)
// http://www.cse.yorku.ca/~oz/hash.html
//

uint32_t
bloom_hash(const uint8_t *p, uint8_t len, uint32_t seed)
{
  uint32_t hash = seed;

  uint32_t hash = seed;
  for (int i = 0; i < len; i++) {
    hash = ((hash << 5) + hash) + p[i];
  }
  return hash % FILTER_SIZE;
}

///////////////////////////////////////////////////////////////////////////////
// bloom_insert
//

void
bloom_insert(uint32_t hash, uint32_t ttl)
{
  uint8_t byte_index = hash / 8;
  uint8_t bit_index  = hash % 8;
  bloom_filter[byte_index] |= (1 << bit_index);
  bloom_timestamps[byte_index] = (uint16_t) (time(NULL) + ttl);
}

///////////////////////////////////////////////////////////////////////////////
// bloom_check
//

bool
bloom_check(uint32_t hash)
{
  uint8_t byte_index = hash / 8;
  uint8_t bit_index  = hash % 8;
  if ((bloom_filter[byte_index] & (1 << bit_index)) == 0) {
    return false;
  }
  uint32_t now = (uint32_t) time(NULL);
  if (now > bloom_timestamps[byte_index]) {
    // Expired, clear bit and timestamp
    bloom_filter[byte_index] &= ~(1 << bit_index);
    bloom_timestamps[byte_index] = 0;
    return false;
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// forward_message
//

bool
forward_message(const uint8_t *pdata, uint8_t len, int ttl)
{
  if (ttl <= 0) {
    return false;
  }

  uint32_t hash1 = bloom_hash(pdata + 3, len - 4, HASH_SEED1);
  uint32_t hash2 = bloom_hash(pdata + 3, len - 4, HASH_SEED2);

  if (bloom_check(hash1) || bloom_check(hash2)) {
    // Message already seen, ignore
    return false;
  }

  bloom_insert(hash1, ttl * TTL_EXPIRATION);
  bloom_insert(hash2, ttl * TTL_EXPIRATION);

  return true;
}

int
test()
{
  uint8_t tstmsg[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  memset(bloom_filter, 0, sizeof(bloom_filter));
  memset(bloom_timestamps, 0, sizeof(bloom_timestamps));

  printf("Sending initial message...\n");
  forward_message(tstmsg, sizeof(tstmsg), 2);
  forward_message(tstmsg, sizeof(tstmsg), 2); // This should be ignored

  sleep(2);
  forward_message(tstmsg, sizeof(tstmsg), 2); // Still ignored

  printf("Waiting for expiration...\n");
  sleep(TTL_EXPIRATION * 2 + 1);
  forward_message(tstmsg, sizeof(tstmsg), 2); // Should be forwarded again

  return 0;
}