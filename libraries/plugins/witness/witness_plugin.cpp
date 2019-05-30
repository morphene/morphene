#include <morphene/plugins/witness/witness_plugin.hpp>

#include <morphene/chain/database_exceptions.hpp>
#include <morphene/chain/account_object.hpp>
#include <morphene/chain/witness_objects.hpp>
#include <morphene/chain/index.hpp>
#include <morphene/chain/util/impacted.hpp>

#include <morphene/utilities/key_conversion.hpp>
#include <morphene/utilities/plugin_utilities.hpp>

#include <fc/io/json.hpp>
#include <fc/macros.hpp>
#include <fc/smart_ref_impl.hpp>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>


#define DISTANCE_CALC_PRECISION (10000)
#define BLOCK_PRODUCING_LAG_TIME (750)
#define BLOCK_PRODUCTION_LOOP_SLEEP_TIME (200000)


namespace morphene { namespace plugins { namespace witness {

using namespace morphene::chain;

using std::string;
using std::vector;

namespace bpo = boost::program_options;


void new_chain_banner( const chain::database& db )
{
   std::cerr << "\n"
      "********************************\n"
      "*                              *\n"
      "*   ------- NEW CHAIN ------   *\n"
      "*   - Welcome to Morphene! -   *\n"
      "*   ------------------------   *\n"
      "*                              *\n"
      "********************************\n"
      "\n";
   return;
}

namespace detail {

   class witness_plugin_impl {
   public:
      witness_plugin_impl( boost::asio::io_service& io ) :
         _timer(io),
         _chain_plugin( appbase::app().get_plugin< morphene::plugins::chain::chain_plugin >() ),
         _db( appbase::app().get_plugin< morphene::plugins::chain::chain_plugin >().db() )
         {}

      void on_pre_apply_block( const chain::block_notification& note );
      void on_post_apply_block( const chain::block_notification& note );
      void on_pre_apply_operation( const chain::operation_notification& note );
      void on_post_apply_operation( const chain::operation_notification& note );

      void schedule_production_loop();
      block_production_condition::block_production_condition_enum block_production_loop();
      block_production_condition::block_production_condition_enum maybe_produce_block(fc::mutable_variant_object& capture);

      boost::program_options::variables_map _options;
      bool     _production_enabled              = false;
      uint32_t _required_witness_participation  = 33 * MORPHENE_1_PERCENT;
      uint32_t _production_skip_flags = morphene::chain::database::skip_nothing;
      uint32_t _mining_threads = 0;

      uint64_t         _head_block_num       = 0;
      uint64_t         _total_hashes         = 0;
      fc::time_point   _hash_start_time;

      std::vector<std::shared_ptr<fc::thread> > _thread_pool;

      std::map< morphene::protocol::public_key_type, fc::ecc::private_key > _private_keys;
      std::set<string>                                                      _witnesses;
      std::map<string,public_key_type>                                      _miners;
      chain::legacy_chain_properties                                        _miner_prop_vote;
      boost::asio::deadline_timer                                           _timer;

      plugins::chain::chain_plugin& _chain_plugin;
      chain::database&              _db;
      boost::signals2::connection   _pre_apply_block_conn;
      boost::signals2::connection   _post_apply_block_conn;
      boost::signals2::connection   _pre_apply_operation_conn;
      boost::signals2::connection   _post_apply_operation_conn;
      boost::signals2::connection   _applied_block_conn;
   };

