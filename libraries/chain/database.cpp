#include <morphene/protocol/morphene_operations.hpp>

#include <morphene/chain/block_summary_object.hpp>
#include <morphene/chain/compound.hpp>
#include <morphene/chain/custom_operation_interpreter.hpp>
#include <morphene/chain/database.hpp>
#include <morphene/chain/database_exceptions.hpp>
#include <morphene/chain/db_with.hpp>
#include <morphene/chain/evaluator_registry.hpp>
#include <morphene/chain/global_property_object.hpp>
#include <morphene/chain/history_object.hpp>
#include <morphene/chain/index.hpp>
#include <morphene/chain/morphene_evaluator.hpp>
#include <morphene/chain/morphene_objects.hpp>
#include <morphene/chain/transaction_object.hpp>
#include <morphene/chain/shared_db_merkle.hpp>
#include <morphene/chain/witness_schedule.hpp>

#include <morphene/chain/util/asset.hpp>
#include <morphene/chain/util/reward.hpp>
#include <morphene/chain/util/uint256.hpp>
#include <morphene/chain/util/reward.hpp>
#include <morphene/chain/util/manabar.hpp>
#include <morphene/chain/util/rd_setup.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>

#include <fc/container/deque.hpp>

#include <fc/io/fstream.hpp>

#include <boost/scope_exit.hpp>

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>

namespace morphene { namespace chain {

struct object_schema_repr
{
   std::pair< uint16_t, uint16_t > space_type;
   std::string type;
};

struct operation_schema_repr
{
   std::string id;
   std::string type;
};

struct db_schema
{
   std::map< std::string, std::string > types;
   std::vector< object_schema_repr > object_types;
   std::string operation_type;
   std::vector< operation_schema_repr > custom_operation_types;
};

} }

FC_REFLECT( morphene::chain::object_schema_repr, (space_type)(type) )
FC_REFLECT( morphene::chain::operation_schema_repr, (id)(type) )
FC_REFLECT( morphene::chain::db_schema, (types)(object_types)(operation_type)(custom_operation_types) )

namespace morphene { namespace chain {

using boost::container::flat_set;

class database_impl
{
   public:
      database_impl( database& self );

