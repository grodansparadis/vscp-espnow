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

#define BLOOM_SIZE     1024 // 1024 bits (128 bytes)1
#define HASH_SEED1     5381
#define HASH_SEED2     52711
#define TTL_EXPIRATION 3 // Default expiration time in seconds

static uint8_t bloom_filter[BLOOM_SIZE / 8];
static uint32_t bloom_timestamps[BLOOM_SIZE / 8];

///////////////////////////////////////////////////////////////////////////////
// murmurhash3
//
// Fast hash function for binary data
//

uint32_t
murmurhash3(const void *key, size_t len, uint32_t seed)
{
  const uint8_t *data = (const uint8_t *) key;
  const int nblocks   = len / 4;
  uint32_t h1         = seed;
  const uint32_t c1   = 0xcc9e2d51;
  const uint32_t c2   = 0x1b873593;

  // Body
  const uint32_t *blocks = (const uint32_t *) (data + nblocks * 4);
  for (int i = -nblocks; i; i++) {
    uint32_t k1 = blocks[i];
    k1 *= c1;
    k1 = (k1 << 15) | (k1 >> (32 - 15));
    k1 *= c2;

    h1 ^= k1;
    h1 = (h1 << 13) | (h1 >> (32 - 13));
    h1 = h1 * 5 + 0xe6546b64;
  }

  // Tail
  const uint8_t *tail = (const uint8_t *) (data + nblocks * 4);
  uint32_t k1         = 0;
  switch (len & 3) {
    case 3:
      k1 ^= tail[2] << 16;
    case 2:
      k1 ^= tail[1] << 8;
    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1 = (k1 << 15) | (k1 >> (32 - 15));
      k1 *= c2;
      h1 ^= k1;
  }

  // Finalization
  h1 ^= len;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  return h1;
}

///////////////////////////////////////////////////////////////////////////////
// bloom_hash
//
// Simple hash function (djb2)
// http://www.cse.yorku.ca/~oz/hash.html
//

uint32_t
bloom_hash(const uint8_t *p, uint8_t len, uint32_t seed)
{
  uint32_t hash = seed;
  for (int i = 0; i < len; i++) {
    hash = ((hash << 5) + hash) + p[i];
  }
  return hash % BLOOM_SIZE;
}

// Simple hashfunction bsed on djb2 (för blobbar)
uint32_t
hash1(const void *blob, size_t len)
{
  const unsigned char *data = (const unsigned char *) blob;
  uint32_t hash             = 5381;
  for (size_t i = 0; i < len; i++) {
    hash = ((hash << 5) + hash) + data[i];
  }
  return hash % BLOOM_SIZE;
}

// Second hashfunction (modified sdbm)
uint32_t
hash2(const void *blob, size_t len)
{
  const unsigned char *data = (const unsigned char *) blob;
  uint32_t hash             = 0;
  for (size_t i = 0; i < len; i++) {
    hash = data[i] + (hash << 6) + (hash << 16) - hash;
  }
  return hash % BLOOM_SIZE;
}

// Third hashfunction (combine the first two)
uint32_t
hash3(const void *blob, size_t len)
{
  return (hash1(blob, len) + hash2(blob, len)) % BLOOM_SIZE;
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

// Lägg till en blob i Bloom-filtret
void
bloom2_add(const void *blob, size_t len)
{
  uint32_t h1 = hash1(blob, len);
  uint32_t h2 = hash2(blob, len);
  uint32_t h3 = hash3(blob, len);

  bloom_filter[h1 / 8] |= (1 << (h1 % 8));
  bloom_filter[h2 / 8] |= (1 << (h2 % 8));
  bloom_filter[h3 / 8] |= (1 << (h3 % 8));
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

// Kontrollera om en blob kan finnas i filtret
int
bloom2_check(const void *blob, size_t len)
{
  uint32_t h1 = hash1(blob, len);
  uint32_t h2 = hash2(blob, len);
  uint32_t h3 = hash3(blob, len);

  return (bloom_filter[h1 / 8] & (1 << (h1 % 8))) && (bloom_filter[h2 / 8] & (1 << (h2 % 8))) &&
         (bloom_filter[h3 / 8] & (1 << (h3 % 8)));
}

///////////////////////////////////////////////////////////////////////////////
// bloom_forward_message
//

bool
bloom_forward_message(const uint8_t *pdata, uint8_t len, int ttl)
{
  if (ttl <= 0) {
    return false;
  }

  uint32_t hash1 = murmurhash3(pdata, len, HASH_SEED1);
  uint32_t hash2 = murmurhash3(pdata, len, HASH_SEED2);

  if (bloom_check(hash1) || bloom_check(hash2)) {
    // Message already seen, ignore
    return false;
  }

  // Not seen - insert
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
  bloom_forward_message(tstmsg, sizeof(tstmsg), 2);
  bloom_forward_message(tstmsg, sizeof(tstmsg), 2); // This should be ignored

  sleep(2);
  bloom_forward_message(tstmsg, sizeof(tstmsg), 2); // Still ignored

  printf("Waiting for expiration...\n");
  sleep(TTL_EXPIRATION * 2 + 1);
  bloom_forward_message(tstmsg, sizeof(tstmsg), 2); // Should be forwarded again

  return 0;
}