   void check_memo( const string& memo, const chain::account_object& account, const account_authority_object& auth )
   {
      vector< public_key_type > keys;

      try
      {
         // Check if memo is a private key
         keys.push_back( fc::ecc::extended_private_key::from_base58( memo ).get_public_key() );
      }
      catch( fc::parse_error_exception& ) {}
      catch( fc::assert_exception& ) {}

      // Get possible keys if memo was an account password
      string owner_seed = account.name + "owner" + memo;
      auto owner_secret = fc::sha256::hash( owner_seed.c_str(), owner_seed.size() );
      keys.push_back( fc::ecc::private_key::regenerate( owner_secret ).get_public_key() );

      string active_seed = account.name + "active" + memo;
      auto active_secret = fc::sha256::hash( active_seed.c_str(), active_seed.size() );
      keys.push_back( fc::ecc::private_key::regenerate( active_secret ).get_public_key() );

      string posting_seed = account.name + "posting" + memo;
      auto posting_secret = fc::sha256::hash( posting_seed.c_str(), posting_seed.size() );
      keys.push_back( fc::ecc::private_key::regenerate( posting_secret ).get_public_key() );

      // Check keys against public keys in authorites
      for( auto& key_weight_pair : auth.owner.key_auths )
      {
         for( auto& key : keys )
            MORPHENE_ASSERT( key_weight_pair.first != key,  plugin_exception,
               "Detected private owner key in memo field. You should change your owner keys." );
      }

      for( auto& key_weight_pair : auth.active.key_auths )
      {
         for( auto& key : keys )
            MORPHENE_ASSERT( key_weight_pair.first != key,  plugin_exception,
               "Detected private active key in memo field. You should change your active keys." );
      }

      for( auto& key_weight_pair : auth.posting.key_auths )
      {
         for( auto& key : keys )
            MORPHENE_ASSERT( key_weight_pair.first != key,  plugin_exception,
               "Detected private posting key in memo field. You should change your posting keys." );
      }

      const auto& memo_key = account.memo_key;
      for( auto& key : keys )
         MORPHENE_ASSERT( memo_key != key,  plugin_exception,
            "Detected private memo key in memo field. You should change your memo key." );
   }

   struct operation_visitor
   {
      operation_visitor( const chain::database& db ) : _db( db ) {}

      const chain::database& _db;

      typedef void result_type;

      template< typename T >
      void operator()( const T& )const {}

      void operator()( const transfer_operation& o )const
      {
         if( o.memo.length() > 0 )
            check_memo( o.memo,
                        _db.get< chain::account_object, chain::by_name >( o.from ),
                        _db.get< account_authority_object, chain::by_account >( o.from ) );
      }
   };

   void witness_plugin_impl::on_pre_apply_block( const chain::block_notification& b )
   {
      return;
   }

   void witness_plugin_impl::on_pre_apply_operation( const chain::operation_notification& note )
   {
      if( _db.is_producing() )
      {
         note.op.visit( operation_visitor( _db ) );
      }
   }

   void witness_plugin_impl::on_post_apply_operation( const chain::operation_notification& note )
   {
      switch( note.op.which() )
      {
         default:
            break;
      }
   }

   void witness_plugin_impl::on_post_apply_block( const block_notification& note )
   {
      return;
   }

   void witness_plugin_impl::schedule_production_loop() {
      // Sleep for 200ms, before checking the block production
      fc::time_point now = fc::time_point::now();
      int64_t time_to_sleep = BLOCK_PRODUCTION_LOOP_SLEEP_TIME - (now.time_since_epoch().count() % BLOCK_PRODUCTION_LOOP_SLEEP_TIME);
      if (time_to_sleep < 50000) // we must sleep for at least 50ms
          time_to_sleep += BLOCK_PRODUCTION_LOOP_SLEEP_TIME;

      _timer.expires_from_now( boost::posix_time::microseconds( time_to_sleep ) );
      _timer.async_wait( boost::bind( &witness_plugin_impl::block_production_loop, this ) );
   }