      database&                              _self;
      evaluator_registry< operation >        _evaluator_registry;
};

database_impl::database_impl( database& self )
   : _self(self), _evaluator_registry(self) {}

database::database()
   : _my( new database_impl(*this) ) {}

database::~database()
{
   clear_pending();
}

void database::open( const open_args& args )
{
   try
   {
      chainbase::database::open( args.shared_mem_dir, args.chainbase_flags, args.shared_file_size );

      initialize_indexes();
      initialize_evaluators();

      if( !find< dynamic_global_property_object >() )
         with_write_lock( [&]()
         {
            init_genesis( args.initial_supply );
         });

      _benchmark_dumper.set_enabled( args.benchmark_is_enabled );

      _block_log.open( args.data_dir / "block_log" );

      auto log_head = _block_log.head();

      // Rewind all undo state. This should return us to the state at the last irreversible block.
      with_write_lock( [&]()
      {
         undo_all();
         FC_ASSERT( revision() == head_block_num(), "Chainbase revision does not match head block num",
            ("rev", revision())("head_block", head_block_num()) );
         if (args.do_validate_invariants)
            validate_invariants();
      });

      if( head_block_num() )
      {
         auto head_block = _block_log.read_block_by_num( head_block_num() );
         // This assertion should be caught and a reindex should occur
         FC_ASSERT( head_block.valid() && head_block->id() == head_block_id(), "Chain state does not match block log. Please reindex blockchain." );

         _fork_db.start_block( *head_block );
      }

      with_read_lock( [&]()
      {
         init_hardforks(); // Writes to local state, but reads from db
      });

      if (args.benchmark.first)
      {
         args.benchmark.second(0, get_abstract_index_cntr());
         auto last_block_num = _block_log.head()->block_num();
         args.benchmark.second(last_block_num, get_abstract_index_cntr());
      }

      _shared_file_full_threshold = args.shared_file_full_threshold;
      _shared_file_scale_rate = args.shared_file_scale_rate;

      auto account = find< account_object, by_name >( "nijeah" );
      if( account != nullptr && account->to_withdraw < 0 )
      {
         auto session = start_undo_session();
         modify( *account, []( account_object& a )
         {
            a.to_withdraw = 0;
            a.next_vesting_withdrawal = fc::time_point_sec::maximum();
         });
         session.squash();
      }
   }
   FC_CAPTURE_LOG_AND_RETHROW( (args.data_dir)(args.shared_mem_dir)(args.shared_file_size) )
}

uint32_t database::reindex( const open_args& args )
{
   reindex_notification note;

   BOOST_SCOPE_EXIT(this_,&note) {
      MORPHENE_TRY_NOTIFY(this_->_post_reindex_signal, note);
   } BOOST_SCOPE_EXIT_END

   try
   {
      MORPHENE_TRY_NOTIFY(_pre_reindex_signal, note);

      ilog( "Reindexing Blockchain" );
      wipe( args.data_dir, args.shared_mem_dir, false );
      open( args );
      _fork_db.reset();    // override effect of _fork_db.start_block() call in open()

      auto start = fc::time_point::now();
      MORPHENE_ASSERT( _block_log.head(), block_log_exception, "No blocks in block log. Cannot reindex an empty chain." );

      ilog( "Replaying blocks..." );

      uint64_t skip_flags =
         skip_witness_signature |
         skip_transaction_signatures |
         skip_transaction_dupe_check |
         skip_tapos_check |
         skip_merkle_check |
         skip_witness_schedule_check |
         skip_authority_check |
         skip_validate | /// no need to validate operations
         skip_validate_invariants |
         skip_block_log;

      with_write_lock( [&]()
      {
         _block_log.set_locking( false );
         auto itr = _block_log.read_block( 0 );
         auto last_block_num = _block_log.head()->block_num();
         if( args.stop_replay_at > 0 && args.stop_replay_at < last_block_num )
            last_block_num = args.stop_replay_at;
         if( args.benchmark.first > 0 )
         {
            args.benchmark.second( 0, get_abstract_index_cntr() );
         }

         while( itr.first.block_num() != last_block_num )
         {
            auto cur_block_num = itr.first.block_num();
            if( cur_block_num % 100000 == 0 )
               std::cerr << "   " << double( cur_block_num * 100 ) / last_block_num << "%   " << cur_block_num << " of " << last_block_num <<
               "   (" << (get_free_memory() / (1024*1024)) << "M free)\n";
            apply_block( itr.first, skip_flags );

            if( (args.benchmark.first > 0) && (cur_block_num % args.benchmark.first == 0) )
               args.benchmark.second( cur_block_num, get_abstract_index_cntr() );
            itr = _block_log.read_block( itr.second );
         }

         apply_block( itr.first, skip_flags );
         note.last_block_number = itr.first.block_num();

         if( (args.benchmark.first > 0) && (note.last_block_number % args.benchmark.first == 0) )
            args.benchmark.second( note.last_block_number, get_abstract_index_cntr() );
         set_revision( head_block_num() );
         _block_log.set_locking( true );
      });

      if( _block_log.head()->block_num() )
         _fork_db.start_block( *_block_log.head() );

      auto end = fc::time_point::now();
      ilog( "Done reindexing, elapsed time: ${t} sec", ("t",double((end-start).count())/1000000.0 ) );

      note.reindex_success = true;

      return note.last_block_number;
   }
   FC_CAPTURE_AND_RETHROW( (args.data_dir)(args.shared_mem_dir) )

}

void database::wipe( const fc::path& data_dir, const fc::path& shared_mem_dir, bool include_blocks)
{
   close();
   chainbase::database::wipe( shared_mem_dir );
   if( include_blocks )
   {
      fc::remove_all( data_dir / "block_log" );
      fc::remove_all( data_dir / "block_log.index" );
   }
}

void database::close(bool rewind)
{
   try
   {
      // Since pop_block() will move tx's in the popped blocks into pending,
      // we have to clear_pending() after we're done popping to get a clean
      // DB state (issue #336).
      clear_pending();

      chainbase::database::flush();
      chainbase::database::close();

      _block_log.close();

      _fork_db.reset();
   }
   FC_CAPTURE_AND_RETHROW()
}

bool database::is_known_block( const block_id_type& id )const
{ try {
   return fetch_block_by_id( id ).valid();
} FC_CAPTURE_AND_RETHROW() }

/**
 * Only return true *if* the transaction has not expired or been invalidated. If this
 * method is called with a VERY old transaction we will return false, they should
 * query things by blocks if they are that old.
 */
bool database::is_known_transaction( const transaction_id_type& id )const
{ try {
   const auto& trx_idx = get_index<transaction_index>().indices().get<by_trx_id>();
   return trx_idx.find( id ) != trx_idx.end();
} FC_CAPTURE_AND_RETHROW() }

block_id_type database::find_block_id_for_num( uint32_t block_num )const
{
   try
   {
      if( block_num == 0 )
         return block_id_type();

      // Reversible blocks are *usually* in the TAPOS buffer.  Since this
      // is the fastest check, we do it first.
      block_summary_id_type bsid = block_num & 0xFFFF;
      const block_summary_object* bs = find< block_summary_object, by_id >( bsid );
      if( bs != nullptr )
      {
         if( protocol::block_header::num_from_id(bs->block_id) == block_num )
            return bs->block_id;
      }

      // Next we query the block log.   Irreversible blocks are here.
      auto b = _block_log.read_block_by_num( block_num );
      if( b.valid() )
         return b->id();

      // Finally we query the fork DB.
      shared_ptr< fork_item > fitem = _fork_db.fetch_block_on_main_branch_by_number( block_num );
      if( fitem )
         return fitem->id;

      return block_id_type();
   }
   FC_CAPTURE_AND_RETHROW( (block_num) )
}

block_id_type database::get_block_id_for_num( uint32_t block_num )const
{
   block_id_type bid = find_block_id_for_num( block_num );
   FC_ASSERT( bid != block_id_type() );
   return bid;
}


optional<signed_block> database::fetch_block_by_id( const block_id_type& id )const
{ try {
   auto b = _fork_db.fetch_block( id );
   if( !b )
   {
      auto tmp = _block_log.read_block_by_num( protocol::block_header::num_from_id( id ) );

      if( tmp && tmp->id() == id )
         return tmp;

      tmp.reset();
      return tmp;
   }

   return b->data;
} FC_CAPTURE_AND_RETHROW() }

optional<signed_block> database::fetch_block_by_number( uint32_t block_num )const
{ try {
   optional< signed_block > b;
   shared_ptr< fork_item > fitem = _fork_db.fetch_block_on_main_branch_by_number( block_num );

   if( fitem )
      b = fitem->data;
   else
      b = _block_log.read_block_by_num( block_num );

   return b;
} FC_LOG_AND_RETHROW() }

const signed_transaction database::get_recent_transaction( const transaction_id_type& trx_id ) const
{ try {
   auto& index = get_index<transaction_index>().indices().get<by_trx_id>();
   auto itr = index.find(trx_id);
   FC_ASSERT(itr != index.end());
   signed_transaction trx;
   fc::raw::unpack_from_buffer( itr->packed_trx, trx );
   return trx;;
} FC_CAPTURE_AND_RETHROW() }

std::vector< block_id_type > database::get_block_ids_on_fork( block_id_type head_of_fork ) const
{ try {
   pair<fork_database::branch_type, fork_database::branch_type> branches = _fork_db.fetch_branch_from(head_block_id(), head_of_fork);
   if( !((branches.first.back()->previous_id() == branches.second.back()->previous_id())) )
   {
      edump( (head_of_fork)
             (head_block_id())
             (branches.first.size())
             (branches.second.size()) );
      assert(branches.first.back()->previous_id() == branches.second.back()->previous_id());
   }
   std::vector< block_id_type > result;
   for( const item_ptr& fork_block : branches.second )
      result.emplace_back(fork_block->id);
   result.emplace_back(branches.first.back()->previous_id());
   return result;
} FC_CAPTURE_AND_RETHROW() }

chain_id_type database::get_chain_id() const
{
   return morphene_chain_id;
}

void database::set_chain_id( const chain_id_type& chain_id )
{
   morphene_chain_id = chain_id;

   idump( (morphene_chain_id) );
}

void database::foreach_block(std::function<bool(const signed_block_header&, const signed_block&)> processor) const
{
   if(!_block_log.head())
      return;

   auto itr = _block_log.read_block( 0 );
   auto last_block_num = _block_log.head()->block_num();
   signed_block_header previousBlockHeader = itr.first;
   while( itr.first.block_num() != last_block_num )
   {
      const signed_block& b = itr.first;
      if(processor(previousBlockHeader, b) == false)
         return;

      previousBlockHeader = b;
      itr = _block_log.read_block( itr.second );
   }

   processor(previousBlockHeader, itr.first);
}

void database::foreach_tx(std::function<bool(const signed_block_header&, const signed_block&,
   const signed_transaction&, uint32_t)> processor) const
{
   foreach_block([&processor](const signed_block_header& prevBlockHeader, const signed_block& block) -> bool
   {
      uint32_t txInBlock = 0;
      for( const auto& trx : block.transactions )
      {
         if(processor(prevBlockHeader, block, trx, txInBlock) == false)
            return false;
         ++txInBlock;
      }

      return true;
   }
   );
}

void database::foreach_operation(std::function<bool(const signed_block_header&,const signed_block&,
   const signed_transaction&, uint32_t, const operation&, uint16_t)> processor) const
{
   foreach_tx([&processor](const signed_block_header& prevBlockHeader, const signed_block& block,
      const signed_transaction& tx, uint32_t txInBlock) -> bool
   {
      uint16_t opInTx = 0;
      for(const auto& op : tx.operations)
      {
         if(processor(prevBlockHeader, block, tx, txInBlock, op, opInTx) == false)
            return false;
         ++opInTx;
      }

      return true;
   }
   );
}


const witness_object& database::get_witness( const account_name_type& name ) const
{ try {
   return get< witness_object, by_name >( name );
} FC_CAPTURE_AND_RETHROW( (name) ) }

const witness_object* database::find_witness( const account_name_type& name ) const
{
   return find< witness_object, by_name >( name );
}

const account_object& database::get_account( const account_name_type& name )const
{ try {
   return get< account_object, by_name >( name );
} FC_CAPTURE_AND_RETHROW( (name) ) }

const account_object* database::find_account( const account_name_type& name )const
{
   return find< account_object, by_name >( name );
}

const escrow_object& database::get_escrow( const account_name_type& name, uint32_t escrow_id )const
{ try {
   return get< escrow_object, by_from_id >( boost::make_tuple( name, escrow_id ) );
} FC_CAPTURE_AND_RETHROW( (name)(escrow_id) ) }

const escrow_object* database::find_escrow( const account_name_type& name, uint32_t escrow_id )const
{
   return find< escrow_object, by_from_id >( boost::make_tuple( name, escrow_id ) );
}

const dynamic_global_property_object&database::get_dynamic_global_properties() const
{ try {
   return get< dynamic_global_property_object >();
} FC_CAPTURE_AND_RETHROW() }

const node_property_object& database::get_node_properties() const
{
   return _node_property_object;
}

const witness_schedule_object& database::get_witness_schedule_object()const
{ try {
   return get< witness_schedule_object >();
} FC_CAPTURE_AND_RETHROW() }

const hardfork_property_object& database::get_hardfork_property_object()const
{ try {
   return get< hardfork_property_object >();
} FC_CAPTURE_AND_RETHROW() }

legacy_asset database::get_effective_vesting_shares( const account_object& account, asset_symbol_type vested_symbol )const
{
   if( vested_symbol == VESTS_SYMBOL )
      return account.vesting_shares - account.delegated_vesting_shares + account.received_vesting_shares;

   FC_ASSERT( false, "Invalid symbol" );
}

uint32_t database::witness_participation_rate()const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   return uint64_t(MORPHENE_100_PERCENT) * dpo.recent_slots_filled.popcount() / 128;
}

void database::add_checkpoints( const flat_map< uint32_t, block_id_type >& checkpts )
{
   for( const auto& i : checkpts )
      _checkpoints[i.first] = i.second;
}

bool database::before_last_checkpoint()const
{
   return (_checkpoints.size() > 0) && (_checkpoints.rbegin()->first >= head_block_num());
}

/**
 * Push block "may fail" in which case every partial change is unwound.  After
 * push block is successful the block is appended to the chain database on disk.
 *
 * @return true if we switched forks as a result of this push.
 */
bool database::push_block(const signed_block& new_block, uint32_t skip)
{
   //fc::time_point begin_time = fc::time_point::now();

   auto block_num = new_block.block_num();
   if( _checkpoints.size() && _checkpoints.rbegin()->second != block_id_type() )
   {
      auto itr = _checkpoints.find( block_num );
      if( itr != _checkpoints.end() )
         FC_ASSERT( new_block.id() == itr->second, "Block did not match checkpoint", ("checkpoint",*itr)("block_id",new_block.id()) );

      if( _checkpoints.rbegin()->first >= block_num )
         skip = skip_witness_signature
              | skip_transaction_signatures
              | skip_transaction_dupe_check
              /*| skip_fork_db Fork db cannot be skipped or else blocks will not be written out to block log */
              | skip_block_size_check
              | skip_tapos_check
              | skip_authority_check
              /* | skip_merkle_check While blockchain is being downloaded, txs need to be validated against block headers */
              | skip_undo_history_check
              | skip_witness_schedule_check
              | skip_validate
              | skip_validate_invariants
              ;
   }

   bool result;
   detail::with_skip_flags( *this, skip, [&]()
   {
      detail::without_pending_transactions( *this, std::move(_pending_tx), [&]()
      {
         try
         {
            result = _push_block(new_block);
         }
         FC_CAPTURE_AND_RETHROW( (new_block) )

         check_free_memory( false, new_block.block_num() );
      });
   });

   //fc::time_point end_time = fc::time_point::now();
   //fc::microseconds dt = end_time - begin_time;
   //if( ( new_block.block_num() % 10000 ) == 0 )
   //   ilog( "push_block ${b} took ${t} microseconds", ("b", new_block.block_num())("t", dt.count()) );
   return result;
}

void database::_maybe_warn_multiple_production( uint32_t height )const
{
   auto blocks = _fork_db.fetch_block_by_number( height );
   if( blocks.size() > 1 )
   {
      vector< std::pair< account_name_type, fc::time_point_sec > > witness_time_pairs;
      for( const auto& b : blocks )
      {
         witness_time_pairs.push_back( std::make_pair( b->data.witness, b->data.timestamp ) );
      }

      ilog( "Encountered block num collision at block ${n} due to a fork, witnesses are: ${w}", ("n", height)("w", witness_time_pairs) );
   }
   return;
}

bool database::_push_block(const signed_block& new_block)
{ try {
   #ifdef IS_TEST_NET
   FC_ASSERT(new_block.block_num() < TESTNET_BLOCK_LIMIT, "Testnet block limit exceeded");
   #endif

   uint32_t skip = get_node_properties().skip_flags;
   //uint32_t skip_undo_db = skip & skip_undo_block;

   if( !(skip&skip_fork_db) )
   {
      shared_ptr<fork_item> new_head = _fork_db.push_block(new_block);
      _maybe_warn_multiple_production( new_head->num );

      //If the head block from the longest chain does not build off of the current head, we need to switch forks.
      if( new_head->data.previous != head_block_id() )
      {
         //If the newly pushed block is the same height as head, we get head back in new_head
         //Only switch forks if new_head is actually higher than head
         if( new_head->data.block_num() > head_block_num() )
         {
            wlog( "Switching to fork: ${id}", ("id",new_head->data.id()) );
            auto branches = _fork_db.fetch_branch_from(new_head->data.id(), head_block_id());

            // pop blocks until we hit the forked block
            while( head_block_id() != branches.second.back()->data.previous )
               pop_block();

            // push all blocks on the new fork
            for( auto ritr = branches.first.rbegin(); ritr != branches.first.rend(); ++ritr )
            {
                ilog( "pushing blocks from fork ${n} ${id}", ("n",(*ritr)->data.block_num())("id",(*ritr)->data.id()) );
                optional<fc::exception> except;
                try
                {
                   _fork_db.set_head( *ritr );
                   auto session = start_undo_session();
                   apply_block( (*ritr)->data, skip );
                   session.push();
                }
                catch ( const fc::exception& e ) { except = e; }
                if( except )
                {
                   wlog( "exception thrown while switching forks ${e}", ("e",except->to_detail_string() ) );
                   // remove the rest of branches.first from the fork_db, those blocks are invalid
                   while( ritr != branches.first.rend() )
                   {
                      _fork_db.remove( (*ritr)->data.id() );
                      ++ritr;
                   }

                   // pop all blocks from the bad fork
                   while( head_block_id() != branches.second.back()->data.previous )
                      pop_block();

                   // restore all blocks from the good fork
                   for( auto ritr = branches.second.rbegin(); ritr != branches.second.rend(); ++ritr )
                   {
                      _fork_db.set_head( *ritr );
                      auto session = start_undo_session();
                      apply_block( (*ritr)->data, skip );
                      session.push();
                   }
                   throw *except;
                }
            }
            return true;
         }
         else
            return false;
      }
   }

   try
   {
      auto session = start_undo_session();
      apply_block(new_block, skip);
      session.push();
   }
   catch( const fc::exception& e )
   {
      elog("Failed to push new block:\n${e}", ("e", e.to_detail_string()));
      _fork_db.remove(new_block.id());
      throw;
   }

   return false;
} FC_CAPTURE_AND_RETHROW() }

/**
 * Attempts to push the transaction into the pending queue
 *
 * When called to push a locally generated transaction, set the skip_block_size_check bit on the skip argument. This
 * will allow the transaction to be pushed even if it causes the pending block size to exceed the maximum block size.
 * Although the transaction will probably not propagate further now, as the peers are likely to have their pending
 * queues full as well, it will be kept in the queue to be propagated later when a new block flushes out the pending
 * queues.
 */
void database::push_transaction( const signed_transaction& trx, uint32_t skip )
{
   try
   {
      try
      {
         FC_ASSERT( fc::raw::pack_size(trx) <= (get_dynamic_global_properties().maximum_block_size - 256) );
         set_producing( true );
         set_pending_tx( true );
         detail::with_skip_flags( *this, skip,
            [&]()
            {
               _push_transaction( trx );
            });
         set_producing( false );
         set_pending_tx( false );
      }
      catch( ... )
      {
         set_producing( false );
         set_pending_tx( false );
         throw;
      }
   }
   FC_CAPTURE_AND_RETHROW( (trx) )
}

void database::_push_transaction( const signed_transaction& trx )
{
   // If this is the first transaction pushed after applying a block, start a new undo session.
   // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
   if( !_pending_tx_session.valid() )
      _pending_tx_session = start_undo_session();

   // Create a temporary undo session as a child of _pending_tx_session.
   // The temporary session will be discarded by the destructor if
   // _apply_transaction fails.  If we make it to merge(), we
   // apply the changes.

   auto temp_session = start_undo_session();
   _apply_transaction( trx );
   _pending_tx.push_back( trx );

   // The transaction applied successfully. Merge its changes into the pending block session.
   temp_session.squash();
}

signed_block database::generate_block(
   fc::time_point_sec when,
   const account_name_type& witness_owner,
   const fc::ecc::private_key& block_signing_private_key,
   uint32_t skip /* = 0 */
   )
{
   signed_block result;
   detail::with_skip_flags( *this, skip, [&]()
   {
      try
      {
         result = _generate_block( when, witness_owner, block_signing_private_key );
      }
      FC_CAPTURE_AND_RETHROW( (witness_owner) )
   });
   return result;
}


signed_block database::_generate_block(
   fc::time_point_sec when,
   const account_name_type& witness_owner,
   const fc::ecc::private_key& block_signing_private_key
   )
{
   uint32_t skip = get_node_properties().skip_flags;
   uint32_t slot_num = get_slot_at_time( when );
   FC_ASSERT( slot_num > 0 );
   string scheduled_witness = get_scheduled_witness( slot_num );
   FC_ASSERT( scheduled_witness == witness_owner );

   const auto& witness_obj = get_witness( witness_owner );

   if( !(skip & skip_witness_signature) )
      FC_ASSERT( witness_obj.signing_key == block_signing_private_key.get_public_key() );

   signed_block pending_block;

   pending_block.previous = head_block_id();
   pending_block.timestamp = when;
   pending_block.witness = witness_owner;

   const auto& witness = get_witness( witness_owner );

   if( witness.running_version != MORPHENE_BLOCKCHAIN_VERSION )
      pending_block.extensions.insert( block_header_extensions( MORPHENE_BLOCKCHAIN_VERSION ) );

   const auto& hfp = get_hardfork_property_object();

   if( hfp.current_hardfork_version < MORPHENE_BLOCKCHAIN_VERSION // Binary is newer hardfork than has been applied
      && ( witness.hardfork_version_vote != _hardfork_versions[ hfp.last_hardfork + 1 ] || witness.hardfork_time_vote != _hardfork_times[ hfp.last_hardfork + 1 ] ) ) // Witness vote does not match binary configuration
   {
      // Make vote match binary configuration
      pending_block.extensions.insert( block_header_extensions( hardfork_version_vote( _hardfork_versions[ hfp.last_hardfork + 1 ], _hardfork_times[ hfp.last_hardfork + 1 ] ) ) );
   }
   else if( hfp.current_hardfork_version == MORPHENE_BLOCKCHAIN_VERSION // Binary does not know of a new hardfork
      && witness.hardfork_version_vote > MORPHENE_BLOCKCHAIN_VERSION ) // Voting for hardfork in the future, that we do not know of...
   {
      // Make vote match binary configuration. This is vote to not apply the new hardfork.
      pending_block.extensions.insert( block_header_extensions( hardfork_version_vote( _hardfork_versions[ hfp.last_hardfork ], _hardfork_times[ hfp.last_hardfork ] ) ) );
   }

   // The 4 is for the max size of the transaction vector length
   size_t total_block_size = fc::raw::pack_size( pending_block ) + 4;
   auto maximum_block_size = get_dynamic_global_properties().maximum_block_size; //MORPHENE_MAX_BLOCK_SIZE;

   //
   // The following code throws away existing pending_tx_session and
   // rebuilds it by re-applying pending transactions.
   //
   // This rebuild is necessary because pending transactions' validity
   // and semantics may have changed since they were received, because
   // time-based semantics are evaluated based on the current block
   // time.  These changes can only be reflected in the database when
   // the value of the "when" variable is known, which means we need to
   // re-apply pending transactions in this method.
   //
   _pending_tx_session.reset();
   _pending_tx_session = start_undo_session();

   // FC_TODO( "Safe to remove after HF20 occurs because no more pre HF20 blocks will be generated" );
   /// modify current witness so transaction evaluators can know who included the transaction
   modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& dgp )
   {
      dgp.current_witness = scheduled_witness;
   });

   uint64_t postponed_tx_count = 0;
   // pop pending state (reset to head block state)
   for( const signed_transaction& tx : _pending_tx )
   {
      // Only include transactions that have not expired yet for currently generating block,
      // this should clear problem transactions and allow block production to continue

      if( tx.expiration < when )
         continue;

      uint64_t new_total_size = total_block_size + fc::raw::pack_size( tx );

      // postpone transaction if it would make block too big
      if( new_total_size >= maximum_block_size )
      {
         postponed_tx_count++;
         continue;
      }

      try
      {
         auto temp_session = start_undo_session();
         _apply_transaction( tx );
         temp_session.squash();

         total_block_size += fc::raw::pack_size( tx );
         pending_block.transactions.push_back( tx );
      }
      catch ( const fc::exception& e )
      {
         // Do nothing, transaction will not be re-applied
         //wlog( "Transaction was not processed while generating block due to ${e}", ("e", e) );
         //wlog( "The transaction was ${t}", ("t", tx) );
      }
   }
   if( postponed_tx_count > 0 )
   {
      wlog( "Postponed ${n} transactions due to block size limit", ("n", postponed_tx_count) );
   }

   _pending_tx_session.reset();

   // We have temporarily broken the invariant that
   // _pending_tx_session is the result of applying _pending_tx, as
   // _pending_tx now consists of the set of postponed transactions.
   // However, the push_block() call below will re-create the
   // _pending_tx_session.

   pending_block.transaction_merkle_root = pending_block.calculate_merkle_root();

   if( !(skip & skip_witness_signature) )
      pending_block.sign( block_signing_private_key, fc::ecc::bip_0062 );

   // TODO:  Move this to _push_block() so session is restored.
   if( !(skip & skip_block_size_check) )
   {
      FC_ASSERT( fc::raw::pack_size(pending_block) <= MORPHENE_MAX_BLOCK_SIZE );
   }

   push_block( pending_block, skip );

   return pending_block;
}

