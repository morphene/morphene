#include <morphene/chain/morphene_evaluator.hpp>
#include <morphene/chain/database.hpp>
#include <morphene/chain/custom_operation_interpreter.hpp>
#include <morphene/chain/morphene_objects.hpp>
#include <morphene/chain/witness_objects.hpp>
#include <morphene/chain/block_summary_object.hpp>

#include <morphene/chain/util/reward.hpp>
#include <morphene/chain/util/manabar.hpp>

#include <fc/macros.hpp>

#ifndef IS_LOW_MEM
// FC_TODO( "After we vendor fc, also vendor diff_match_patch and fix these warnings" )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic push
#if !defined( __clang__ )
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <diff_match_patch.h>
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#include <boost/locale/encoding_utf.hpp>

using boost::locale::conv::utf_to_utf;

std::wstring utf8_to_wstring(const std::string& str)
{
    return utf_to_utf<wchar_t>(str.c_str(), str.c_str() + str.size());
}

std::string wstring_to_utf8(const std::wstring& str)
{
    return utf_to_utf<char>(str.c_str(), str.c_str() + str.size());
}

#endif

#include <fc/uint128.hpp>
#include <fc/utf8.hpp>

#include <limits>

namespace morphene { namespace chain {
   using fc::uint128_t;

struct strcmp_equal
{
   bool operator()( const shared_string& a, const string& b )
   {
      return a.size() == b.size() || std::strcmp( a.c_str(), b.c_str() ) == 0;
   }
};

template< bool force_canon >
void copy_legacy_chain_properties( chain_properties& dest, const legacy_chain_properties& src )
{
   dest.account_creation_fee = src.account_creation_fee;
   dest.maximum_block_size = src.maximum_block_size;
}

void witness_update_evaluator::do_apply( const witness_update_operation& o )
{
   _db.get_account( o.owner ); // verify owner exists

   FC_ASSERT( o.props.account_creation_fee.amount <= MORPHENE_MAX_ACCOUNT_CREATION_FEE, "account_creation_fee greater than maximum account creation fee" );
   FC_ASSERT( o.props.maximum_block_size <= MORPHENE_SOFT_MAX_BLOCK_SIZE, "Max block size cannot be more than 2MiB" );

   const auto& by_witness_name_idx = _db.get_index< witness_index >().indices().get< by_name >();
   auto wit_itr = by_witness_name_idx.find( o.owner );
   if( wit_itr != by_witness_name_idx.end() )
   {
      _db.modify( *wit_itr, [&]( witness_object& w ) {
         from_string( w.url, o.url );
         w.signing_key        = o.block_signing_key;
         copy_legacy_chain_properties< false >( w.props, o.props );
      });
   }
   else
   {
      _db.create< witness_object >( [&]( witness_object& w ) {
         w.owner              = o.owner;
         from_string( w.url, o.url );
         w.signing_key        = o.block_signing_key;
         w.created            = _db.head_block_time();
         copy_legacy_chain_properties< false >( w.props, o.props );
      });
   }
}

struct witness_properties_change_flags
{
   uint32_t account_creation_changed       : 1;
   uint32_t max_block_changed              : 1;
   uint32_t account_subsidy_budget_changed : 1;
   uint32_t account_subsidy_decay_changed  : 1;
   uint32_t key_changed                    : 1;
   uint32_t url_changed                    : 1;
};

void witness_set_properties_evaluator::do_apply( const witness_set_properties_operation& o )
{
   const auto& witness = _db.get< witness_object, by_name >( o.owner ); // verifies witness exists;

   // Capture old properties. This allows only updating the object once.
   chain_properties  props;
   public_key_type   signing_key;
   string            url;

   witness_properties_change_flags flags;

   auto itr = o.props.find( "key" );

   // This existence of 'key' is checked in witness_set_properties_operation::validate
   fc::raw::unpack_from_vector( itr->second, signing_key );
   FC_ASSERT( signing_key == witness.signing_key, "'key' does not match witness signing key.",
      ("key", signing_key)("signing_key", witness.signing_key) );

   itr = o.props.find( "account_creation_fee" );
   flags.account_creation_changed = itr != o.props.end();
   if( flags.account_creation_changed )
   {
      fc::raw::unpack_from_vector( itr->second, props.account_creation_fee );
      if( _db.is_producing() )
      {
         FC_ASSERT( props.account_creation_fee.amount <= MORPHENE_MAX_ACCOUNT_CREATION_FEE, "account_creation_fee greater than maximum account creation fee" );
      }
   }

   itr = o.props.find( "maximum_block_size" );
   flags.max_block_changed = itr != o.props.end();
   if( flags.max_block_changed )
   {
      fc::raw::unpack_from_vector( itr->second, props.maximum_block_size );
   }

   itr = o.props.find( "account_subsidy_budget" );
   flags.account_subsidy_budget_changed = itr != o.props.end();
   if( flags.account_subsidy_budget_changed )
   {
      fc::raw::unpack_from_vector( itr->second, props.account_subsidy_budget );
   }

   itr = o.props.find( "account_subsidy_decay" );
   flags.account_subsidy_decay_changed = itr != o.props.end();
   if( flags.account_subsidy_decay_changed )
   {
      fc::raw::unpack_from_vector( itr->second, props.account_subsidy_decay );
   }

   itr = o.props.find( "new_signing_key" );
   flags.key_changed = itr != o.props.end();
   if( flags.key_changed )
   {
      fc::raw::unpack_from_vector( itr->second, signing_key );
   }

   itr = o.props.find( "url" );
   flags.url_changed = itr != o.props.end();
   if( flags.url_changed )
   {
      fc::raw::unpack_from_vector< std::string >( itr->second, url );
   }

   _db.modify( witness, [&]( witness_object& w )
   {
      if( flags.account_creation_changed )
      {
         w.props.account_creation_fee = props.account_creation_fee;
      }

      if( flags.max_block_changed )
      {
         w.props.maximum_block_size = props.maximum_block_size;
      }

      if( flags.account_subsidy_budget_changed )
      {
         w.props.account_subsidy_budget = props.account_subsidy_budget;
      }

      if( flags.account_subsidy_decay_changed )
      {
         w.props.account_subsidy_decay = props.account_subsidy_decay;
      }

      if( flags.key_changed )
      {
         w.signing_key = signing_key;
      }

      if( flags.url_changed )
      {
         from_string( w.url, url );
      }
   });
}

void verify_authority_accounts_exist(
   const database& db,
   const authority& auth,
   const account_name_type& auth_account,
   authority::classification auth_class)
{
   for( const std::pair< account_name_type, weight_type >& aw : auth.account_auths )
   {
      const account_object* a = db.find_account( aw.first );
      FC_ASSERT( a != nullptr, "New ${ac} authority on account ${aa} references non-existing account ${aref}",
         ("aref", aw.first)("ac", auth_class)("aa", auth_account) );
   }
}

void initialize_account_object( account_object& acc, const account_name_type& name, const public_key_type& key,
   const dynamic_global_property_object& props, const bool& mined, const account_name_type& recovery_account, uint32_t hardfork )
{
   acc.name = name;
   acc.mined = mined;
   acc.memo_key = key;
   acc.created = props.time;
   acc.voting_manabar.last_update_time = props.time.sec_since_epoch();

   if( recovery_account != MORPHENE_TEMP_ACCOUNT )
   {
      acc.recovery_account = recovery_account;
   }
}

void account_create_evaluator::do_apply( const account_create_operation& o )
{
   const auto& creator = _db.get_account( o.creator );

   const auto& props = _db.get_dynamic_global_properties();

   FC_ASSERT( creator.balance >= o.fee, "Insufficient balance to create account.", ( "creator.balance", creator.balance )( "required", o.fee ) );

   const witness_schedule_object& wso = _db.get_witness_schedule_object();

   if( _db.is_producing() )
   {
      FC_ASSERT( o.fee <= legacy_asset( MORPHENE_MAX_ACCOUNT_CREATION_FEE, MORPH_SYMBOL ), "Account creation fee cannot be too large" );
   }

   FC_ASSERT( o.fee == wso.median_props.account_creation_fee, "Must pay the exact account creation fee. paid: ${p} fee: ${f}",
               ("p", o.fee)
               ("f", wso.median_props.account_creation_fee) );

   if( _db.is_producing() )
   {
      validate_auth_size( o.owner );
      validate_auth_size( o.active );
      validate_auth_size( o.posting );
   }

   verify_authority_accounts_exist( _db, o.owner, o.new_account_name, authority::owner );
   verify_authority_accounts_exist( _db, o.active, o.new_account_name, authority::active );
   verify_authority_accounts_exist( _db, o.posting, o.new_account_name, authority::posting );

   _db.adjust_balance( creator, -o.fee );

   _db.adjust_balance( _db.get< account_object, by_name >( MORPHENE_NULL_ACCOUNT ), o.fee );

   _db.create< account_object >( [&]( account_object& acc )
   {
      initialize_account_object( acc, o.new_account_name, o.memo_key, props, false, o.creator, _db.get_hardfork() );
      #ifndef IS_LOW_MEM
         from_string( acc.json_metadata, o.json_metadata );
      #endif
   });

   _db.create< account_authority_object >( [&]( account_authority_object& auth )
   {
      auth.account = o.new_account_name;
      auth.owner = o.owner;
      auth.active = o.active;
      auth.posting = o.posting;
      auth.last_owner_update = fc::time_point_sec::min();
   });
}

void account_create_with_delegation_evaluator::do_apply( const account_create_with_delegation_operation& o )
{
   if( _db.is_producing() )
   {
      FC_ASSERT( o.fee <= legacy_asset( MORPHENE_MAX_ACCOUNT_CREATION_FEE, MORPH_SYMBOL ), "Account creation fee cannot be too large" );
   }

   const auto& creator = _db.get_account( o.creator );
   const auto& props = _db.get_dynamic_global_properties();
   const witness_schedule_object& wso = _db.get_witness_schedule_object();

   FC_ASSERT( creator.balance >= o.fee, "Insufficient balance to create account.",
               ( "creator.balance", creator.balance )
               ( "required", o.fee ) );

   FC_ASSERT( creator.vesting_shares - creator.delegated_vesting_shares - legacy_asset( creator.to_withdraw - creator.withdrawn, VESTS_SYMBOL ) >= o.delegation, "Insufficient vesting shares to delegate to new account.",
               ( "creator.vesting_shares", creator.vesting_shares )
               ( "creator.delegated_vesting_shares", creator.delegated_vesting_shares )( "required", o.delegation ) );

   auto target_delegation = legacy_asset( wso.median_props.account_creation_fee.amount * MORPHENE_CREATE_ACCOUNT_WITH_MORPHENE_MODIFIER * MORPHENE_CREATE_ACCOUNT_DELEGATION_RATIO, MORPH_SYMBOL ) * props.get_vesting_share_price();

   auto current_delegation = legacy_asset( o.fee.amount * MORPHENE_CREATE_ACCOUNT_DELEGATION_RATIO, MORPH_SYMBOL ) * props.get_vesting_share_price() + o.delegation;

   FC_ASSERT( current_delegation >= target_delegation, "Inssufficient Delegation ${f} required, ${p} provided.",
               ("f", target_delegation )
               ( "p", current_delegation )
               ( "account_creation_fee", wso.median_props.account_creation_fee )
               ( "o.fee", o.fee )
               ( "o.delegation", o.delegation ) );

   FC_ASSERT( o.fee >= wso.median_props.account_creation_fee, "Insufficient Fee: ${f} required, ${p} provided.",
               ("f", wso.median_props.account_creation_fee)
               ("p", o.fee) );

   if( _db.is_producing() )
   {
      validate_auth_size( o.owner );
      validate_auth_size( o.active );
      validate_auth_size( o.posting );
   }

   for( const auto& a : o.owner.account_auths )
   {
      _db.get_account( a.first );
   }

   for( const auto& a : o.active.account_auths )
   {
      _db.get_account( a.first );
   }

   for( const auto& a : o.posting.account_auths )
   {
      _db.get_account( a.first );
   }

   _db.modify( creator, [&]( account_object& c )
   {
      c.balance -= o.fee;
      c.delegated_vesting_shares += o.delegation;
   });

   _db.adjust_balance( _db.get< account_object, by_name >( MORPHENE_NULL_ACCOUNT ), o.fee );

   _db.create< account_object >( [&]( account_object& acc )
   {
      initialize_account_object( acc, o.new_account_name, o.memo_key, props, false, o.creator, _db.get_hardfork() );
      acc.received_vesting_shares = o.delegation;

      #ifndef IS_LOW_MEM
         from_string( acc.json_metadata, o.json_metadata );
      #endif
   });

   _db.create< account_authority_object >( [&]( account_authority_object& auth )
   {
      auth.account = o.new_account_name;
      auth.owner = o.owner;
      auth.active = o.active;
      auth.posting = o.posting;
      auth.last_owner_update = fc::time_point_sec::min();
   });

   if( o.delegation.amount > 0 )
   {
      _db.create< vesting_delegation_object >( [&]( vesting_delegation_object& vdo )
      {
         vdo.delegator = o.creator;
         vdo.delegatee = o.new_account_name;
         vdo.vesting_shares = o.delegation;
         vdo.min_delegation_time = _db.head_block_time() + MORPHENE_CREATE_ACCOUNT_DELEGATION_TIME;
      });
   }
}


void account_update_evaluator::do_apply( const account_update_operation& o )
{
   FC_ASSERT( o.account != MORPHENE_TEMP_ACCOUNT, "Cannot update temp account." );

   if( o.posting )
      o.posting->validate();

   const auto& account = _db.get_account( o.account );
   const auto& account_auth = _db.get< account_authority_object, by_account >( o.account );

   if( _db.is_producing() )
   {
      if( o.owner )
         validate_auth_size( *o.owner );
      if( o.active )
         validate_auth_size( *o.active );
      if( o.posting )
         validate_auth_size( *o.posting );
   }

   if( o.owner )
   {
#ifndef IS_TEST_NET
      FC_ASSERT( _db.head_block_time() - account_auth.last_owner_update > MORPHENE_OWNER_UPDATE_LIMIT, "Owner authority can only be updated once an hour." );
#endif

      verify_authority_accounts_exist( _db, *o.owner, o.account, authority::owner );

      _db.update_owner_authority( account, *o.owner );
   }
   if( o.active )
      verify_authority_accounts_exist( _db, *o.active, o.account, authority::active );
   if( o.posting )
      verify_authority_accounts_exist( _db, *o.posting, o.account, authority::posting );

   _db.modify( account, [&]( account_object& acc )
   {
      if( o.memo_key != public_key_type() )
            acc.memo_key = o.memo_key;

      acc.last_account_update = _db.head_block_time();

      #ifndef IS_LOW_MEM
        if ( o.json_metadata.size() > 0 )
            from_string( acc.json_metadata, o.json_metadata );
      #endif
   });

   if( o.active || o.posting )
   {
      _db.modify( account_auth, [&]( account_authority_object& auth)
      {
         if( o.active )  auth.active  = *o.active;
         if( o.posting ) auth.posting = *o.posting;
      });
   }

}

void escrow_transfer_evaluator::do_apply( const escrow_transfer_operation& o )
{
   try
   {
      const auto& from_account = _db.get_account(o.from);
      _db.get_account(o.to);
      _db.get_account(o.agent);

      FC_ASSERT( o.ratification_deadline > _db.head_block_time(), "The escorw ratification deadline must be after head block time." );
      FC_ASSERT( o.escrow_expiration > _db.head_block_time(), "The escrow expiration must be after head block time." );

      legacy_asset morph_spent = o.morph_amount;
      if( o.fee.symbol == MORPH_SYMBOL )
         morph_spent += o.fee;

      FC_ASSERT( from_account.balance >= morph_spent, "Account cannot cover MORPH costs of escrow. Required: ${r} Available: ${a}", ("r",morph_spent)("a",from_account.balance) );

      _db.adjust_balance( from_account, -morph_spent );

      _db.create<escrow_object>([&]( escrow_object& esc )
      {
         esc.escrow_id              = o.escrow_id;
         esc.from                   = o.from;
         esc.to                     = o.to;
         esc.agent                  = o.agent;
         esc.ratification_deadline  = o.ratification_deadline;
         esc.escrow_expiration      = o.escrow_expiration;
         esc.morph_balance          = o.morph_amount;
         esc.pending_fee            = o.fee;
      });
   }
   FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_approve_evaluator::do_apply( const escrow_approve_operation& o )
{
   try
   {

      const auto& escrow = _db.get_escrow( o.from, o.escrow_id );

      FC_ASSERT( escrow.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", escrow.to) );
      FC_ASSERT( escrow.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", escrow.agent) );
      FC_ASSERT( escrow.ratification_deadline >= _db.head_block_time(), "The escrow ratification deadline has passed. Escrow can no longer be ratified." );

      bool reject_escrow = !o.approve;

      if( o.who == o.to )
      {
         FC_ASSERT( !escrow.to_approved, "Account 'to' (${t}) has already approved the escrow.", ("t", o.to) );

         if( !reject_escrow )
         {
            _db.modify( escrow, [&]( escrow_object& esc )
            {
               esc.to_approved = true;
            });
         }
      }
      if( o.who == o.agent )
      {
         FC_ASSERT( !escrow.agent_approved, "Account 'agent' (${a}) has already approved the escrow.", ("a", o.agent) );

         if( !reject_escrow )
         {
            _db.modify( escrow, [&]( escrow_object& esc )
            {
               esc.agent_approved = true;
            });
         }
      }

      if( reject_escrow )
      {
         _db.adjust_balance( o.from, escrow.morph_balance );
         _db.adjust_balance( o.from, escrow.pending_fee );

         _db.remove( escrow );
      }
      else if( escrow.to_approved && escrow.agent_approved )
      {
         _db.adjust_balance( o.agent, escrow.pending_fee );

         _db.modify( escrow, [&]( escrow_object& esc )
         {
            esc.pending_fee.amount = 0;
         });
      }
   }
   FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_dispute_evaluator::do_apply( const escrow_dispute_operation& o )
{
   try
   {
      _db.get_account( o.from ); // Verify from account exists

      const auto& e = _db.get_escrow( o.from, o.escrow_id );
      FC_ASSERT( _db.head_block_time() < e.escrow_expiration, "Disputing the escrow must happen before expiration." );
      FC_ASSERT( e.to_approved && e.agent_approved, "The escrow must be approved by all parties before a dispute can be raised." );
      FC_ASSERT( !e.disputed, "The escrow is already under dispute." );
      FC_ASSERT( e.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.to) );
      FC_ASSERT( e.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.agent) );

      _db.modify( e, [&]( escrow_object& esc )
      {
         esc.disputed = true;
      });
   }
   FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_release_evaluator::do_apply( const escrow_release_operation& o )
{
   try
   {
      _db.get_account(o.from); // Verify from account exists

      const auto& e = _db.get_escrow( o.from, o.escrow_id );
      FC_ASSERT( e.morph_balance >= o.morph_amount, "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}", ("a", o.morph_amount)("b", e.morph_balance) );
      FC_ASSERT( e.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.to) );
      FC_ASSERT( e.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.agent) );
      FC_ASSERT( o.receiver == e.from || o.receiver == e.to, "Funds must be released to 'from' (${f}) or 'to' (${t})", ("f", e.from)("t", e.to) );
      FC_ASSERT( e.to_approved && e.agent_approved, "Funds cannot be released prior to escrow approval." );

      // If there is a dispute regardless of expiration, the agent can release funds to either party
      if( e.disputed )
      {
         FC_ASSERT( o.who == e.agent, "Only 'agent' (${a}) can release funds in a disputed escrow.", ("a", e.agent) );
      }
      else
      {
         FC_ASSERT( o.who == e.from || o.who == e.to, "Only 'from' (${f}) and 'to' (${t}) can release funds from a non-disputed escrow", ("f", e.from)("t", e.to) );

         if( e.escrow_expiration > _db.head_block_time() )
         {
            // If there is no dispute and escrow has not expired, either party can release funds to the other.
            if( o.who == e.from )
            {
               FC_ASSERT( o.receiver == e.to, "Only 'from' (${f}) can release funds to 'to' (${t}).", ("f", e.from)("t", e.to) );
            }
            else if( o.who == e.to )
            {
               FC_ASSERT( o.receiver == e.from, "Only 'to' (${t}) can release funds to 'from' (${t}).", ("f", e.from)("t", e.to) );
            }
         }
      }
      // If escrow expires and there is no dispute, either party can release funds to either party.

      _db.adjust_balance( o.receiver, o.morph_amount );

      _db.modify( e, [&]( escrow_object& esc )
      {
         esc.morph_balance -= o.morph_amount;
      });

      if( e.morph_balance.amount == 0 )
      {
         _db.remove( e );
      }
   }
   FC_CAPTURE_AND_RETHROW( (o) )
}

void transfer_evaluator::do_apply( const transfer_operation& o )
{
   FC_ASSERT( _db.get_balance( o.from, o.amount.symbol ) >= o.amount, "Account does not have sufficient funds for transfer." );
   _db.adjust_balance( o.from, -o.amount );
   _db.adjust_balance( o.to, o.amount );
}

void transfer_to_vesting_evaluator::do_apply( const transfer_to_vesting_operation& o )
{
   const auto& from_account = _db.get_account(o.from);
   const auto& to_account = o.to.size() ? _db.get_account(o.to) : from_account;

   FC_ASSERT( _db.get_balance( from_account, o.amount.symbol) >= o.amount,
              "Account does not have sufficient liquid amount for transfer." );
   _db.adjust_balance( from_account, -o.amount );
   _db.create_vesting( to_account, o.amount );
}

void withdraw_vesting_evaluator::do_apply( const withdraw_vesting_operation& o )
{
   const auto& account = _db.get_account( o.account );

   if( o.vesting_shares.amount < 0 )
   {
      // TODO: Update this to a HF 20 check
#ifndef IS_TEST_NET
      if( _db.head_block_num() > 23847548 )
      {
#endif
         FC_ASSERT( false, "Cannot withdraw negative VESTS. account: ${account}, vests:${vests}",
            ("account", o.account)("vests", o.vesting_shares) );
#ifndef IS_TEST_NET
      }
#endif

      // else, no-op
      return;
   }


   FC_ASSERT( account.vesting_shares >= legacy_asset( 0, VESTS_SYMBOL ), "Account does not have sufficient VESTS for withdraw." );
   FC_ASSERT( account.vesting_shares - account.delegated_vesting_shares >= o.vesting_shares, "Account does not have sufficient VESTS for withdraw." );

   if( o.vesting_shares.amount == 0 )
   {
      FC_ASSERT( account.vesting_withdraw_rate.amount  != 0, "This operation would not change the vesting withdraw rate." );

      _db.modify( account, [&]( account_object& a ) {
         a.vesting_withdraw_rate = legacy_asset( 0, VESTS_SYMBOL );
         a.next_vesting_withdrawal = time_point_sec::maximum();
         a.to_withdraw = 0;
         a.withdrawn = 0;
      });
   }
   else
   {
      auto vesting_withdraw_intervals = MORPHENE_VESTING_WITHDRAW_INTERVALS; /// 13 weeks = 1 quarter of a year

      _db.modify( account, [&]( account_object& a )
      {
         auto new_vesting_withdraw_rate = legacy_asset( o.vesting_shares.amount / vesting_withdraw_intervals, VESTS_SYMBOL );

         if( new_vesting_withdraw_rate.amount == 0 )
            new_vesting_withdraw_rate.amount = 1;

         FC_ASSERT( account.vesting_withdraw_rate  != new_vesting_withdraw_rate, "This operation would not change the vesting withdraw rate." );

         a.vesting_withdraw_rate = new_vesting_withdraw_rate;
         a.next_vesting_withdrawal = _db.head_block_time() + fc::seconds(MORPHENE_VESTING_WITHDRAW_INTERVAL_SECONDS);
         a.to_withdraw = o.vesting_shares.amount;
         a.withdrawn = 0;
      });
   }
}

void set_withdraw_vesting_route_evaluator::do_apply( const set_withdraw_vesting_route_operation& o )
{
   try
   {
   const auto& from_account = _db.get_account( o.from_account );
   const auto& to_account = _db.get_account( o.to_account );
   const auto& wd_idx = _db.get_index< withdraw_vesting_route_index >().indices().get< by_withdraw_route >();
   auto itr = wd_idx.find( boost::make_tuple( from_account.name, to_account.name ) );

   if( itr == wd_idx.end() )
   {
      FC_ASSERT( o.percent != 0, "Cannot create a 0% destination." );
      FC_ASSERT( from_account.withdraw_routes < MORPHENE_MAX_WITHDRAW_ROUTES, "Account already has the maximum number of routes." );

      _db.create< withdraw_vesting_route_object >( [&]( withdraw_vesting_route_object& wvdo )
      {
         wvdo.from_account = from_account.name;
         wvdo.to_account = to_account.name;
         wvdo.percent = o.percent;
         wvdo.auto_vest = o.auto_vest;
      });

      _db.modify( from_account, [&]( account_object& a )
      {
         a.withdraw_routes++;
      });
   }
   else if( o.percent == 0 )
   {
      _db.remove( *itr );

      _db.modify( from_account, [&]( account_object& a )
      {
         a.withdraw_routes--;
      });
   }
   else
   {
      _db.modify( *itr, [&]( withdraw_vesting_route_object& wvdo )
      {
         wvdo.from_account = from_account.name;
         wvdo.to_account = to_account.name;
         wvdo.percent = o.percent;
         wvdo.auto_vest = o.auto_vest;
      });
   }

   itr = wd_idx.upper_bound( boost::make_tuple( from_account.name, account_name_type() ) );
   uint16_t total_percent = 0;

   while( itr->from_account == from_account.name && itr != wd_idx.end() )
   {
      total_percent += itr->percent;
      ++itr;
   }

   FC_ASSERT( total_percent <= MORPHENE_100_PERCENT, "More than 100% of vesting withdrawals allocated to destinations." );
   }
   FC_CAPTURE_AND_RETHROW()
}

void account_witness_proxy_evaluator::do_apply( const account_witness_proxy_operation& o )
{
   const auto& account = _db.get_account( o.account );
   FC_ASSERT( account.proxy != o.proxy, "Proxy must change." );

   FC_ASSERT( account.can_vote, "Account has declined the ability to vote and cannot proxy votes." );

   /// remove all current votes
   std::array<share_type, MORPHENE_MAX_PROXY_RECURSION_DEPTH+1> delta;
   delta[0] = -account.vesting_shares.amount;
   for( int i = 0; i < MORPHENE_MAX_PROXY_RECURSION_DEPTH; ++i )
      delta[i+1] = -account.proxied_vsf_votes[i];
   _db.adjust_proxied_witness_votes( account, delta );

   if( o.proxy.size() ) {
      const auto& new_proxy = _db.get_account( o.proxy );
      flat_set<account_id_type> proxy_chain( { account.id, new_proxy.id } );
      proxy_chain.reserve( MORPHENE_MAX_PROXY_RECURSION_DEPTH + 1 );

      /// check for proxy loops and fail to update the proxy if it would create a loop
      auto cprox = &new_proxy;
      while( cprox->proxy.size() != 0 ) {
         const auto next_proxy = _db.get_account( cprox->proxy );
         FC_ASSERT( proxy_chain.insert( next_proxy.id ).second, "This proxy would create a proxy loop." );
         cprox = &next_proxy;
         FC_ASSERT( proxy_chain.size() <= MORPHENE_MAX_PROXY_RECURSION_DEPTH, "Proxy chain is too long." );
      }

      /// clear all individual vote records
      _db.clear_witness_votes( account );

      _db.modify( account, [&]( account_object& a ) {
         a.proxy = o.proxy;
      });

      /// add all new votes
      for( int i = 0; i <= MORPHENE_MAX_PROXY_RECURSION_DEPTH; ++i )
         delta[i] = -delta[i];
      _db.adjust_proxied_witness_votes( account, delta );
   } else { /// we are clearing the proxy which means we simply update the account
      _db.modify( account, [&]( account_object& a ) {
          a.proxy = o.proxy;
      });
   }
}


void account_witness_vote_evaluator::do_apply( const account_witness_vote_operation& o )
{
   const auto& voter = _db.get_account( o.account );
   FC_ASSERT( voter.proxy.size() == 0, "A proxy is currently set, please clear the proxy before voting for a witness." );

   if( o.approve )
      FC_ASSERT( voter.can_vote, "Account has declined its voting rights." );

   const auto& witness = _db.get_witness( o.witness );

   const auto& by_account_witness_idx = _db.get_index< witness_vote_index >().indices().get< by_account_witness >();
   auto itr = by_account_witness_idx.find( boost::make_tuple( voter.name, witness.owner ) );

   if( itr == by_account_witness_idx.end() ) {
      FC_ASSERT( o.approve, "Vote doesn't exist, user must indicate a desire to approve witness." );
      FC_ASSERT( voter.witnesses_voted_for < MORPHENE_MAX_ACCOUNT_WITNESS_VOTES, "Account has voted for too many witnesses." ); // TODO: Remove after hardfork 2

      _db.create<witness_vote_object>( [&]( witness_vote_object& v ) {
          v.witness = witness.owner;
          v.account = voter.name;
      });

      _db.adjust_witness_vote( witness, voter.witness_vote_weight() );
      _db.modify( voter, [&]( account_object& a ) {
         a.witnesses_voted_for++;
      });

   } else {
      FC_ASSERT( !o.approve, "Vote currently exists, user must indicate a desire to reject witness." );

      _db.adjust_witness_vote( witness, -voter.witness_vote_weight() );
      _db.modify( voter, [&]( account_object& a ) {
         a.witnesses_voted_for--;
      });
      _db.remove( *itr );
   }
}

void custom_evaluator::do_apply( const custom_operation& o )
{
   database& d = db();
   if( d.is_producing() )
      FC_ASSERT( o.data.size() <= 8192, "custom_operation must be less than 8k" );

   FC_ASSERT( o.required_auths.size() <= MORPHENE_MAX_AUTHORITY_MEMBERSHIP, "Too many auths specified. Max: ${m}, Current: ${n}", ("m",MORPHENE_MAX_AUTHORITY_MEMBERSHIP)("n", o.required_auths.size()) );
}

void custom_json_evaluator::do_apply( const custom_json_operation& o )
{
   database& d = db();

   if( d.is_producing() )
      FC_ASSERT( o.json.length() <= 8192, "custom_json_operation json must be less than 8k" );

   size_t num_auths = o.required_auths.size() + o.required_posting_auths.size();
   FC_ASSERT( num_auths <= MORPHENE_MAX_AUTHORITY_MEMBERSHIP, "Too many auths specified. Max: ${m}, Current: ${n}", ("m",MORPHENE_MAX_AUTHORITY_MEMBERSHIP)("n", num_auths) );

   std::shared_ptr< custom_operation_interpreter > eval = d.get_custom_json_evaluator( o.id );
   if( !eval )
      return;

   try
   {
      eval->apply( o );
   }
   catch( const fc::exception& e )
   {
      if( d.is_producing() )
         throw e;
   }
   catch(...)
   {
      elog( "Unexpected exception applying custom json evaluator." );
   }
}


void custom_binary_evaluator::do_apply( const custom_binary_operation& o )
{
   database& d = db();
   if( d.is_producing() )
   {
      FC_ASSERT( o.data.size() <= 8192, "custom_binary_operation data must be less than 8k" );
      FC_ASSERT( false, "custom_binary_operation is deprecated" );
   }

   size_t num_auths = o.required_owner_auths.size() + o.required_active_auths.size() + o.required_posting_auths.size();
   for( const auto& auth : o.required_auths )
   {
      num_auths += auth.key_auths.size() + auth.account_auths.size();
   }

   FC_ASSERT( num_auths <= MORPHENE_MAX_AUTHORITY_MEMBERSHIP, "Too many auths specified. Max: 10, Current: ${n}", ("n", num_auths) );

   std::shared_ptr< custom_operation_interpreter > eval = d.get_custom_json_evaluator( o.id );
   if( !eval )
      return;

   try
   {
      eval->apply( o );
   }
   catch( const fc::exception& e )
   {
      if( d.is_producing() )
         throw e;
   }
   catch(...)
   {
      elog( "Unexpected exception applying custom json evaluator." );
   }
}

void pow_evaluator::do_apply( const pow_operation& o )
{
   database& db = this->db();

   const auto& dgp = db.get_dynamic_global_properties();
   uint32_t target_pow = db.get_pow_summary_target();
   account_name_type worker_account;

   const auto& work = o.work.get< pow >();
   FC_ASSERT( work.input.prev_block == db.head_block_id(), "pow not for last block" );
   FC_ASSERT( work.pow_summary < target_pow, "insufficient work difficulty", ("pow", work.pow_summary)("target", target_pow) );
   worker_account = work.input.worker_account;

   FC_ASSERT( o.props.maximum_block_size >= MORPHENE_MIN_BLOCK_SIZE_LIMIT * 2, "Voted maximum block size is too small." );

   db.modify( dgp, [&]( dynamic_global_property_object& p )
   {
      p.total_pow++;
      p.num_pow_witnesses++;
   });

   const auto& accounts_by_name = db.get_index<account_index>().indices().get<by_name>();
   auto itr = accounts_by_name.find( worker_account );
   if(itr == accounts_by_name.end())
   {
      FC_ASSERT( o.new_owner_key.valid(), "New owner key is not valid." );
      db.create< account_object >( [&]( account_object& acc )
      {
         initialize_account_object( acc, worker_account, *o.new_owner_key, dgp, true /*mined*/, account_name_type(), _db.get_hardfork() );
         // ^ empty recovery account parameter means highest voted witness at time of recovery
      });

      db.create< account_authority_object >( [&]( account_authority_object& auth )
      {
         auth.account = worker_account;
         auth.owner = authority( 1, *o.new_owner_key, 1);
         auth.active = auth.owner;
         auth.posting = auth.owner;
      });

      db.create<witness_object>( [&]( witness_object& w )
      {
          w.owner             = worker_account;
          copy_legacy_chain_properties< true >( w.props, o.props );
          w.signing_key       = *o.new_owner_key;
          w.pow_worker        = dgp.total_pow;
      });
   }
   else
   {
      FC_ASSERT( !o.new_owner_key.valid(), "Cannot specify an owner key unless creating account." );
      const witness_object* cur_witness = db.find_witness( worker_account );
      FC_ASSERT( cur_witness, "Witness must be created for existing account before mining.");
      FC_ASSERT( cur_witness->pow_worker == 0, "This account is already scheduled for pow block production." );
      db.modify(*cur_witness, [&]( witness_object& w )
      {
          copy_legacy_chain_properties< true >( w.props, o.props );
          w.pow_worker        = dgp.total_pow;
      });
   }
}

void claim_account_evaluator::do_apply( const claim_account_operation& o )
{
   const auto& creator = _db.get_account( o.creator );
   const auto& wso = _db.get_witness_schedule_object();

   FC_ASSERT( creator.balance >= o.fee, "Insufficient balance to create account.", ( "creator.balance", creator.balance )( "required", o.fee ) );

   if( o.fee.amount == 0 )
   {
      const auto& gpo = _db.get_dynamic_global_properties();

      // This block is a little weird. We want to enforce that only elected witnesses can include the transaction, but
      // we do not want to prevent the transaction from propogating on the p2p network. Because we do not know what type of
      // witness will have produced the including block when the tx is broadcast, we need to disregard this assertion when the tx
      // is propogating, but require it when applying the block.
      if( !_db.is_pending_tx() )
      {
         const auto& current_witness = _db.get_witness( gpo.current_witness );
         FC_ASSERT( current_witness.schedule == witness_object::elected, "Subsidized accounts can only be claimed by elected witnesses. current_witness:${w} witness_type:${t}",
            ("w",current_witness.owner)("t",current_witness.schedule) );

         FC_ASSERT( current_witness.available_witness_account_subsidies >= MORPHENE_ACCOUNT_SUBSIDY_PRECISION, "Witness ${w} does not have enough subsidized accounts to claim",
            ("w", current_witness.owner) );

         _db.modify( current_witness, [&]( witness_object& w )
         {
            w.available_witness_account_subsidies -= MORPHENE_ACCOUNT_SUBSIDY_PRECISION;
         });
      }

      FC_ASSERT( gpo.available_account_subsidies >= MORPHENE_ACCOUNT_SUBSIDY_PRECISION, "There are not enough subsidized accounts to claim" );

      _db.modify( gpo, [&]( dynamic_global_property_object& gpo )
      {
         gpo.available_account_subsidies -= MORPHENE_ACCOUNT_SUBSIDY_PRECISION;
      });
   }
   else
   {
      FC_ASSERT( o.fee == wso.median_props.account_creation_fee,
         "Cannot pay more than account creation fee. paid: ${p} fee: ${f}",
         ("p", o.fee.amount.value)
         ("f", wso.median_props.account_creation_fee) );
   }

   _db.adjust_balance( _db.get_account( MORPHENE_NULL_ACCOUNT ), o.fee );

   _db.modify( creator, [&]( account_object& a )
   {
      a.balance -= o.fee;
      a.pending_claimed_accounts++;
   });
}

void create_claimed_account_evaluator::do_apply( const create_claimed_account_operation& o )
{
   const auto& creator = _db.get_account( o.creator );
   const auto& props = _db.get_dynamic_global_properties();

   FC_ASSERT( creator.pending_claimed_accounts > 0, "${creator} has no claimed accounts to create", ( "creator", o.creator ) );

   verify_authority_accounts_exist( _db, o.owner, o.new_account_name, authority::owner );
   verify_authority_accounts_exist( _db, o.active, o.new_account_name, authority::active );
   verify_authority_accounts_exist( _db, o.posting, o.new_account_name, authority::posting );

   _db.modify( creator, [&]( account_object& a )
   {
      a.pending_claimed_accounts--;
   });

   _db.create< account_object >( [&]( account_object& acc )
   {
      initialize_account_object( acc, o.new_account_name, o.memo_key, props, false, o.creator, _db.get_hardfork() );
      #ifndef IS_LOW_MEM
         from_string( acc.json_metadata, o.json_metadata );
      #endif
   });

   _db.create< account_authority_object >( [&]( account_authority_object& auth )
   {
      auth.account = o.new_account_name;
      auth.owner = o.owner;
      auth.active = o.active;
      auth.posting = o.posting;
      auth.last_owner_update = fc::time_point_sec::min();
   });

}

void request_account_recovery_evaluator::do_apply( const request_account_recovery_operation& o )
{
   const auto& account_to_recover = _db.get_account( o.account_to_recover );

   if ( account_to_recover.recovery_account.length() )   // Make sure recovery matches expected recovery account
   {
      FC_ASSERT( account_to_recover.recovery_account == o.recovery_account, "Cannot recover an account that does not have you as there recovery partner." );
      if( o.recovery_account == MORPHENE_TEMP_ACCOUNT )
         wlog( "Recovery by temp account" );
   }
   else                                                  // Empty string recovery account defaults to top witness
      FC_ASSERT( _db.get_index< witness_index >().indices().get< by_vote_name >().begin()->owner == o.recovery_account, "Top witness must recover an account with no recovery partner." );

   const auto& recovery_request_idx = _db.get_index< account_recovery_request_index >().indices().get< by_account >();
   auto request = recovery_request_idx.find( o.account_to_recover );

   if( request == recovery_request_idx.end() ) // New Request
   {
      FC_ASSERT( !o.new_owner_authority.is_impossible(), "Cannot recover using an impossible authority." );
      FC_ASSERT( o.new_owner_authority.weight_threshold, "Cannot recover using an open authority." );

      validate_auth_size( o.new_owner_authority );

      // Check accounts in the new authority exist
      for( auto& a : o.new_owner_authority.account_auths )
      {
         _db.get_account( a.first );
      }

      _db.create< account_recovery_request_object >( [&]( account_recovery_request_object& req )
      {
         req.account_to_recover = o.account_to_recover;
         req.new_owner_authority = o.new_owner_authority;
         req.expires = _db.head_block_time() + MORPHENE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD;
      });
   }
   else if( o.new_owner_authority.weight_threshold == 0 ) // Cancel Request if authority is open
   {
      _db.remove( *request );
   }
   else // Change Request
   {
      FC_ASSERT( !o.new_owner_authority.is_impossible(), "Cannot recover using an impossible authority." );

      // Check accounts in the new authority exist
      for( auto& a : o.new_owner_authority.account_auths )
      {
         _db.get_account( a.first );
      }

      _db.modify( *request, [&]( account_recovery_request_object& req )
      {
         req.new_owner_authority = o.new_owner_authority;
         req.expires = _db.head_block_time() + MORPHENE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD;
      });
   }
}

void recover_account_evaluator::do_apply( const recover_account_operation& o )
{
   const auto& account = _db.get_account( o.account_to_recover );

   FC_ASSERT( _db.head_block_time() - account.last_account_recovery > MORPHENE_OWNER_UPDATE_LIMIT, "Owner authority can only be updated once an hour." );

   const auto& recovery_request_idx = _db.get_index< account_recovery_request_index >().indices().get< by_account >();
   auto request = recovery_request_idx.find( o.account_to_recover );

   FC_ASSERT( request != recovery_request_idx.end(), "There are no active recovery requests for this account." );
   FC_ASSERT( request->new_owner_authority == o.new_owner_authority, "New owner authority does not match recovery request." );

   const auto& recent_auth_idx = _db.get_index< owner_authority_history_index >().indices().get< by_account >();
   auto hist = recent_auth_idx.lower_bound( o.account_to_recover );
   bool found = false;

   while( hist != recent_auth_idx.end() && hist->account == o.account_to_recover && !found )
   {
      found = hist->previous_owner_authority == o.recent_owner_authority;
      if( found ) break;
      ++hist;
   }

   FC_ASSERT( found, "Recent authority not found in authority history." );

   _db.remove( *request ); // Remove first, update_owner_authority may invalidate iterator
   _db.update_owner_authority( account, o.new_owner_authority );
   _db.modify( account, [&]( account_object& a )
   {
      a.last_account_recovery = _db.head_block_time();
   });
}

void change_recovery_account_evaluator::do_apply( const change_recovery_account_operation& o )
{
   _db.get_account( o.new_recovery_account ); // Simply validate account exists
   const auto& account_to_recover = _db.get_account( o.account_to_recover );

   const auto& change_recovery_idx = _db.get_index< change_recovery_account_request_index >().indices().get< by_account >();
   auto request = change_recovery_idx.find( o.account_to_recover );

   if( request == change_recovery_idx.end() ) // New request
   {
      _db.create< change_recovery_account_request_object >( [&]( change_recovery_account_request_object& req )
      {
         req.account_to_recover = o.account_to_recover;
         req.recovery_account = o.new_recovery_account;
         req.effective_on = _db.head_block_time() + MORPHENE_OWNER_AUTH_RECOVERY_PERIOD;
      });
   }
   else if( account_to_recover.recovery_account != o.new_recovery_account ) // Change existing request
   {
      _db.modify( *request, [&]( change_recovery_account_request_object& req )
      {
         req.recovery_account = o.new_recovery_account;
         req.effective_on = _db.head_block_time() + MORPHENE_OWNER_AUTH_RECOVERY_PERIOD;
      });
   }
   else // Request exists and changing back to current recovery account
   {
      _db.remove( *request );
   }
}

void reset_account_evaluator::do_apply( const reset_account_operation& op )
{
   FC_ASSERT( false, "Reset Account Operation is currently disabled." );
/*
   const auto& acnt = _db.get_account( op.account_to_reset );
   auto band = _db.find< account_bandwidth_object, by_account_bandwidth_type >( boost::make_tuple( op.account_to_reset, bandwidth_type::old_forum ) );
   if( band != nullptr )
      FC_ASSERT( ( _db.head_block_time() - band->last_bandwidth_update ) > fc::days(60), "Account must be inactive for 60 days to be eligible for reset" );
   FC_ASSERT( acnt.reset_account == op.reset_account, "Reset account does not match reset account on account." );

   _db.update_owner_authority( acnt, op.new_owner_authority );
*/
}

void set_reset_account_evaluator::do_apply( const set_reset_account_operation& op )
{
   FC_ASSERT( false, "Set Reset Account Operation is currently disabled." );
/*
   const auto& acnt = _db.get_account( op.account );
   _db.get_account( op.reset_account );

   FC_ASSERT( acnt.reset_account == op.current_reset_account, "Current reset account does not match reset account on account." );
   FC_ASSERT( acnt.reset_account != op.reset_account, "Reset account must change" );

   _db.modify( acnt, [&]( account_object& a )
   {
       a.reset_account = op.reset_account;
   });
*/
}

void delegate_vesting_shares_evaluator::do_apply( const delegate_vesting_shares_operation& op )
{
   const auto& delegator = _db.get_account( op.delegator );
   const auto& delegatee = _db.get_account( op.delegatee );
   auto delegation = _db.find< vesting_delegation_object, by_delegation >( boost::make_tuple( op.delegator, op.delegatee ) );

   legacy_asset available_shares;

   auto max_mana = util::get_effective_vesting_shares( delegator );

   _db.modify( delegator, [&]( account_object& a )
   {
      util::manabar_params params( max_mana, MORPHENE_VOTING_MANA_REGENERATION_SECONDS );
      a.voting_manabar.regenerate_mana( params, _db.head_block_time() );
   });

   available_shares = legacy_asset( delegator.voting_manabar.current_mana, VESTS_SYMBOL );

   // Assume delegated VESTS are used first when consuming mana. You cannot delegate received vesting shares
   available_shares.amount = std::min( available_shares.amount, max_mana - delegator.received_vesting_shares.amount );

   if( delegator.next_vesting_withdrawal < fc::time_point_sec::maximum()
      && delegator.to_withdraw - delegator.withdrawn > delegator.vesting_withdraw_rate.amount )
   {
      /*
      current voting mana does not include the current week's power down:

      std::min(
         account.vesting_withdraw_rate.amount.value,           // Weekly amount
         account.to_withdraw.value - account.withdrawn.value   // Or remainder
         );

      But an account cannot delegate **any** VESTS that they are powering down.
      The remaining withdrawal needs to be added in but then the current week is double counted.
      */

      auto weekly_withdraw = legacy_asset( std::min(
         delegator.vesting_withdraw_rate.amount.value,           // Weekly amount
         delegator.to_withdraw.value - delegator.withdrawn.value   // Or remainder
         ), VESTS_SYMBOL );

      available_shares += weekly_withdraw - legacy_asset( delegator.to_withdraw - delegator.withdrawn, VESTS_SYMBOL );
   }

   const auto& wso = _db.get_witness_schedule_object();
   const auto& gpo = _db.get_dynamic_global_properties();

   // HF 20 increase fee meaning by 30x, reduce these thresholds to compensate.
   auto min_delegation = legacy_asset( wso.median_props.account_creation_fee.amount / 3, MORPH_SYMBOL ) * gpo.get_vesting_share_price();
   auto min_update = legacy_asset( wso.median_props.account_creation_fee.amount / 30, MORPH_SYMBOL ) * gpo.get_vesting_share_price();

   // If delegation doesn't exist, create it
   if( delegation == nullptr )
   {
      FC_ASSERT( available_shares >= op.vesting_shares, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
         ("acc", op.delegator)("r", op.vesting_shares)("a", available_shares) );
      FC_ASSERT( op.vesting_shares >= min_delegation, "Account must delegate a minimum of ${v}", ("v", min_delegation) );

      _db.create< vesting_delegation_object >( [&]( vesting_delegation_object& obj )
      {
         obj.delegator = op.delegator;
         obj.delegatee = op.delegatee;
         obj.vesting_shares = op.vesting_shares;
         obj.min_delegation_time = _db.head_block_time();
      });

      _db.modify( delegator, [&]( account_object& a )
      {
         a.delegated_vesting_shares += op.vesting_shares;
         a.voting_manabar.use_mana( op.vesting_shares.amount.value );
      });

      _db.modify( delegatee, [&]( account_object& a )
      {
         util::manabar_params params( util::get_effective_vesting_shares( a ), MORPHENE_VOTING_MANA_REGENERATION_SECONDS );
         a.voting_manabar.regenerate_mana( params, _db.head_block_time() );
         a.voting_manabar.use_mana( -op.vesting_shares.amount.value );
         a.received_vesting_shares += op.vesting_shares;
      });
   }
   // Else if the delegation is increasing
   else if( op.vesting_shares >= delegation->vesting_shares )
   {
      auto delta = op.vesting_shares - delegation->vesting_shares;

      FC_ASSERT( delta >= min_update, "VESTS increase is not enough of a difference. min_update: ${min}", ("min", min_update) );
      FC_ASSERT( available_shares >= delta, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
         ("acc", op.delegator)("r", delta)("a", available_shares) );

      _db.modify( delegator, [&]( account_object& a )
      {
         a.delegated_vesting_shares += delta;
         a.voting_manabar.use_mana( delta.amount.value );
      });

      _db.modify( delegatee, [&]( account_object& a )
      {
         util::manabar_params params( util::get_effective_vesting_shares( a ), MORPHENE_VOTING_MANA_REGENERATION_SECONDS );
         a.voting_manabar.regenerate_mana( params, _db.head_block_time() );
         a.voting_manabar.use_mana( -delta.amount.value );
         a.received_vesting_shares += delta;
      });

      _db.modify( *delegation, [&]( vesting_delegation_object& obj )
      {
         obj.vesting_shares = op.vesting_shares;
      });
   }
   // Else the delegation is decreasing
   else /* delegation->vesting_shares > op.vesting_shares */
   {
      auto delta = delegation->vesting_shares - op.vesting_shares;

      if( op.vesting_shares.amount > 0 )
      {
         FC_ASSERT( delta >= min_update, "VESTS decrease is not enough of a difference. min_update: ${min}", ("min", min_update) );
         FC_ASSERT( op.vesting_shares >= min_delegation, "Delegation must be removed or leave minimum delegation amount of ${v}", ("v", min_delegation) );
      }
      else
      {
         FC_ASSERT( delegation->vesting_shares.amount > 0, "Delegation would set vesting_shares to zero, but it is already zero");
      }

      _db.create< vesting_delegation_expiration_object >( [&]( vesting_delegation_expiration_object& obj )
      {
         obj.delegator = op.delegator;
         obj.vesting_shares = delta;
         obj.expiration = std::max( _db.head_block_time() + gpo.delegation_return_period, delegation->min_delegation_time );
      });

      _db.modify( delegatee, [&]( account_object& a )
      {
         a.received_vesting_shares -= delta;
         a.voting_manabar.use_mana( delta.amount.value );

         if( a.voting_manabar.current_mana < 0 )
         {
            a.voting_manabar.current_mana = 0;
         }
      });

      if( op.vesting_shares.amount > 0 )
      {
         _db.modify( *delegation, [&]( vesting_delegation_object& obj )
         {
            obj.vesting_shares = op.vesting_shares;
         });
      }
      else
      {
         _db.remove( *delegation );
      }
   }
}

void create_auction_evaluator::do_apply ( const create_auction_operation& op )
{
   ilog("create_auction_operation eval");
   _db.create< auction_object >( [&]( auction_object& w ) {
      w.witness = op.witness;
      w.permlink = op.permlink;
      w.created = _db.head_block_time();
   });
}

void update_auction_evaluator::do_apply ( const update_auction_operation& op )
{
   ilog("update_auction_operation eval");
   auto auction = _db.find< auction_object, by_permlink >( op.permlink );
   FC_ASSERT(auction != nullptr, "Unable to find auction with permlink: ${p}", ("p",op.permlink));
}

void delete_auction_evaluator::do_apply ( const delete_auction_operation& op )
{
   ilog("delete_auction_operation eval");
   auto auction = _db.find< auction_object, by_permlink >( op.permlink );
   FC_ASSERT(auction != nullptr, "Unable to find auction with permlink: ${p}", ("p",op.permlink));
   _db.remove( *auction );
}

} } // morphene::chain
