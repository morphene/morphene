#pragma once

#include <morphene/plugins/database_api/database_api.hpp>
#include <morphene/plugins/account_history_api/account_history_api.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>
#include <fc/api.hpp>

namespace morphene { namespace wallet {

using std::vector;
using fc::variant;
using fc::optional;

using namespace chain;
using namespace plugins;
/*using namespace plugins::database_api;
using namespace plugins::account_history;
using namespace plugins::witness;*/

/**
 * This is a dummy API so that the wallet can create properly formatted API calls
 */
struct remote_node_api
{
   database_api::get_version_return get_version();
   database_api::state get_state( string );
   vector< account_name_type > get_active_witnesses();
   fc::variant_object get_config();
   database_api::api_dynamic_global_property_object get_dynamic_global_properties();
   database_api::api_chain_properties get_chain_properties();
   database_api::api_witness_schedule_object get_witness_schedule();
   database_api::scheduled_hardfork get_next_scheduled_hardfork();
   vector< database_api::extended_account > get_accounts( vector< account_name_type > );
   uint64_t get_account_count();
   vector< database_api::api_owner_authority_history_object > get_owner_history( account_name_type );
   optional< database_api::api_account_recovery_request_object > get_recovery_request( account_name_type );
   vector< optional< database_api::api_witness_object > > get_witnesses( vector< witness_id_type > );
   optional< database_api::api_witness_object > get_witness_by_account( account_name_type );
   uint64_t get_witness_count();
   string get_transaction_hex( database_api::signed_transaction );
   set< public_key_type > get_required_signatures( database_api::signed_transaction, flat_set< public_key_type > );
   set< public_key_type > get_potential_signatures( database_api::signed_transaction );
   bool verify_authority( database_api::signed_transaction );
   bool verify_account_authority( string, flat_set< public_key_type > );
   vector< account_history::api_operation_object > get_account_history( account_name_type, uint64_t, uint32_t );
   void broadcast_transaction( database_api::signed_transaction );
   database_api::broadcast_transaction_synchronous_return broadcast_transaction_synchronous( database_api::signed_transaction );
};

} }

FC_API( morphene::wallet::remote_node_api,
        (get_version)
        (get_state)
        (get_active_witnesses)
        (get_config)
        (get_dynamic_global_properties)
        (get_chain_properties)
        (get_witness_schedule)
        (get_next_scheduled_hardfork)
        (get_accounts)
        (get_account_count)
        (get_owner_history)
        (get_recovery_request)
        (get_witnesses)
        (get_witness_by_account)
        (get_witness_count)
        (get_transaction_hex)
        (get_required_signatures)
        (get_potential_signatures)
        (verify_authority)
        (verify_account_authority)
        (get_account_history)
        (broadcast_transaction)
        (broadcast_transaction_synchronous)
      )