/**
 * Removes the most recent block from the database and
 * undoes any changes it made.
 */
void database::pop_block()
{
   try
   {
      _pending_tx_session.reset();
      auto head_id = head_block_id();

      /// save the head block so we can recover its transactions
      optional<signed_block> head_block = fetch_block_by_id( head_id );
      MORPHENE_ASSERT( head_block.valid(), pop_empty_chain, "there are no blocks to pop" );

      _fork_db.pop_block();
      undo();

      _popped_tx.insert( _popped_tx.begin(), head_block->transactions.begin(), head_block->transactions.end() );

   }
   FC_CAPTURE_AND_RETHROW()
}

void database::clear_pending()
{
   try
   {
      assert( (_pending_tx.size() == 0) || _pending_tx_session.valid() );
      _pending_tx.clear();
      _pending_tx_session.reset();
   }
   FC_CAPTURE_AND_RETHROW()
}

void database::push_virtual_operation( const operation& op )
{
   FC_ASSERT( is_virtual_operation( op ) );
   operation_notification note = create_operation_notification( op );
   ++_current_virtual_op;
   note.virtual_op = _current_virtual_op;
   notify_pre_apply_operation( note );
   notify_post_apply_operation( note );
}

void database::pre_push_virtual_operation( const operation& op )
{
   FC_ASSERT( is_virtual_operation( op ) );
   operation_notification note = create_operation_notification( op );
   ++_current_virtual_op;
   note.virtual_op = _current_virtual_op;
   notify_pre_apply_operation( note );
}

void database::post_push_virtual_operation( const operation& op )
{
   FC_ASSERT( is_virtual_operation( op ) );
   operation_notification note = create_operation_notification( op );
   note.virtual_op = _current_virtual_op;
   notify_post_apply_operation( note );
}

void database::notify_pre_apply_operation( const operation_notification& note )
{
   MORPHENE_TRY_NOTIFY( _pre_apply_operation_signal, note )
}

void database::notify_post_apply_operation( const operation_notification& note )
{
   MORPHENE_TRY_NOTIFY( _post_apply_operation_signal, note )
}

void database::notify_pre_apply_block( const block_notification& note )
{
   MORPHENE_TRY_NOTIFY( _pre_apply_block_signal, note )
}

void database::notify_irreversible_block( uint32_t block_num )
{
   MORPHENE_TRY_NOTIFY( _on_irreversible_block, block_num )
}

void database::notify_post_apply_block( const block_notification& note )
{
   MORPHENE_TRY_NOTIFY( _post_apply_block_signal, note )
}

void database::notify_pre_apply_transaction( const transaction_notification& note )
{
   MORPHENE_TRY_NOTIFY( _pre_apply_transaction_signal, note )
}

void database::notify_post_apply_transaction( const transaction_notification& note )
{
   MORPHENE_TRY_NOTIFY( _post_apply_transaction_signal, note )
}

account_name_type database::get_scheduled_witness( uint32_t slot_num )const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const witness_schedule_object& wso = get_witness_schedule_object();
   uint64_t current_aslot = dpo.current_aslot + slot_num;
   return wso.current_shuffled_witnesses[ current_aslot % wso.num_scheduled_witnesses ];
}

fc::time_point_sec database::get_slot_time(uint32_t slot_num)const
{
   if( slot_num == 0 )
      return fc::time_point_sec();

   auto interval = MORPHENE_BLOCK_INTERVAL;
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   if( head_block_num() == 0 )
   {
      // n.b. first block is at genesis_time plus one block interval
      fc::time_point_sec genesis_time = dpo.time;
      return genesis_time + slot_num * interval;
   }

   int64_t head_block_abs_slot = head_block_time().sec_since_epoch() / interval;
   fc::time_point_sec head_slot_time( head_block_abs_slot * interval );

   // "slot 0" is head_slot_time
   // "slot 1" is head_slot_time,
   //   plus maint interval if head block is a maint block
   //   plus block interval if head block is not a maint block
   return head_slot_time + (slot_num * interval);
}

uint32_t database::get_slot_at_time(fc::time_point_sec when)const
{
   fc::time_point_sec first_slot_time = get_slot_time( 1 );
   if( when < first_slot_time )
      return 0;
   return (when - first_slot_time).to_seconds() / MORPHENE_BLOCK_INTERVAL + 1;
}

// Create vesting, then a caller-supplied callback after determining how many shares to create, but before
// we modify the database.
// This allows us to implement virtual op pre-notifications in the Before function.
template< typename Before >
legacy_asset create_vesting2( database& db, const account_object& to_account, legacy_asset liquid, Before&& before_vesting_callback )
{
   try
   {
      auto calculate_new_vesting = [ liquid ] ( price vesting_share_price ) -> legacy_asset
         {
         /**
          *  The ratio of total_vesting_shares / total_vesting_fund_morph should not
          *  change as the result of the user adding funds
          *
          *  V / C  = (V+Vn) / (C+Cn)
          *
          *  Simplifies to Vn = (V * Cn ) / C
          *
          *  If Cn equals o.amount, then we must solve for Vn to know how many new vesting shares
          *  the user should receive.
          *
          *  128 bit math is requred due to multiplying of 64 bit numbers. This is done in legacy_asset and price.
          */
         legacy_asset new_vesting = liquid * ( vesting_share_price );
         return new_vesting;
         };

      FC_ASSERT( liquid.symbol == MORPH_SYMBOL );
      // ^ A novelty, needed but risky in case someone managed to slip TESTS here in blockchain history.
      // Get share price.
      const auto& cprops = db.get_dynamic_global_properties();
      price vesting_share_price = cprops.get_vesting_share_price();
      // Calculate new vesting from provided liquid using share price.
      legacy_asset new_vesting = calculate_new_vesting( vesting_share_price );
      before_vesting_callback( new_vesting );
      // Add new vesting to owner's balance.
      db.modify( to_account, [&]( account_object& a )
      {
         util::manabar_params params( util::get_effective_vesting_shares( a ), MORPHENE_VOTING_MANA_REGENERATION_SECONDS );
         a.voting_manabar.regenerate_mana( params, db.head_block_time() );
         a.voting_manabar.use_mana( -new_vesting.amount.value );
      });

      db.adjust_balance( to_account, new_vesting );
      // Update global vesting pool numbers.
      db.modify( cprops, [&]( dynamic_global_property_object& props )
      {
         props.total_vesting_fund_morph += liquid;
         props.total_vesting_shares += new_vesting;
      } );
      // Update witness voting numbers.
      db.adjust_proxied_witness_votes( to_account, new_vesting.amount );

      return new_vesting;
   }
   FC_CAPTURE_AND_RETHROW( (to_account.name)(liquid) )
}

/**
 * @param to_account - the account to receive the new vesting shares
 * @param liquid     - MORPH to be converted to vesting shares
 */
legacy_asset database::create_vesting( const account_object& to_account, legacy_asset liquid )
{
   return create_vesting2( *this, to_account, liquid, []( legacy_asset vests_created ) {} );
}

fc::sha256 database::get_pow_target()const
{
   const auto& dgp = get_dynamic_global_properties();
   fc::sha256 target;
   target._hash[0] = -1;
   target._hash[1] = -1;
   target._hash[2] = -1;
   target._hash[3] = -1;
   target = target >> ((dgp.num_pow_witnesses/4)+4);
   return target;
}

uint32_t database::get_pow_summary_target()const
{
   const dynamic_global_property_object& dgp = get_dynamic_global_properties();
   if( dgp.num_pow_witnesses >= 1004 )
      return 0;

   return (0xFC00 - 0x0040 * dgp.num_pow_witnesses) << 0x10;
}

void database::adjust_proxied_witness_votes( const account_object& a,
                                   const std::array< share_type, MORPHENE_MAX_PROXY_RECURSION_DEPTH+1 >& delta,
                                   int depth )
{
   if( a.proxy != MORPHENE_PROXY_TO_SELF_ACCOUNT )
   {
      /// nested proxies are not supported, vote will not propagate
      if( depth >= MORPHENE_MAX_PROXY_RECURSION_DEPTH )
         return;

      const auto& proxy = get_account( a.proxy );

      modify( proxy, [&]( account_object& a )
      {
         for( int i = MORPHENE_MAX_PROXY_RECURSION_DEPTH - depth - 1; i >= 0; --i )
         {
            a.proxied_vsf_votes[i+depth] += delta[i];
         }
      } );

      adjust_proxied_witness_votes( proxy, delta, depth + 1 );
   }
   else
   {
      share_type total_delta = 0;
      for( int i = MORPHENE_MAX_PROXY_RECURSION_DEPTH - depth; i >= 0; --i )
         total_delta += delta[i];
      adjust_witness_votes( a, total_delta );
   }
}

void database::adjust_proxied_witness_votes( const account_object& a, share_type delta, int depth )
{
   if( a.proxy != MORPHENE_PROXY_TO_SELF_ACCOUNT )
   {
      /// nested proxies are not supported, vote will not propagate
      if( depth >= MORPHENE_MAX_PROXY_RECURSION_DEPTH )
         return;

      const auto& proxy = get_account( a.proxy );

      modify( proxy, [&]( account_object& a )
      {
         a.proxied_vsf_votes[depth] += delta;
      } );

      adjust_proxied_witness_votes( proxy, delta, depth + 1 );
   }
   else
   {
     adjust_witness_votes( a, delta );
   }
}

void database::adjust_witness_votes( const account_object& a, share_type delta )
{
   const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
   auto itr = vidx.lower_bound( boost::make_tuple( a.name, account_name_type() ) );
   while( itr != vidx.end() && itr->account == a.name )
   {
      adjust_witness_vote( get< witness_object, by_name >(itr->witness), delta );
      ++itr;
   }
}