   block_production_condition::block_production_condition_enum witness_plugin_impl::block_production_loop()
   {
      if( fc::time_point::now() < fc::time_point(MORPHENE_GENESIS_TIME) )
      {
         wlog( "waiting until genesis time to produce block: ${t}", ("t",MORPHENE_GENESIS_TIME) );
         schedule_production_loop();
         return block_production_condition::wait_for_genesis;
      }

      block_production_condition::block_production_condition_enum result;
      fc::mutable_variant_object capture;
      try
      {
         result = maybe_produce_block(capture);
      }
      catch( const fc::canceled_exception& )
      {
         //We're trying to exit. Go ahead and let this one out.
         throw;
      }
      catch( const chain::unknown_hardfork_exception& e )
      {
         // Hit a hardfork that the current node know nothing about, stop production and inform user
         elog( "${e}\nNode may be out of date...", ("e", e.to_detail_string()) );
         throw;
      }
      catch( const fc::exception& e )
      {
         elog("Got exception while generating block:\n${e}", ("e", e.to_detail_string()));
         result = block_production_condition::exception_producing_block;
      }

      switch(result)
      {
         case block_production_condition::produced:
            ilog("Generated block #${n} with timestamp ${t} at time ${c}", (capture));
            break;
         case block_production_condition::not_synced:
   //         ilog("Not producing block because production is disabled until we receive a recent block (see: --enable-stale-production)");
            break;
         case block_production_condition::not_my_turn:
   //         ilog("Not producing block because it isn't my turn");
            break;
         case block_production_condition::not_time_yet:
   //         ilog("Not producing block because slot has not yet arrived");
            break;
         case block_production_condition::no_private_key:
            ilog("Not producing block because I don't have the private key for ${scheduled_key}", (capture) );
            break;
         case block_production_condition::low_participation:
            elog("Not producing block because node appears to be on a minority fork with only ${pct}% witness participation", (capture) );
            break;
         case block_production_condition::lag:
            elog("Not producing block because node didn't wake up within ${t}ms of the slot time.", ("t", BLOCK_PRODUCING_LAG_TIME));
            break;
         case block_production_condition::consecutive:
            elog("Not producing block because the last block was generated by the same witness.\nThis node is probably disconnected from the network so block production has been disabled.\nDisable this check with --allow-consecutive option.");
            break;
         case block_production_condition::exception_producing_block:
            elog( "exception producing block" );
            break;
         case block_production_condition::wait_for_genesis:
            break;
      }

      schedule_production_loop();
      return result;
   }

