#pragma once

#include <fc/container/flat.hpp>
#include <morphene/protocol/operations.hpp>
#include <morphene/protocol/transaction.hpp>

#include <fc/string.hpp>

namespace morphene { namespace app {

using namespace fc;

void operation_get_impacted_accounts(
   const morphene::protocol::operation& op,
   fc::flat_set<protocol::account_name_type>& result );

void transaction_get_impacted_accounts(
   const morphene::protocol::transaction& tx,
   fc::flat_set<protocol::account_name_type>& result
   );

} } // morphene::app