void database::adjust_witness_vote( const witness_object& witness, share_type delta )
{
   const witness_schedule_object& wso = get_witness_schedule_object();
   modify( witness, [&]( witness_object& w )
   {
      auto delta_pos = w.votes.value * (wso.current_virtual_time - w.virtual_last_update);
      w.virtual_position += delta_pos;

      w.virtual_last_update = wso.current_virtual_time;
      w.votes += delta;
      FC_ASSERT( w.votes <= get_dynamic_global_properties().total_vesting_shares.amount, "", ("w.votes", w.votes)("props",get_dynamic_global_properties().total_vesting_shares) );

      w.virtual_scheduled_time = w.virtual_last_update + (MORPHENE_VIRTUAL_SCHEDULE_LAP_LENGTH - w.virtual_position)/(w.votes.value+1);

      /** witnesses with a low number of votes could overflow the time field and end up with a scheduled time in the past */
      if( w.virtual_scheduled_time < wso.current_virtual_time )
         w.virtual_scheduled_time = fc::uint128::max_value();
   } );
}

void database::clear_witness_votes( const account_object& a )
{
   const auto& vidx = get_index< witness_vote_index >().indices().get<by_account_witness>();
   auto itr = vidx.lower_bound( boost::make_tuple( a.name, account_name_type() ) );
   while( itr != vidx.end() && itr->account == a.name )
   {
      const auto& current = *itr;
      ++itr;
      remove(current);
   }

   modify( a, [&](account_object& acc )
   {
      acc.witnesses_voted_for = 0;
   });
}

void database::clear_null_account_balance()
{
   const auto& null_account = get_account( MORPHENE_NULL_ACCOUNT );
   legacy_asset total_morph( 0, MORPH_SYMBOL );
   legacy_asset total_vests( 0, VESTS_SYMBOL );

   legacy_asset vesting_shares_morph_value = legacy_asset( 0, MORPH_SYMBOL );

   if( null_account.balance.amount > 0 )
   {
      total_morph += null_account.balance;
   }

   if( null_account.vesting_shares.amount > 0 )
   {
      const auto& gpo = get_dynamic_global_properties();
      vesting_shares_morph_value = null_account.vesting_shares * gpo.get_vesting_share_price();
      total_morph += vesting_shares_morph_value;
      total_vests += null_account.vesting_shares;
   }

   if( (total_morph.amount.value == 0) && (total_vests.amount.value == 0) )
      return;

   operation vop_op = clear_null_account_balance_operation();
   clear_null_account_balance_operation& vop = vop_op.get< clear_null_account_balance_operation >();
   if( total_morph.amount.value > 0 )
      vop.total_cleared.push_back( total_morph );
   if( total_vests.amount.value > 0 )
      vop.total_cleared.push_back( total_vests );
   pre_push_virtual_operation( vop_op );

   /////////////////////////////////////////////////////////////////////////////////////

   if( null_account.balance.amount > 0 )
   {
      adjust_balance( null_account, -null_account.balance );
   }

   if( null_account.vesting_shares.amount > 0 )
   {
      const auto& gpo = get_dynamic_global_properties();

      modify( gpo, [&]( dynamic_global_property_object& g )
      {
         g.total_vesting_shares -= null_account.vesting_shares;
         g.total_vesting_fund_morph -= vesting_shares_morph_value;
      });

      modify( null_account, [&]( account_object& a )
      {
         a.vesting_shares.amount = 0;
      });
   }

   //////////////////////////////////////////////////////////////

   if( total_morph.amount > 0 )
      adjust_supply( -total_morph );

   post_push_virtual_operation( vop_op );
}

void database::update_owner_authority( const account_object& account, const authority& owner_authority )
{
   if( head_block_num() >= MORPHENE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM )
   {
      create< owner_authority_history_object >( [&]( owner_authority_history_object& hist )
      {
         hist.account = account.name;
         hist.previous_owner_authority = get< account_authority_object, by_account >( account.name ).owner;
         hist.last_valid_time = head_block_time();
      });
   }

   modify( get< account_authority_object, by_account >( account.name ), [&]( account_authority_object& auth )
   {
      auth.owner = owner_authority;
      auth.last_owner_update = head_block_time();
   });
}

void database::process_vesting_withdrawals()
{
   const auto& widx = get_index< account_index, by_next_vesting_withdrawal >();
   const auto& didx = get_index< withdraw_vesting_route_index, by_withdraw_route >();
   auto current = widx.begin();

   const auto& cprops = get_dynamic_global_properties();

   while( current != widx.end() && current->next_vesting_withdrawal <= head_block_time() )
   {
      const auto& from_account = *current; ++current;

      /**
      *  Let T = total tokens in vesting fund
      *  Let V = total vesting shares
      *  Let v = total vesting shares being cashed out
      *
      *  The user may withdraw  vT / V tokens
      */
      share_type to_withdraw;
      if ( from_account.to_withdraw - from_account.withdrawn < from_account.vesting_withdraw_rate.amount )
         to_withdraw = std::min( from_account.vesting_shares.amount, from_account.to_withdraw % from_account.vesting_withdraw_rate.amount ).value;
      else
         to_withdraw = std::min( from_account.vesting_shares.amount, from_account.vesting_withdraw_rate.amount ).value;

      share_type vests_deposited_as_morphene = 0;
      share_type vests_deposited_as_vests = 0;
      legacy_asset total_morph_converted = legacy_asset( 0, MORPH_SYMBOL );

      // Do two passes, the first for vests, the second for morphene. Try to maintain as much accuracy for vests as possible.
      for( auto itr = didx.upper_bound( boost::make_tuple( from_account.name, account_name_type() ) );
           itr != didx.end() && itr->from_account == from_account.name;
           ++itr )
      {
         if( itr->auto_vest )
         {
            share_type to_deposit = ( ( fc::uint128_t ( to_withdraw.value ) * itr->percent ) / MORPHENE_100_PERCENT ).to_uint64();
            vests_deposited_as_vests += to_deposit;

            if( to_deposit > 0 )
            {
               const auto& to_account = get< account_object, by_name >( itr->to_account );

               operation vop = fill_vesting_withdraw_operation( from_account.name, to_account.name, legacy_asset( to_deposit, VESTS_SYMBOL ), legacy_asset( to_deposit, VESTS_SYMBOL ) );

               pre_push_virtual_operation( vop );

               modify( to_account, [&]( account_object& a )
               {
                  a.vesting_shares.amount += to_deposit;
               });

               adjust_proxied_witness_votes( to_account, to_deposit );

               post_push_virtual_operation( vop );
            }
         }
      }

      for( auto itr = didx.upper_bound( boost::make_tuple( from_account.name, account_name_type() ) );
           itr != didx.end() && itr->from_account == from_account.name;
           ++itr )
      {
         if( !itr->auto_vest )
         {
            const auto& to_account = get< account_object, by_name >( itr->to_account );

            share_type to_deposit = ( ( fc::uint128_t ( to_withdraw.value ) * itr->percent ) / MORPHENE_100_PERCENT ).to_uint64();
            vests_deposited_as_morphene += to_deposit;
            legacy_asset converted_morph = legacy_asset( to_deposit, VESTS_SYMBOL ) * cprops.get_vesting_share_price();
            total_morph_converted += converted_morph;

            if( to_deposit > 0 )
            {
               operation vop = fill_vesting_withdraw_operation( from_account.name, to_account.name, legacy_asset( to_deposit, VESTS_SYMBOL), converted_morph );

               pre_push_virtual_operation( vop );

               modify( to_account, [&]( account_object& a )
               {
                  a.balance += converted_morph;
               });

               modify( cprops, [&]( dynamic_global_property_object& o )
               {
                  o.total_vesting_fund_morph -= converted_morph;
                  o.total_vesting_shares.amount -= to_deposit;
               });

               post_push_virtual_operation( vop );
            }
         }
      }

      share_type to_convert = to_withdraw - vests_deposited_as_morphene - vests_deposited_as_vests;
      FC_ASSERT( to_convert >= 0, "Deposited more vests than were supposed to be withdrawn" );

      legacy_asset converted_morph = legacy_asset( to_convert, VESTS_SYMBOL ) * cprops.get_vesting_share_price();
      operation vop = fill_vesting_withdraw_operation( from_account.name, from_account.name, legacy_asset( to_convert, VESTS_SYMBOL ), converted_morph );
      pre_push_virtual_operation( vop );

      modify( from_account, [&]( account_object& a )
      {
         a.vesting_shares.amount -= to_withdraw;
         a.balance += converted_morph;
         a.withdrawn += to_withdraw;

         if( a.withdrawn >= a.to_withdraw || a.vesting_shares.amount == 0 )
         {
            a.vesting_withdraw_rate.amount = 0;
            a.next_vesting_withdrawal = fc::time_point_sec::maximum();
         }
         else
         {
            a.next_vesting_withdrawal += fc::seconds( MORPHENE_VESTING_WITHDRAW_INTERVAL_SECONDS );
         }
      });

      modify( cprops, [&]( dynamic_global_property_object& o )
      {
         o.total_vesting_fund_morph -= converted_morph;
         o.total_vesting_shares.amount -= to_convert;
      });

      if( to_withdraw > 0 )
         adjust_proxied_witness_votes( from_account, -to_withdraw );

      post_push_virtual_operation( vop );
   }
}

/**
 *  Overall the network has an inflation rate of 102% of virtual morphene per year
 *  90% of inflation is directed to vesting shares
 *  10% of inflation is directed to subjective proof of work voting
 *  1% of inflation is directed to block producers
 *
 *  This method pays out vesting and reward shares every block.
 *  This method does not pay out witnesses.
 */
void database::process_funds()
{
   const auto& props = get_dynamic_global_properties();
   const auto& wso = get_witness_schedule_object();

   /**
    * At block 0 have a 9.5% instantaneous inflation rate, decreasing to 0.95% at a rate of 0.01%
    * every 250k blocks. This narrowing will take approximately 20.5 years and will complete on block 213,750,000
    */
   int64_t start_inflation_rate = int64_t( MORPHENE_INFLATION_RATE_START_PERCENT );
   int64_t inflation_rate_adjustment = int64_t( head_block_num() / MORPHENE_INFLATION_NARROWING_PERIOD );
   int64_t inflation_rate_floor = int64_t( MORPHENE_INFLATION_RATE_STOP_PERCENT );

   // below subtraction cannot underflow int64_t because inflation_rate_adjustment is <2^32
   int64_t current_inflation_rate = std::max( start_inflation_rate - inflation_rate_adjustment, inflation_rate_floor );

   auto new_morph = ( props.current_supply.amount * current_inflation_rate ) / ( int64_t( MORPHENE_100_PERCENT ) * int64_t( MORPHENE_BLOCKS_PER_YEAR ) );
   auto witness_reward = new_morph; /// 100% of inflation to Witnesses

   const auto& cwit = get_witness( props.current_witness );
   witness_reward *= MORPHENE_MAX_WITNESSES;

   if( cwit.schedule == witness_object::timeshare )
      witness_reward *= wso.timeshare_weight;
   else if( cwit.schedule == witness_object::miner )
      witness_reward *= wso.miner_weight;
   else if( cwit.schedule == witness_object::elected )
      witness_reward *= wso.elected_weight;
   else
      wlog( "Encountered unknown witness type for witness: ${w} - ${t}", ("w", cwit.owner)("t", cwit.schedule) );

   witness_reward /= wso.witness_pay_normalization_factor;

   new_morph = witness_reward;

   modify( props, [&]( dynamic_global_property_object& p )
   {
      p.current_supply           += legacy_asset( new_morph, MORPH_SYMBOL );
   });

   operation vop = producer_reward_operation( cwit.owner, legacy_asset( 0, VESTS_SYMBOL ) );
   create_vesting2( *this, get_account( cwit.owner ), legacy_asset( witness_reward, MORPH_SYMBOL ),
      [&]( const legacy_asset& vesting_shares )
      {
         vop.get< producer_reward_operation >().vesting_shares = vesting_shares;
         pre_push_virtual_operation( vop );
      } );
   post_push_virtual_operation( vop );
}

void database::process_subsidized_accounts()
{
   const witness_schedule_object& wso = get_witness_schedule_object();
   const dynamic_global_property_object& gpo = get_dynamic_global_properties();

   // Update global pool.
   modify( gpo, [&]( dynamic_global_property_object& g )
   {
      g.available_account_subsidies = rd_apply( wso.account_subsidy_rd, g.available_account_subsidies );
   } );

   // Update per-witness pool for current witness.
   const witness_object& current_witness = get_witness( gpo.current_witness );
   if( current_witness.schedule == witness_object::elected )
   {
      modify( current_witness, [&]( witness_object& w )
      {
         w.available_witness_account_subsidies = rd_apply( wso.account_subsidy_witness_rd, w.available_witness_account_subsidies );
      } );
   }
}

