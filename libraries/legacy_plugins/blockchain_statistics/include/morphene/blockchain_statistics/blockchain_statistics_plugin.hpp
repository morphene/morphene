#pragma once
#include <morphene/app/plugin.hpp>

#include <morphene/chain/morphene_object_types.hpp>

#include <boost/multi_index/composite_key.hpp>

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef MORPHENE_BLOCKCHAIN_STATISTICS_SPACE_ID
#define MORPHENE_BLOCKCHAIN_STATISTICS_SPACE_ID 9
#endif

#ifndef MORPHENE_BLOCKCHAIN_STATISTICS_PLUGIN_NAME
#define MORPHENE_BLOCKCHAIN_STATISTICS_PLUGIN_NAME "chain_stats"
#endif

namespace morphene { namespace blockchain_statistics {

using namespace morphene::chain;
using app::application;

enum blockchain_statistics_object_type
{
   bucket_object_type = ( MORPHENE_BLOCKCHAIN_STATISTICS_SPACE_ID << 8 )
};

namespace detail
{
   class blockchain_statistics_plugin_impl;
}

class blockchain_statistics_plugin : public morphene::app::plugin
{
   public:
      blockchain_statistics_plugin( application* app );
      virtual ~blockchain_statistics_plugin();

      virtual std::string plugin_name()const override { return MORPHENE_BLOCKCHAIN_STATISTICS_PLUGIN_NAME; }
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg ) override;
      virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
      virtual void plugin_startup() override;

      const flat_set< uint32_t >& get_tracked_buckets() const;
      uint32_t get_max_history_per_bucket() const;

   private:
      friend class detail::blockchain_statistics_plugin_impl;
      std::unique_ptr< detail::blockchain_statistics_plugin_impl > _my;
};

struct bucket_object : public object< bucket_object_type, bucket_object >
{
   template< typename Constructor, typename Allocator >
   bucket_object( Constructor&& c, allocator< Allocator > a )
   {
      c( *this );
   }

   id_type              id;

   fc::time_point_sec   open;                                        ///< Open time of the bucket
   uint32_t             seconds = 0;                                 ///< Seconds accounted for in the bucket
   uint32_t             blocks = 0;                                  ///< Blocks produced
   uint32_t             bandwidth = 0;                               ///< Bandwidth in bytes
   uint32_t             operations = 0;                              ///< Operations evaluated
   uint32_t             transactions = 0;                            ///< Transactions processed
   uint32_t             transfers = 0;                               ///< Account to account transfers
   share_type           morph_transferred = 0;                       ///< MORPH transferred from account to account
   uint32_t             paid_accounts_created = 0;                   ///< Accounts created with fee
   uint32_t             transfers_to_vesting = 0;                    ///< Transfers of MORPH into VESTS
   share_type           morph_vested = 0;                            ///< Ammount of MORPH vested
   uint32_t             new_vesting_withdrawal_requests = 0;         ///< New vesting withdrawal requests
   uint32_t             modified_vesting_withdrawal_requests = 0;    ///< Changes to vesting withdrawal requests
   share_type           vesting_withdraw_rate_delta = 0;
   uint32_t             vesting_withdrawals_processed = 0;           ///< Number of vesting withdrawals
   uint32_t             finished_vesting_withdrawals = 0;            ///< Processed vesting withdrawals that are now finished
   share_type           vests_withdrawn = 0;                         ///< Ammount of VESTS withdrawn to MORPH
   share_type           vests_transferred = 0;                       ///< Ammount of VESTS transferred to another account
};

typedef oid< bucket_object > bucket_id_type;

struct by_id;
struct by_bucket;
typedef multi_index_container<
   bucket_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< bucket_object, bucket_id_type, &bucket_object::id > >,
      ordered_unique< tag< by_bucket >,
         composite_key< bucket_object,
            member< bucket_object, uint32_t, &bucket_object::seconds >,
            member< bucket_object, fc::time_point_sec, &bucket_object::open >
         >
      >
   >,
   allocator< bucket_object >
> bucket_index;

} } // morphene::blockchain_statistics

FC_REFLECT( morphene::blockchain_statistics::bucket_object,
   (id)
   (open)
   (seconds)
   (blocks)
   (bandwidth)
   (operations)
   (transactions)
   (transfers)
   (morph_transferred)
   (paid_accounts_created)
   (transfers_to_vesting)
   (morph_vested)
   (new_vesting_withdrawal_requests)
   (modified_vesting_withdrawal_requests)
   (vesting_withdraw_rate_delta)
   (vesting_withdrawals_processed)
   (finished_vesting_withdrawals)
   (vests_withdrawn)
   (vests_transferred) )
CHAINBASE_SET_INDEX_TYPE( morphene::blockchain_statistics::bucket_object, morphene::blockchain_statistics::bucket_index )
