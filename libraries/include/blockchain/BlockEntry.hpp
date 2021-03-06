#pragma once
#include <blockchain/Block.hpp>

namespace TiValue {
    namespace blockchain {

        struct BlockEntry : public TiValue::blockchain::DigestBlock
        {
            BlockIdType       id;
            uint64_t            block_size = 0; /* Bytes */
            fc::microseconds    latency; /* Time between block timestamp and first push_block */

            ShareType          signee_shares_issued = 0;
            ShareType          signee_fees_collected = 0;
            ShareType          signee_fees_destroyed = 0;
            fc::ripemd160       random_seed;

            fc::microseconds    processing_time; /* Time taken for extend_chain to run */
        };
        typedef optional<BlockEntry> oBlockEntry;

    }
} // TiValue::blockchain

FC_REFLECT_DERIVED(TiValue::blockchain::BlockEntry,
    (TiValue::blockchain::DigestBlock),
    (id)
    (block_size)
    (latency)
    (signee_shares_issued)
    (signee_fees_collected)
    (signee_fees_destroyed)
    (random_seed)
    (processing_time)
    )
