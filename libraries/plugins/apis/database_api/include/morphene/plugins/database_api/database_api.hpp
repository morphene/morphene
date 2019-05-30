#pragma once

#include <morphene/plugins/json_rpc/utility.hpp>

#include <morphene/plugins/database_api/database_api_args.hpp>
#include <morphene/plugins/database_api/database_api_objects.hpp>

#include <morphene/plugins/network_broadcast_api/network_broadcast_api.hpp>
#include <morphene/plugins/network_broadcast_api/network_broadcast_api_plugin.hpp>

#include <morphene/plugins/block_api/block_api.hpp>
#include <morphene/plugins/block_api/block_api_plugin.hpp>

#include <morphene/plugins/account_history_api/account_history_api.hpp>
#include <morphene/plugins/account_history_api/account_history_api_plugin.hpp>

#define DATABASE_API_SINGLE_QUERY_LIMIT 1000

namespace morphene { namespace plugins { namespace database_api {

class database_api_impl;

#define DEFINE_API_ARGS( api_name, arg_type, return_type )  \
typedef arg_type api_name ## _args;                         \
typedef return_type api_name ## _return;

DEFINE_API_ARGS( get_block_header,                       vector< variant >,   optional< block_header > )
DEFINE_API_ARGS( get_block,                              vector< variant >,   optional< signed_block > )
DEFINE_API_ARGS( get_ops_in_block,                       vector< variant >,   vector< account_history::api_operation_object > )
DEFINE_API_ARGS( get_config,                             vector< variant >,   fc::variant_object )
DEFINE_API_ARGS( get_miner_queue,                        vector< variant >,   vector<account_name_type> )
DEFINE_API_ARGS( get_state,                              vector< variant >,   state )
DEFINE_API_ARGS( get_chain_properties,                   vector< variant >,   api_chain_properties )
DEFINE_API_ARGS( get_next_scheduled_hardfork,            vector< variant >,   scheduled_hardfork )
DEFINE_API_ARGS( get_accounts,                           vector< variant >,   vector< extended_account > )
DEFINE_API_ARGS( get_account_history,                    vector< variant >,   vector< account_history::api_operation_object > )
DEFINE_API_ARGS( get_account_count,                      vector< variant >,   uint64_t )
DEFINE_API_ARGS( get_owner_history,                      vector< variant >,   vector< api_owner_authority_history_object > )
DEFINE_API_ARGS( get_recovery_request,                   vector< variant >,   optional< api_account_recovery_request_object > )
DEFINE_API_ARGS( get_withdraw_routes,                    vector< variant >,   vector< database_api::api_withdraw_vesting_route_object > )
DEFINE_API_ARGS( get_witnesses,                          vector< variant >,   vector< optional< api_witness_object > > )
DEFINE_API_ARGS( get_witnesses_by_vote,                  vector< variant >,   vector< optional< api_witness_object > > )
DEFINE_API_ARGS( get_witness_by_account,                 vector< variant >,   optional< api_witness_object > )
DEFINE_API_ARGS( get_witness_count,                      vector< variant >,   uint64_t )
DEFINE_API_ARGS( broadcast_transaction,                  vector< variant >,   json_rpc::void_type )
DEFINE_API_ARGS( get_auction,                            vector< variant >,   api_auction_object )
DEFINE_API_ARGS( get_auctions_by_status,                 vector< variant >,   vector< api_auction_object > )
DEFINE_API_ARGS( get_auctions_by_status_start_time,      vector< variant >,   vector< api_auction_object > )
DEFINE_API_ARGS( get_auctions_by_status_end_time,        vector< variant >,   vector< api_auction_object > )
DEFINE_API_ARGS( get_bids,                               vector< variant >,   vector< api_bid_object > )

typedef vector< variant > get_account_history_args;

#undef DEFINE_API_ARGS

class database_api
{
   public:
      database_api();
      ~database_api();

      DECLARE_API(

         /////////////
         // Globals //
         /////////////

         (get_block_header)
         (get_block)
         (get_ops_in_block)

         /**
         * @brief Retrieve compile-time constants
         */
         (get_config)

         /**
          * @brief Return version information and chain_id of running node
          */
         (get_version)

         /**
         * @brief Retrieve the current @ref dynamic_global_property_object
         */
         (get_dynamic_global_properties)
         (get_witness_schedule)
         (get_hardfork_properties)

         ///////////////
         // Witnesses //
         ///////////////
         (list_witnesses)
         (find_witnesses)
         (list_witness_votes)
         (get_active_witnesses)

         //////////////
         // Accounts //
         //////////////

         /**
         * @brief List accounts ordered by specified key
         *
         */
         (list_accounts)

         /**
         * @brief Find accounts by primary key (account name)
         */
         (find_accounts)
         (list_owner_histories)
         (find_owner_histories)
         (list_account_recovery_requests)
         (find_account_recovery_requests)
         (list_change_recovery_account_requests)
         (find_change_recovery_account_requests)
         (list_escrows)
         (find_escrows)
         (get_withdraw_routes)
         (list_withdraw_vesting_routes)
         (find_withdraw_vesting_routes)
         (list_vesting_delegations)
         (find_vesting_delegations)
         (list_vesting_delegation_expirations)
         (find_vesting_delegation_expirations)

         ////////////////////////////
         // Authority / validation //
         ////////////////////////////

         /// @brief Get a hexdump of the serialized binary form of a transaction
         (get_transaction_hex)

         /**
         *  This API will take a partially signed transaction and a set of public keys that the owner has the ability to sign for
         *  and return the minimal subset of public keys that should add signatures to the transaction.
         */
         (get_required_signatures)

         /**
         *  This method will return the set of all public keys that could possibly sign for a given transaction.  This call can
         *  be used by wallets to filter their set of public keys to just the relevant subset prior to calling @ref get_required_signatures
         *  to get the minimum subset.
         */
         (get_potential_signatures)

         /**
         * @return true of the @ref trx has all of the required signatures, otherwise throws an exception
         */
         (verify_authority)

         /**
         * @return true if the signers have enough authority to authorize an account
         */
         (verify_account_authority)

         /*
          * This is a general purpose API that checks signatures against accounts for an arbitrary sha256 hash
          * using the existing authority structures in Morphene
          */
         (verify_signatures)

         (get_miner_queue)

         (get_state)
         (get_chain_properties)
         (get_next_scheduled_hardfork)
         (get_accounts)
         (get_account_history)
         (get_account_count)
         (get_owner_history)
         (get_recovery_request)
         (get_witnesses)
         (get_witnesses_by_vote)
         (get_witness_by_account)
         (get_witness_count)
         (broadcast_transaction)
         (broadcast_transaction_synchronous)
         (get_auction)
         (get_auctions_by_status)
         (get_auctions_by_status_start_time)
         (get_auctions_by_status_end_time)
         (get_bids)
      )

   private:
      friend class database_api_plugin;
      void api_startup();

      std::unique_ptr< database_api_impl > my;
};

} } } //morphene::plugins::database_api
