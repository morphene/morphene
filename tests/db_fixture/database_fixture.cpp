#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>

#include <steem/utilities/tempdir.hpp>

#include <steem/chain/history_object.hpp>
#include <steem/chain/steem_objects.hpp>

#include <steem/plugins/account_history/account_history_plugin.hpp>
#include <steem/plugins/chain/chain_plugin.hpp>
#include <steem/plugins/rc/rc_plugin.hpp>
#include <steem/plugins/webserver/webserver_plugin.hpp>
#include <steem/plugins/witness/witness_plugin.hpp>

#include <steem/plugins/condenser_api/condenser_api_plugin.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/smart_ref_impl.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>

#include "database_fixture.hpp"

//using namespace steem::chain::test;

uint32_t STEEM_TESTING_GENESIS_TIMESTAMP = 1431700000;

using namespace steem::plugins::webserver;
using namespace steem::plugins::database_api;
using namespace steem::plugins::block_api;
using steem::plugins::condenser_api::condenser_api_plugin;

namespace steem { namespace chain {

using std::cout;
using std::cerr;

clean_database_fixture::clean_database_fixture()
{
   try {
   int argc = boost::unit_test::framework::master_test_suite().argc;
   char** argv = boost::unit_test::framework::master_test_suite().argv;
   for( int i=1; i<argc; i++ )
   {
      const std::string arg = argv[i];
      if( arg == "--record-assert-trip" )
         fc::enable_record_assert_trip = true;
      if( arg == "--show-test-names" )
         std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name << std::endl;
   }

   appbase::app().register_plugin< steem::plugins::account_history::account_history_plugin >();
   db_plugin = &appbase::app().register_plugin< steem::plugins::debug_node::debug_node_plugin >();
   appbase::app().register_plugin< steem::plugins::rc::rc_plugin >();
   appbase::app().register_plugin< steem::plugins::witness::witness_plugin >();

   db_plugin->logging = false;
   appbase::app().initialize<
      steem::plugins::account_history::account_history_plugin,
      steem::plugins::debug_node::debug_node_plugin,
      steem::plugins::rc::rc_plugin,
      steem::plugins::witness::witness_plugin
      >( argc, argv );

   steem::plugins::rc::rc_plugin_skip_flags rc_skip;
   rc_skip.skip_reject_not_enough_rc = 1;
   rc_skip.skip_deduct_rc = 0;
   rc_skip.skip_negative_rc_balance = 1;
   rc_skip.skip_reject_unknown_delta_vests = 0;
   appbase::app().get_plugin< steem::plugins::rc::rc_plugin >().set_rc_plugin_skip_flags( rc_skip );

   db = &appbase::app().get_plugin< steem::plugins::chain::chain_plugin >().db();
   BOOST_REQUIRE( db );

   init_account_pub_key = init_account_priv_key.get_public_key();

   open_database();

   generate_block();
   db->set_hardfork( STEEM_BLOCKCHAIN_VERSION.minor() );
   generate_block();

   vest( "initminer", 10000 );

   // Fill up the rest of the required miners
   for( int i = STEEM_NUM_INIT_MINERS; i < STEEM_MAX_WITNESSES; i++ )
   {
      account_create( STEEM_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( STEEM_INIT_MINER_NAME + fc::to_string( i ), STEEM_MIN_PRODUCER_REWARD.amount.value );
      witness_create( STEEM_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, STEEM_MIN_PRODUCER_REWARD.amount );
   }

   validate_database();
   } catch ( const fc::exception& e )
   {
      edump( (e.to_detail_string()) );
      throw;
   }

   return;
}

clean_database_fixture::~clean_database_fixture()
{ try {
   // If we're unwinding due to an exception, don't do any more checks.
   // This way, boost test's last checkpoint tells us approximately where the error was.
   if( !std::uncaught_exception() )
   {
      BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
   }

   if( data_dir )
      db->wipe( data_dir->path(), data_dir->path(), true );
   return;
} FC_CAPTURE_AND_LOG( () )
   exit(1);
}

void clean_database_fixture::validate_database()
{
   database_fixture::validate_database();
   appbase::app().get_plugin< steem::plugins::rc::rc_plugin >().validate_database();
}

void clean_database_fixture::resize_shared_mem( uint64_t size )
{
   db->wipe( data_dir->path(), data_dir->path(), true );
   int argc = boost::unit_test::framework::master_test_suite().argc;
   char** argv = boost::unit_test::framework::master_test_suite().argv;
   for( int i=1; i<argc; i++ )
   {
      const std::string arg = argv[i];
      if( arg == "--record-assert-trip" )
         fc::enable_record_assert_trip = true;
      if( arg == "--show-test-names" )
         std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name << std::endl;
   }
   init_account_pub_key = init_account_priv_key.get_public_key();

   {
      database::open_args args;
      args.data_dir = data_dir->path();
      args.shared_mem_dir = args.data_dir;
      args.initial_supply = INITIAL_TEST_SUPPLY;
      args.shared_file_size = size;
      db->open( args );
   }

   boost::program_options::variables_map options;


   generate_block();
   db->set_hardfork( STEEM_BLOCKCHAIN_VERSION.minor() );
   generate_block();

   vest( "initminer", 10000 );

   // Fill up the rest of the required miners
   for( int i = STEEM_NUM_INIT_MINERS; i < STEEM_MAX_WITNESSES; i++ )
   {
      account_create( STEEM_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( STEEM_INIT_MINER_NAME + fc::to_string( i ), STEEM_MIN_PRODUCER_REWARD.amount.value );
      witness_create( STEEM_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, STEEM_MIN_PRODUCER_REWARD.amount );
   }

   validate_database();
}

live_database_fixture::live_database_fixture()
{
   try
   {
      int argc = boost::unit_test::framework::master_test_suite().argc;
      char** argv = boost::unit_test::framework::master_test_suite().argv;

      ilog( "Loading saved chain" );
      _chain_dir = fc::current_path() / "test_blockchain";
      FC_ASSERT( fc::exists( _chain_dir ), "Requires blockchain to test on in ./test_blockchain" );

      appbase::app().register_plugin< steem::plugins::account_history::account_history_plugin >();
      appbase::app().initialize<
         steem::plugins::account_history::account_history_plugin
         >( argc, argv );

      db = &appbase::app().get_plugin< steem::plugins::chain::chain_plugin >().db();
      BOOST_REQUIRE( db );

      {
         database::open_args args;
         args.data_dir = _chain_dir;
         args.shared_mem_dir = args.data_dir;
         db->open( args );
      }

      validate_database();
      generate_block();

      ilog( "Done loading saved chain" );
   }
   FC_LOG_AND_RETHROW()
}

live_database_fixture::~live_database_fixture()
{
   try
   {
      // If we're unwinding due to an exception, don't do any more checks.
      // This way, boost test's last checkpoint tells us approximately where the error was.
      if( !std::uncaught_exception() )
      {
         BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
      }

      db->pop_block();
      db->close();
      return;
   }
   FC_CAPTURE_AND_LOG( () )
   exit(1);
}

fc::ecc::private_key database_fixture::generate_private_key(string seed)
{
   static const fc::ecc::private_key committee = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );
   if( seed == "init_key" )
      return committee;
   return fc::ecc::private_key::regenerate( fc::sha256::hash( seed ) );
}

void database_fixture::open_database()
{
   if( !data_dir )
   {
      data_dir = fc::temp_directory( steem::utilities::temp_directory_path() );
      db->_log_hardforks = false;

      database::open_args args;
      args.data_dir = data_dir->path();
      args.shared_mem_dir = args.data_dir;
      args.initial_supply = INITIAL_TEST_SUPPLY;
      args.shared_file_size = 1024 * 1024 * 8;     // 8MB file for testing
      db->open(args);
   }
}

void database_fixture::generate_block(uint32_t skip, const fc::ecc::private_key& key, int miss_blocks)
{
   skip |= default_skip;
   db_plugin->debug_generate_blocks( steem::utilities::key_to_wif( key ), 1, skip, miss_blocks );
}

void database_fixture::generate_blocks( uint32_t block_count )
{
   auto produced = db_plugin->debug_generate_blocks( debug_key, block_count, default_skip, 0 );
   BOOST_REQUIRE( produced == block_count );
}

void database_fixture::generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks)
{
   db_plugin->debug_generate_blocks_until( debug_key, timestamp, miss_intermediate_blocks, default_skip );
   BOOST_REQUIRE( ( db->head_block_time() - timestamp ).to_seconds() < STEEM_BLOCK_INTERVAL );
}

const account_object& database_fixture::account_create(
   const string& name,
   const string& creator,
   const private_key_type& creator_key,
   const share_type& fee,
   const public_key_type& key,
   const public_key_type& post_key,
   const string& json_metadata
   )
{
   try
   {
      auto actual_fee = std::min( fee, db->get_witness_schedule_object().median_props.account_creation_fee.amount );
      auto fee_remainder = fee - actual_fee;

      account_create_operation op;
      op.new_account_name = name;
      op.creator = creator;
      op.fee = asset( actual_fee, STEEM_SYMBOL );
      op.owner = authority( 1, key, 1 );
      op.active = authority( 1, key, 1 );
      op.posting = authority( 1, post_key, 1 );
      op.memo_key = key;
      op.json_metadata = json_metadata;

      trx.operations.push_back( op );

      trx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      sign( trx, creator_key );
      trx.validate();
      db->push_transaction( trx, 0 );
      trx.clear();

      if( fee_remainder > 0 )
      {
         vest( STEEM_INIT_MINER_NAME, name, asset( fee_remainder, STEEM_SYMBOL ) );
      }

      const account_object& acct = db->get_account( name );

      return acct;
   }
   FC_CAPTURE_AND_RETHROW( (name)(creator) )
}

const account_object& database_fixture::account_create(
   const string& name,
   const public_key_type& key,
   const public_key_type& post_key
)
{
   try
   {
      return account_create(
         name,
         STEEM_INIT_MINER_NAME,
         init_account_priv_key,
         std::max( db->get_witness_schedule_object().median_props.account_creation_fee.amount * STEEM_CREATE_ACCOUNT_WITH_STEEM_MODIFIER, share_type( 100 ) ),
         key,
         post_key,
         "" );
   }
   FC_CAPTURE_AND_RETHROW( (name) );
}

const account_object& database_fixture::account_create(
   const string& name,
   const public_key_type& key
)
{
   return account_create( name, key, key );
}

const witness_object& database_fixture::witness_create(
   const string& owner,
   const private_key_type& owner_key,
   const string& url,
   const public_key_type& signing_key,
   const share_type& fee )
{
   try
   {
      witness_update_operation op;
      op.owner = owner;
      op.url = url;
      op.block_signing_key = signing_key;
      op.fee = asset( fee, STEEM_SYMBOL );

      trx.operations.push_back( op );
      trx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      sign( trx, owner_key );
      trx.validate();
      db->push_transaction( trx, 0 );
      trx.clear();

      return db->get_witness( owner );
   }
   FC_CAPTURE_AND_RETHROW( (owner)(url) )
}

void database_fixture::fund(
   const string& account_name,
   const share_type& amount
   )
{
   try
   {
      transfer( STEEM_INIT_MINER_NAME, account_name, asset( amount, STEEM_SYMBOL ) );

   } FC_CAPTURE_AND_RETHROW( (account_name)(amount) )
}

void database_fixture::fund(
   const string& account_name,
   const asset& amount
   )
{
   try
   {
      db_plugin->debug_update( [=]( database& db)
      {
         if( amount.symbol.space() == asset_symbol_type::smt_nai_space )
         {
            db.adjust_balance(account_name, amount);
            db.adjust_supply(amount);
            return;
         }

         db.modify( db.get_account( account_name ), [&]( account_object& a )
         {
            if( amount.symbol == STEEM_SYMBOL )
               a.balance += amount;
         });

         db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
         {
            if( amount.symbol == STEEM_SYMBOL )
               gpo.current_supply += amount;
         });
      }, default_skip );
   }
   FC_CAPTURE_AND_RETHROW( (account_name)(amount) )
}

void database_fixture::transfer(
   const string& from,
   const string& to,
   const asset& amount )
{
   try
   {
      transfer_operation op;
      op.from = from;
      op.to = to;
      op.amount = amount;

      trx.operations.push_back( op );
      trx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      trx.validate();

      if( from == STEEM_INIT_MINER_NAME )
      {
         sign( trx, init_account_priv_key );
      }

      db->push_transaction( trx, ~0 );
      trx.clear();
   } FC_CAPTURE_AND_RETHROW( (from)(to)(amount) )
}

void database_fixture::vest( const string& from, const string& to, const asset& amount )
{
   try
   {
      FC_ASSERT( amount.symbol == STEEM_SYMBOL, "Can only vest TESTS" );

      transfer_to_vesting_operation op;
      op.from = from;
      op.to = to;
      op.amount = amount;

      trx.operations.push_back( op );
      trx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      trx.validate();

      // This sign() call fixes some tests, like withdraw_vesting_apply, that use this method
      //   with debug_plugin such that trx may be re-applied with less generous skip flags.
      if( from == STEEM_INIT_MINER_NAME )
      {
         sign( trx, init_account_priv_key );
      }

      db->push_transaction( trx, ~0 );
      trx.clear();
   } FC_CAPTURE_AND_RETHROW( (from)(to)(amount) )
}

void database_fixture::vest( const string& from, const share_type& amount )
{
   try
   {
      transfer_to_vesting_operation op;
      op.from = from;
      op.to = "";
      op.amount = asset( amount, STEEM_SYMBOL );

      trx.operations.push_back( op );
      trx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      trx.validate();

      if( from == STEEM_INIT_MINER_NAME )
      {
         sign( trx, init_account_priv_key );
      }

      db->push_transaction( trx, ~0 );
      trx.clear();
   } FC_CAPTURE_AND_RETHROW( (from)(amount) )
}

void database_fixture::proxy( const string& account, const string& proxy )
{
   try
   {
      account_witness_proxy_operation op;
      op.account = account;
      op.proxy = proxy;
      trx.operations.push_back( op );
      db->push_transaction( trx, ~0 );
      trx.clear();
   } FC_CAPTURE_AND_RETHROW( (account)(proxy) )
}

void database_fixture::set_price_feed( const price& new_price )
{
   for( size_t i = 1; i < 8; i++ )
   {
      witness_set_properties_operation op;
      op.owner = STEEM_INIT_MINER_NAME + fc::to_string( i );
      op.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );

      trx.operations.push_back( op );
      trx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      db->push_transaction( trx, ~0 );
      trx.clear();
   }

