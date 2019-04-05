#include <morphene/protocol/morphene_operations.hpp>

#include <fc/macros.hpp>
#include <fc/io/json.hpp>
#include <fc/macros.hpp>

#include <locale>

namespace morphene { namespace protocol {

   void validate_auth_size( const authority& a )
   {
      size_t size = a.account_auths.size() + a.key_auths.size();
      FC_ASSERT( size <= MORPHENE_MAX_AUTHORITY_MEMBERSHIP, "Authority membership exceeded. Max: 10 Current: ${n}", ("n", size) );
   }

   void account_create_operation::validate() const
   {
      validate_account_name( new_account_name );
      FC_ASSERT( is_asset_type( fee, MORPH_SYMBOL ), "Account creation fee must be MORPH" );
      owner.validate();
      active.validate();

      if ( json_metadata.size() > 0 )
      {
         FC_ASSERT( fc::is_utf8(json_metadata), "JSON Metadata not formatted in UTF8" );
         FC_ASSERT( fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON" );
      }
      FC_ASSERT( fee >= asset( 0, MORPH_SYMBOL ), "Account creation fee cannot be negative" );
   }

   void account_create_with_delegation_operation::validate() const
   {
      validate_account_name( new_account_name );
      validate_account_name( creator );
      FC_ASSERT( is_asset_type( fee, MORPH_SYMBOL ), "Account creation fee must be MORPH" );
      FC_ASSERT( is_asset_type( delegation, VESTS_SYMBOL ), "Delegation must be VESTS" );

      owner.validate();
      active.validate();
      posting.validate();

      if( json_metadata.size() > 0 )
      {
         FC_ASSERT( fc::is_utf8(json_metadata), "JSON Metadata not formatted in UTF8" );
         FC_ASSERT( fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON" );
      }

      FC_ASSERT( fee >= asset( 0, MORPH_SYMBOL ), "Account creation fee cannot be negative" );
      FC_ASSERT( delegation >= asset( 0, VESTS_SYMBOL ), "Delegation cannot be negative" );
   }

   void account_update_operation::validate() const
   {
      validate_account_name( account );
      /*if( owner )
         owner->validate();
      if( active )
         active->validate();
      if( posting )
         posting->validate();*/

      if ( json_metadata.size() > 0 )
      {
         FC_ASSERT( fc::is_utf8(json_metadata), "JSON Metadata not formatted in UTF8" );
         FC_ASSERT( fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON" );
      }
   }

   void claim_account_operation::validate()const
   {
      validate_account_name( creator );
      FC_ASSERT( is_asset_type( fee, MORPH_SYMBOL ), "Account creation fee must be MORPH" );
      FC_ASSERT( fee >= asset( 0, MORPH_SYMBOL ), "Account creation fee cannot be negative" );
      FC_ASSERT( fee <= asset( MORPHENE_MAX_ACCOUNT_CREATION_FEE, MORPH_SYMBOL ), "Account creation fee cannot be too large" );

      FC_ASSERT( extensions.size() == 0, "There are no extensions for claim_account_operation." );
   }

   void create_claimed_account_operation::validate()const
   {
      validate_account_name( creator );
      validate_account_name( new_account_name );
      owner.validate();
      active.validate();
      posting.validate();
      validate_auth_size( owner );
      validate_auth_size( active );
      validate_auth_size( posting );

      if( json_metadata.size() > 0 )
      {
         FC_ASSERT( fc::is_utf8(json_metadata), "JSON Metadata not formatted in UTF8" );
         FC_ASSERT( fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON" );
      }

      FC_ASSERT( extensions.size() == 0, "There are no extensions for create_claimed_account_operation." );
   }

   void transfer_operation::validate() const
   { try {
      validate_account_name( from );
      validate_account_name( to );
      FC_ASSERT( amount.symbol != VESTS_SYMBOL, "transferring of VESTS is not allowed." );
      FC_ASSERT( amount.amount > 0, "Cannot transfer a negative amount (aka: stealing)" );
      FC_ASSERT( memo.size() < MORPHENE_MAX_MEMO_SIZE, "Memo is too large" );
      FC_ASSERT( fc::is_utf8( memo ), "Memo is not UTF8" );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void transfer_to_vesting_operation::validate() const
   {
      validate_account_name( from );
      FC_ASSERT( amount.symbol == MORPH_SYMBOL ||
                 ( amount.symbol.space() == asset_symbol_type::smt_nai_space && amount.symbol.is_vesting() == false ),
                 "Amount must be MORPH" );
      if ( to != account_name_type() ) validate_account_name( to );
      FC_ASSERT( amount.amount > 0, "Must transfer a nonzero amount" );
   }

   void withdraw_vesting_operation::validate() const
   {
      validate_account_name( account );
      FC_ASSERT( is_asset_type( vesting_shares, VESTS_SYMBOL), "Amount must be VESTS"  );
   }

   void set_withdraw_vesting_route_operation::validate() const
   {
      validate_account_name( from_account );
      validate_account_name( to_account );
      FC_ASSERT( 0 <= percent && percent <= MORPHENE_100_PERCENT, "Percent must be valid Morphene percent" );
   }

   void witness_update_operation::validate() const
   {
      validate_account_name( owner );

      FC_ASSERT( url.size() <= MORPHENE_MAX_WITNESS_URL_LENGTH, "URL is too long" );

      FC_ASSERT( url.size() > 0, "URL size must be greater than 0" );
      FC_ASSERT( fc::is_utf8( url ), "URL is not valid UTF8" );
      FC_ASSERT( fee >= asset( 0, MORPH_SYMBOL ), "Fee cannot be negative" );
   }

   void witness_set_properties_operation::validate() const
   {
      validate_account_name( owner );

      // current signing key must be present
      FC_ASSERT( props.find( "key" ) != props.end(), "No signing key provided" );

      auto itr = props.find( "account_creation_fee" );
      if( itr != props.end() )
      {
         asset account_creation_fee;
         fc::raw::unpack_from_vector( itr->second, account_creation_fee );
         FC_ASSERT( account_creation_fee.symbol == MORPH_SYMBOL, "account_creation_fee must be in MORPH" );
         FC_ASSERT( account_creation_fee.amount >= MORPHENE_MIN_ACCOUNT_CREATION_FEE, "account_creation_fee smaller than minimum account creation fee" );
      }

      itr = props.find( "maximum_block_size" );
      if( itr != props.end() )
      {
         uint32_t maximum_block_size;
         fc::raw::unpack_from_vector( itr->second, maximum_block_size );
         FC_ASSERT( maximum_block_size >= MORPHENE_MIN_BLOCK_SIZE_LIMIT, "maximum_block_size smaller than minimum max block size" );
      }

      itr = props.find( "new_signing_key" );
      if( itr != props.end() )
      {
         public_key_type signing_key;
         fc::raw::unpack_from_vector( itr->second, signing_key );
         FC_UNUSED( signing_key ); // This tests the deserialization of the key
      }

      itr = props.find( "url" );
      if( itr != props.end() )
      {
         std::string url;
         fc::raw::unpack_from_vector< std::string >( itr->second, url );

         FC_ASSERT( url.size() <= MORPHENE_MAX_WITNESS_URL_LENGTH, "URL is too long" );
         FC_ASSERT( url.size() > 0, "URL size must be greater than 0" );
         FC_ASSERT( fc::is_utf8( url ), "URL is not valid UTF8" );
      }

      itr = props.find( "account_subsidy_budget" );
      if( itr != props.end() )
      {
         int32_t account_subsidy_budget;
         fc::raw::unpack_from_vector( itr->second, account_subsidy_budget ); // Checks that the value can be deserialized
         FC_ASSERT( account_subsidy_budget >= MORPHENE_RD_MIN_BUDGET, "Budget must be at least ${n}", ("n", MORPHENE_RD_MIN_BUDGET) );
         FC_ASSERT( account_subsidy_budget <= MORPHENE_RD_MAX_BUDGET, "Budget must be at most ${n}", ("n", MORPHENE_RD_MAX_BUDGET) );
      }

      itr = props.find( "account_subsidy_decay" );
      if( itr != props.end() )
      {
         uint32_t account_subsidy_decay;
         fc::raw::unpack_from_vector( itr->second, account_subsidy_decay ); // Checks that the value can be deserialized
         FC_ASSERT( account_subsidy_decay >= MORPHENE_RD_MIN_DECAY, "Decay must be at least ${n}", ("n", MORPHENE_RD_MIN_DECAY) );
         FC_ASSERT( account_subsidy_decay <= MORPHENE_RD_MAX_DECAY, "Decay must be at most ${n}", ("n", MORPHENE_RD_MAX_DECAY) );
      }
   }

   void account_witness_vote_operation::validate() const
   {
      validate_account_name( account );
      validate_account_name( witness );
   }

   void account_witness_proxy_operation::validate() const
   {
      validate_account_name( account );
      if( proxy.size() )
         validate_account_name( proxy );
      FC_ASSERT( proxy != account, "Cannot proxy to self" );
   }

   void custom_operation::validate() const {
      /// required auth accounts are the ones whose bandwidth is consumed
      FC_ASSERT( required_auths.size() > 0, "at least one account must be specified" );
   }
   void custom_json_operation::validate() const {
      /// required auth accounts are the ones whose bandwidth is consumed
      FC_ASSERT( (required_auths.size() + required_posting_auths.size()) > 0, "at least one account must be specified" );
      FC_ASSERT( id.size() <= 32, "id is too long" );
      FC_ASSERT( fc::is_utf8(json), "JSON Metadata not formatted in UTF8" );
      FC_ASSERT( fc::json::is_valid(json), "JSON Metadata not valid JSON" );
   }
   void custom_binary_operation::validate() const {
      /// required auth accounts are the ones whose bandwidth is consumed
      FC_ASSERT( (required_owner_auths.size() + required_active_auths.size() + required_posting_auths.size()) > 0, "at least one account must be specified" );
      FC_ASSERT( id.size() <= 32, "id is too long" );
      for( const auto& a : required_auths ) a.validate();
   }

   void escrow_transfer_operation::validate()const
   {
      validate_account_name( from );
      validate_account_name( to );
      validate_account_name( agent );
      FC_ASSERT( fee.amount >= 0, "fee cannot be negative" );
      FC_ASSERT( morph_amount.amount >= 0, "morph amount cannot be negative" );
      FC_ASSERT( from != agent && to != agent, "agent must be a third party" );
      FC_ASSERT( fee.symbol == MORPH_SYMBOL, "fee must be MORPH" );
      FC_ASSERT( morph_amount.symbol == MORPH_SYMBOL, "morph amount must contain MORPH" );
      FC_ASSERT( ratification_deadline < escrow_expiration, "ratification deadline must be before escrow expiration" );
      if ( json_meta.size() > 0 )
      {
         FC_ASSERT( fc::is_utf8(json_meta), "JSON Metadata not formatted in UTF8" );
         FC_ASSERT( fc::json::is_valid(json_meta), "JSON Metadata not valid JSON" );
      }
   }

   void escrow_approve_operation::validate()const
   {
      validate_account_name( from );
      validate_account_name( to );
      validate_account_name( agent );
      validate_account_name( who );
      FC_ASSERT( who == to || who == agent, "to or agent must approve escrow" );
   }

   void escrow_dispute_operation::validate()const
   {
      validate_account_name( from );
      validate_account_name( to );
      validate_account_name( agent );
      validate_account_name( who );
      FC_ASSERT( who == from || who == to, "who must be from or to" );
   }

   void escrow_release_operation::validate()const
   {
      validate_account_name( from );
      validate_account_name( to );
      validate_account_name( agent );
      validate_account_name( who );
      validate_account_name( receiver );
      FC_ASSERT( who == from || who == to || who == agent, "who must be from or to or agent" );
      FC_ASSERT( receiver == from || receiver == to, "receiver must be from or to" );
      FC_ASSERT( morph_amount.amount >= 0, "morph amount cannot be negative" );
      FC_ASSERT( morph_amount.symbol == MORPH_SYMBOL, "morph amount must contain MORPH" );
   }

   void request_account_recovery_operation::validate()const
   {
      validate_account_name( recovery_account );
      validate_account_name( account_to_recover );
      new_owner_authority.validate();
   }

   void recover_account_operation::validate()const
   {
      validate_account_name( account_to_recover );
      FC_ASSERT( !( new_owner_authority == recent_owner_authority ), "Cannot set new owner authority to the recent owner authority" );
      FC_ASSERT( !new_owner_authority.is_impossible(), "new owner authority cannot be impossible" );
      FC_ASSERT( !recent_owner_authority.is_impossible(), "recent owner authority cannot be impossible" );
      FC_ASSERT( new_owner_authority.weight_threshold, "new owner authority cannot be trivial" );
      new_owner_authority.validate();
      recent_owner_authority.validate();
   }

   void change_recovery_account_operation::validate()const
   {
      validate_account_name( account_to_recover );
      validate_account_name( new_recovery_account );
   }

   void reset_account_operation::validate()const
   {
      validate_account_name( reset_account );
      validate_account_name( account_to_reset );
      FC_ASSERT( !new_owner_authority.is_impossible(), "new owner authority cannot be impossible" );
      FC_ASSERT( new_owner_authority.weight_threshold, "new owner authority cannot be trivial" );
      new_owner_authority.validate();
   }

   void set_reset_account_operation::validate()const
   {
      validate_account_name( account );
      if( current_reset_account.size() )
         validate_account_name( current_reset_account );
      validate_account_name( reset_account );
      FC_ASSERT( current_reset_account != reset_account, "new reset account cannot be current reset account" );
   }

   void delegate_vesting_shares_operation::validate()const
   {
      validate_account_name( delegator );
      validate_account_name( delegatee );
      FC_ASSERT( delegator != delegatee, "You cannot delegate VESTS to yourself" );
      FC_ASSERT( is_asset_type( vesting_shares, VESTS_SYMBOL ), "Delegation must be VESTS" );
      FC_ASSERT( vesting_shares >= asset( 0, VESTS_SYMBOL ), "Delegation cannot be negative" );
   }

} } // morphene::protocol
