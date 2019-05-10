#pragma once
#include <morphene/chain/account_object.hpp>
#include <morphene/chain/block_summary_object.hpp>
#include <morphene/chain/global_property_object.hpp>
#include <morphene/chain/history_object.hpp>
#include <morphene/chain/morphene_objects.hpp>
#include <morphene/chain/transaction_object.hpp>
#include <morphene/chain/witness_objects.hpp>
#include <morphene/chain/database.hpp>

#include <morphene/plugins/account_history_api/account_history_api.hpp>

namespace morphene { namespace plugins { namespace database_api {

using namespace morphene::chain;

typedef change_recovery_account_request_object api_change_recovery_account_request_object;
typedef block_summary_object                   api_block_summary_object;
typedef dynamic_global_property_object         api_dynamic_global_property_object;
typedef escrow_object                          api_escrow_object;
typedef withdraw_vesting_route_object          api_withdraw_vesting_route_object;
typedef witness_vote_object                    api_witness_vote_object;
typedef vesting_delegation_object              api_vesting_delegation_object;
typedef vesting_delegation_expiration_object   api_vesting_delegation_expiration_object;

struct api_account_object
{
   api_account_object( const account_object& a, const database& db ) :
      id( a.id ),
      name( a.name ),
      memo_key( a.memo_key ),
      json_metadata( to_string( a.json_metadata ) ),
      proxy( a.proxy ),
      last_account_update( a.last_account_update ),
      created( a.created ),
      recovery_account( a.recovery_account ),
      reset_account( a.reset_account ),
      last_account_recovery( a.last_account_recovery ),
      can_vote( a.can_vote ),
      voting_manabar( a.voting_manabar ),
      balance( a.balance ),
      vesting_shares( a.vesting_shares ),
      delegated_vesting_shares( a.delegated_vesting_shares ),
      received_vesting_shares( a.received_vesting_shares ),
      vesting_withdraw_rate( a.vesting_withdraw_rate ),
      next_vesting_withdrawal( a.next_vesting_withdrawal ),
      withdrawn( a.withdrawn ),
      to_withdraw( a.to_withdraw ),
      withdraw_routes( a.withdraw_routes ),
      witnesses_voted_for( a.witnesses_voted_for ),
      pending_claimed_accounts( a.pending_claimed_accounts )
   {
      size_t n = a.proxied_vsf_votes.size();
      proxied_vsf_votes.reserve( n );
      for( size_t i=0; i<n; i++ )
         proxied_vsf_votes.push_back( a.proxied_vsf_votes[i] );

      const auto& auth = db.get< account_authority_object, by_account >( name );
      owner = authority( auth.owner );
      active = authority( auth.active );
      posting = authority( auth.posting );
      last_owner_update = auth.last_owner_update;
   }


   api_account_object(){}

   account_id_type   id;

   account_name_type name;
   authority         owner;
   authority         active;
   authority         posting;
   public_key_type   memo_key;
   string            json_metadata;
   account_name_type proxy;

   time_point_sec    last_owner_update;
   time_point_sec    last_account_update;

   time_point_sec    created;
   account_name_type recovery_account;
   account_name_type reset_account;
   time_point_sec    last_account_recovery;

   bool              can_vote = false;
   util::manabar     voting_manabar;

   legacy_asset      balance;

   legacy_asset      vesting_shares;
   legacy_asset      delegated_vesting_shares;
   legacy_asset      received_vesting_shares;
   legacy_asset      vesting_withdraw_rate;
   time_point_sec    next_vesting_withdrawal;
   share_type        withdrawn;
   share_type        to_withdraw;
   uint16_t          withdraw_routes = 0;

   vector< share_type > proxied_vsf_votes;

   uint16_t          witnesses_voted_for = 0;

   share_type        pending_claimed_accounts = 0;
};

struct extended_account : public api_account_object
{
   extended_account(){}
   extended_account( const api_account_object& a ) :
      api_account_object( a ) {}

   legacy_asset                                               vesting_balance;
   map< uint64_t, account_history::api_operation_object >     transfer_history;
   map< uint64_t, account_history::api_operation_object >     other_history;
   set< string >                                              witness_votes;
};

struct api_owner_authority_history_object
{
   api_owner_authority_history_object( const owner_authority_history_object& o ) :
      id( o.id ),
      account( o.account ),
      previous_owner_authority( authority( o.previous_owner_authority ) ),
      last_valid_time( o.last_valid_time )
   {}