   generate_blocks( STEEM_BLOCKS_PER_HOUR );

   BOOST_REQUIRE(
#ifdef IS_TEST_NET
      !db->skip_price_feed_limit_check ||
#endif
      db->get(feed_history_id_type()).current_median_history == new_price
   );
}

void database_fixture::set_witness_props( const flat_map< string, vector< char > >& props )
{
   trx.clear();
   for( size_t i=0; i<STEEM_MAX_WITNESSES; i++ )
   {
      witness_set_properties_operation op;
      op.owner = STEEM_INIT_MINER_NAME + (i == 0 ? "" : fc::to_string( i ));
      op.props = props;
      if( props.find( "key" ) == props.end() )
         op.props["key"] = fc::raw::pack_to_vector( init_account_pub_key );

      trx.operations.push_back( op );
      trx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      db->push_transaction( trx, ~0 );
      trx.clear();
   }

   const witness_schedule_object* wso = &(db->get_witness_schedule_object());
   uint32_t old_next_shuffle = wso->next_shuffle_block_num;

   for( size_t i=0; i<2*STEEM_MAX_WITNESSES+1; i++ )
   {
      generate_block();
      wso = &(db->get_witness_schedule_object());
      if( wso->next_shuffle_block_num != old_next_shuffle )
         return;
   }
   FC_ASSERT( false, "Couldn't apply properties in ${n} blocks", ("n", 2*STEEM_MAX_WITNESSES+1) );
}

