
#include <morphene/plugins/rc/resource_count.hpp>
#include <morphene/plugins/rc/resource_sizes.hpp>

#include <morphene/protocol/operations.hpp>
#include <morphene/protocol/transaction.hpp>

namespace morphene { namespace plugins { namespace rc {

using namespace morphene::protocol;

struct count_operation_visitor
{
   typedef void result_type;

   mutable int32_t market_op_count = 0;
   mutable int32_t new_account_op_count = 0;
   mutable int64_t state_bytes_count = 0;
   mutable int64_t execution_time_count = 0;

   const state_object_size_info& _w;
   const operation_exec_info& _e;

   count_operation_visitor( const state_object_size_info& w, const operation_exec_info& e ) : _w(w), _e(e) {}

   int64_t get_authority_byte_count( const authority& auth )const
   {
      return _w.authority_base_size
           + _w.authority_account_member_size * auth.account_auths.size()
           + _w.authority_key_member_size * auth.key_auths.size();
   }

   void operator()( const account_create_operation& op )const
   {
      state_bytes_count +=
           _w.account_object_base_size
         + _w.account_authority_object_base_size
         + get_authority_byte_count( op.owner )
         + get_authority_byte_count( op.active )
         + get_authority_byte_count( op.posting );
      execution_time_count += _e.account_create_operation_exec_time;
   }

   void operator()( const account_create_with_delegation_operation& op )const
   {
      state_bytes_count +=
           _w.account_object_base_size
         + _w.account_authority_object_base_size
         + get_authority_byte_count( op.owner )
         + get_authority_byte_count( op.active )
         + get_authority_byte_count( op.posting )
         + _w.vesting_delegation_object_base_size;
      execution_time_count += _e.account_create_with_delegation_operation_exec_time;
   }

   void operator()( const account_witness_vote_operation& op )const
   {
      state_bytes_count += _w.witness_vote_object_base_size;
      execution_time_count += _e.account_witness_vote_operation_exec_time;
   }

   void operator()( const create_claimed_account_operation& op )const
   {
      state_bytes_count +=
           _w.account_object_base_size
         + _w.account_authority_object_base_size
         + get_authority_byte_count( op.owner )
         + get_authority_byte_count( op.active )
         + get_authority_byte_count( op.posting );
      execution_time_count += _e.create_claimed_account_operation_exec_time;
   }

   void operator()( const delegate_vesting_shares_operation& op )const
   {
      state_bytes_count += std::max(
         _w.vesting_delegation_object_base_size,
         _w.vesting_delegation_expiration_object_base_size
         );
      execution_time_count += _e.delegate_vesting_shares_operation_exec_time;
   }

   void operator()( const escrow_transfer_operation& op )const
   {
      state_bytes_count += _w.escrow_object_base_size;
      execution_time_count += _e.escrow_transfer_operation_exec_time;
   }

   void operator()( const request_account_recovery_operation& op )const
   {
      state_bytes_count += _w.account_recovery_request_object_base_size;
      execution_time_count += _e.request_account_recovery_operation_exec_time;
   }

   void operator()( const set_withdraw_vesting_route_operation& op )const
   {
      state_bytes_count += _w.withdraw_vesting_route_object_base_size;
      execution_time_count += _e.set_withdraw_vesting_route_operation_exec_time;
   }

   void operator()( const witness_update_operation& op )const
   {
      state_bytes_count +=
           _w.witness_object_base_size
         + _w.witness_object_url_char_size * op.url.size();
      execution_time_count += _e.witness_update_operation_exec_time;
   }

   void operator()( const transfer_operation& )const
   {
      execution_time_count += _e.transfer_operation_exec_time;
      market_op_count++;
   }

   void operator()( const transfer_to_vesting_operation& )const
   {
      execution_time_count += _e.transfer_to_vesting_operation_exec_time;
      market_op_count++;
   }

   void operator()( const withdraw_vesting_operation& op )const
   {
      execution_time_count += _e.withdraw_vesting_operation_exec_time;
   }

   void operator()( const account_update_operation& )const
   {
      execution_time_count += _e.account_update_operation_exec_time;
   }

   void operator()( const account_witness_proxy_operation& )const
   {
      execution_time_count += _e.account_witness_proxy_operation_exec_time;
   }

   void operator()( const change_recovery_account_operation& )const
   {
      execution_time_count += _e.change_recovery_account_operation_exec_time;
   }

   void operator()( const claim_account_operation& o )const
   {
      execution_time_count += _e.claim_account_operation_exec_time;

      if( o.fee.amount == 0 )
      {
         new_account_op_count++;
      }
   }

   void operator()( const custom_operation& )const
   {
      execution_time_count += _e.custom_operation_exec_time;
   }

   void operator()( const custom_json_operation& o )const
   {
      auto exec_time = _e.custom_operation_exec_time;

      if( o.id == "follow" )
      {
         exec_time *= EXEC_FOLLOW_CUSTOM_OP_SCALE;
      }

      execution_time_count += exec_time;
   }

   void operator()( const custom_binary_operation& o )const
   {
      auto exec_time = _e.custom_operation_exec_time;

      if( o.id == "follow" )
      {
         exec_time *= EXEC_FOLLOW_CUSTOM_OP_SCALE;
      }

      execution_time_count += exec_time;
   }

   void operator()( const escrow_approve_operation& )const
   {
      execution_time_count += _e.escrow_approve_operation_exec_time;
   }

   void operator()( const escrow_dispute_operation& )const
   {
      execution_time_count += _e.escrow_dispute_operation_exec_time;
   }

   void operator()( const escrow_release_operation& )const
   {
      execution_time_count += _e.escrow_release_operation_exec_time;
   }

   void operator()( const witness_set_properties_operation& )const
   {
      execution_time_count += _e.witness_set_properties_operation_exec_time;
   }

   void operator()( const recover_account_operation& ) const {}
   void operator()( const reset_account_operation& ) const {}
   void operator()( const set_reset_account_operation& ) const {}
   void operator()( const pow_operation& ) const {}

   // Virtual Ops
   void operator()( const fill_vesting_withdraw_operation& ) const {}
   void operator()( const shutdown_witness_operation& ) const {}
   void operator()( const hardfork_operation& ) const {}
   void operator()( const return_vesting_delegation_operation& ) const {}
   void operator()( const producer_reward_operation& ) const {}
   void operator()( const clear_null_account_balance_operation& ) const {}

};

void count_resources(
   const signed_transaction& tx,
   count_resources_result& result )
{
   static const state_object_size_info size_info;
   static const operation_exec_info exec_info;
   const int64_t tx_size = int64_t( fc::raw::pack_size( tx ) );
   count_operation_visitor vtor( size_info, exec_info );

   result.resource_count[ resource_history_bytes ] += tx_size;

   for( const operation& op : tx.operations )
   {
      op.visit( vtor );
   }

   result.resource_count[ resource_new_accounts ] += vtor.new_account_op_count;

   if( vtor.market_op_count > 0 )
      result.resource_count[ resource_market_bytes ] += tx_size;

   result.resource_count[ resource_state_bytes ] +=
        size_info.transaction_object_base_size
      + size_info.transaction_object_byte_size * tx_size
      + vtor.state_bytes_count;

   result.resource_count[ resource_execution_time ] += vtor.execution_time_count;
}

} } } // morphene::plugins::rc