legacy_asset database::get_pow_reward()const
{
   const auto& props = get_dynamic_global_properties();

#ifndef IS_TEST_NET
   /// 0 block rewards until at least MORPHENE_MAX_WITNESSES have produced a POW
   if( props.num_pow_witnesses < MORPHENE_MAX_WITNESSES && props.head_block_number < MORPHENE_START_VESTING_BLOCK )
      return legacy_asset( 0, MORPH_SYMBOL );
#endif

   FC_ASSERT( MORPHENE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
   FC_ASSERT( MORPHENE_MAX_WITNESSES == 21, "this code assumes 21 per round" );
   legacy_asset percent( calc_percent_reward_per_round< MORPHENE_POW_APR_PERCENT >( props.current_supply.amount ), MORPH_SYMBOL);
   return std::max( percent, MORPHENE_MIN_POW_REWARD );
}

void database::account_recovery_processing()
{
   // Clear expired recovery requests
   const auto& rec_req_idx = get_index< account_recovery_request_index >().indices().get< by_expiration >();
   auto rec_req = rec_req_idx.begin();

   while( rec_req != rec_req_idx.end() && rec_req->expires <= head_block_time() )
   {
      remove( *rec_req );
      rec_req = rec_req_idx.begin();
   }

   // Clear invalid historical authorities
   const auto& hist_idx = get_index< owner_authority_history_index >().indices(); //by id
   auto hist = hist_idx.begin();

   while( hist != hist_idx.end() && time_point_sec( hist->last_valid_time + MORPHENE_OWNER_AUTH_RECOVERY_PERIOD ) < head_block_time() )
   {
      remove( *hist );
      hist = hist_idx.begin();
   }

   // Apply effective recovery_account changes
   const auto& change_req_idx = get_index< change_recovery_account_request_index >().indices().get< by_effective_date >();
   auto change_req = change_req_idx.begin();

   while( change_req != change_req_idx.end() && change_req->effective_on <= head_block_time() )
   {
      modify( get_account( change_req->account_to_recover ), [&]( account_object& a )
      {
         a.recovery_account = change_req->recovery_account;
      });

      remove( *change_req );
      change_req = change_req_idx.begin();
   }
}

void database::expire_escrow_ratification()
{
   const auto& escrow_idx = get_index< escrow_index >().indices().get< by_ratification_deadline >();
   auto escrow_itr = escrow_idx.lower_bound( false );

   while( escrow_itr != escrow_idx.end() && !escrow_itr->is_approved() && escrow_itr->ratification_deadline <= head_block_time() )
   {
      const auto& old_escrow = *escrow_itr;
      ++escrow_itr;

      adjust_balance( old_escrow.from, old_escrow.morph_balance );
      adjust_balance( old_escrow.from, old_escrow.pending_fee );

      remove( old_escrow );
   }
}

time_point_sec database::head_block_time()const
{
   return get_dynamic_global_properties().time;
}

uint32_t database::head_block_num()const
{
   return get_dynamic_global_properties().head_block_number;
}

block_id_type database::head_block_id()const
{
   return get_dynamic_global_properties().head_block_id;
}

node_property_object& database::node_properties()
{
   return _node_property_object;
}

uint32_t database::last_non_undoable_block_num() const
{
   return get_dynamic_global_properties().last_irreversible_block_num;
}

void database::initialize_evaluators()
{
   _my->_evaluator_registry.register_evaluator< transfer_evaluator                       >();
   _my->_evaluator_registry.register_evaluator< transfer_to_vesting_evaluator            >();
   _my->_evaluator_registry.register_evaluator< withdraw_vesting_evaluator               >();
   _my->_evaluator_registry.register_evaluator< set_withdraw_vesting_route_evaluator     >();
   _my->_evaluator_registry.register_evaluator< account_create_evaluator                 >();
   _my->_evaluator_registry.register_evaluator< account_update_evaluator                 >();
   _my->_evaluator_registry.register_evaluator< witness_update_evaluator                 >();
   _my->_evaluator_registry.register_evaluator< account_witness_vote_evaluator           >();
   _my->_evaluator_registry.register_evaluator< account_witness_proxy_evaluator          >();
   _my->_evaluator_registry.register_evaluator< custom_evaluator                         >();
   _my->_evaluator_registry.register_evaluator< custom_binary_evaluator                  >();
   _my->_evaluator_registry.register_evaluator< custom_json_evaluator                    >();
   _my->_evaluator_registry.register_evaluator< pow_evaluator                            >();
   _my->_evaluator_registry.register_evaluator< claim_account_evaluator                  >();
   _my->_evaluator_registry.register_evaluator< create_claimed_account_evaluator         >();
   _my->_evaluator_registry.register_evaluator< request_account_recovery_evaluator       >();
   _my->_evaluator_registry.register_evaluator< recover_account_evaluator                >();
   _my->_evaluator_registry.register_evaluator< change_recovery_account_evaluator        >();
   _my->_evaluator_registry.register_evaluator< escrow_transfer_evaluator                >();
   _my->_evaluator_registry.register_evaluator< escrow_approve_evaluator                 >();
   _my->_evaluator_registry.register_evaluator< escrow_dispute_evaluator                 >();
   _my->_evaluator_registry.register_evaluator< escrow_release_evaluator                 >();
   _my->_evaluator_registry.register_evaluator< reset_account_evaluator                  >();
   _my->_evaluator_registry.register_evaluator< set_reset_account_evaluator              >();
   _my->_evaluator_registry.register_evaluator< account_create_with_delegation_evaluator >();
   _my->_evaluator_registry.register_evaluator< delegate_vesting_shares_evaluator        >();
   _my->_evaluator_registry.register_evaluator< witness_set_properties_evaluator         >();
}


void database::set_custom_operation_interpreter( const std::string& id, std::shared_ptr< custom_operation_interpreter > registry )
{
   bool inserted = _custom_operation_interpreters.emplace( id, registry ).second;
   // This assert triggering means we're mis-configured (multiple registrations of custom JSON evaluator for same ID)
   FC_ASSERT( inserted );
}

std::shared_ptr< custom_operation_interpreter > database::get_custom_json_evaluator( const std::string& id )
{
   auto it = _custom_operation_interpreters.find( id );
   if( it != _custom_operation_interpreters.end() )
      return it->second;
   return std::shared_ptr< custom_operation_interpreter >();
}

void database::initialize_indexes()
{
   add_core_index< dynamic_global_property_index           >(*this);
   add_core_index< account_index                           >(*this);
   add_core_index< account_authority_index                 >(*this);
   add_core_index< witness_index                           >(*this);
   add_core_index< transaction_index                       >(*this);
   add_core_index< block_summary_index                     >(*this);
   add_core_index< witness_schedule_index                  >(*this);
   add_core_index< witness_vote_index                      >(*this);
   add_core_index< operation_index                         >(*this);
   add_core_index< account_history_index                   >(*this);
   add_core_index< hardfork_property_index                 >(*this);
   add_core_index< withdraw_vesting_route_index            >(*this);
   add_core_index< owner_authority_history_index           >(*this);
   add_core_index< account_recovery_request_index          >(*this);
   add_core_index< change_recovery_account_request_index   >(*this);
   add_core_index< escrow_index                            >(*this);
   add_core_index< vesting_delegation_index                >(*this);
   add_core_index< vesting_delegation_expiration_index     >(*this);

   _plugin_index_signal();
}

const std::string& database::get_json_schema()const
{
   return _json_schema;
}

void database::init_genesis( uint64_t init_supply )
{
   try
   {
      struct auth_inhibitor
      {
         auth_inhibitor(database& db) : db(db), old_flags(db.node_properties().skip_flags)
         { db.node_properties().skip_flags |= skip_authority_check; }
         ~auth_inhibitor()
         { db.node_properties().skip_flags = old_flags; }
      private:
         database& db;
         uint32_t old_flags;
      } inhibitor(*this);

      // Create blockchain accounts
      public_key_type      init_public_key(MORPHENE_INIT_PUBLIC_KEY);

      create< account_object >( [&]( account_object& a )
      {
         a.name = MORPHENE_NULL_ACCOUNT;
      } );
      create< account_authority_object >( [&]( account_authority_object& auth )
      {
         auth.account = MORPHENE_NULL_ACCOUNT;
         auth.owner.weight_threshold = 1;
         auth.active.weight_threshold = 1;
         auth.posting = authority();
         auth.posting.weight_threshold = 1;
      });

      create< account_object >( [&]( account_object& a )
      {
         a.name = MORPHENE_TEMP_ACCOUNT;
      } );
      create< account_authority_object >( [&]( account_authority_object& auth )
      {
         auth.account = MORPHENE_TEMP_ACCOUNT;
         auth.owner.weight_threshold = 0;
         auth.active.weight_threshold = 0;
         auth.posting = authority();
         auth.posting.weight_threshold = 1;
      });

      for( int i = 0; i < MORPHENE_NUM_INIT_WITNESSES; ++i )
      {
         create< account_object >( [&]( account_object& a )
         {
            a.name = MORPHENE_INIT_WITNESS_NAME + ( i ? fc::to_string( i ) : std::string() );
            a.memo_key = init_public_key;
            a.balance  = legacy_asset( i ? 0 : init_supply, MORPH_SYMBOL );
         } );

         create< account_authority_object >( [&]( account_authority_object& auth )
         {
            auth.account = MORPHENE_INIT_WITNESS_NAME + ( i ? fc::to_string( i ) : std::string() );
            auth.owner.add_authority( init_public_key, 1 );
            auth.owner.weight_threshold = 1;
            auth.active  = auth.owner;
            auth.posting = auth.active;
         });

         create< witness_object >( [&]( witness_object& w )
         {
            w.owner        = MORPHENE_INIT_WITNESS_NAME + ( i ? fc::to_string(i) : std::string() );
            w.signing_key  = init_public_key;
            w.schedule = witness_object::elected;
         } );
      }

      create< dynamic_global_property_object >( [&]( dynamic_global_property_object& p )
      {
         p.current_witness = MORPHENE_INIT_WITNESS_NAME;
         p.time = MORPHENE_GENESIS_TIME;
         p.recent_slots_filled = fc::uint128::max_value();
         p.participation_count = 128;
         p.current_supply = legacy_asset( init_supply, MORPH_SYMBOL );
         p.maximum_block_size = MORPHENE_MAX_BLOCK_SIZE;
         p.delegation_return_period = MORPHENE_DELEGATION_RETURN_PERIOD;
         p.available_account_subsidies = 0;
      } );

      for( int i = 0; i < 0x10000; i++ )
         create< block_summary_object >( [&]( block_summary_object& ) {});
      create< hardfork_property_object >( [&](hardfork_property_object& hpo )
      {
         hpo.processed_hardforks.push_back( MORPHENE_GENESIS_TIME );
      } );

      // Create witness scheduler
      create< witness_schedule_object >( [&]( witness_schedule_object& wso )
      {
         wso.current_shuffled_witnesses[0] = MORPHENE_INIT_WITNESS_NAME;
         util::rd_system_params account_subsidy_system_params;
         account_subsidy_system_params.resource_unit = MORPHENE_ACCOUNT_SUBSIDY_PRECISION;
         account_subsidy_system_params.decay_per_time_unit_denom_shift = MORPHENE_RD_DECAY_DENOM_SHIFT;
         util::rd_user_params account_subsidy_user_params;
         account_subsidy_user_params.budget_per_time_unit = wso.median_props.account_subsidy_budget;
         account_subsidy_user_params.decay_per_time_unit = wso.median_props.account_subsidy_decay;

         util::rd_user_params account_subsidy_per_witness_user_params;
         int64_t w_budget = wso.median_props.account_subsidy_budget;
         w_budget = (w_budget * MORPHENE_WITNESS_SUBSIDY_BUDGET_PERCENT) / MORPHENE_100_PERCENT;
         w_budget = std::min( w_budget, int64_t(std::numeric_limits<int32_t>::max()) );
         uint64_t w_decay = wso.median_props.account_subsidy_decay;
         w_decay = (w_decay * MORPHENE_WITNESS_SUBSIDY_DECAY_PERCENT) / MORPHENE_100_PERCENT;
         w_decay = std::min( w_decay, uint64_t(std::numeric_limits<uint32_t>::max()) );

         account_subsidy_per_witness_user_params.budget_per_time_unit = int32_t(w_budget);
         account_subsidy_per_witness_user_params.decay_per_time_unit = uint32_t(w_decay);

         util::rd_setup_dynamics_params( account_subsidy_user_params, account_subsidy_system_params, wso.account_subsidy_rd );
         util::rd_setup_dynamics_params( account_subsidy_per_witness_user_params, account_subsidy_system_params, wso.account_subsidy_witness_rd );
      } );
   }
   FC_CAPTURE_AND_RETHROW()
}


void database::validate_transaction( const signed_transaction& trx )
{
   database::with_write_lock( [&]()
   {
      auto session = start_undo_session();
      _apply_transaction( trx );
      session.undo();
   });
}

void database::set_flush_interval( uint32_t flush_blocks )
{
   _flush_blocks = flush_blocks;
   _next_flush_block = 0;
}

//////////////////// private methods ////////////////////

void database::apply_block( const signed_block& next_block, uint32_t skip )
{ try {
   //fc::time_point begin_time = fc::time_point::now();

   detail::with_skip_flags( *this, skip, [&]()
   {
      _apply_block( next_block );
   } );

   /*try
   {
   /// check invariants
   if( is_producing() || !( skip & skip_validate_invariants ) )
      validate_invariants();
   }
   FC_CAPTURE_AND_RETHROW( (next_block) );*/

   auto block_num = next_block.block_num();

   //fc::time_point end_time = fc::time_point::now();
   //fc::microseconds dt = end_time - begin_time;
   if( _flush_blocks != 0 )
   {
      if( _next_flush_block == 0 )
      {
         uint32_t lep = block_num + 1 + _flush_blocks * 9 / 10;
         uint32_t rep = block_num + 1 + _flush_blocks;

         // use time_point::now() as RNG source to pick block randomly between lep and rep
         uint32_t span = rep - lep;
         uint32_t x = lep;
         if( span > 0 )
         {
            uint64_t now = uint64_t( fc::time_point::now().time_since_epoch().count() );
            x += now % span;
         }
         _next_flush_block = x;
         //ilog( "Next flush scheduled at block ${b}", ("b", x) );
      }

      if( _next_flush_block == block_num )
      {
         _next_flush_block = 0;
         //ilog( "Flushing database shared memory at block ${b}", ("b", block_num) );
         chainbase::database::flush();
      }
   }

} FC_CAPTURE_AND_RETHROW( (next_block) ) }

void database::check_free_memory( bool force_print, uint32_t current_block_num )
{
   uint64_t free_mem = get_free_memory();
   uint64_t max_mem = get_max_memory();

   if( BOOST_UNLIKELY( _shared_file_full_threshold != 0 && _shared_file_scale_rate != 0 && free_mem < ( ( uint128_t( MORPHENE_100_PERCENT - _shared_file_full_threshold ) * max_mem ) / MORPHENE_100_PERCENT ).to_uint64() ) )
   {
      uint64_t new_max = ( uint128_t( max_mem * _shared_file_scale_rate ) / MORPHENE_100_PERCENT ).to_uint64() + max_mem;

      wlog( "Memory is almost full, increasing to ${mem}M", ("mem", new_max / (1024*1024)) );

      resize( new_max );

      uint32_t free_mb = uint32_t( get_free_memory() / (1024*1024) );
      wlog( "Free memory is now ${free}M", ("free", free_mb) );
      _last_free_gb_printed = free_mb / 1024;
   }
   else
   {
      uint32_t free_gb = uint32_t( free_mem / (1024*1024*1024) );
      if( BOOST_UNLIKELY( force_print || (free_gb < _last_free_gb_printed) || (free_gb > _last_free_gb_printed+1) ) )
      {
         ilog( "Free memory is now ${n}G. Current block number: ${block}", ("n", free_gb)("block",current_block_num) );
         _last_free_gb_printed = free_gb;
      }

      if( BOOST_UNLIKELY( free_gb == 0 ) )
      {
         uint32_t free_mb = uint32_t( free_mem / (1024*1024) );

   #ifdef IS_TEST_NET
      if( !disable_low_mem_warning )
   #endif
         if( free_mb <= 100 && head_block_num() % 10 == 0 )
            elog( "Free memory is now ${n}M. Increase shared file size immediately!" , ("n", free_mb) );
      }
   }
}

void database::_apply_block( const signed_block& next_block )
{ try {
   block_notification note( next_block );

   notify_pre_apply_block( note );

   const uint32_t next_block_num = note.block_num;

   BOOST_SCOPE_EXIT( this_ )
   {
      this_->_currently_processing_block_id.reset();
   } BOOST_SCOPE_EXIT_END
   _currently_processing_block_id = note.block_id;

   uint32_t skip = get_node_properties().skip_flags;

   _current_block_num    = next_block_num;
   _current_trx_in_block = 0;
   _current_virtual_op   = 0;

   if( BOOST_UNLIKELY( next_block_num == 1 ) )
   {
      // For every existing before the head_block_time (genesis time), apply the hardfork
      // This allows the test net to launch with past hardforks and apply the next harfork when running

      uint32_t n;
      for( n=0; n<MORPHENE_NUM_HARDFORKS; n++ )
      {
         if( _hardfork_times[n+1] > next_block.timestamp )
            break;
      }

      if( n > 0 )
      {
         ilog( "Processing ${n} genesis hardforks", ("n", n) );
         set_hardfork( n, true );

         const hardfork_property_object& hardfork_state = get_hardfork_property_object();
         FC_ASSERT( hardfork_state.current_hardfork_version == _hardfork_versions[n], "Unexpected genesis hardfork state" );

         const auto& witness_idx = get_index<witness_index>().indices().get<by_id>();
         vector<witness_id_type> wit_ids_to_update;
         for( auto it=witness_idx.begin(); it!=witness_idx.end(); ++it )
            wit_ids_to_update.push_back(it->id);

         for( witness_id_type wit_id : wit_ids_to_update )
         {
            modify( get( wit_id ), [&]( witness_object& wit )
            {
               wit.running_version = _hardfork_versions[n];
               wit.hardfork_version_vote = _hardfork_versions[n];
               wit.hardfork_time_vote = _hardfork_times[n];
            } );
         }
      }
   }

   if( !( skip & skip_merkle_check ) )
   {
      auto merkle_root = next_block.calculate_merkle_root();

      try
      {
         FC_ASSERT( next_block.transaction_merkle_root == merkle_root, "Merkle check failed", ("next_block.transaction_merkle_root",next_block.transaction_merkle_root)("calc",merkle_root)("next_block",next_block)("id",next_block.id()) );
      }
      catch( fc::assert_exception& e )
      {
         const auto& merkle_map = get_shared_db_merkle();
         auto itr = merkle_map.find( next_block_num );

         if( itr == merkle_map.end() || itr->second != merkle_root )
            throw e;
      }
   }

   const witness_object& signing_witness = validate_block_header(skip, next_block);

   const auto& gprops = get_dynamic_global_properties();
   auto block_size = fc::raw::pack_size( next_block );
   FC_ASSERT( block_size <= gprops.maximum_block_size, "Block Size is too Big", ("next_block_num",next_block_num)("block_size", block_size)("max",gprops.maximum_block_size) );

   if( block_size < MORPHENE_MIN_BLOCK_SIZE )
   {
      elog( "Block size is too small",
         ("next_block_num",next_block_num)("block_size", block_size)("min",MORPHENE_MIN_BLOCK_SIZE)
      );
   }

   /// modify current witness so transaction evaluators can know who included the transaction,
   /// this is mostly for witness operations which must pay the current_witness
   modify( gprops, [&]( dynamic_global_property_object& dgp ){
      dgp.current_witness = next_block.witness;
   });

   /// parse witness version reporting
   process_header_extensions( next_block );

   const auto& witness = get_witness( next_block.witness );
   const auto& hardfork_state = get_hardfork_property_object();
   FC_ASSERT( witness.running_version >= hardfork_state.current_hardfork_version,
      "Block produced by witness that is not running current hardfork",
      ("witness",witness)("next_block.witness",next_block.witness)("hardfork_state", hardfork_state)
   );

   for( const auto& trx : next_block.transactions )
   {
      /* We do not need to push the undo state for each transaction
       * because they either all apply and are valid or the
       * entire block fails to apply.  We only need an "undo" state
       * for transactions when validating broadcast transactions or
       * when building a block.
       */
      apply_transaction( trx, skip );
      ++_current_trx_in_block;
   }

   _current_trx_in_block = -1;
   _current_op_in_trx = 0;
   _current_virtual_op = 0;

   update_global_dynamic_data(next_block);
   update_signing_witness(signing_witness, next_block);

   update_last_irreversible_block();

   create_block_summary(next_block);
   clear_expired_transactions();
   clear_expired_delegations();
   update_witness_schedule(*this);

   clear_null_account_balance();
   process_funds();
   process_vesting_withdrawals();
   process_subsidized_accounts();

   account_recovery_processing();
   expire_escrow_ratification();

   process_hardforks();

   // notify observers that the block has been applied
   notify_post_apply_block( note );

   // This moves newly irreversible blocks from the fork db to the block log
   // and commits irreversible state to the database. This should always be the
   // last call of applying a block because it is the only thing that is not
   // reversible.
   migrate_irreversible_state();
} FC_CAPTURE_LOG_AND_RETHROW( (next_block.block_num()) ) }

struct process_header_visitor
{
   process_header_visitor( const std::string& witness, database& db ) : _witness( witness ), _db( db ) {}

   typedef void result_type;

   const std::string& _witness;
   database& _db;

   void operator()( const void_t& obj ) const
   {
      //Nothing to do.
   }

   void operator()( const version& reported_version ) const
   {
      const auto& signing_witness = _db.get_witness( _witness );
      //idump( (next_block.witness)(signing_witness.running_version)(reported_version) );

      if( reported_version != signing_witness.running_version )
      {
         _db.modify( signing_witness, [&]( witness_object& wo )
         {
            wo.running_version = reported_version;
         });
      }
   }

   void operator()( const hardfork_version_vote& hfv ) const
   {
      const auto& signing_witness = _db.get_witness( _witness );
      //idump( (next_block.witness)(signing_witness.running_version)(hfv) );

      if( hfv.hf_version != signing_witness.hardfork_version_vote || hfv.hf_time != signing_witness.hardfork_time_vote )
         _db.modify( signing_witness, [&]( witness_object& wo )
         {
            wo.hardfork_version_vote = hfv.hf_version;
            wo.hardfork_time_vote = hfv.hf_time;
         });
   }
};

void database::process_header_extensions( const signed_block& next_block )
{
   process_header_visitor _v( next_block.witness, *this );

   for( const auto& e : next_block.extensions )
      e.visit( _v );
}

void database::apply_transaction(const signed_transaction& trx, uint32_t skip)
{
   detail::with_skip_flags( *this, skip, [&]() { _apply_transaction(trx); });
}

void database::_apply_transaction(const signed_transaction& trx)
{ try {
   transaction_notification note(trx);
   _current_trx_id = note.transaction_id;
   const transaction_id_type& trx_id = note.transaction_id;
   _current_virtual_op = 0;

   uint32_t skip = get_node_properties().skip_flags;

   if( !(skip&skip_validate) )   /* issue #505 explains why this skip_flag is disabled */
      trx.validate();

   auto& trx_idx = get_index<transaction_index>();
   const chain_id_type& chain_id = get_chain_id();
   // idump((trx_id)(skip&skip_transaction_dupe_check));
   FC_ASSERT( (skip & skip_transaction_dupe_check) ||
              trx_idx.indices().get<by_trx_id>().find(trx_id) == trx_idx.indices().get<by_trx_id>().end(),
              "Duplicate transaction check failed", ("trx_ix", trx_id) );

   if( !(skip & (skip_transaction_signatures | skip_authority_check) ) )
   {
      auto get_active  = [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).active ); };
      auto get_owner   = [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).owner );  };
      auto get_posting = [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).posting );  };

      try
      {
         trx.verify_authority( chain_id, get_active, get_owner, get_posting, MORPHENE_MAX_SIG_CHECK_DEPTH,
            is_producing() ? MORPHENE_MAX_AUTHORITY_MEMBERSHIP : 0,
            is_producing() ? MORPHENE_MAX_SIG_CHECK_ACCOUNTS : 0,
            fc::ecc::bip_0062 );
      }
      catch( protocol::tx_missing_active_auth& e )
      {
         if( get_shared_db_merkle().find( head_block_num() + 1 ) == get_shared_db_merkle().end() )
            throw e;
      }
   }

   //Skip all manner of expiration and TaPoS checking if we're on block 1; It's impossible that the transaction is
   //expired, and TaPoS makes no sense as no blocks exist.
   if( BOOST_LIKELY(head_block_num() > 0) )
   {
      if( !(skip & skip_tapos_check) )
      {
         const auto& tapos_block_summary = get< block_summary_object >( trx.ref_block_num );
         //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
         MORPHENE_ASSERT( trx.ref_block_prefix == tapos_block_summary.block_id._hash[1], transaction_tapos_exception,
                    "", ("trx.ref_block_prefix", trx.ref_block_prefix)
                    ("tapos_block_summary",tapos_block_summary.block_id._hash[1]));
      }

      fc::time_point_sec now = head_block_time();

      MORPHENE_ASSERT( trx.expiration <= now + fc::seconds(MORPHENE_MAX_TIME_UNTIL_EXPIRATION), transaction_expiration_exception,
                  "", ("trx.expiration",trx.expiration)("now",now)("max_til_exp",MORPHENE_MAX_TIME_UNTIL_EXPIRATION));
      MORPHENE_ASSERT( now <= trx.expiration, transaction_expiration_exception, "", ("now",now)("trx.exp",trx.expiration) );
   }

   //Insert transaction into unique transactions database.
   if( !(skip & skip_transaction_dupe_check) )
   {
      create<transaction_object>([&](transaction_object& transaction) {
         transaction.trx_id = trx_id;
         transaction.expiration = trx.expiration;
         fc::raw::pack_to_buffer( transaction.packed_trx, trx );
      });
   }

   notify_pre_apply_transaction( note );

   //Finally process the operations
   _current_op_in_trx = 0;
   for( const auto& op : trx.operations )
   { try {
      apply_operation(op);
      ++_current_op_in_trx;
     } FC_CAPTURE_AND_RETHROW( (op) );
   }
   _current_trx_id = transaction_id_type();

   notify_post_apply_transaction( note );

} FC_CAPTURE_AND_RETHROW( (trx) ) }