const asset& database_fixture::get_balance( const string& account_name )const
{
  return db->get_account( account_name ).balance;
}

void database_fixture::sign(signed_transaction& trx, const fc::ecc::private_key& key)
{
   trx.sign( key, db->get_chain_id(), default_sig_canon );
}

vector< operation > database_fixture::get_last_operations( uint32_t num_ops )
{
   vector< operation > ops;
   const auto& acc_hist_idx = db->get_index< account_history_index >().indices().get< by_id >();
   auto itr = acc_hist_idx.end();

   while( itr != acc_hist_idx.begin() && ops.size() < num_ops )
   {
      itr--;
      const buffer_type& _serialized_op = db->get(itr->op).serialized_op;
      std::vector<char> serialized_op;
      serialized_op.reserve( _serialized_op.size() );
      std::copy( _serialized_op.begin(), _serialized_op.end(), std::back_inserter( serialized_op ) );
      ops.push_back( fc::raw::unpack_from_vector< steem::chain::operation >( serialized_op ) );
   }

   return ops;
}

void database_fixture::validate_database()
{
   try
   {
      db->validate_invariants();
   }
   FC_LOG_AND_RETHROW();
}

json_rpc_database_fixture::json_rpc_database_fixture()
{
   try {
   int argc = boost::unit_test::framework::master_test_suite().argc;
   char** argv = boost::unit_test::framework::master_test_suite().argv;
   for( int i=1; i<argc; i++ )
   {
      const std::string arg = argv[i];
      if( arg == "--record-assert-trip" )
         fc::enable_record_assert_trip = true;
      if( arg == "--show-test-names" )
         std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name << std::endl;
   }

   appbase::app().register_plugin< steem::plugins::account_history::account_history_plugin >();
   db_plugin = &appbase::app().register_plugin< steem::plugins::debug_node::debug_node_plugin >();
   appbase::app().register_plugin< steem::plugins::witness::witness_plugin >();
   rpc_plugin = &appbase::app().register_plugin< steem::plugins::json_rpc::json_rpc_plugin >();
   appbase::app().register_plugin< steem::plugins::block_api::block_api_plugin >();
   appbase::app().register_plugin< steem::plugins::database_api::database_api_plugin >();
   appbase::app().register_plugin< steem::plugins::condenser_api::condenser_api_plugin >();

   db_plugin->logging = false;
   appbase::app().initialize<
      steem::plugins::account_history::account_history_plugin,
      steem::plugins::debug_node::debug_node_plugin,
      steem::plugins::json_rpc::json_rpc_plugin,
      steem::plugins::block_api::block_api_plugin,
      steem::plugins::database_api::database_api_plugin,
      steem::plugins::condenser_api::condenser_api_plugin
      >( argc, argv );

   appbase::app().get_plugin< steem::plugins::condenser_api::condenser_api_plugin >().plugin_startup();

   db = &appbase::app().get_plugin< steem::plugins::chain::chain_plugin >().db();
   BOOST_REQUIRE( db );

   init_account_pub_key = init_account_priv_key.get_public_key();

   open_database();

   generate_block();
   db->set_hardfork( STEEM_BLOCKCHAIN_VERSION.minor() );
   generate_block();

   vest( "initminer", 10000 );

   // Fill up the rest of the required miners
   for( int i = STEEM_NUM_INIT_MINERS; i < STEEM_MAX_WITNESSES; i++ )
   {
      account_create( STEEM_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( STEEM_INIT_MINER_NAME + fc::to_string( i ), STEEM_MIN_PRODUCER_REWARD.amount.value );
      witness_create( STEEM_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, STEEM_MIN_PRODUCER_REWARD.amount );
   }

   validate_database();
   } catch ( const fc::exception& e )
   {
      edump( (e.to_detail_string()) );
      throw;
   }

   return;
}

json_rpc_database_fixture::~json_rpc_database_fixture()
{ try {
   // If we're unwinding due to an exception, don't do any more checks.
   // This way, boost test's last checkpoint tells us approximately where the error was.
   if( !std::uncaught_exception() )
   {
      BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
   }

   if( data_dir )
      db->wipe( data_dir->path(), data_dir->path(), true );
   return;
} FC_CAPTURE_AND_RETHROW() }

fc::variant json_rpc_database_fixture::get_answer( std::string& request )
{
   return fc::json::from_string( rpc_plugin->call( request ) );
}

void check_id_equal( const fc::variant& id_a, const fc::variant& id_b )
{
   BOOST_REQUIRE( id_a.get_type() == id_b.get_type() );

   switch( id_a.get_type() )
   {
      case fc::variant::int64_type:
         BOOST_REQUIRE( id_a.as_int64() == id_b.as_int64() );
         break;
      case fc::variant::uint64_type:
         BOOST_REQUIRE( id_a.as_uint64() == id_b.as_uint64() );
         break;
      case fc::variant::string_type:
         BOOST_REQUIRE( id_a.as_string() == id_b.as_string() );
         break;
      case fc::variant::null_type:
         break;
      default:
         BOOST_REQUIRE( false );
   }
}

void json_rpc_database_fixture::review_answer( fc::variant& answer, int64_t code, bool is_warning, bool is_fail, fc::optional< fc::variant > id )
{
   fc::variant_object error;
   int64_t answer_code;

   if( is_fail )
   {
      if( id.valid() && code != JSON_RPC_INVALID_REQUEST )
      {
         BOOST_REQUIRE( answer.get_object().contains( "id" ) );
         check_id_equal( answer[ "id" ], *id );
      }

      BOOST_REQUIRE( answer.get_object().contains( "error" ) );
      BOOST_REQUIRE( answer["error"].is_object() );
      error = answer["error"].get_object();
      BOOST_REQUIRE( error.contains( "code" ) );
      BOOST_REQUIRE( error["code"].is_int64() );
      answer_code = error["code"].as_int64();
      BOOST_REQUIRE( answer_code == code );
      if( is_warning )
         BOOST_TEST_MESSAGE( error["message"].as_string() );
   }
   else
   {
      BOOST_REQUIRE( answer.get_object().contains( "result" ) );
      BOOST_REQUIRE( answer.get_object().contains( "id" ) );
      if( id.valid() )
         check_id_equal( answer[ "id" ], *id );
   }
}

void json_rpc_database_fixture::make_array_request( std::string& request, int64_t code, bool is_warning, bool is_fail )
{
   fc::variant answer = get_answer( request );
   BOOST_REQUIRE( answer.is_array() );

   fc::variants request_array = fc::json::from_string( request ).get_array();
   fc::variants array = answer.get_array();

   BOOST_REQUIRE( array.size() == request_array.size() );
   for( size_t i = 0; i < array.size(); ++i )
   {
      fc::optional< fc::variant > id;

      try
      {
         id = request_array[i][ "id" ];
      }
      catch( ... ) {}

      review_answer( array[i], code, is_warning, is_fail, id );
   }
}

fc::variant json_rpc_database_fixture::make_request( std::string& request, int64_t code, bool is_warning, bool is_fail )
{
   fc::variant answer = get_answer( request );
   BOOST_REQUIRE( answer.is_object() );
   fc::optional< fc::variant > id;

   try
   {
      id = fc::json::from_string( request ).get_object()[ "id" ];
   }
   catch( ... ) {}

   review_answer( answer, code, is_warning, is_fail, id );

   return answer;
}

void json_rpc_database_fixture::make_positive_request( std::string& request )
{
   make_request( request, 0/*code*/, false/*is_warning*/, false/*is_fail*/);
}

namespace test {

bool _push_block( database& db, const signed_block& b, uint32_t skip_flags /* = 0 */ )
{
   return db.push_block( b, skip_flags);
}

void _push_transaction( database& db, const signed_transaction& tx, uint32_t skip_flags /* = 0 */ )
{ try {
   db.push_transaction( tx, skip_flags );
} FC_CAPTURE_AND_RETHROW((tx)) }

} // steem::chain::test

} } // steem::chain
