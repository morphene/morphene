#include <morphene/plugins/account_history_api/account_history_api_plugin.hpp>
#include <morphene/plugins/account_history_api/account_history_api.hpp>

namespace morphene { namespace plugins { namespace account_history {

namespace detail {

class abstract_account_history_api_impl
{
   public:
      abstract_account_history_api_impl() : _db( appbase::app().get_plugin< morphene::plugins::chain::chain_plugin >().db() ) {}
      virtual ~abstract_account_history_api_impl() {}

      virtual get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) = 0;
      virtual get_transaction_return get_transaction( const get_transaction_args& ) = 0;
      virtual get_account_history_return get_account_history( const get_account_history_args& ) = 0;
      virtual enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) = 0;

      chain::database& _db;
};

class account_history_api_chainbase_impl : public abstract_account_history_api_impl
{
   public:
      account_history_api_chainbase_impl() : abstract_account_history_api_impl() {}
      ~account_history_api_chainbase_impl() {}

      get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) override;
      get_transaction_return get_transaction( const get_transaction_args& ) override;
      get_account_history_return get_account_history( const get_account_history_args& ) override;
      enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) override;
};

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_ops_in_block )
{
   return _db.with_read_lock( [&]()
   {
      const auto& idx = _db.get_index< chain::operation_index, chain::by_location >();
      auto itr = idx.lower_bound( args.block_num );

      get_ops_in_block_return result;

      while( itr != idx.end() && itr->block == args.block_num )
      {
         api_operation_object temp = *itr;
         if( !args.only_virtual || is_virtual_operation( temp.op ) )
            result.ops.emplace( std::move( temp ) );
         ++itr;
      }

      return result;
   });
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_transaction )
{
#ifdef SKIP_BY_TX_ID
   FC_ASSERT( false, "This node's operator has disabled operation indexing by transaction_id" );
#else

   return _db.with_read_lock( [&]()
   {
      get_transaction_return result;

      const auto& idx = _db.get_index< chain::operation_index, chain::by_transaction_id >();
      auto itr = idx.lower_bound( args.id );
      if( itr != idx.end() && itr->trx_id == args.id )
      {
         auto blk = _db.fetch_block_by_number( itr->block );
         FC_ASSERT( blk.valid() );
         FC_ASSERT( blk->transactions.size() > itr->trx_in_block );
         result = blk->transactions[itr->trx_in_block];
         result.block_num       = itr->block;
         result.transaction_num = itr->trx_in_block;
      }
      else
      {
         FC_ASSERT( false, "Unknown Transaction ${t}", ("t",args.id) );
      }

      return result;
   });
#endif
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_account_history )
{
   FC_ASSERT( args.limit <= 10000, "limit of ${l} is greater than maxmimum allowed", ("l",args.limit) );
   FC_ASSERT( args.start >= args.limit, "start must be greater than limit" );

   return _db.with_read_lock( [&]()
   {
      const auto& idx = _db.get_index< chain::account_history_index, chain::by_account >();
      auto itr = idx.lower_bound( boost::make_tuple( args.account, args.start ) );
      uint32_t n = 0;

      get_account_history_return result;
      while( true )
      {
         if( itr == idx.end() )
            break;
         if( itr->account != args.account )
            break;
         if( n >= args.limit )
            break;
         result.history[ itr->sequence ] = _db.get( itr->op );
         ++itr;
         ++n;
      }

      return result;
   });
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, enum_virtual_ops )
{
   FC_ASSERT( false, "This API is not supported for account history backed by Chainbase" );
}

} // detail

account_history_api::account_history_api()
{
   auto ah_cb = appbase::app().find_plugin< morphene::plugins::account_history::account_history_plugin >();

   if( ah_cb != nullptr )
   {
      my = std::make_unique< detail::account_history_api_chainbase_impl >();
   }
   else
   {
      FC_ASSERT( false, "Account History API only works if account_history plugin is enabled" );
   }

   JSON_RPC_REGISTER_API( MORPHENE_ACCOUNT_HISTORY_API_PLUGIN_NAME );
}

account_history_api::~account_history_api() {}

DEFINE_LOCKLESS_APIS( account_history_api ,
   (get_ops_in_block)
   (get_transaction)
   (get_account_history)
   (enum_virtual_ops)
)

} } } // morphene::plugins::account_history
