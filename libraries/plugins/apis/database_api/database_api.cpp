#include <appbase/application.hpp>

#include <morphene/plugins/database_api/database_api.hpp>
#include <morphene/plugins/database_api/database_api_plugin.hpp>

#include <morphene/protocol/get_config.hpp>
#include <morphene/protocol/exceptions.hpp>
#include <morphene/protocol/transaction_util.hpp>

#include <morphene/utilities/git_revision.hpp>

#include <fc/git_revision.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/thread/future.hpp>
#include <boost/thread/lock_guard.hpp>

namespace morphene { namespace plugins { namespace database_api {

typedef std::function< void( const broadcast_transaction_synchronous_return& ) > confirmation_callback;

class database_api_impl
{
   public:
      database_api_impl();
      ~database_api_impl();

      DECLARE_API_IMPL
      (
         (get_config)
         (get_version)
         (get_block_header)
         (get_block)
         (get_ops_in_block)
         (get_dynamic_global_properties)
         (get_witness_schedule)
         (get_hardfork_properties)
         (list_witnesses)
         (find_witnesses)
         (list_witness_votes)
         (get_active_witnesses)
         (list_accounts)
         (find_accounts)
         (list_owner_histories)
         (find_owner_histories)
         (list_account_recovery_requests)
         (find_account_recovery_requests)
         (list_change_recovery_account_requests)
         (find_change_recovery_account_requests)
         (list_escrows)
         (find_escrows)
         (list_withdraw_vesting_routes)
         (find_withdraw_vesting_routes)
         (list_vesting_delegations)
         (find_vesting_delegations)
         (list_vesting_delegation_expirations)
         (find_vesting_delegation_expirations)
         (get_transaction_hex)
         (get_required_signatures)
         (get_potential_signatures)
         (verify_authority)
         (verify_account_authority)
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

      template< typename ResultType >
      static ResultType on_push_default( const ResultType& r ) { return r; }

      template< typename IndexType, typename OrderType, typename ValueType, typename ResultType, typename OnPush >
      void iterate_results( ValueType start, vector< ResultType >& result, uint32_t limit, OnPush&& on_push = &database_api_impl::on_push_default< ResultType > )
      {
         const auto& idx = _db.get_index< IndexType, OrderType >();
         auto itr = idx.lower_bound( start );
         auto end = idx.end();

         while( result.size() < limit && itr != end )
         {
            result.push_back( on_push( *itr ) );
            ++itr;
         }
      }

      void on_post_apply_block( const signed_block& b );

      morphene::plugins::chain::chain_plugin&                              _chain;

      chain::database& _db;
      std::shared_ptr< network_broadcast_api::network_broadcast_api >   _network_broadcast_api;
      p2p::p2p_plugin*                                                  _p2p = nullptr;
      std::shared_ptr< block_api::block_api >                           _block_api;
      std::shared_ptr< account_history::account_history_api >           _account_history_api;

      map< transaction_id_type, confirmation_callback >                 _callbacks;
      map< time_point_sec, vector< transaction_id_type > >              _callback_expirations;
      boost::signals2::connection                                       _on_post_apply_block_conn;

      boost::mutex                                                      _mtx;
};

void database_api_impl::on_post_apply_block( const signed_block& b )
{ try {
   boost::lock_guard< boost::mutex > guard( _mtx );
   int32_t block_num = int32_t(b.block_num());
   if( _callbacks.size() )
   {
      for( size_t trx_num = 0; trx_num < b.transactions.size(); ++trx_num )
      {
         const auto& trx = b.transactions[trx_num];
         auto id = trx.id();
         auto itr = _callbacks.find( id );
         if( itr == _callbacks.end() ) continue;
         itr->second( broadcast_transaction_synchronous_return( id, block_num, int32_t( trx_num ), false ) );
         _callbacks.erase( itr );
      }
   }

   /// clear all expirations
   while( true )
   {
      auto exp_it = _callback_expirations.begin();
      if( exp_it == _callback_expirations.end() )
         break;
      if( exp_it->first >= b.timestamp )
         break;
      for( const transaction_id_type& txid : exp_it->second )
      {
         auto cb_it = _callbacks.find( txid );
         // If it's empty, that means the transaction has been confirmed and has been deleted by the above check.
         if( cb_it == _callbacks.end() )
            continue;

         confirmation_callback callback = cb_it->second;
         transaction_id_type txid_byval = txid;    // can't pass in by reference as it's going to be deleted
         callback( broadcast_transaction_synchronous_return( txid_byval, block_num, -1, true ) );

         _callbacks.erase( cb_it );
      }
      _callback_expirations.erase( exp_it );
   }
} FC_LOG_AND_RETHROW() }

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructors                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

database_api::database_api()
   : my( new database_api_impl() )
{
   JSON_RPC_REGISTER_API( MORPHENE_DATABASE_API_PLUGIN_NAME );
}

database_api::~database_api() {}

database_api_impl::database_api_impl() :
   _chain( appbase::app().get_plugin< morphene::plugins::chain::chain_plugin >() ),
   _db( _chain.db() )
{
   _on_post_apply_block_conn = _db.add_post_apply_block_handler(
      [&]( const block_notification& note ){ on_post_apply_block( note.block ); },
      appbase::app().get_plugin< morphene::plugins::database_api::database_api_plugin >(),
      0 );
}

database_api_impl::~database_api_impl() {}

void database_api::api_startup()
{
   auto network_broadcast = appbase::app().find_plugin< network_broadcast_api::network_broadcast_api_plugin >();
   if( network_broadcast != nullptr )
   {
      my->_network_broadcast_api = network_broadcast->api;
   }

   auto p2p = appbase::app().find_plugin< p2p::p2p_plugin >();
   if( p2p != nullptr )
   {
      my->_p2p = p2p;
   }

   auto block = appbase::app().find_plugin< block_api::block_api_plugin >();
   if( block != nullptr )
   {
      my->_block_api = block->api;
   }

   auto account_history = appbase::app().find_plugin< account_history::account_history_api_plugin >();
   if( account_history != nullptr )
   {
      my->_account_history_api = account_history->api;
   }
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Globals                                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, get_block_header )
{
   FC_ASSERT( _block_api, "block_api_plugin not enabled." );
   return _block_api->get_block_header( { args[0].as< uint32_t >() } ).header;
}

DEFINE_API_IMPL( database_api_impl, get_block )
{
   FC_ASSERT( _block_api, "block_api_plugin not enabled." );
   get_block_return result;
   auto b = _block_api->get_block( { args[0].as< uint32_t >() } ).block;


   if( b )
   {
      result = signed_block( *b );
      // uint32_t n = uint32_t( b->transactions.size() );
      // uint32_t block_num = block_header::num_from_id( b->block_id );
      // for( uint32_t i=0; i<n; i++ )
      // {
      //    result->transactions[i].transaction_id = b->transactions[i].id();
      //    result->transactions[i].block_num = block_num;
      //    result->transactions[i].transaction_num = i;
      // }
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, get_ops_in_block )
{
   FC_ASSERT( _account_history_api, "account_history_api_plugin not enabled." );

   auto ops = _account_history_api->get_ops_in_block( { args[0].as< uint32_t >(), args[1].as< bool >() } ).ops;
   get_ops_in_block_return result;

   for( auto& op_obj : ops )
   {
      result.push_back( op_obj );
   }

   return result;
}


DEFINE_API_IMPL( database_api_impl, get_config )
{
   return morphene::protocol::get_config();
}

DEFINE_API_IMPL( database_api_impl, get_version )
{
   return get_version_return
   (
      fc::string( MORPHENE_BLOCKCHAIN_VERSION ),
      fc::string( morphene::utilities::git_revision_sha ),
      fc::string( fc::git_revision_sha ),
      _db.get_chain_id()
   );
}

DEFINE_API_IMPL( database_api_impl, get_dynamic_global_properties )
{
   return _db.get_dynamic_global_properties();
}

DEFINE_API_IMPL( database_api_impl, get_witness_schedule )
{
   return api_witness_schedule_object( _db.get_witness_schedule_object() );
}

DEFINE_API_IMPL( database_api_impl, get_hardfork_properties )
{
   return _db.get_hardfork_property_object();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Witnesses                                                        //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, list_witnesses )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_witnesses_return result;
   result.witnesses.reserve( args.limit );

   switch( args.order )
   {
      case( by_name ):
      {
         iterate_results< chain::witness_index, chain::by_name >(
            args.start.as< protocol::account_name_type >(),
            result.witnesses,
            args.limit,
            [&]( const witness_object& w ){ return api_witness_object( w ); } );
         break;
      }
      case( by_vote_name ):
      {
         auto key = args.start.as< std::pair< share_type, account_name_type > >();
         iterate_results< chain::witness_index, chain::by_vote_name >(
            boost::make_tuple( key.first, key.second ),
            result.witnesses,
            args.limit,
            [&]( const witness_object& w ){ return api_witness_object( w ); } );
         break;
      }
      case( by_schedule_time ):
      {
         auto key = args.start.as< std::pair< fc::uint128, account_name_type > >();
         auto wit_id = _db.get< chain::witness_object, chain::by_name >( key.second ).id;
         iterate_results< chain::witness_index, chain::by_schedule_time >(
            boost::make_tuple( key.first, wit_id ),
            result.witnesses,
            args.limit,
            [&]( const witness_object& w ){ return api_witness_object( w ); } );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_witnesses )
{
   FC_ASSERT( args.owners.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   find_witnesses_return result;

   for( auto& o : args.owners )
   {
      auto witness = _db.find< chain::witness_object, chain::by_name >( o );

      if( witness != nullptr )
         result.witnesses.push_back( api_witness_object( *witness ) );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, list_witness_votes )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_witness_votes_return result;
   result.votes.reserve( args.limit );

   switch( args.order )
   {
      case( by_account_witness ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::witness_vote_index, chain::by_account_witness >(
            boost::make_tuple( key.first, key.second ),
            result.votes,
            args.limit,
            [&]( const witness_vote_object& v ){ return api_witness_vote_object( v ); } );
         break;
      }
      case( by_witness_account ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::witness_vote_index, chain::by_witness_account >(
            boost::make_tuple( key.first, key.second ),
            result.votes,
            args.limit,
            [&]( const witness_vote_object& v ){ return api_witness_vote_object( v ); } );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, get_miner_queue )
{
   return _db.with_read_lock( [&]()
   {
      vector<account_name_type> result;
      const auto& pow_idx = _db.get_index<witness_index>().indices().get<by_pow>();

      auto itr = pow_idx.upper_bound(0);
      while( itr != pow_idx.end() ) {
         if( itr->pow_worker )
            result.push_back( itr->owner );
         ++itr;
      }
      return result;
   });
}

DEFINE_API_IMPL( database_api_impl, get_active_witnesses )
{
   const auto& wso = _db.get_witness_schedule_object();
   size_t n = wso.current_shuffled_witnesses.size();
   get_active_witnesses_return result;
   result.witnesses.reserve( n );
   for( size_t i=0; i<n; i++ )
      result.witnesses.push_back( wso.current_shuffled_witnesses[i] );
   return result;
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Accounts                                                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////

/* Accounts */

DEFINE_API_IMPL( database_api_impl, list_accounts )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_accounts_return result;
   result.accounts.reserve( args.limit );

   switch( args.order )
   {
      case( by_name ):
      {
         iterate_results< chain::account_index, chain::by_name >(
            args.start.as< protocol::account_name_type >(),
            result.accounts,
            args.limit,
            [&]( const account_object& a ){ return api_account_object( a, _db ); } );
         break;
      }
      case( by_proxy ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::account_index, chain::by_proxy >(
            boost::make_tuple( key.first, key.second ),
            result.accounts,
            args.limit,
            [&]( const account_object& a ){ return api_account_object( a, _db ); } );
         break;
      }
      case( by_next_vesting_withdrawal ):
      {
         auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
         iterate_results< chain::account_index, chain::by_next_vesting_withdrawal >(
            boost::make_tuple( key.first, key.second ),
            result.accounts,
            args.limit,
            [&]( const account_object& a ){ return api_account_object( a, _db ); } );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_accounts )
{
   find_accounts_return result;
   FC_ASSERT( args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   for( auto& a : args.accounts )
   {
      auto acct = _db.find< chain::account_object, chain::by_name >( a );
      if( acct != nullptr )
         result.accounts.push_back( api_account_object( *acct, _db ) );
   }

   return result;
}


/* Owner Auth Histories */

DEFINE_API_IMPL( database_api_impl, list_owner_histories )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_owner_histories_return result;
   result.owner_auths.reserve( args.limit );

   auto key = args.start.as< std::pair< account_name_type, fc::time_point_sec > >();
   iterate_results< chain::owner_authority_history_index, chain::by_account >(
      boost::make_tuple( key.first, key.second ),
      result.owner_auths,
      args.limit,
      [&]( const owner_authority_history_object& o ){ return api_owner_authority_history_object( o ); } );

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_owner_histories )
{
   find_owner_histories_return result;

   const auto& hist_idx = _db.get_index< chain::owner_authority_history_index, chain::by_account >();
   auto itr = hist_idx.lower_bound( args.owner );

   while( itr != hist_idx.end() && itr->account == args.owner && result.owner_auths.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.owner_auths.push_back( api_owner_authority_history_object( *itr ) );
      ++itr;
   }

   return result;
}


/* Account Recovery Requests */

DEFINE_API_IMPL( database_api_impl, list_account_recovery_requests )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_account_recovery_requests_return result;
   result.requests.reserve( args.limit );

   switch( args.order )
   {
      case( by_account ):
      {
         iterate_results< chain::account_recovery_request_index, chain::by_account >(
            args.start.as< account_name_type >(),
            result.requests,
            args.limit,
            [&]( const account_recovery_request_object& a ){ return api_account_recovery_request_object( a ); } );
         break;
      }
      case( by_expiration ):
      {
         auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
         iterate_results< chain::account_recovery_request_index, chain::by_expiration >(
            boost::make_tuple( key.first, key.second ),
            result.requests,
            args.limit,
            [&]( const account_recovery_request_object& a ){ return api_account_recovery_request_object( a ); } );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_account_recovery_requests )
{
   find_account_recovery_requests_return result;
   FC_ASSERT( args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   for( auto& a : args.accounts )
   {
      auto request = _db.find< chain::account_recovery_request_object, chain::by_account >( a );

      if( request != nullptr )
         result.requests.push_back( api_account_recovery_request_object( *request ) );
   }

   return result;
}


/* Change Recovery Account Requests */

DEFINE_API_IMPL( database_api_impl, list_change_recovery_account_requests )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_change_recovery_account_requests_return result;
   result.requests.reserve( args.limit );

   switch( args.order )
   {
      case( by_account ):
      {
         iterate_results< chain::change_recovery_account_request_index, chain::by_account >(
            args.start.as< account_name_type >(),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< change_recovery_account_request_object > );
         break;
      }
      case( by_effective_date ):
      {
         auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
         iterate_results< chain::change_recovery_account_request_index, chain::by_effective_date >(
            boost::make_tuple( key.first, key.second ),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< change_recovery_account_request_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_change_recovery_account_requests )
{
   find_change_recovery_account_requests_return result;
   FC_ASSERT( args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   for( auto& a : args.accounts )
   {
      auto request = _db.find< chain::change_recovery_account_request_object, chain::by_account >( a );

      if( request != nullptr )
         result.requests.push_back( *request );
   }

   return result;
}


/* Escrows */

DEFINE_API_IMPL( database_api_impl, list_escrows )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_escrows_return result;
   result.escrows.reserve( args.limit );

   switch( args.order )
   {
      case( by_from_id ):
      {
         auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
         iterate_results< chain::escrow_index, chain::by_from_id >(
            boost::make_tuple( key.first, key.second ),
            result.escrows,
            args.limit,
            &database_api_impl::on_push_default< escrow_object > );
         break;
      }
      case( by_ratification_deadline ):
      {
         auto key = args.start.as< std::vector< fc::variant > >();
         FC_ASSERT( key.size() == 3, "by_ratification_deadline start requires 3 values. (bool, time_point_sec, escrow_id_type)" );
         iterate_results< chain::escrow_index, chain::by_ratification_deadline >(
            boost::make_tuple( key[0].as< bool >(), key[1].as< fc::time_point_sec >(), key[2].as< escrow_id_type >() ),
            result.escrows,
            args.limit,
            &database_api_impl::on_push_default< escrow_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_escrows )
{
   find_escrows_return result;

   const auto& escrow_idx = _db.get_index< chain::escrow_index, chain::by_from_id >();
   auto itr = escrow_idx.lower_bound( args.from );

   while( itr != escrow_idx.end() && itr->from == args.from && result.escrows.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.escrows.push_back( *itr );
      ++itr;
   }

   return result;
}


/* Withdraw Vesting Routes */

DEFINE_API_IMPL( database_api_impl, list_withdraw_vesting_routes )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_withdraw_vesting_routes_return result;
   result.routes.reserve( args.limit );

   switch( args.order )
   {
      case( by_withdraw_route ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::withdraw_vesting_route_index, chain::by_withdraw_route >(
            boost::make_tuple( key.first, key.second ),
            result.routes,
            args.limit,
            &database_api_impl::on_push_default< withdraw_vesting_route_object > );
         break;
      }
      case( by_destination ):
      {
         auto key = args.start.as< std::pair< account_name_type, withdraw_vesting_route_id_type > >();
         iterate_results< chain::withdraw_vesting_route_index, chain::by_destination >(
            boost::make_tuple( key.first, key.second ),
            result.routes,
            args.limit,
            &database_api_impl::on_push_default< withdraw_vesting_route_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_withdraw_vesting_routes )
{
   find_withdraw_vesting_routes_return result;

   switch( args.order )
   {
      case( by_withdraw_route ):
      {
         const auto& route_idx = _db.get_index< chain::withdraw_vesting_route_index, chain::by_withdraw_route >();
         auto itr = route_idx.lower_bound( args.account );

         while( itr != route_idx.end() && itr->from_account == args.account && result.routes.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
         {
            result.routes.push_back( *itr );
            ++itr;
         }

         break;
      }
      case( by_destination ):
      {
         const auto& route_idx = _db.get_index< chain::withdraw_vesting_route_index, chain::by_destination >();
         auto itr = route_idx.lower_bound( args.account );

         while( itr != route_idx.end() && itr->to_account == args.account && result.routes.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
         {
            result.routes.push_back( *itr );
            ++itr;
         }

         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}


/* Vesting Delegations */

DEFINE_API_IMPL( database_api_impl, list_vesting_delegations )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_vesting_delegations_return result;
   result.delegations.reserve( args.limit );

   switch( args.order )
   {
      case( by_delegation ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::vesting_delegation_index, chain::by_delegation >(
            boost::make_tuple( key.first, key.second ),
            result.delegations,
            args.limit,
            &database_api_impl::on_push_default< api_vesting_delegation_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_vesting_delegations )
{
   find_vesting_delegations_return result;
   const auto& delegation_idx = _db.get_index< chain::vesting_delegation_index, chain::by_delegation >();
   auto itr = delegation_idx.lower_bound( args.account );

   while( itr != delegation_idx.end() && itr->delegator == args.account && result.delegations.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.delegations.push_back( api_vesting_delegation_object( *itr ) );
      ++itr;
   }

   return result;
}


/* Vesting Delegation Expirations */

DEFINE_API_IMPL( database_api_impl, list_vesting_delegation_expirations )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_vesting_delegation_expirations_return result;
   result.delegations.reserve( args.limit );

   switch( args.order )
   {
      case( by_expiration ):
      {
         auto key = args.start.as< std::pair< time_point_sec, vesting_delegation_expiration_id_type > >();
         iterate_results< chain::vesting_delegation_expiration_index, chain::by_expiration >(
            boost::make_tuple( key.first, key.second ),
            result.delegations,
            args.limit,
            &database_api_impl::on_push_default< api_vesting_delegation_expiration_object > );
         break;
      }
      case( by_account_expiration ):
      {
         auto key = args.start.as< std::vector< fc::variant > >();
         FC_ASSERT( key.size() == 3, "by_account_expiration start requires 3 values. (account_name_type, time_point_sec, vesting_delegation_expiration_id_type" );
         iterate_results< chain::vesting_delegation_expiration_index, chain::by_account_expiration >(
            boost::make_tuple( key[0].as< account_name_type >(), key[1].as< time_point_sec >(), key[2].as< vesting_delegation_expiration_id_type >() ),
            result.delegations,
            args.limit,
            &database_api_impl::on_push_default< api_vesting_delegation_expiration_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_vesting_delegation_expirations )
{
   find_vesting_delegation_expirations_return result;
   const auto& del_exp_idx = _db.get_index< chain::vesting_delegation_expiration_index, chain::by_account_expiration >();
   auto itr = del_exp_idx.lower_bound( args.account );

   while( itr != del_exp_idx.end() && itr->delegator == args.account && result.delegations.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.delegations.push_back( *itr );
      ++itr;
   }

   return result;
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Authority / Validation                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, get_transaction_hex )
{
   return get_transaction_hex_return( { fc::to_hex( fc::raw::pack_to_vector( args.trx ) ) } );
}

DEFINE_API_IMPL( database_api_impl, get_required_signatures )
{
   get_required_signatures_return result;
   result.keys = args.trx.get_required_signatures( _db.get_chain_id(),
                                                   args.available_keys,
                                                   [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).active  ); },
                                                   [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner   ); },
                                                   [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting ); },
                                                   MORPHENE_MAX_SIG_CHECK_DEPTH,
                                                   fc::ecc::canonical_signature_type::bip_0062 );

   return result;
}

DEFINE_API_IMPL( database_api_impl, get_potential_signatures )
{
   get_potential_signatures_return result;
   args.trx.get_required_signatures(
      _db.get_chain_id(),
      flat_set< public_key_type >(),
      [&]( account_name_type account_name )
      {
         const auto& auth = _db.get< chain::account_authority_object, chain::by_account >( account_name ).active;
         for( const auto& k : auth.get_keys() )
            result.keys.insert( k );
         return authority( auth );
      },
      [&]( account_name_type account_name )
      {
         const auto& auth = _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner;
         for( const auto& k : auth.get_keys() )
            result.keys.insert( k );
         return authority( auth );
      },
      [&]( account_name_type account_name )
      {
         const auto& auth = _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting;
         for( const auto& k : auth.get_keys() )
            result.keys.insert( k );
         return authority( auth );
      },
      MORPHENE_MAX_SIG_CHECK_DEPTH,
      fc::ecc::canonical_signature_type::bip_0062
   );

   return result;
}

DEFINE_API_IMPL( database_api_impl, verify_authority )
{
   args.trx.verify_authority(_db.get_chain_id(),
                           [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).active  ); },
                           [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner   ); },
                           [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting ); },
                           MORPHENE_MAX_SIG_CHECK_DEPTH,
                           MORPHENE_MAX_AUTHORITY_MEMBERSHIP,
                           MORPHENE_MAX_SIG_CHECK_ACCOUNTS,
                           fc::ecc::canonical_signature_type::bip_0062 );
   return verify_authority_return( { true } );
}

// TODO: This is broken. By the look of is, it has been since BitShares. verify_authority always
// returns false because the TX is not signed.
DEFINE_API_IMPL( database_api_impl, verify_account_authority )
{
   auto account = _db.find< chain::account_object, chain::by_name >( args.account );
   FC_ASSERT( account != nullptr, "no such account" );

   /// reuse trx.verify_authority by creating a dummy transfer
   verify_authority_args vap;
   transfer_operation op;
   op.from = account->name;
   vap.trx.operations.emplace_back( op );

   return verify_authority( vap );
}

DEFINE_API_IMPL( database_api_impl, verify_signatures )
{
   // get_signature_keys can throw for dup sigs. Allow this to throw.
   flat_set< public_key_type > sig_keys;
   for( const auto&  sig : args.signatures )
   {
      MORPHENE_ASSERT(
         sig_keys.insert( fc::ecc::public_key( sig, args.hash ) ).second,
         protocol::tx_duplicate_sig,
         "Duplicate Signature detected" );
   }

   verify_signatures_return result;
   result.valid = true;

   // verify authority throws on failure, catch and return false
   try
   {
      morphene::protocol::verify_authority< verify_signatures_args >(
         { args },
         sig_keys,
         [this]( const string& name ) { return authority( _db.get< chain::account_authority_object, chain::by_account >( name ).owner ); },
         [this]( const string& name ) { return authority( _db.get< chain::account_authority_object, chain::by_account >( name ).active ); },
         [this]( const string& name ) { return authority( _db.get< chain::account_authority_object, chain::by_account >( name ).posting ); },
         MORPHENE_MAX_SIG_CHECK_DEPTH );
   }
   catch( fc::exception& ) { result.valid = false; }

   return result;
}

DEFINE_API_IMPL( database_api_impl, get_state )
{
   string path = args[0].as< string >();
   state _state;
   _state.props         = get_dynamic_global_properties( {} );
   _state.current_route = path;
   try
   {
      if( path.size() && path[0] == '/' )
         path = path.substr(1); /// remove '/' from front
      if( !path.size() )
         path = '/';

      set<string> accounts;
      vector<string> part; part.reserve(4);
      boost::split( part, path, boost::is_any_of("/") );
      part.resize(std::max( part.size(), size_t(4) ) ); // at least 4
      auto tag = fc::to_lower( part[1] );
      if( part[0].size() && part[0][0] == '@' ) {
         auto acnt = part[0].substr(1);
         _state.accounts[acnt] = api_account_object( _db.get_account( acnt ), _db );
         // auto& eacnt = _state.accounts[acnt];
         // if( part[1] == "transfers" )
         // {
         //    // push transfers if account history enabled
         // }
      }
      else if( part[0] == "miners" || part[0] == "~miners") {
         _state.pow_queue = get_miner_queue({});
      }
      else {
         elog( "What... no matches" );
      }
      for( const auto& a : accounts )
      {
         _state.accounts.erase("");
         _state.accounts[a] = api_account_object( _db.get_account( a ), _db );
      }
      _state.witness_schedule = get_witness_schedule( {} );
   }
   catch ( const fc::exception& e )
   {
      _state.error = e.to_detail_string();
   }
   return _state;
}

DEFINE_API_IMPL( database_api_impl, get_chain_properties )
{
   return api_chain_properties( get_witness_schedule( {} ).median_props );
}

DEFINE_API_IMPL( database_api_impl, get_next_scheduled_hardfork )
{
   scheduled_hardfork shf;
   const auto& hpo = _db.get( hardfork_property_id_type() );
   shf.hf_version = hpo.next_hardfork;
   shf.live_time = hpo.next_hardfork_time;
   return shf;
}

DEFINE_API_IMPL( database_api_impl, get_accounts )
{
   vector< account_name_type > names = args[0].as< vector< account_name_type > >();
   const auto& idx  = _db.get_index< chain::account_index >().indices().get< chain::by_name >();
   const auto& vidx = _db.get_index< chain::witness_vote_index >().indices().get< chain::by_account_witness >();
   vector< extended_account > results;
   results.reserve(names.size());
   for( const auto& name: names )
   {
      auto itr = idx.find( name );
      if ( itr != idx.end() )
      {
         results.emplace_back( api_account_object( *itr, _db ) );
         auto vitr = vidx.lower_bound( boost::make_tuple( itr->name, account_name_type() ) );
         while( vitr != vidx.end() && vitr->account == itr->name ) {
            results.back().witness_votes.insert( _db.get< chain::witness_object, chain::by_name >( vitr->witness ).owner );
            ++vitr;
         }
      }
   }
   return results;
}


DEFINE_API_IMPL( database_api_impl, get_account_history )
{
   auto account = args[0].as<account_name_type>();
   auto start = args[1].as<uint64_t>();
   auto limit = args[2].as<uint32_t>();

   FC_ASSERT( limit <= 10000, "limit of ${l} is greater than maxmimum allowed", ("l",limit) );
   FC_ASSERT( start >= limit, "start must be greater than limit" );

   return _db.with_read_lock( [&]()
   {
      const auto& idx = _db.get_index< chain::account_history_index, chain::by_account >();
      auto itr = idx.lower_bound( boost::make_tuple( account, start ) );
      uint32_t n = 0;

      get_account_history_return result;
      while( true )
      {
         if( itr == idx.end() )
            break;
         if( itr->account != account )
            break;
         if( n >= limit )
            break;
         result.push_back(_db.get( itr->op ));
         ++itr;
         ++n;
      }

      return result;
   });
}

DEFINE_API_IMPL( database_api_impl, get_account_count )
{
   return _db.get_index<chain::account_index>().indices().size();
}

DEFINE_API_IMPL( database_api_impl, get_owner_history )
{
   return find_owner_histories( { args[0].as< string >() } ).owner_auths;
}

DEFINE_API_IMPL( database_api_impl, get_recovery_request )
{
   get_recovery_request_return result;
   auto requests = find_account_recovery_requests(args[0].as< find_account_recovery_requests_args >()).requests;
   if( requests.size() )
      result = requests[0];
   return result;
}

DEFINE_API_IMPL( database_api_impl, get_witnesses )
{
   vector< witness_id_type > witness_ids = args[0].as< vector< witness_id_type > >();
   get_witnesses_return result;
   result.reserve( witness_ids.size() );
   std::transform(
      witness_ids.begin(),
      witness_ids.end(),
      std::back_inserter(result),
      [this](witness_id_type id) -> optional< api_witness_object >
      {
         if( auto o = _db.find(id) )
            return api_witness_object( *o );
         return {};
      });
   return result;
}

DEFINE_API_IMPL( database_api_impl, get_witness_by_account )
{
   auto witnesses = find_witnesses(
      {
         { args[0].as< account_name_type >() }
      }).witnesses;
   get_witness_by_account_return result;
   if( witnesses.size() )
      result = api_witness_object( witnesses[0] );
   return result;
}

DEFINE_API_IMPL( database_api_impl, get_witness_count )
{
   return _db.get_index< chain::witness_index >().indices().size();
}

DEFINE_API_IMPL( database_api_impl, broadcast_transaction )
{
   FC_ASSERT( _network_broadcast_api, "network_broadcast_api_plugin not enabled." );
   return _network_broadcast_api->broadcast_transaction( args[0].as< network_broadcast_api::broadcast_transaction_args >() );
}

DEFINE_API_IMPL( database_api_impl, broadcast_transaction_synchronous )
{
   FC_ASSERT( _network_broadcast_api, "network_broadcast_api_plugin not enabled." );
   FC_ASSERT( _p2p != nullptr, "p2p_plugin not enabled." );
   signed_transaction trx = args[0].as< signed_transaction >();
   auto txid = trx.id();
   boost::promise< broadcast_transaction_synchronous_return > p;
   {
      boost::lock_guard< boost::mutex > guard( _mtx );
      FC_ASSERT( _callbacks.find( txid ) == _callbacks.end(), "Transaction is a duplicate" );
      _callbacks[ txid ] = [&p]( const broadcast_transaction_synchronous_return& r )
      {
         p.set_value( r );
      };
      _callback_expirations[ trx.expiration ].push_back( txid );
   }
   try
   {
      /* It may look strange to call these without the lock and then lock again in the case of an exception,
       * but it is correct and avoids deadlock. accept_transaction is trained along with all other writes, including
       * pushing blocks. Pushing blocks do not originate from this code path and will never have this lock.
       * However, it will trigger the on_post_apply_block callback and then attempt to acquire the lock. In this case,
       * this thread will be waiting on accept_block so it can write and the block thread will be waiting on this
       * thread for the lock.
       */
      _chain.accept_transaction( trx );
      _p2p->broadcast_transaction( trx );
   }
   catch( fc::exception& e )
   {
      boost::lock_guard< boost::mutex > guard( _mtx );
      // The callback may have been cleared in the meantine, so we need to check for existence.
      auto c_itr = _callbacks.find( txid );
      if( c_itr != _callbacks.end() ) _callbacks.erase( c_itr );
      // We do not need to clean up _callback_expirations because on_post_apply_block handles this case.
      throw e;
   }
   catch( ... )
   {
      boost::lock_guard< boost::mutex > guard( _mtx );
      // The callback may have been cleared in the meantine, so we need to check for existence.
      auto c_itr = _callbacks.find( txid );
      if( c_itr != _callbacks.end() ) _callbacks.erase( c_itr );
      throw fc::unhandled_exception(
         FC_LOG_MESSAGE( warn, "Unknown error occured when pushing transaction" ),
         std::current_exception() );
   }
   return p.get_future().get();
}

DEFINE_API_IMPL( database_api_impl, get_auction )
{
   auto result = _db.get< chain::auction_object, chain::by_permlink >( args[0].as< string >() );
   return result;
}

DEFINE_API_IMPL( database_api_impl, get_auctions_by_status )
{
   vector< string > statuses = args[0].as< vector< string > >();
   const auto& auction_idx = _db.get_index<auction_index>().indices().get<by_status>();
   vector< api_auction_object > results;

   for( const auto& status: statuses )
   {
      auto itr = auction_idx.lower_bound(status);
      auto itr_end = auction_idx.upper_bound(status);
      while( itr != itr_end && results.size() < args[1].as< uint32_t >())
      {
         results.push_back( *itr );
         ++itr;
      }
   }
   return results;
}

DEFINE_API_IMPL( database_api_impl, get_auctions_by_status_start_time )
{
   vector< string > statuses = args[0].as< vector< string > >();
   fc::time_point_sec start = args[1].as< fc::time_point_sec >();
   uint32_t limit = args[2].as< uint32_t >();

   const auto& auction_idx = _db.get_index<auction_index>().indices().get<by_status_start_time>();
   vector< api_auction_object > results;

   for( const auto& status: statuses )
   {
      auto itr = auction_idx.lower_bound(boost::make_tuple( status, start ));
      auto itr_end = auction_idx.upper_bound(boost::make_tuple( status, fc::time_point_sec::maximum() ));
      while( itr != itr_end && results.size() < limit)
      {
         results.push_back( *itr );
         ++itr;
      }
   }
   return results;
}

DEFINE_API_IMPL( database_api_impl, get_auctions_by_status_end_time )
{
   vector< string > statuses = args[0].as< vector< string > >();
   fc::time_point_sec start = args[1].as< fc::time_point_sec >();
   uint32_t limit = args[2].as< uint32_t >();

   const auto& auction_idx = _db.get_index<auction_index>().indices().get<by_status_start_time>();
   vector< api_auction_object > results;

   for( const auto& status: statuses )
   {
      auto itr = auction_idx.lower_bound(boost::make_tuple( status, start ));
      auto itr_end = auction_idx.upper_bound(boost::make_tuple( status, fc::time_point_sec::maximum() ));
      while( itr != itr_end && results.size() < limit)
      {
         results.push_back( *itr );
         ++itr;
      }
   }
   return results;
}

DEFINE_API_IMPL( database_api_impl, get_bids )
{
   vector< api_bid_object > result;
   const auto& bid_idx = _db.get_index<bid_index>().indices().get<by_permlink>();
   auto permlink = args[0].as< string >();
   auto itr = bid_idx.lower_bound(permlink);
   while( itr != bid_idx.end() && result.size() < args[1].as< uint32_t >()) {
      if( itr->permlink == permlink) {
         result.push_back( *itr );
      }
      ++itr;
   }
   return result;
}

DEFINE_LOCKLESS_APIS( database_api,
   (get_config)
   (get_version)
   (broadcast_transaction)
   (broadcast_transaction_synchronous)
)

DEFINE_READ_APIS( database_api,
   (get_block_header)
   (get_block)
   (get_ops_in_block)
   (get_dynamic_global_properties)
   (get_witness_schedule)
   (get_hardfork_properties)
   (list_witnesses)
   (find_witnesses)
   (list_witness_votes)
   (get_active_witnesses)
   (list_accounts)
   (find_accounts)
   (list_owner_histories)
   (find_owner_histories)
   (list_account_recovery_requests)
   (find_account_recovery_requests)
   (list_change_recovery_account_requests)
   (find_change_recovery_account_requests)
   (list_escrows)
   (find_escrows)
   (list_withdraw_vesting_routes)
   (find_withdraw_vesting_routes)
   (list_vesting_delegations)
   (find_vesting_delegations)
   (list_vesting_delegation_expirations)
   (find_vesting_delegation_expirations)
   (get_transaction_hex)
   (get_required_signatures)
   (get_potential_signatures)
   (verify_authority)
   (verify_account_authority)
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
   (get_witness_by_account)
   (get_witness_count)
   (get_auction)
   (get_auctions_by_status)
   (get_auctions_by_status_start_time)
   (get_auctions_by_status_end_time)
   (get_bids)
)

} } } // morphene::plugins::database_api