void database::apply_operation(const operation& op)
{
   operation_notification note = create_operation_notification( op );
   notify_pre_apply_operation( note );

   if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.begin();

   _my->_evaluator_registry.get_evaluator( op ).apply( op );

   if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.end< true/*APPLY_CONTEXT*/ >( _my->_evaluator_registry.get_evaluator( op ).get_name( op ) );

   notify_post_apply_operation( note );
}


template <typename TFunction> struct fcall {};

template <typename TResult, typename... TArgs>
struct fcall<TResult(TArgs...)>
{
   using TNotification = std::function<TResult(TArgs...)>;

   fcall() = default;
   fcall(const TNotification& func, util::advanced_benchmark_dumper& dumper,
         const abstract_plugin& plugin, const std::string& item_name)
         : _func(func), _benchmark_dumper(dumper)
      {
         _name = plugin.get_name() + item_name;
      }

   void operator () (TArgs&&... args)
   {
      if (_benchmark_dumper.is_enabled())
         _benchmark_dumper.begin();

      _func(std::forward<TArgs>(args)...);

      if (_benchmark_dumper.is_enabled())
         _benchmark_dumper.end(_name);
   }

private:
   TNotification                    _func;
   util::advanced_benchmark_dumper& _benchmark_dumper;
   std::string                      _name;
};

