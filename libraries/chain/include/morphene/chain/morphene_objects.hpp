#pragma once

#include <morphene/protocol/base.hpp>
#include <morphene/protocol/authority.hpp>
#include <morphene/protocol/morphene_operations.hpp>
#include <morphene/protocol/misc_utilities.hpp>

#include <morphene/chain/morphene_object_types.hpp>

#include <boost/multi_index/composite_key.hpp>
#include <boost/multiprecision/cpp_int.hpp>


namespace morphene { namespace chain {

   using morphene::protocol::asset;
   using morphene::protocol::legacy_asset;
   using morphene::protocol::price;
   using morphene::protocol::asset_symbol_type;
   using chainbase::t_deque;

   class escrow_object : public object< escrow_object_type, escrow_object >
   {
      public:
         template< typename Constructor, typename Allocator >
         escrow_object( Constructor&& c, allocator< Allocator > a )
         {
            c( *this );
         }

         escrow_object(){}

         id_type           id;

         uint32_t          escrow_id = 20;
         account_name_type from;
         account_name_type to;
         account_name_type agent;
         time_point_sec    ratification_deadline;
         time_point_sec    escrow_expiration;
         legacy_asset      morph_balance;
         legacy_asset      pending_fee;
         bool              to_approved = false;
         bool              agent_approved = false;
         bool              disputed = false;

         bool              is_approved()const { return to_approved && agent_approved; }
   };


   /**
    * @breif a route to send withdrawn vesting shares.
    */
   class withdraw_vesting_route_object : public object< withdraw_vesting_route_object_type, withdraw_vesting_route_object >
   {
      public:
         template< typename Constructor, typename Allocator >
         withdraw_vesting_route_object( Constructor&& c, allocator< Allocator > a )
         {
            c( *this );
         }

         withdraw_vesting_route_object(){}

         id_type  id;

         account_name_type from_account;
         account_name_type to_account;
         uint16_t          percent = 0;
         bool              auto_vest = false;
   };

   class auction_object : public object< auction_object_type, auction_object >
   {
      public:
         template< typename Constructor, typename Allocator >
         auction_object( Constructor&& c, allocator< Allocator > a )
         {
            c( *this );
         }

         auction_object(){}

         id_type           id;

         account_name_type consigner;
         string            permlink;
         string            status = "pending";
         time_point_sec    start_time = fc::time_point_sec::min();
         time_point_sec    end_time = fc::time_point_sec::maximum();
         uint32_t          bids_count = 0;
         legacy_asset      total_payout = legacy_asset( 0, MORPH_SYMBOL );
         account_name_type last_bidder;
         legacy_asset      fee = legacy_asset( 0, MORPH_SYMBOL );
         time_point_sec    created = fc::time_point_sec::min();
         time_point_sec    last_paid = fc::time_point_sec::min();
         time_point_sec    last_updated = fc::time_point_sec::min();
   };

   class bid_object : public object< bid_object_type, bid_object >
   {
      public:
         template< typename Constructor, typename Allocator >
         bid_object( Constructor&& c, allocator< Allocator > a )
         {
            c( *this );
         }

         bid_object(){}

         id_type           id;

         account_name_type bidder;
         string            permlink;
         time_point_sec    created;
   };

   struct by_withdraw_route;
   struct by_destination;
   typedef multi_index_container<
      withdraw_vesting_route_object,
      indexed_by<
         ordered_unique< tag< by_id >, member< withdraw_vesting_route_object, withdraw_vesting_route_id_type, &withdraw_vesting_route_object::id > >,
         ordered_unique< tag< by_withdraw_route >,
            composite_key< withdraw_vesting_route_object,
               member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::from_account >,
               member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
         >,
         ordered_unique< tag< by_destination >,
            composite_key< withdraw_vesting_route_object,
               member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >,
               member< withdraw_vesting_route_object, withdraw_vesting_route_id_type, &withdraw_vesting_route_object::id >
            >
         >
      >,
      allocator< withdraw_vesting_route_object >
   > withdraw_vesting_route_index;

   struct by_from_id;
   struct by_ratification_deadline;
   typedef multi_index_container<
      escrow_object,
      indexed_by<
         ordered_unique< tag< by_id >, member< escrow_object, escrow_id_type, &escrow_object::id > >,
         ordered_unique< tag< by_from_id >,
            composite_key< escrow_object,
               member< escrow_object, account_name_type,  &escrow_object::from >,
               member< escrow_object, uint32_t, &escrow_object::escrow_id >
            >
         >,
         ordered_unique< tag< by_ratification_deadline >,
            composite_key< escrow_object,
               const_mem_fun< escrow_object, bool, &escrow_object::is_approved >,
               member< escrow_object, time_point_sec, &escrow_object::ratification_deadline >,
               member< escrow_object, escrow_id_type, &escrow_object::id >
            >,
            composite_key_compare< std::less< bool >, std::less< time_point_sec >, std::less< escrow_id_type > >
         >
      >,
      allocator< escrow_object >
   > escrow_index;

   struct by_permlink;
   struct by_status;
   typedef multi_index_container<
      auction_object,
      indexed_by<
         ordered_unique< tag< by_id >, member< auction_object, auction_id_type, &auction_object::id > >,
         ordered_unique< tag< by_permlink >, member< auction_object, string, &auction_object::permlink > >,
         ordered_non_unique< tag< by_status >, member< auction_object, string, &auction_object::status > >
      >,
      allocator< auction_object >
   > auction_index;


   struct by_bidder;
   struct by_permlink;
   typedef multi_index_container<
      bid_object,
      indexed_by<
         ordered_unique< tag< by_id >, member< bid_object, bid_id_type, &bid_object::id > >,
         ordered_non_unique< tag< by_bidder >, member< bid_object, account_name_type, &bid_object::bidder > >,
         ordered_non_unique< tag< by_permlink >, member< bid_object, string, &bid_object::permlink > >
      >,
      allocator< bid_object >
   > bid_index;

} } // morphene::chain

#include <morphene/chain/account_object.hpp>

FC_REFLECT( morphene::chain::withdraw_vesting_route_object,
             (id)(from_account)(to_account)(percent)(auto_vest) )
CHAINBASE_SET_INDEX_TYPE( morphene::chain::withdraw_vesting_route_object, morphene::chain::withdraw_vesting_route_index )

FC_REFLECT( morphene::chain::escrow_object,
             (id)(escrow_id)(from)(to)(agent)
             (ratification_deadline)(escrow_expiration)
             (morph_balance)(pending_fee)
             (to_approved)(agent_approved)(disputed) )
CHAINBASE_SET_INDEX_TYPE( morphene::chain::escrow_object, morphene::chain::escrow_index )

FC_REFLECT( morphene::chain::auction_object,
             (id)(consigner)(permlink)(status)
             (start_time)(end_time)(bids_count)(total_payout)(fee)
             (created)(last_paid)(last_updated) )
CHAINBASE_SET_INDEX_TYPE( morphene::chain::auction_object, morphene::chain::auction_index )

FC_REFLECT( morphene::chain::bid_object,
             (id)(bidder)(permlink)(created) )
CHAINBASE_SET_INDEX_TYPE( morphene::chain::bid_object, morphene::chain::bid_index )
