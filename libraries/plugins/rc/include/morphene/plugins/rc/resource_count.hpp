#pragma once

#include <morphene/protocol/types.hpp>

#include <fc/int_array.hpp>
#include <fc/reflect/reflect.hpp>
#include <vector>

#define MORPHENE_NUM_RESOURCE_TYPES     5

namespace morphene { namespace protocol {
struct signed_transaction;
} } // morphene::protocol

namespace morphene { namespace plugins { namespace rc {

enum rc_resource_types
{
   resource_history_bytes,
   resource_new_accounts,
   resource_market_bytes,
   resource_state_bytes,
   resource_execution_time
};

typedef fc::int_array< int64_t, MORPHENE_NUM_RESOURCE_TYPES > resource_count_type;

struct count_resources_result
{
   resource_count_type                            resource_count;
};

void count_resources(
   const morphene::protocol::signed_transaction& tx,
   count_resources_result& result );

} } } // morphene::plugins::rc

FC_REFLECT_ENUM( morphene::plugins::rc::rc_resource_types,
    (resource_history_bytes)
    (resource_new_accounts)
    (resource_market_bytes)
    (resource_state_bytes)
    (resource_execution_time)
   )

FC_REFLECT( morphene::plugins::rc::count_resources_result,
   (resource_count)
)

FC_REFLECT_TYPENAME( morphene::plugins::rc::resource_count_type )