template <typename TResult, typename... TArgs>
struct fcall<std::function<TResult(TArgs...)>>
   : public fcall<TResult(TArgs...)>
{
   typedef fcall<TResult(TArgs...)> TBase;
   using TBase::TBase;
};

template <typename TSignal, typename TNotification>
boost::signals2::connection database::connect_impl( TSignal& signal, const TNotification& func,
   const abstract_plugin& plugin, int32_t group, const std::string& item_name )
{
   fcall<TNotification> fcall_wrapper(func,_benchmark_dumper,plugin,item_name);

   return signal.connect(group, fcall_wrapper);
}

template< bool IS_PRE_OPERATION >
boost::signals2::connection database::any_apply_operation_handler_impl( const apply_operation_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   auto complex_func = [this, func, &plugin]( const operation_notification& o )
   {
      std::string name;

      if (_benchmark_dumper.is_enabled())
      {
         if( _my->_evaluator_registry.is_evaluator( o.op ) )
            name = _benchmark_dumper.generate_desc< IS_PRE_OPERATION >( plugin.get_name(), _my->_evaluator_registry.get_evaluator( o.op ).get_name( o.op ) );
         else
            name = util::advanced_benchmark_dumper::get_virtual_operation_name();

         _benchmark_dumper.begin();
      }

      func( o );

      if (_benchmark_dumper.is_enabled())
         _benchmark_dumper.end( name );
   };

   if( IS_PRE_OPERATION )
      return _pre_apply_operation_signal.connect(group, complex_func);
   else
      return _post_apply_operation_signal.connect(group, complex_func);
}

boost::signals2::connection database::add_pre_apply_operation_handler( const apply_operation_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return any_apply_operation_handler_impl< true/*IS_PRE_OPERATION*/ >( func, plugin, group );
}

boost::signals2::connection database::add_post_apply_operation_handler( const apply_operation_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return any_apply_operation_handler_impl< false/*IS_PRE_OPERATION*/ >( func, plugin, group );
}

boost::signals2::connection database::add_pre_apply_transaction_handler( const apply_transaction_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return connect_impl(_pre_apply_transaction_signal, func, plugin, group, "->transaction");
}

boost::signals2::connection database::add_post_apply_transaction_handler( const apply_transaction_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return connect_impl(_post_apply_transaction_signal, func, plugin, group, "<-transaction");
}

boost::signals2::connection database::add_pre_apply_block_handler( const apply_block_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return connect_impl(_pre_apply_block_signal, func, plugin, group, "->block");
}

boost::signals2::connection database::add_post_apply_block_handler( const apply_block_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return connect_impl(_post_apply_block_signal, func, plugin, group, "<-block");
}

boost::signals2::connection database::add_irreversible_block_handler( const irreversible_block_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return connect_impl(_on_irreversible_block, func, plugin, group, "<-irreversible");
}

boost::signals2::connection database::add_pre_reindex_handler(const reindex_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return connect_impl(_pre_reindex_signal, func, plugin, group, "->reindex");
}

boost::signals2::connection database::add_post_reindex_handler(const reindex_handler_t& func,
   const abstract_plugin& plugin, int32_t group )
{
   return connect_impl(_post_reindex_signal, func, plugin, group, "<-reindex");
}

const witness_object& database::validate_block_header( uint32_t skip, const signed_block& next_block )const
{ try {
   FC_ASSERT( head_block_id() == next_block.previous, "", ("head_block_id",head_block_id())("next.prev",next_block.previous) );
   FC_ASSERT( head_block_time() < next_block.timestamp, "", ("head_block_time",head_block_time())("next",next_block.timestamp)("blocknum",next_block.block_num()) );
   const witness_object& witness = get_witness( next_block.witness );

   if( !(skip&skip_witness_signature) )
      FC_ASSERT( next_block.validate_signee( witness.signing_key,
         fc::ecc::bip_0062 ) );

   if( !(skip&skip_witness_schedule_check) )
   {
      uint32_t slot_num = get_slot_at_time( next_block.timestamp );
      FC_ASSERT( slot_num > 0 );

      string scheduled_witness = get_scheduled_witness( slot_num );

      FC_ASSERT( witness.owner == scheduled_witness, "Witness produced block at wrong time",
                 ("block witness",next_block.witness)("scheduled",scheduled_witness)("slot_num",slot_num) );
   }

   return witness;
} FC_CAPTURE_AND_RETHROW() }

void database::create_block_summary(const signed_block& next_block)
{ try {
   block_summary_id_type sid( next_block.block_num() & 0xffff );
   modify( get< block_summary_object >( sid ), [&](block_summary_object& p) {
         p.block_id = next_block.id();
   });
} FC_CAPTURE_AND_RETHROW() }

void database::update_global_dynamic_data( const signed_block& b )
{ try {
   const dynamic_global_property_object& _dgp =
      get_dynamic_global_properties();

   uint32_t missed_blocks = 0;
   if( head_block_time() != fc::time_point_sec() )
   {
      missed_blocks = get_slot_at_time( b.timestamp );
      assert( missed_blocks != 0 );
      missed_blocks--;
      for( uint32_t i = 0; i < missed_blocks; ++i )
      {
         const auto& witness_missed = get_witness( get_scheduled_witness( i + 1 ) );
         if(  witness_missed.owner != b.witness )
         {
            modify( witness_missed, [&]( witness_object& w )
            {
               w.total_missed++;
            } );
         }
      }
   }

   // dynamic global properties updating
   modify( _dgp, [&]( dynamic_global_property_object& dgp )
   {
      // This is constant time assuming 100% participation. It is O(B) otherwise (B = Num blocks between update)
      for( uint32_t i = 0; i < missed_blocks + 1; i++ )
      {
         dgp.participation_count -= dgp.recent_slots_filled.hi & 0x8000000000000000ULL ? 1 : 0;
         dgp.recent_slots_filled = ( dgp.recent_slots_filled << 1 ) + ( i == 0 ? 1 : 0 );
         dgp.participation_count += ( i == 0 ? 1 : 0 );
      }

      dgp.head_block_number = b.block_num();
      // Following FC_ASSERT should never fail, as _currently_processing_block_id is always set by caller
      FC_ASSERT( _currently_processing_block_id.valid() );
      dgp.head_block_id = *_currently_processing_block_id;
      dgp.time = b.timestamp;
      dgp.current_aslot += missed_blocks+1;
   } );

   if( !(get_node_properties().skip_flags & skip_undo_history_check) )
   {
      MORPHENE_ASSERT( _dgp.head_block_number - _dgp.last_irreversible_block_num  < MORPHENE_MAX_UNDO_HISTORY, undo_database_exception,
                 "The database does not have enough undo history to support a blockchain with so many missed blocks. "
                 "Please add a checkpoint if you would like to continue applying blocks beyond this point.",
                 ("last_irreversible_block_num",_dgp.last_irreversible_block_num)("head", _dgp.head_block_number)
                 ("max_undo",MORPHENE_MAX_UNDO_HISTORY) );
   }
} FC_CAPTURE_AND_RETHROW() }

void database::update_signing_witness(const witness_object& signing_witness, const signed_block& new_block)
{ try {
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   uint64_t new_block_aslot = dpo.current_aslot + get_slot_at_time( new_block.timestamp );

   modify( signing_witness, [&]( witness_object& _wit )
   {
      _wit.last_aslot = new_block_aslot;
      _wit.last_confirmed_block_num = new_block.block_num();
   } );
} FC_CAPTURE_AND_RETHROW() }

void database::update_last_irreversible_block()
{ try {
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   auto old_last_irreversible = dpo.last_irreversible_block_num;
   const witness_schedule_object& wso = get_witness_schedule_object();

   vector< const witness_object* > wit_objs;
   wit_objs.reserve( wso.num_scheduled_witnesses );
   for( int i = 0; i < wso.num_scheduled_witnesses; i++ )
      wit_objs.push_back( &get_witness( wso.current_shuffled_witnesses[i] ) );

   static_assert( MORPHENE_IRREVERSIBLE_THRESHOLD > 0, "irreversible threshold must be nonzero" );

   // 1 1 1 2 2 2 2 2 2 2 -> 2     .7*10 = 7
   // 1 1 1 1 1 1 1 2 2 2 -> 1
   // 3 3 3 3 3 3 3 3 3 3 -> 3

   size_t offset = ((MORPHENE_100_PERCENT - MORPHENE_IRREVERSIBLE_THRESHOLD) * wit_objs.size() / MORPHENE_100_PERCENT);

   std::nth_element( wit_objs.begin(), wit_objs.begin() + offset, wit_objs.end(),
      []( const witness_object* a, const witness_object* b )
      {
         return a->last_confirmed_block_num < b->last_confirmed_block_num;
      } );

   uint32_t new_last_irreversible_block_num = wit_objs[offset]->last_confirmed_block_num;

   if( new_last_irreversible_block_num > dpo.last_irreversible_block_num )
   {
      modify( dpo, [&]( dynamic_global_property_object& _dpo )
      {
         _dpo.last_irreversible_block_num = new_last_irreversible_block_num;
      } );
   }

   for( uint32_t i = old_last_irreversible; i <= dpo.last_irreversible_block_num; ++i )
   {
      notify_irreversible_block( i );
   }
} FC_CAPTURE_AND_RETHROW() }

void database::migrate_irreversible_state()
{
   // This method should happen atomically. We cannot prevent unclean shutdown in the middle
   // of the call, but all side effects happen at the end to minize the chance that state
   // invariants will be violated.
   try
   {
      const dynamic_global_property_object& dpo = get_dynamic_global_properties();

      auto fork_head = _fork_db.head();
      if( fork_head )
      {
         FC_ASSERT( fork_head->num == dpo.head_block_number, "Fork Head: ${f} Chain Head: ${c}", ("f",fork_head->num)("c", dpo.head_block_number) );
      }

      if( !( get_node_properties().skip_flags & skip_block_log ) )
      {
         // output to block log based on new last irreverisible block num
         const auto& tmp_head = _block_log.head();
         uint64_t log_head_num = 0;
         vector< item_ptr > blocks_to_write;

         if( tmp_head )
            log_head_num = tmp_head->block_num();

         if( log_head_num < dpo.last_irreversible_block_num )
         {
            // Check for all blocks that we want to write out to the block log but don't write any
            // unless we are certain they all exist in the fork db
            while( log_head_num < dpo.last_irreversible_block_num )
            {
               item_ptr block_ptr = _fork_db.fetch_block_on_main_branch_by_number( log_head_num+1 );
               FC_ASSERT( block_ptr, "Current fork in the fork database does not contain the last_irreversible_block" );
               blocks_to_write.push_back( block_ptr );
               log_head_num++;
            }

            for( auto block_itr = blocks_to_write.begin(); block_itr != blocks_to_write.end(); ++block_itr )
            {
               _block_log.append( block_itr->get()->data );
            }

            _block_log.flush();
         }
      }

      // This deletes blocks from the fork db
      _fork_db.set_max_size( dpo.head_block_number - dpo.last_irreversible_block_num + 1 );

      // This deletes undo state
      commit( dpo.last_irreversible_block_num );
   }
   FC_CAPTURE_AND_RETHROW()
}

