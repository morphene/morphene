#pragma once
#include <fc/uint128.hpp>

#include <morphene/chain/morphene_object_types.hpp>

#include <morphene/protocol/asset.hpp>

namespace morphene { namespace chain {

   using morphene::protocol::asset;
   using morphene::protocol::legacy_asset;
   using morphene::protocol::price;

   /**
    * @class dynamic_global_property_object
    * @brief Maintains global state information
    * @ingroup object
    * @ingroup implementation
    *
    * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
    * current values of global blockchain properties.
    */
   class dynamic_global_property_object : public object< dynamic_global_property_object_type, dynamic_global_property_object>
   {
      public:
         template< typename Constructor, typename Allocator >
         dynamic_global_property_object( Constructor&& c, allocator< Allocator > a )
         {
            c( *this );
         }

         dynamic_global_property_object(){}

         id_type           id;

         uint32_t          head_block_number = 0;
         block_id_type     head_block_id;
         time_point_sec    time;
         account_name_type current_witness;

         /**
          *  The total POW accumulated, aka the sum of num_pow_witness at the time new POW is added
          */
         uint64_t total_pow = -1;

         /**
          * The current count of how many pending POW witnesses there are, determines the difficulty
          * of doing pow
          */
         uint32_t num_pow_witnesses = 0;

         legacy_asset       current_supply             = legacy_asset( 0, MORPH_SYMBOL );
         legacy_asset       total_vesting_fund_morph   = legacy_asset( 0, MORPH_SYMBOL );
         legacy_asset       total_vesting_shares       = legacy_asset( 0, VESTS_SYMBOL );

         price       get_vesting_share_price() const
         {
            if ( total_vesting_fund_morph.amount == 0 || total_vesting_shares.amount == 0 )
               return price ( legacy_asset( 1000, MORPH_SYMBOL ), legacy_asset( 1000000, VESTS_SYMBOL ) );

            return price( total_vesting_shares, total_vesting_fund_morph );
         }

         /**
          *  Maximum block size is decided by the set of active witnesses which change every round.
          *  Each witness posts what they think the maximum size should be as part of their witness
          *  properties, the median size is chosen to be the maximum block size for the round.
          *
          *  @note the minimum value for maximum_block_size is defined by the protocol to prevent the
          *  network from getting stuck by witnesses attempting to set this too low.
          */
         uint32_t     maximum_block_size = 0;

         /**
          * The current absolute slot number.  Equal to the total
          * number of slots since genesis.  Also equal to the total
          * number of missed slots plus head_block_number.
          */
         uint64_t      current_aslot = 0;

         /**
          * used to compute witness participation.
          */
         fc::uint128_t recent_slots_filled;
         uint8_t       participation_count = 0; ///< Divide by 128 to compute participation percentage

         uint32_t last_irreversible_block_num = 0;

         uint32_t delegation_return_period = MORPHENE_DELEGATION_RETURN_PERIOD;

         int64_t available_account_subsidies = 0;
   };

   typedef multi_index_container<
      dynamic_global_property_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            member< dynamic_global_property_object, dynamic_global_property_object::id_type, &dynamic_global_property_object::id > >
      >,
      allocator< dynamic_global_property_object >
   > dynamic_global_property_index;

} } // morphene::chain

FC_REFLECT( morphene::chain::dynamic_global_property_object,
             (id)
             (head_block_number)
             (head_block_id)
             (time)
             (current_witness)
             (total_pow)
             (num_pow_witnesses)
             (current_supply)
             (total_vesting_fund_morph)
             (total_vesting_shares)
             (maximum_block_size)
             (current_aslot)
             (recent_slots_filled)
             (participation_count)
             (last_irreversible_block_num)
             (delegation_return_period)
             (available_account_subsidies)
          )
CHAINBASE_SET_INDEX_TYPE( morphene::chain::dynamic_global_property_object, morphene::chain::dynamic_global_property_index )
