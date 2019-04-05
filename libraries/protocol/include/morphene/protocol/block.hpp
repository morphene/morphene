#pragma once
#include <morphene/protocol/block_header.hpp>
#include <morphene/protocol/transaction.hpp>

namespace morphene { namespace protocol {

   struct signed_block : public signed_block_header
   {
      checksum_type calculate_merkle_root()const;
      vector<signed_transaction> transactions;
   };

} } // morphene::protocol

FC_REFLECT_DERIVED( morphene::protocol::signed_block, (morphene::protocol::signed_block_header), (transactions) )