   api_owner_authority_history_object() {}

   owner_authority_history_id_type  id;

   account_name_type                account;
   authority                        previous_owner_authority;
   time_point_sec                   last_valid_time;
};

struct api_account_recovery_request_object
{
   api_account_recovery_request_object( const account_recovery_request_object& o ) :
      id( o.id ),
      account_to_recover( o.account_to_recover ),
      new_owner_authority( authority( o.new_owner_authority ) ),
      expires( o.expires )
   {}

   api_account_recovery_request_object() {}

   account_recovery_request_id_type id;
   account_name_type                account_to_recover;
   authority                        new_owner_authority;
   time_point_sec                   expires;
};

struct api_account_history_object
{

};

struct api_witness_object
{
   api_witness_object( const witness_object& w ) :
      id( w.id ),
      owner( w.owner ),
      created( w.created ),
      url( to_string( w.url ) ),
      total_missed( w.total_missed ),
      last_aslot( w.last_aslot ),
      last_confirmed_block_num( w.last_confirmed_block_num ),
      pow_worker( w.pow_worker ),
      signing_key( w.signing_key ),
      props( w.props ),
      votes( w.votes ),
      virtual_last_update( w.virtual_last_update ),
      virtual_position( w.virtual_position ),
      virtual_scheduled_time( w.virtual_scheduled_time ),
      running_version( w.running_version ),
      hardfork_version_vote( w.hardfork_version_vote ),
      hardfork_time_vote( w.hardfork_time_vote ),
      available_witness_account_subsidies( w.available_witness_account_subsidies )
   {}

   api_witness_object() {}

   witness_id_type   id;
   account_name_type owner;
   time_point_sec    created;
   string            url;
   uint32_t          total_missed = 0;
   uint64_t          last_aslot = 0;
   uint64_t          last_confirmed_block_num = 0;
   uint64_t          pow_worker;
   public_key_type   signing_key;
   chain_properties  props;
   share_type        votes;
   fc::uint128       virtual_last_update;
   fc::uint128       virtual_position;
   fc::uint128       virtual_scheduled_time;
   version           running_version;
   hardfork_version  hardfork_version_vote;
   time_point_sec    hardfork_time_vote;
   int64_t           available_witness_account_subsidies = 0;
};

struct api_witness_schedule_object
{
   api_witness_schedule_object() {}

   api_witness_schedule_object( const witness_schedule_object& wso) :
      id( wso.id ),
      current_virtual_time( wso.current_virtual_time ),
      next_shuffle_block_num( wso.next_shuffle_block_num ),
      num_scheduled_witnesses( wso.num_scheduled_witnesses ),
      elected_weight( wso.elected_weight ),
      timeshare_weight( wso.timeshare_weight ),
      witness_pay_normalization_factor( wso.witness_pay_normalization_factor ),
      median_props( wso.median_props ),
      majority_version( wso.majority_version ),
      max_voted_witnesses( wso.max_voted_witnesses ),
      max_runner_witnesses( wso.max_runner_witnesses ),
      hardfork_required_witnesses( wso.hardfork_required_witnesses ),
      account_subsidy_rd( wso.account_subsidy_rd ),
      account_subsidy_witness_rd( wso.account_subsidy_witness_rd ),
      min_witness_account_subsidy_decay( wso.min_witness_account_subsidy_decay )
   {
      size_t n = wso.current_shuffled_witnesses.size();
      current_shuffled_witnesses.reserve( n );
      std::transform(wso.current_shuffled_witnesses.begin(), wso.current_shuffled_witnesses.end(),
                     std::back_inserter(current_shuffled_witnesses),
                     [](const account_name_type& s) -> std::string { return s; } );
                     // ^ fixed_string std::string operator used here.
   }

   witness_schedule_id_type   id;

   fc::uint128                current_virtual_time;
   uint32_t                   next_shuffle_block_num;
   vector<string>             current_shuffled_witnesses;   // fc::array<account_name_type,...> -> vector<string>
   uint8_t                    num_scheduled_witnesses;
   uint8_t                    elected_weight;
   uint8_t                    timeshare_weight;
   uint32_t                   witness_pay_normalization_factor;
   chain_properties           median_props;
   version                    majority_version;