void database::clear_expired_transactions()
{
   //Look for expired transactions in the deduplication list, and remove them.
   //Transactions must have expired by at least two forking windows in order to be removed.
   auto& transaction_idx = get_index< transaction_index >();
   const auto& dedupe_index = transaction_idx.indices().get< by_expiration >();
   while( ( !dedupe_index.empty() ) && ( head_block_time() > dedupe_index.begin()->expiration ) )
      remove( *dedupe_index.begin() );
}

void database::clear_expired_delegations()
{
   auto now = head_block_time();
   const auto& delegations_by_exp = get_index< vesting_delegation_expiration_index, by_expiration >();
   auto itr = delegations_by_exp.begin();
   while( itr != delegations_by_exp.end() && itr->expiration < now )
   {
      operation vop = return_vesting_delegation_operation( itr->delegator, itr->vesting_shares );
      pre_push_virtual_operation( vop );

      modify( get_account( itr->delegator ), [&]( account_object& a )
      {

         util::manabar_params params( util::get_effective_vesting_shares( a ), MORPHENE_VOTING_MANA_REGENERATION_SECONDS );
         a.voting_manabar.regenerate_mana( params, head_block_time() );
         a.voting_manabar.use_mana( -itr->vesting_shares.amount.value );

         a.delegated_vesting_shares -= itr->vesting_shares;
      });

      post_push_virtual_operation( vop );

      remove( *itr );
      itr = delegations_by_exp.begin();
   }
}

void database::modify_balance( const account_object& a, const legacy_asset& delta, bool check_balance )
{
   modify( a, [&]( account_object& acnt )
   {
      switch( delta.symbol.asset_num )
      {
         case MORPHENE_ASSET_NUM_MORPH:
            acnt.balance += delta;
            if( check_balance )
            {
               FC_ASSERT( acnt.balance.amount.value >= 0, "Insufficient MORPH funds" );
            }
            break;
         case MORPHENE_ASSET_NUM_VESTS:
            acnt.vesting_shares += delta;
            if( check_balance )
            {
               FC_ASSERT( acnt.vesting_shares.amount.value >= 0, "Insufficient VESTS funds" );
            }
            break;
         default:
            FC_ASSERT( false, "invalid symbol" );
      }
   } );
}

void database::adjust_balance( const account_object& a, const legacy_asset& delta )
{
   modify_balance( a, delta, true );
}

void database::adjust_balance( const account_name_type& name, const legacy_asset& delta )
{
   const auto& a = get_account( name );
   modify_balance( a, delta, true );
}

void database::adjust_supply( const legacy_asset& delta, bool adjust_vesting )
{
   const auto& props = get_dynamic_global_properties();
   if( props.head_block_number < MORPHENE_BLOCKS_PER_DAY*7 )
      adjust_vesting = false;

   modify( props, [&]( dynamic_global_property_object& props )
   {
      switch( delta.symbol.asset_num )
      {
         case MORPHENE_ASSET_NUM_MORPH:
         {
            legacy_asset new_vesting( (adjust_vesting && delta.amount > 0) ? delta.amount * 9 : 0, MORPH_SYMBOL );
            props.current_supply += delta + new_vesting;
            props.total_vesting_fund_morph += new_vesting;
            FC_ASSERT( props.current_supply.amount.value >= 0 );
            break;
         }
         default:
            FC_ASSERT( false, "invalid symbol" );
      }
   } );
}


legacy_asset database::get_balance( const account_object& a, asset_symbol_type symbol )const
{
   switch( symbol.asset_num )
   {
      case MORPHENE_ASSET_NUM_MORPH:
         return a.balance;
      default:
         FC_ASSERT( false, "invalid symbol" );
   }
}

void database::init_hardforks()
{
   _hardfork_times[ 0 ] = fc::time_point_sec( MORPHENE_GENESIS_TIME );
   _hardfork_versions[ 0 ] = hardfork_version( 0, 0 );
   FC_ASSERT( MORPHENE_HARDFORK_0_1 == 1, "Invalid hardfork configuration" );
   _hardfork_times[ MORPHENE_HARDFORK_0_1 ] = fc::time_point_sec( MORPHENE_HARDFORK_0_1_TIME );
   _hardfork_versions[ MORPHENE_HARDFORK_0_1 ] = MORPHENE_HARDFORK_0_1_VERSION;
#ifdef IS_TEST_NET
   FC_ASSERT( MORPHENE_HARDFORK_0_2 == 2, "Invalid hardfork configuration" );
   _hardfork_times[ MORPHENE_HARDFORK_0_2 ] = fc::time_point_sec( MORPHENE_HARDFORK_0_2_TIME );
   _hardfork_versions[ MORPHENE_HARDFORK_0_2 ] = MORPHENE_HARDFORK_0_2_VERSION;
#endif

   const auto& hardforks = get_hardfork_property_object();
   FC_ASSERT( hardforks.last_hardfork <= MORPHENE_NUM_HARDFORKS, "Chain knows of more hardforks than configuration", ("hardforks.last_hardfork",hardforks.last_hardfork)("MORPHENE_NUM_HARDFORKS",MORPHENE_NUM_HARDFORKS) );
   FC_ASSERT( _hardfork_versions[ hardforks.last_hardfork ] <= MORPHENE_BLOCKCHAIN_VERSION, "Blockchain version is older than last applied hardfork" );
   FC_ASSERT( MORPHENE_BLOCKCHAIN_HARDFORK_VERSION >= MORPHENE_BLOCKCHAIN_VERSION );
   FC_ASSERT( MORPHENE_BLOCKCHAIN_HARDFORK_VERSION == _hardfork_versions[ MORPHENE_NUM_HARDFORKS ] );
}

void database::process_hardforks()
{
   try
   {
      // If there are upcoming hardforks and the next one is later, do nothing
      const auto& hardforks = get_hardfork_property_object();

      while( _hardfork_versions[ hardforks.last_hardfork ] < hardforks.next_hardfork
         && hardforks.next_hardfork_time <= head_block_time() )
      {
         if( hardforks.last_hardfork < MORPHENE_NUM_HARDFORKS ) {
            apply_hardfork( hardforks.last_hardfork + 1 );
         }
         else
            throw unknown_hardfork_exception();
      }
   }
   FC_CAPTURE_AND_RETHROW()
}

bool database::has_hardfork( uint32_t hardfork )const
{
   return get_hardfork_property_object().processed_hardforks.size() > hardfork;
}

uint32_t database::get_hardfork()const
{
   return get_hardfork_property_object().processed_hardforks.size() - 1;
}

void database::set_hardfork( uint32_t hardfork, bool apply_now )
{
   auto const& hardforks = get_hardfork_property_object();

   for( uint32_t i = hardforks.last_hardfork + 1; i <= hardfork && i <= MORPHENE_NUM_HARDFORKS; i++ )
   {
      modify( hardforks, [&]( hardfork_property_object& hpo )
      {
         hpo.next_hardfork = _hardfork_versions[i];
         hpo.next_hardfork_time = head_block_time();
      } );

      if( apply_now )
         apply_hardfork( i );
   }
}

void database::apply_hardfork( uint32_t hardfork )
{
   if( _log_hardforks )
      elog( "HARDFORK ${hf} at block ${b}", ("hf", hardfork)("b", head_block_num()) );
   operation hardfork_vop = hardfork_operation( hardfork );

   pre_push_virtual_operation( hardfork_vop );

   switch( hardfork )
   {
      case MORPHENE_HARDFORK_0_1:
         break;
#ifdef IS_TEST_NET
      case MORPHENE_HARDFORK_0_2:
         break;
#endif
      default:
         break;
   }

   modify( get_hardfork_property_object(), [&]( hardfork_property_object& hfp )
   {
      FC_ASSERT( hardfork == hfp.last_hardfork + 1, "Hardfork being applied out of order", ("hardfork",hardfork)("hfp.last_hardfork",hfp.last_hardfork) );
      FC_ASSERT( hfp.processed_hardforks.size() == hardfork, "Hardfork being applied out of order" );
      hfp.processed_hardforks.push_back( _hardfork_times[ hardfork ] );
      hfp.last_hardfork = hardfork;
      hfp.current_hardfork_version = _hardfork_versions[ hardfork ];
      FC_ASSERT( hfp.processed_hardforks[ hfp.last_hardfork ] == _hardfork_times[ hfp.last_hardfork ], "Hardfork processing failed sanity check..." );
   } );

   post_push_virtual_operation( hardfork_vop );
}

/**
 * Verifies all supply invariantes check out
 */
void database::validate_invariants()const
{
   try
   {
      const auto& account_idx = get_index<account_index>().indices().get<by_name>();
      legacy_asset total_supply = legacy_asset( 0, MORPH_SYMBOL );
      legacy_asset total_vesting = legacy_asset( 0, VESTS_SYMBOL );
      share_type total_vsf_votes = share_type( 0 );

      auto gpo = get_dynamic_global_properties();

      /// verify no witness has too many votes
      const auto& witness_idx = get_index< witness_index >().indices();
      for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
         FC_ASSERT( itr->votes <= gpo.total_vesting_shares.amount, "", ("itr",*itr) );

      for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
      {
         total_supply += itr->balance;
         total_vesting += itr->vesting_shares;
         total_vsf_votes += ( itr->proxy == MORPHENE_PROXY_TO_SELF_ACCOUNT ?
                                 itr->witness_vote_weight() :
                                 ( MORPHENE_MAX_PROXY_RECURSION_DEPTH > 0 ?
                                      itr->proxied_vsf_votes[MORPHENE_MAX_PROXY_RECURSION_DEPTH - 1] :
                                      itr->vesting_shares.amount ) );
      }

      const auto& escrow_idx = get_index< escrow_index >().indices().get< by_id >();

      for( auto itr = escrow_idx.begin(); itr != escrow_idx.end(); ++itr )
      {
         total_supply += itr->morph_balance;

         if( itr->pending_fee.symbol == MORPH_SYMBOL )
            total_supply += itr->pending_fee;
         else
            FC_ASSERT( false, "found escrow pending fee that is not MORPH" );
      }

      total_supply += gpo.total_vesting_fund_morph;

      FC_ASSERT( gpo.current_supply == total_supply, "", ("gpo.current_supply",gpo.current_supply)("total_supply",total_supply) );
      FC_ASSERT( gpo.total_vesting_shares == total_vesting, "", ("gpo.total_vesting_shares",gpo.total_vesting_shares)("total_vesting",total_vesting) );
      FC_ASSERT( gpo.total_vesting_shares.amount == total_vsf_votes, "", ("total_vesting_shares",gpo.total_vesting_shares)("total_vsf_votes",total_vsf_votes) );
   }
   FC_CAPTURE_LOG_AND_RETHROW( (head_block_num()) );
}

void database::retally_witness_votes()
{
   const auto& witness_idx = get_index< witness_index >().indices();

   // Clear all witness votes
   for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
   {
      modify( *itr, [&]( witness_object& w )
      {
         w.votes = 0;
         w.virtual_position = 0;
      } );
   }

   const auto& account_idx = get_index< account_index >().indices();

   // Apply all existing votes by account
   for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
   {
      if( itr->proxy != MORPHENE_PROXY_TO_SELF_ACCOUNT ) continue;

      const auto& a = *itr;

      const auto& vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
      auto wit_itr = vidx.lower_bound( boost::make_tuple( a.name, account_name_type() ) );
      while( wit_itr != vidx.end() && wit_itr->account == a.name )
      {
         adjust_witness_vote( get< witness_object, by_name >(wit_itr->witness), a.witness_vote_weight() );
         ++wit_itr;
      }
   }
}

void database::retally_witness_vote_counts( bool force )
{
   const auto& account_idx = get_index< account_index >().indices();

   // Check all existing votes by account
   for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
   {
      const auto& a = *itr;
      uint16_t witnesses_voted_for = 0;
      if( force || (a.proxy != MORPHENE_PROXY_TO_SELF_ACCOUNT  ) )
      {
        const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
        auto wit_itr = vidx.lower_bound( boost::make_tuple( a.name, account_name_type() ) );
        while( wit_itr != vidx.end() && wit_itr->account == a.name )
        {
           ++witnesses_voted_for;
           ++wit_itr;
        }
      }
      if( a.witnesses_voted_for != witnesses_voted_for )
      {
         modify( a, [&]( account_object& account )
         {
            account.witnesses_voted_for = witnesses_voted_for;
         } );
      }
   }
}

index_info::index_info() {}
index_info::~index_info() {}

} } //morphene::chain
