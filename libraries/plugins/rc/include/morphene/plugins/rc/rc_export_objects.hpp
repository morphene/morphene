#pragma once

#include <morphene/protocol/types.hpp>

#include <morphene/plugins/block_data_export/exportable_block_data.hpp>
#include <morphene/plugins/rc/resource_count.hpp>

#include <fc/int_array.hpp>

#include <vector>

namespace morphene { namespace plugins { namespace rc {

using morphene::plugins::block_data_export::exportable_block_data;
using morphene::protocol::account_name_type;

struct rc_transaction_info
{
   account_name_type         resource_user;
   count_resources_result    usage;
   fc::int_array< int64_t, MORPHENE_NUM_RESOURCE_TYPES >
                             cost;
};

struct rc_block_info
{
   resource_count_type       dt;
   resource_count_type       decay;
   resource_count_type       budget;
   resource_count_type       usage;
   resource_count_type       adjustment;
   resource_count_type       pool;
};

struct exp_rc_data
   : public exportable_block_data
{
   exp_rc_data();
   virtual ~exp_rc_data();

   virtual void to_variant( fc::variant& v )const override;

   rc_block_info                       block_info;
   std::vector< rc_transaction_info >  tx_info;
};

} } } // morphene::plugins::rc

FC_REFLECT( morphene::plugins::rc::rc_transaction_info,
   (resource_user)
   (usage)
   (cost)
)

FC_REFLECT( morphene::plugins::rc::rc_block_info,
   (dt)
   (decay)
   (budget)
   (usage)
   (adjustment)
   (pool)
)

FC_REFLECT( morphene::plugins::rc::exp_rc_data,
   (block_info)
   (tx_info)
)
