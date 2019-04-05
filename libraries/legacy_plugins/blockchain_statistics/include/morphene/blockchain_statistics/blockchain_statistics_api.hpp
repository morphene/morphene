#pragma once

#include <morphene/blockchain_statistics/blockchain_statistics_plugin.hpp>

#include <fc/api.hpp>

namespace morphene { namespace app {
   struct api_context;
} }

namespace morphene { namespace blockchain_statistics {

namespace detail
{
   class blockchain_statistics_api_impl;
}

struct statistics
{
   uint32_t             blocks = 0;                                  ///< Blocks produced
   uint32_t             bandwidth = 0;                               ///< Bandwidth in bytes
   uint32_t             operations = 0;                              ///< Operations evaluated
   uint32_t             transactions = 0;                            ///< Transactions processed
   uint32_t             transfers = 0;                               ///< Account to account transfers
   share_type           morph_transferred = 0;                       ///< MORPH transferred from account to account
   uint32_t             accounts_created = 0;                        ///< Total accounts created
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
   share_type           morph_converted = 0;                         ///< Amount of MORPH that was converted

   statistics& operator += ( const bucket_object& b );
};

class blockchain_statistics_api
{
   public:
      blockchain_statistics_api( const morphene::app::api_context& ctx );

      void on_api_startup();

      /**
      * @brief Gets statistics over the time window length, interval, that contains time, open.
      * @param open The opening time, or a time contained within the window.
      * @param interval The size of the window for which statistics were aggregated.
      * @returns Statistics for the window.
      */
      statistics get_stats_for_time( fc::time_point_sec open, uint32_t interval )const;

      /**
      * @brief Aggregates statistics over a time interval.
      * @param start The beginning time of the window.
      * @param stop The end time of the window. stop must take place after start.
      * @returns Aggregated statistics over the interval.
      */
      statistics get_stats_for_interval( fc::time_point_sec start, fc::time_point_sec end )const;

      /**
       * @brief Returns lifetime statistics.
       */
      statistics get_lifetime_stats()const;

   private:
      std::shared_ptr< detail::blockchain_statistics_api_impl > my;
};

} } // morphene::blockchain_statistics

FC_REFLECT( morphene::blockchain_statistics::statistics,
   (blocks)
   (bandwidth)
   (operations)
   (transactions)
   (transfers)
   (morph_transferred)
   (accounts_created)
   (paid_accounts_created)
   (transfers_to_vesting)
   (morph_vested)
   (new_vesting_withdrawal_requests)
   (modified_vesting_withdrawal_requests)
   (vesting_withdraw_rate_delta)
   (vesting_withdrawals_processed)
   (finished_vesting_withdrawals)
   (vests_withdrawn)
   (vests_transferred)
   (morph_converted) )


FC_API( morphene::blockchain_statistics::blockchain_statistics_api,
   (get_stats_for_time)
   (get_stats_for_interval)
   (get_lifetime_stats)
)