   block_production_condition::block_production_condition_enum witness_plugin_impl::maybe_produce_block(fc::mutable_variant_object& capture)
   {
      fc::time_point now_fine = fc::time_point::now();
      fc::time_point_sec now = now_fine + fc::microseconds( 500000 );

      // If the next block production opportunity is in the present or future, we're synced.
      if( !_production_enabled )
      {
         if( _db.get_slot_time(1) >= now )
            _production_enabled = true;
         else
            return block_production_condition::not_synced;
      }

      // is anyone scheduled to produce now or one second in the future?
      uint32_t slot = _db.get_slot_at_time( now );
      if( slot == 0 )
      {
         capture("next_time", _db.get_slot_time(1));
         return block_production_condition::not_time_yet;
      }

      //
      // this assert should not fail, because now <= _db.head_block_time()
      // should have resulted in slot == 0.
      //
      // if this assert triggers, there is a serious bug in get_slot_at_time()
      // which would result in allowing a later block to have a timestamp
      // less than or equal to the previous block
      //
      assert( now > _db.head_block_time() );

      chain::account_name_type scheduled_witness = _db.get_scheduled_witness( slot );
      // we must control the witness scheduled to produce the next block.
      if( _witnesses.find( scheduled_witness ) == _witnesses.end() )
      {
         capture("scheduled_witness", scheduled_witness);
         return block_production_condition::not_my_turn;
      }

      fc::time_point_sec scheduled_time = _db.get_slot_time( slot );
      chain::public_key_type scheduled_key = _db.get< chain::witness_object, chain::by_name >(scheduled_witness).signing_key;
      auto private_key_itr = this->_private_keys.find( scheduled_key );

      if( private_key_itr == this->_private_keys.end() )
      {
         capture("scheduled_witness", scheduled_witness);
         capture("scheduled_key", scheduled_key);
         return block_production_condition::no_private_key;
      }

      uint32_t prate = _db.witness_participation_rate();
      if( prate < _required_witness_participation )
      {
         capture("pct", uint32_t(100*uint64_t(prate) / MORPHENE_1_PERCENT));
         return block_production_condition::low_participation;
      }

      if( llabs((scheduled_time - now).count()) > fc::milliseconds( BLOCK_PRODUCING_LAG_TIME ).count() )
      {
         capture("scheduled_time", scheduled_time)("now", now);
         return block_production_condition::lag;
      }

      auto block = _chain_plugin.generate_block(
         scheduled_time,
         scheduled_witness,
         private_key_itr->second,
         _production_skip_flags
         );
      capture("n", block.block_num())("t", block.timestamp)("c", now);

      appbase::app().get_plugin< morphene::plugins::p2p::p2p_plugin >().broadcast_block( block );
      return block_production_condition::produced;
   }

} // detail


witness_plugin::witness_plugin() {}
witness_plugin::~witness_plugin() {}

void witness_plugin::set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg)
{
   string witness_id_example = "initwitness";
   cfg.add_options()
         ("enable-stale-production", bpo::value<bool>()->default_value( false ), "Enable block production, even if the chain is stale.")
         ("required-participation", bpo::value< uint32_t >()->default_value( 33 ), "Percent of witnesses (0-99) that must be participating in order to produce blocks")
         ("witness,w", bpo::value<vector<string>>()->composing()->multitoken(),
            ("name of witness controlled by this node (e.g. " + witness_id_example + " )" ).c_str() )
         ("miner,m", bpo::value<vector<string>>()->composing()->multitoken(), "name of miner and its private key (e.g. [\"account\",\"WIF PRIVATE KEY\"] )" )
         ("mining-threads,t", bpo::value<uint32_t>(),"Number of threads to use for proof of work mining" )
         ("private-key", bpo::value<vector<string>>()->composing()->multitoken(), "WIF PRIVATE KEY to be used by one or more witnesses" )
         ("witness-skip-enforce-bandwidth", bpo::value<bool>()->default_value( true ), "Skip enforcing bandwidth restrictions. Default is true in favor of rc_plugin." )
         ("miner-account-creation-fee", bpo::value<uint64_t>()->implicit_value(100000),"Account creation fee to be voted on upon successful POW - Minimum fee is 100.000 MORPH (written as 100000)")
         ("miner-maximum-block-size", bpo::value<uint32_t>()->implicit_value(131072),"Maximum block size (in bytes) to be voted on upon successful POW - Max block size must be between 128 KB and 750 MB")
         ;
   cli.add_options()
         ("enable-stale-production", bpo::bool_switch()->default_value( false ), "Enable block production, even if the chain is stale.")
         ("witness-skip-enforce-bandwidth", bpo::bool_switch()->default_value( true ), "Skip enforcing bandwidth restrictions. Default is true in favor of rc_plugin." )
         ;
}

void witness_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   ilog( "Initializing witness plugin" );
   my = std::make_unique< detail::witness_plugin_impl >( appbase::app().get_io_service() );

   MORPHENE_LOAD_VALUE_SET( options, "witness", my->_witnesses, morphene::protocol::account_name_type )

   if( options.count("miner") ) {

      const vector<string> miner_to_wif_pair_strings = options["miner"].as<vector<string>>();
      for( auto p : miner_to_wif_pair_strings )
      {
         auto m = dejsonify<pair<string,string>>(p);
         fc::optional<fc::ecc::private_key> private_key = morphene::utilities::wif_to_key(m.second);
         FC_ASSERT( private_key.valid(), "unable to parse private key" );
         my->_private_keys[private_key->get_public_key()] = *private_key;
         my->_miners[m.first] = private_key->get_public_key();
      }
   }

