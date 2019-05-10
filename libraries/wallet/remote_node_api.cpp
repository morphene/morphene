#include <morphene/wallet/remote_node_api.hpp>

namespace morphene { namespace wallet {

// This class exists only to provide method signature information to fc::api, not to execute calls.

database_api::get_version_return remote_node_api::get_version()
{
   FC_ASSERT( false );
}

database_api::state remote_node_api::get_state( string )
{
   FC_ASSERT( false );
}

vector< account_name_type > remote_node_api::get_active_witnesses()
{
   FC_ASSERT( false );
}

fc::variant_object remote_node_api::get_config()
{
   FC_ASSERT( false );
}

database_api::api_dynamic_global_property_object remote_node_api::get_dynamic_global_properties()
{
   FC_ASSERT( false );
}

database_api::api_chain_properties remote_node_api::get_chain_properties()
{
   FC_ASSERT( false );
}

database_api::api_witness_schedule_object remote_node_api::get_witness_schedule()
{
   FC_ASSERT( false );
}

database_api::scheduled_hardfork remote_node_api::get_next_scheduled_hardfork()
{
   FC_ASSERT( false );
}

vector< database_api::extended_account > remote_node_api::get_accounts( vector< account_name_type > )
{
   FC_ASSERT( false );
}

uint64_t remote_node_api::get_account_count()
{
   FC_ASSERT( false );
}

vector< database_api::api_owner_authority_history_object > remote_node_api::get_owner_history( account_name_type )
{
   FC_ASSERT( false );
}

optional< database_api::api_account_recovery_request_object > remote_node_api::get_recovery_request( account_name_type )
{
   FC_ASSERT( false );
}

vector< optional< database_api::api_witness_object > > remote_node_api::get_witnesses( vector< witness_id_type > )
{
   FC_ASSERT( false );
}

optional< database_api::api_witness_object > remote_node_api::get_witness_by_account( account_name_type )
{
   FC_ASSERT( false );
}

uint64_t remote_node_api::get_witness_count()
{
   FC_ASSERT( false );
}

string remote_node_api::get_transaction_hex( database_api::signed_transaction )
{
   FC_ASSERT( false );
}

set< public_key_type > remote_node_api::get_required_signatures( database_api::signed_transaction, flat_set< public_key_type > )
{
   FC_ASSERT( false );
}

set< public_key_type > remote_node_api::get_potential_signatures( database_api::signed_transaction )
{
   FC_ASSERT( false );
}

bool remote_node_api::verify_authority( database_api::signed_transaction )
{
   FC_ASSERT( false );
}

bool remote_node_api::verify_account_authority( string, flat_set< public_key_type > )
{
   FC_ASSERT( false );
}

vector< account_history::api_operation_object > remote_node_api::get_account_history( account_name_type, uint64_t, uint32_t )
{
   FC_ASSERT( false );
}

void remote_node_api::broadcast_transaction( database_api::signed_transaction )
{
   FC_ASSERT( false );
}

database_api::broadcast_transaction_synchronous_return remote_node_api::broadcast_transaction_synchronous( database_api::signed_transaction )
{
   FC_ASSERT( false );
}

database_api::api_auction_object remote_node_api::get_auction( string )
{
   FC_ASSERT( false );
}

vector< database_api::api_auction_object > remote_node_api::get_auctions_by_status( string, uint32_t )
{
   FC_ASSERT( false );
}

vector< database_api::api_bid_object > remote_node_api::get_bids( string, uint32_t )
{
   FC_ASSERT( false );
}

} }
