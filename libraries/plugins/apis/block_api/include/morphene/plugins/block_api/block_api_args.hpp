#pragma once
#include <morphene/plugins/block_api/block_api_objects.hpp>

#include <morphene/protocol/types.hpp>
#include <morphene/protocol/transaction.hpp>
#include <morphene/protocol/block_header.hpp>

#include <morphene/plugins/json_rpc/utility.hpp>

namespace morphene { namespace plugins { namespace block_api {

/* get_block_header */

struct get_block_header_args
{
   uint32_t block_num;
};

struct get_block_header_return
{
   optional< block_header > header;
};

/* get_block */
struct get_block_args
{
   uint32_t block_num;
};

struct get_block_return
{
   optional< api_signed_block_object > block;
};

} } } // morphene::block_api

FC_REFLECT( morphene::plugins::block_api::get_block_header_args,
   (block_num) )

FC_REFLECT( morphene::plugins::block_api::get_block_header_return,
   (header) )

FC_REFLECT( morphene::plugins::block_api::get_block_args,
   (block_num) )

FC_REFLECT( morphene::plugins::block_api::get_block_return,
   (block) )