   if( options.count("mining-threads") )
   {
      my->_mining_threads = options["mining-threads"].as<uint32_t>();
      my->_thread_pool.resize( my->_mining_threads );
      for( uint32_t i = 0; i < my->_mining_threads; ++i )
         my->_thread_pool[i] = std::make_shared<fc::thread>();
   }

   if( options.count("private-key") )
   {
      const std::vector<std::string> keys = options["private-key"].as<std::vector<std::string>>();
      for (const std::string& wif_key : keys )
      {
         fc::optional<fc::ecc::private_key> private_key = morphene::utilities::wif_to_key(wif_key);
         FC_ASSERT( private_key.valid(), "unable to parse private key" );
         my->_private_keys[private_key->get_public_key()] = *private_key;
      }
   }

   if( options.count("miner-account-creation-fee") )
   {
      const uint64_t account_creation_fee = options["miner-account-creation-fee"].as<uint64_t>();

      if( account_creation_fee < MORPHENE_MIN_ACCOUNT_CREATION_FEE )
         wlog( "miner-account-creation-fee is below the minimum fee, using minimum instead" );
      else
         my->_miner_prop_vote.account_creation_fee.amount = account_creation_fee;
   }

   if( options.count( "miner-maximum-block-size" ) )
   {
      const uint32_t maximum_block_size = options["miner-maximum-block-size"].as<uint32_t>();

      if( maximum_block_size < MORPHENE_MIN_BLOCK_SIZE_LIMIT )
         wlog( "miner-maximum-block-size is below the minimum block size limit, using default of 128 KB instead" );
      else if ( maximum_block_size > MORPHENE_MAX_BLOCK_SIZE )
      {
         wlog( "miner-maximum-block-size is above the maximum block size limit, using maximum of 750 MB instead" );
         my->_miner_prop_vote.maximum_block_size = MORPHENE_MAX_BLOCK_SIZE;
      }
      else
         my->_miner_prop_vote.maximum_block_size = maximum_block_size;
   }

   my->_production_enabled = options.at( "enable-stale-production" ).as< bool >();

   if( my->_witnesses.size() > 0 )
   {
      // It is safe to access rc plugin here because of APPBASE_REQUIRES_PLUGIN
      FC_ASSERT( !appbase::app().get_plugin< rc::rc_plugin >().get_rc_plugin_skip_flags().skip_reject_not_enough_rc,
         "rc-skip-reject-not-enough-rc=false is required to produce blocks" );
   }

   if( options.count( "required-participation" ) )
   {
      my->_required_witness_participation = MORPHENE_1_PERCENT * options.at( "required-participation" ).as< uint32_t >();
   }

   my->_pre_apply_block_conn = my->_db.add_post_apply_block_handler(
      [&]( const chain::block_notification& note ){ my->on_pre_apply_block( note ); }, *this, 0 );
   my->_post_apply_block_conn = my->_db.add_post_apply_block_handler(
      [&]( const chain::block_notification& note ){ my->on_post_apply_block( note ); }, *this, 0 );
   my->_pre_apply_operation_conn = my->_db.add_pre_apply_operation_handler(
      [&]( const chain::operation_notification& note ){ my->on_pre_apply_operation( note ); }, *this, 0);
   my->_post_apply_operation_conn = my->_db.add_pre_apply_operation_handler(
      [&]( const chain::operation_notification& note ){ my->on_post_apply_operation( note ); }, *this, 0);
   if( !my->_miners.empty() )
   {
      my->_applied_block_conn = my->_db.add_post_apply_block_handler(
         [&]( const chain::block_notification& note ){ on_applied_block( note ); }, *this, 0);
   }

   if( (my->_witnesses.size() && my->_private_keys.size()) || (my->_miners.size() && my->_private_keys.size()) )
      my->_chain_plugin.set_write_lock_hold_time( -1 );
} FC_LOG_AND_RETHROW() }