   uint8_t                    max_voted_witnesses;
   uint8_t                    max_runner_witnesses;
   uint8_t                    hardfork_required_witnesses;

   rd_dynamics_params         account_subsidy_rd;
   rd_dynamics_params         account_subsidy_witness_rd;
   int64_t                    min_witness_account_subsidy_decay = 0;
};

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

struct api_hardfork_property_object
{
   api_hardfork_property_object( const hardfork_property_object& h ) :
      id( h.id ),
      last_hardfork( h.last_hardfork ),
      current_hardfork_version( h.current_hardfork_version ),
      next_hardfork( h.next_hardfork ),
      next_hardfork_time( h.next_hardfork_time )
   {
      size_t n = h.processed_hardforks.size();
      processed_hardforks.reserve( n );

      for( size_t i = 0; i < n; i++ )
         processed_hardforks.push_back( h.processed_hardforks[i] );
   }

   api_hardfork_property_object() {}

   hardfork_property_id_type     id;
   vector< fc::time_point_sec >  processed_hardforks;
   uint32_t                      last_hardfork;
   protocol::hardfork_version    current_hardfork_version;
   protocol::hardfork_version    next_hardfork;
   fc::time_point_sec            next_hardfork_time;
};

struct state
{
   string                                             current_route;

   dynamic_global_property_object                     props;

   map< string, api_account_object >                  accounts;

   vector< account_name_type >                        pow_queue;
   map< string, api_witness_object >                  witnesses;
   api_witness_schedule_object                        witness_schedule;
   string                                             error;
};

struct api_chain_properties
{
   api_chain_properties() {}
   api_chain_properties( const chain::chain_properties& c ) :
      account_creation_fee( c.account_creation_fee ),
      maximum_block_size( c.maximum_block_size ),
      account_subsidy_budget( c.account_subsidy_budget ),
      account_subsidy_decay( c.account_subsidy_decay )
   {}

   legacy_asset   account_creation_fee;
   uint32_t       maximum_block_size = MORPHENE_MIN_BLOCK_SIZE_LIMIT * 2;
   int32_t        account_subsidy_budget = MORPHENE_DEFAULT_ACCOUNT_SUBSIDY_BUDGET;
   uint32_t       account_subsidy_decay = MORPHENE_DEFAULT_ACCOUNT_SUBSIDY_DECAY;
};

struct scheduled_hardfork
{
   hardfork_version     hf_version;
   fc::time_point_sec   live_time;
};

enum withdraw_route_type
{
   incoming,
   outgoing,
   all
};

typedef vector< variant > broadcast_transaction_synchronous_args;

struct broadcast_transaction_synchronous_return
{
   broadcast_transaction_synchronous_return() {}
   broadcast_transaction_synchronous_return( transaction_id_type txid, int32_t bn, int32_t tn, bool ex )
   : id(txid), block_num(bn), trx_num(tn), expired(ex) {}

   transaction_id_type   id;
   int32_t               block_num = 0;
   int32_t               trx_num   = 0;
   bool                  expired   = false;
};

struct api_auction_object
{
  api_auction_object() {}
  api_auction_object( const chain::auction_object& c ) :
    id( c.id ),
    title( c.title ),
    permlink( c.permlink ),
    image( c.image ),
    consigner( c.consigner ),
    description( c.description ),
    status( c.status ),
    start_time( c.start_time ),
    end_time( c.end_time ),
    bids_count( c.bids_count ),
    bids_value( c.bids_value ),
    created( c.created ),
    last_paid( c.last_paid ),
    last_updated( c.last_updated ),
    extensions( c.extensions )
  {}

    auction_id_type         id;

    string                  title;
    string                  permlink;
    string                  image;
    account_name_type       consigner;
    string                  description;
    string                  status;
    time_point_sec          start_time;
    time_point_sec          end_time;
    uint32_t                bids_count;
    legacy_asset            bids_value;
    time_point_sec          created;
    time_point_sec          last_paid;
    time_point_sec          last_updated;
    extensions_type         extensions;
};

struct api_bid_object
{
  api_bid_object() {}
  api_bid_object( const chain::bid_object& c ) :
    id( c.id ),
    bidder( c.bidder ),
    permlink( c.permlink ),
    created( c.created )
  {}

