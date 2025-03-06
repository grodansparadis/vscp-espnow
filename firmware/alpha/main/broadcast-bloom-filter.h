// Date: 2021/05/30
//
// Author: Ake Hedman, the VSCP project with CoPilot help
//
// Description:
// This is a header file for the broadcast bloom filter.
//
// The broadcast bloom filter is used to keep track of messages that have been
// forwarded to other devices. This approach uses minimal memory and ensures each
// message is only forwarded once. It has TTL expiration cleanup and multiple hash
// functions to reduce false positives.
//
// Messages expire after the defined TTL_EXPIRATION time (in seconds). The system
// automatically clears expired entries.
//
// Dynamic TTL values per message are used, where the expiration time is scaled by
// the ttl parameter. The message's expiration will now be proportional to its TTL,
// making the system more flexible.
//
// I've added fine-tuned hash functions by introducing a second hash seed and added
// detailed logging for hash insertions, checks, and expirations. Let me know if you'd
// like to optimize memory usage or customize the logging output further!
//
// I've optimized memory usage by reducing the size of the timestamp array, storing one
// timestamp per 32 bits. Let me know if you'd like to fine-tune the hash distribution
// further or add adaptive TTL scaling based on message priority!
//
// The broadcast bloom filter is used to keep track of messages that have been
// forwarded to other devices. This approach uses minimal memory and ensures each
// message is only forwarded once. It has TTL expiration cleanup and multiple hash
// functions to reduce false positives.
//
// Messages expire after the defined TTL_EXPIRATION time (in seconds). The system
// automatically clears expired entries.
//
// Dynamic TTL values per message are used, where the expiration time is scaled by
// the ttl parameter. The message's expiration will now be proportional to its TTL,
// making the system more flexible.
//

#pragma once

#ifndef __BROADCAST_BLOOM_FILTER_H__
#define __BROADCAST_BLOOM_FILTER_H__

/*!
 * \brief Compute the hash of a bloom filter.
 * \param p Pointer to data to hash.
 * \param len Length of data to hash.
 * \param seed Seed for the hash.
 * \return The hash value.
 */
uint32_t
bloom_hash(const uint8_t *p, uint8_t len, uint32_t seed);

/*!
 * \brief Insert a hash into the bloom filter.
 * \param hash The hash to insert.
 * \param ttl Time to live for the hash.
 */
void
bloom_insert(uint32_t hash, uint32_t ttl);

/*!
 * \brief Check if a hash is in the bloom filter.
 * \param hash The hash to check.
 * \return True if the hash is in the bloom filter, false otherwise.
 */
bool
bloom_check(uint32_t hash);

/*!
 * \brief Forward a message to other devices.
 * \param pdata Pointer to the message to forward.
 * \param len Length of data to forward.
 * \param ttl Time to live for the message.
 * \return true if the message was forwarded, false otherwise.
 */
bool
forward_message(const uint8_t *pdate, uint8_t len, int ttl);

#endif
