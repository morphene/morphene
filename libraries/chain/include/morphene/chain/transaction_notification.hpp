#pragma once

#include <morphene/protocol/block.hpp>

namespace morphene { namespace chain {

struct transaction_notification
{
   transaction_notification( const morphene::protocol::signed_transaction& tx ) : transaction(tx)
   {
      transaction_id = tx.id();
   }

   morphene::protocol::transaction_id_type          transaction_id;
   const morphene::protocol::signed_transaction&    transaction;
};

} }
