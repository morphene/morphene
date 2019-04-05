#pragma once
#include <morphene/protocol/base.hpp>
#include <morphene/protocol/block_header.hpp>
#include <morphene/protocol/asset.hpp>

#include <fc/utf8.hpp>

namespace morphene { namespace protocol {

   struct fill_vesting_withdraw_operation : public virtual_operation
   {
      fill_vesting_withdraw_operation(){}
      fill_vesting_withdraw_operation( const string& f, const string& t, const legacy_asset& w, const legacy_asset& d )
         :from_account(f), to_account(t), withdrawn(w), deposited(d) {}

      account_name_type from_account;
      account_name_type to_account;
      legacy_asset             withdrawn;
      legacy_asset             deposited;
   };


   struct shutdown_witness_operation : public virtual_operation
   {
      shutdown_witness_operation(){}
      shutdown_witness_operation( const string& o ):owner(o) {}

      account_name_type owner;
   };


   struct hardfork_operation : public virtual_operation
   {
      hardfork_operation() {}
      hardfork_operation( uint32_t hf_id ) : hardfork_id( hf_id ) {}

      uint32_t         hardfork_id = 0;
   };

   struct return_vesting_delegation_operation : public virtual_operation
   {
      return_vesting_delegation_operation() {}
      return_vesting_delegation_operation( const account_name_type& a, const legacy_asset& v ) : account( a ), vesting_shares( v ) {}

      account_name_type account;
      legacy_asset             vesting_shares;
   };

   struct producer_reward_operation : public virtual_operation
   {
      producer_reward_operation(){}
      producer_reward_operation( const string& p, const legacy_asset& v ) : producer( p ), vesting_shares( v ) {}

      account_name_type producer;
      legacy_asset             vesting_shares;

   };

   struct clear_null_account_balance_operation : public virtual_operation
   {
      vector< asset >   total_cleared;
   };

} } //morphene::protocol

FC_REFLECT( morphene::protocol::fill_vesting_withdraw_operation, (from_account)(to_account)(withdrawn)(deposited) )
FC_REFLECT( morphene::protocol::shutdown_witness_operation, (owner) )
FC_REFLECT( morphene::protocol::hardfork_operation, (hardfork_id) )
FC_REFLECT( morphene::protocol::return_vesting_delegation_operation, (account)(vesting_shares) )
FC_REFLECT( morphene::protocol::producer_reward_operation, (producer)(vesting_shares) )
FC_REFLECT( morphene::protocol::clear_null_account_balance_operation, (total_cleared) )
