#pragma once
#include <morphene/chain/account_object.hpp>
#include <morphene/chain/block_summary_object.hpp>
#include <morphene/chain/global_property_object.hpp>
#include <morphene/chain/history_object.hpp>
#include <morphene/chain/morphene_objects.hpp>
#include <morphene/chain/transaction_object.hpp>
#include <morphene/chain/witness_objects.hpp>
#include <morphene/chain/database.hpp>

namespace morphene { namespace plugins { namespace block_api {

using namespace morphene::chain;

struct api_signed_block_object : public signed_block
{
   api_signed_block_object( const signed_block& block ) : signed_block( block )
   {
      block_id = id();
      signing_key = signee();
      transaction_ids.reserve( transactions.size() );
      for( const signed_transaction& tx : transactions )
         transaction_ids.push_back( tx.id() );
   }
   api_signed_block_object() {}

   block_id_type                 block_id;
   public_key_type               signing_key;
   vector< transaction_id_type > transaction_ids;
};

} } } // morphene::plugins::database_api

FC_REFLECT_DERIVED( morphene::plugins::block_api::api_signed_block_object, (morphene::protocol::signed_block),
                     (block_id)
                     (signing_key)
                     (transaction_ids)
                  )