    bid_id_type             id;

    account_name_type       bidder;
    string                  permlink;
    time_point_sec          created;
};

} } } // morphene::plugins::database_api

FC_REFLECT( morphene::plugins::database_api::api_account_object,
             (id)(name)(owner)(active)(posting)(memo_key)(json_metadata)(proxy)(last_owner_update)(last_account_update)
             (created)
             (recovery_account)(last_account_recovery)(reset_account)
             (can_vote)(voting_manabar)
             (balance)
             (vesting_shares)(delegated_vesting_shares)(received_vesting_shares)(vesting_withdraw_rate)(next_vesting_withdrawal)(withdrawn)(to_withdraw)(withdraw_routes)
             (proxied_vsf_votes)(witnesses_voted_for)
             (pending_claimed_accounts)
          )

FC_REFLECT_DERIVED( morphene::plugins::database_api::extended_account, (morphene::plugins::database_api::api_account_object),
            (vesting_balance)(transfer_history)(other_history)(witness_votes) )

FC_REFLECT( morphene::plugins::database_api::api_owner_authority_history_object,
             (id)
             (account)
             (previous_owner_authority)
             (last_valid_time)
          )

FC_REFLECT( morphene::plugins::database_api::api_account_recovery_request_object,
             (id)
             (account_to_recover)
             (new_owner_authority)
             (expires)
          )

FC_REFLECT( morphene::plugins::database_api::api_witness_object,
             (id)
             (owner)
             (created)
             (url)(votes)(virtual_last_update)(virtual_position)(virtual_scheduled_time)(total_missed)
             (last_aslot)(last_confirmed_block_num)(pow_worker)(signing_key)
             (props)
             (running_version)
             (hardfork_version_vote)(hardfork_time_vote)
             (available_witness_account_subsidies)
          )

FC_REFLECT( morphene::plugins::database_api::api_witness_schedule_object,
             (id)
             (current_virtual_time)
             (next_shuffle_block_num)
             (current_shuffled_witnesses)
             (num_scheduled_witnesses)
             (elected_weight)
             (timeshare_weight)
             (witness_pay_normalization_factor)
             (median_props)
             (majority_version)
             (max_voted_witnesses)
             (max_runner_witnesses)
             (hardfork_required_witnesses)
             (account_subsidy_rd)
             (account_subsidy_witness_rd)
             (min_witness_account_subsidy_decay)
          )

FC_REFLECT_DERIVED( morphene::plugins::database_api::api_signed_block_object, (morphene::protocol::signed_block),
                     (block_id)
                     (signing_key)
                     (transaction_ids)
                  )

FC_REFLECT( morphene::plugins::database_api::api_hardfork_property_object,
            (id)
            (processed_hardforks)
            (last_hardfork)
            (current_hardfork_version)
            (next_hardfork)
            (next_hardfork_time)
          )

FC_REFLECT( morphene::plugins::database_api::state,
            (current_route)(props)(accounts)(pow_queue)(witnesses)(witness_schedule)(error) )

FC_REFLECT( morphene::plugins::database_api::api_chain_properties,
            (account_creation_fee)(maximum_block_size)(account_subsidy_budget)(account_subsidy_decay)
          )

FC_REFLECT( morphene::plugins::database_api::scheduled_hardfork,
            (hf_version)(live_time) )

FC_REFLECT_ENUM( morphene::plugins::database_api::withdraw_route_type, (incoming)(outgoing)(all) )

FC_REFLECT( morphene::plugins::database_api::broadcast_transaction_synchronous_return,
            (id)(block_num)(trx_num)(expired) )

FC_REFLECT( morphene::plugins::database_api::api_auction_object, 
            (id)
            (title)
            (permlink)
            (image)
            (consigner)
            (description)
            (status)
            (start_time)
            (end_time)
            (bids_count)
            (bids_value)
            (created)
            (last_paid)
            (last_updated)
            (extensions)
          )

FC_REFLECT( morphene::plugins::database_api::api_bid_object, 
            (id)
            (bidder)
            (permlink)
            (created)
          )