void witness_plugin::plugin_startup()
{ try {
   ilog("witness plugin:  plugin_startup() begin" );
   chain::database& d = appbase::app().get_plugin< morphene::plugins::chain::chain_plugin >().db();

   if( !my->_witnesses.empty() )
   {
      ilog( "Launching block production for ${n} witnesses.", ("n", my->_witnesses.size()) );
      appbase::app().get_plugin< morphene::plugins::p2p::p2p_plugin >().set_block_production( true );
      if( my->_production_enabled )
      {
         if( d.head_block_num() == 0 )
            new_chain_banner( d );
         my->_production_skip_flags |= chain::database::skip_undo_history_check;
      }
      my->schedule_production_loop();
   } else
      elog("No witnesses configured! Please add witness IDs and private keys to configuration.");

   if( my->_miners.empty() )
   {
      elog("No miners configured! Please add miner names and private keys to configuration.");
   }
   ilog("witness plugin:  plugin_startup() end");
   } FC_CAPTURE_AND_RETHROW() }

void witness_plugin::plugin_shutdown()
{
   try
   {
      if( !my->_miners.empty() )
      {
         ilog( "shutting down mining threads" );
         my->_thread_pool.clear();
         chain::util::disconnect_signal( my->_applied_block_conn );
      }

      chain::util::disconnect_signal( my->_pre_apply_block_conn );
      chain::util::disconnect_signal( my->_post_apply_block_conn );
      chain::util::disconnect_signal( my->_pre_apply_operation_conn );
      chain::util::disconnect_signal( my->_post_apply_operation_conn );

      my->_timer.cancel();
   }
   catch(fc::exception& e)
   {
      edump( (e.to_detail_string()) );
   }
}

/**
 * Every time a block is produced, this method is called. This method will iterate through all
 * mining accounts specified by commandline and for which the private key is known. The first
 * account that isn't already scheduled in the mining queue is selected to mine for the
 * BLOCK_INTERVAL minus 1 second. If a POW is solved or a a new block comes in then the
 * worker will stop early.
 *
 * Work is farmed out to N threads in parallel based upon the value specified on the command line.
 *
 * The miner assumes that the next block will be produced on time and that network propagation
 * will take at least 1 second. This 1 second consists of the time it took to receive the block
 * and how long it will take to broadcast the work. In other words, we assume 0.5s broadcast times
 * and therefore do not even attempt work that cannot be delivered on time.
 */
void witness_plugin::on_applied_block(const chain::block_notification& b)
{ try {
   if( !my->_mining_threads || my->_miners.size() == 0 ) return;
   auto& db = my->_db;
   const auto& dgp = db.get_dynamic_global_properties();
   double hps   = (my->_total_hashes*1000000)/(fc::time_point::now()-my->_hash_start_time).count();
   uint64_t i_hps = uint64_t(hps+0.5);

   uint32_t summary_target = db.get_pow_summary_target();

   double target = fc::sha256::inverse_approx_log_32_double( summary_target );
   static const double max_target = std::ldexp( 1.0, 256 );

   double seconds_needed = 0.0;
   if( i_hps > 0 )
   {
      double hashes_needed = max_target / target;
      seconds_needed = hashes_needed / i_hps;
   }

   uint64_t minutes_needed = uint64_t( seconds_needed / 60.0 + 0.5 );

   fc::sha256 hash_target;
   hash_target.set_to_inverse_approx_log_32( summary_target );

   if( my->_total_hashes > 0 )
      ilog( "hash rate: ${x} hps  target: ${t} queue: ${l} estimated time to produce: ${m} minutes",
              ("x",i_hps) ("t",hash_target.str()) ("m", minutes_needed ) ("l",dgp.num_pow_witnesses)
         );


  my->_head_block_num = b.block.block_num();
  /// save these variables to be captured by worker lambda

  for( const auto& miner : my->_miners ) {
    const auto* w = db.find_witness( miner.first );
    if( !w || w->pow_worker == 0 ) {
       auto miner_pub_key = miner.second; //a.active.key_auths.begin()->first;
       auto priv_key_itr = my->_private_keys.find(miner_pub_key);
       if( priv_key_itr == my->_private_keys.end() ) {
          ilog("Skipping miner for lack of private key");
          continue; /// skipping miner for lack of private key
       }

       auto miner_priv_key = priv_key_itr->second;
       start_mining( miner_pub_key, priv_key_itr->second, miner.first, b );
       break;
    } else {
        ilog( "Skipping miner ${m} because it is already scheduled to produce a block", ("m",miner) );
    }
  } // for miner in miners

} catch ( const fc::exception& e ) { ilog( "exception thrown while attempting to mine" ); }
}

