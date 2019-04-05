#include <morphene/app/plugin.hpp>

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
#ifndef MORPHENE_ACCOUNT_STATISTICS_SPACE_ID
#define MORPHENE_ACCOUNT_STATISTICS_SPACE_ID 10
#endif

#ifndef MORPHENE_ACCOUNT_STATISTICS_PLUGIN_NAME
#define MORPHENE_ACCOUNT_STATISTICS_PLUGIN_NAME "account_stats"
#endif

namespace morphene { namespace account_statistics {

using namespace chain;
using app::application;

enum account_statistics_plugin_object_types
{
   account_stats_bucket_object_type    = ( MORPHENE_ACCOUNT_STATISTICS_SPACE_ID << 8 ),
   account_activity_bucket_object_type = ( MORPHENE_ACCOUNT_STATISTICS_SPACE_ID << 8 ) + 1
};

struct account_stats_bucket_object : public object< account_stats_bucket_object_type, account_stats_bucket_object >
{
   template< typename Constructor, typename Allocator >
   account_stats_bucket_object( Constructor&& c, allocator< Allocator > a )
   {
      c( *this );
   }

   account_stats_bucket_object() {}

   id_type              id;

   fc::time_point_sec   open;                                     ///< Open time of the bucket
   uint32_t             seconds = 0;                              ///< Seconds accounted for in the bucket
   account_name_type    name;                                     ///< Account name
   uint32_t             transactions = 0;                         ///< Transactions this account signed
   uint32_t             total_ops = 0;                            ///< Ops this account was an authority on
   uint32_t             forum_ops = 0;                            ///< Forum operations
   uint32_t             transfers_to = 0;                         ///< Account to account transfers to this account
   uint32_t             transfers_from = 0;                       ///< Account to account transfers from this account
   share_type           morph_sent = 0;                           ///< MORPH sent from this account
   share_type           morph_received = 0;                       ///< MORPH received by this account
   uint32_t             transfers_to_vesting = 0;                 ///< Transfers to vesting by this account. Note: Transfer to vesting from A to B counts as a transfer from A to B followed by a vesting deposit by B.
   share_type           morph_vested = 0;                         ///< MORPH vested by the account
   share_type           new_vests = 0;                            ///< New VESTS by vesting transfers
   uint32_t             new_vesting_withdrawal_requests = 0;      ///< New vesting withdrawal requests
   uint32_t             modified_vesting_withdrawal_requests = 0; ///< Changes to vesting withdraw requests
   uint32_t             vesting_withdrawals_processed = 0;        ///< Vesting withdrawals processed for this account
   uint32_t             finished_vesting_withdrawals = 0;         ///< Processed vesting withdrawals that are now finished
   share_type           vests_withdrawn = 0;                      ///< VESTS withdrawn from the account
   share_type           morph_received_from_withdrawls = 0;       ///< MORPH received from this account's vesting withdrawals
   share_type           morph_received_from_routes = 0;           ///< MORPH received from another account's vesting withdrawals
   share_type           vests_received_from_routes = 0;           ///< VESTS received from another account's vesting withdrawals
};

typedef account_stats_bucket_object::id_type account_stats_bucket_id_type;

struct account_activity_bucket_object : public object< account_activity_bucket_object_type, account_activity_bucket_object >
{
   template< typename Constructor, typename Allocator >
   account_activity_bucket_object( Constructor&& c, allocator< Allocator > a )
   {
      c( *this );
   }

   account_activity_bucket_object() {}

   id_type              id;

   fc::time_point_sec   open;                                  ///< Open time for the bucket
   uint32_t             seconds = 0;                           ///< Seconds accounted for in the bucket
   uint32_t             active_forum_accounts = 0;             ///< Active forum accounts in the bucket
};

typedef account_activity_bucket_object::id_type account_activity_bucket_id_type;

namespace detail
{
   class account_statistics_plugin_impl;
}

class account_statistics_plugin : public morphene::app::plugin
{
   public:
      account_statistics_plugin( application* app );
      virtual ~account_statistics_plugin();

      virtual std::string plugin_name()const override { return MORPHENE_ACCOUNT_STATISTICS_PLUGIN_NAME; }
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg ) override;
      virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
      virtual void plugin_startup() override;

      const flat_set< uint32_t >& get_tracked_buckets() const;
      uint32_t get_max_history_per_bucket() const;
      const flat_set< std::string >& get_tracked_accounts() const;

   private:
      friend class detail::account_statistics_plugin_impl;
      std::unique_ptr< detail::account_statistics_plugin_impl > _my;
};

} } // morphene::account_statistics

FC_REFLECT( morphene::account_statistics::account_stats_bucket_object,
   (id)
   (open)
   (seconds)
   (name)
   (transactions)
   (total_ops)
   (forum_ops)
   (transfers_to)
   (transfers_from)
   (morph_sent)
   (morph_received)
   (transfers_to_vesting)
   (morph_vested)
   (new_vests)
   (new_vesting_withdrawal_requests)
   (modified_vesting_withdrawal_requests)
   (vesting_withdrawals_processed)
   (finished_vesting_withdrawals)
   (vests_withdrawn)
   (morph_received_from_withdrawls)
   (morph_received_from_routes)
   (vests_received_from_routes) )
//SET_INDEX_TYPE( morphene::account_statistics::account_stats_bucket_object,)

FC_REFLECT(
   morphene::account_statistics::account_activity_bucket_object,
   (id)
   (open)
   (seconds)
   (active_forum_accounts)
)