void witness_plugin::start_mining(
   const fc::ecc::public_key& pub,
   const fc::ecc::private_key& pk,
   const string& miner,
   const morphene::chain::block_notification& b )
{
    static uint64_t seed = fc::time_point::now().time_since_epoch().count();
    static uint64_t start = fc::city_hash64( (const char*)&seed, sizeof(seed) );

    auto head_block_num  = b.block.block_num();
    auto head_block_time = b.block.timestamp;
    auto block_id = b.block.id();

    // fc::thread* mainthread = &fc::thread::current();

    my->_total_hashes = 0;
    my->_hash_start_time = fc::time_point::now();

    auto stop = head_block_time + fc::seconds( MORPHENE_BLOCK_INTERVAL * 3 );

    auto& db = my->_db;

    uint32_t thread_num = 0;
    uint32_t num_threads = my->_mining_threads;
    uint32_t target = db.get_pow_summary_target();
    const auto& acct_idx  = db.get_index< account_index, by_name >();
    auto acct_it = acct_idx.find( miner );
    bool has_account = (acct_it != acct_idx.end());
    for( auto& t : my->_thread_pool )
    {
       t->async( [=]()
       {
          chain::pow_operation op;
          chain::pow work;
          work.input.prev_block = block_id;
          work.input.worker_account = miner;
          work.input.nonce = start + thread_num;
          op.props = my->_miner_prop_vote;
          while( true )
          {
             //  if( ((work.input.nonce/num_threads) % 1000) == 0 ) idump((work.input.nonce));
             if( fc::time_point::now() > stop )
             {
                // ilog( "stop mining due to time out, nonce: ${n}", ("n",work.input.nonce) );
                return;
             }
             if( my->_head_block_num != head_block_num )
             {
                // wlog( "stop mining due new block arrival, nonce: ${n}", ("n",work.input.nonce));
                return;
             }
             ++my->_total_hashes;

             work.input.nonce += num_threads;
             work.create( block_id, miner, work.input.nonce );
             // ilog("PowSummary: ${s}, Target: ${t}, VALID? ${v}", ("s",work.pow_summary)("t",target)("v",work.pow_summary < target));
             if( work.pow_summary < target )
             {
                ++my->_head_block_num; /// signal other workers to stop

                chain::signed_transaction trx;
                op.work = work;
                if( !has_account )
                  op.new_owner_key = pub;
                trx.operations.push_back(op);
                trx.ref_block_num = head_block_num;
                trx.ref_block_prefix = work.input.prev_block._hash[1];
                trx.set_expiration( head_block_time + MORPHENE_MAX_TIME_UNTIL_EXPIRATION );
                trx.sign( pk, MORPHENE_CHAIN_ID, fc::ecc::fc_canonical );
                // TODO: Remove this or fix it; buggy
                // mainthread->async( [this,miner,trx]()
                // {
                try
                {
                  appbase::app().get_plugin< morphene::plugins::chain::chain_plugin >().db().push_transaction( trx );
                  ilog( "Broadcasting POW for ${miner}", ("miner",miner) );
                  appbase::app().get_plugin< morphene::plugins::p2p::p2p_plugin >().broadcast_transaction( trx );
                }
                catch( const fc::exception& e )
                {
                  // wdump((e.to_detail_string()));
                }
                // } );
                return;
             }
          }
       } );
       thread_num++;
    }
}

} } } // morphene::plugins::witness
