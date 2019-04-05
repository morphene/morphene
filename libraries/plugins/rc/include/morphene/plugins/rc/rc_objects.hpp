#pragma once
#include <morphene/chain/util/manabar.hpp>

#include <morphene/plugins/rc/rc_utility.hpp>
#include <morphene/plugins/rc/resource_count.hpp>

#include <morphene/chain/morphene_object_types.hpp>

#include <fc/int_array.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace morphene { namespace chain {
struct by_account;
} }

namespace morphene { namespace plugins { namespace rc {

using namespace std;
using namespace morphene::chain;

#ifndef MORPHENE_RC_SPACE_ID
#define MORPHENE_RC_SPACE_ID 16
#endif

#define MORPHENE_RC_DRC_FLOAT_LEVEL   (20*MORPHENE_1_PERCENT)
#define MORPHENE_RC_MAX_DRC_RATE      1000

enum rc_object_types
{
   rc_resource_param_object_type   = ( MORPHENE_RC_SPACE_ID << 8 ),
   rc_pool_object_type             = ( MORPHENE_RC_SPACE_ID << 8 ) + 1,
   rc_account_object_type          = ( MORPHENE_RC_SPACE_ID << 8 ) + 2,
   rc_delegation_pool_object_type  = ( MORPHENE_RC_SPACE_ID << 8 ) + 3,
   rc_indel_edge_object_type       = ( MORPHENE_RC_SPACE_ID << 8 ) + 4,
   rc_outdel_drc_edge_object_type  = ( MORPHENE_RC_SPACE_ID << 8 ) + 5
};

class rc_resource_param_object : public object< rc_resource_param_object_type, rc_resource_param_object >
{
   public:
      template< typename Constructor, typename Allocator >
      rc_resource_param_object( Constructor&& c, allocator< Allocator > a )
      {
         c( *this );
      }

      id_type               id;
      fc::int_array< rc_resource_params, MORPHENE_NUM_RESOURCE_TYPES >
                            resource_param_array;
};

class rc_pool_object : public object< rc_pool_object_type, rc_pool_object >
{
   public:
      template< typename Constructor, typename Allocator >
      rc_pool_object( Constructor&& c, allocator< Allocator > a )
      {
         c( *this );
      }

      id_type               id;
      fc::int_array< int64_t, MORPHENE_NUM_RESOURCE_TYPES >
                            pool_array;
};

class rc_account_object : public object< rc_account_object_type, rc_account_object >
{
   public:
      template< typename Constructor, typename Allocator >
      rc_account_object( Constructor&& c, allocator< Allocator > a )
      {
         c( *this );
      }

      id_type               id;

      account_name_type     account;
      morphene::chain::util::manabar   rc_manabar;
      legacy_asset                 max_rc_creation_adjustment = legacy_asset( 0, VESTS_SYMBOL );

      // This is used for bug-catching, to match that the vesting shares in a
      // pre-op are equal to what they were at the last post-op.
      int64_t               last_max_rc = 0;
};

/**
 * Represents a delegation pool.
 */
class rc_delegation_pool_object : public object< rc_delegation_pool_object_type, rc_delegation_pool_object >
{
   public:
      template< typename Constructor, typename Allocator >
      rc_delegation_pool_object( Constructor&& c, allocator< Allocator > a )
      {
         c( *this );
      }

      id_type                       id;

      account_name_type             account;
      morphene::chain::util::manabar   rc_pool_manabar;
};

/**
 * Represents a delegation from a user to a pool.
 */
class rc_indel_edge_object : public object< rc_indel_edge_object_type, rc_indel_edge_object >
{
   public:
      template< typename Constructor, typename Allocator >
      rc_indel_edge_object( Constructor&& c, allocator< Allocator > a )
      {
         c( *this );
      }

      id_type                       id;
      account_name_type             from_account;
      account_name_type             to_pool;
      share_type                    amount;
};

/**
 * Represents a delegation from a pool to a user based on delegated resource credits (DRC).
 *
 * In the case of a pool that is not under heavy load, DRC:RC has a 1:1 exchange rate.
 *
 * However, if the pool drops below MORPHENE_RC_DRC_FLOAT_LEVEL, DRC:RC exchange rate starts
 * to rise according to `f(x) = 1/(a+b*x)` where `x` is the pool level, and coefficients `a`,
 * `b` are set such that `f(MORPHENE_RC_DRC_FLOAT_LEVEL) = 1` and `f(0) = MORPHENE_RC_MAX_DRC_RATE`.
 *
 * This ensures the limited RC of oversubscribed pools under heavy load are
 * shared "fairly" among their users proportionally to DRC.  This logic
 * provides a smooth transition between the "fiat regime" (i.e. DRC represent
 * a direct allocation of RC) and the "proportional regime" (i.e. DRC represent
 * the fraction of RC that the user is allowed).
 */
class rc_outdel_drc_edge_object : public object< rc_outdel_drc_edge_object_type, rc_outdel_drc_edge_object >
{
   public:
      template< typename Constructor, typename Allocator >
      rc_outdel_drc_edge_object( Constructor&& c, allocator< Allocator > a )
      {
         c( *this );
      }

      id_type                       id;
      account_name_type             from_pool;
      account_name_type             to_account;
      morphene::chain::util::manabar   drc_manabar;
      int64_t                       drc_max_mana = 0;
};

int64_t get_maximum_rc( const morphene::chain::account_object& account, const rc_account_object& rc_account );

using namespace boost::multi_index;

struct by_edge;

typedef multi_index_container<
   rc_resource_param_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< rc_resource_param_object, rc_resource_param_object::id_type, &rc_resource_param_object::id > >
   >,
   allocator< rc_resource_param_object >
> rc_resource_param_index;

typedef multi_index_container<
   rc_pool_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< rc_pool_object, rc_pool_object::id_type, &rc_pool_object::id > >
   >,
   allocator< rc_pool_object >
> rc_pool_index;

typedef multi_index_container<
   rc_account_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< rc_account_object, rc_account_object::id_type, &rc_account_object::id > >,
      ordered_unique< tag< by_name >, member< rc_account_object, account_name_type, &rc_account_object::account > >
   >,
   allocator< rc_account_object >
> rc_account_index;

typedef multi_index_container<
   rc_delegation_pool_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< rc_delegation_pool_object, rc_delegation_pool_object::id_type, &rc_delegation_pool_object::id > >,
      ordered_unique< tag< by_account >, member< rc_delegation_pool_object, account_name_type, &rc_delegation_pool_object::account > >
   >,
   allocator< rc_delegation_pool_object >
> rc_delegation_pool_index;

typedef multi_index_container<
   rc_indel_edge_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< rc_indel_edge_object, rc_indel_edge_object::id_type, &rc_indel_edge_object::id > >,
      ordered_unique< tag< by_edge >,
            composite_key< rc_indel_edge_object,
               member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::from_account >,
               member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::to_pool >
            >
      >
   >,
   allocator< rc_indel_edge_object >
> rc_indel_edge_index;

typedef multi_index_container<
   rc_outdel_drc_edge_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< rc_outdel_drc_edge_object, rc_outdel_drc_edge_object::id_type, &rc_outdel_drc_edge_object::id > >,
      ordered_unique< tag< by_edge >,
            composite_key< rc_outdel_drc_edge_object,
               member< rc_outdel_drc_edge_object, account_name_type, &rc_outdel_drc_edge_object::from_pool >,
               member< rc_outdel_drc_edge_object, account_name_type, &rc_outdel_drc_edge_object::to_account >
            >
      >
   >,
   allocator< rc_outdel_drc_edge_object >
> rc_outdel_drc_edge_index;

} } } // morphene::plugins::rc

FC_REFLECT( morphene::plugins::rc::rc_resource_param_object, (id)(resource_param_array) )
CHAINBASE_SET_INDEX_TYPE( morphene::plugins::rc::rc_resource_param_object, morphene::plugins::rc::rc_resource_param_index )

FC_REFLECT( morphene::plugins::rc::rc_pool_object, (id)(pool_array) )
CHAINBASE_SET_INDEX_TYPE( morphene::plugins::rc::rc_pool_object, morphene::plugins::rc::rc_pool_index )

FC_REFLECT( morphene::plugins::rc::rc_account_object,
   (id)
   (account)
   (rc_manabar)
   (max_rc_creation_adjustment)
   (last_max_rc)
   )
CHAINBASE_SET_INDEX_TYPE( morphene::plugins::rc::rc_account_object, morphene::plugins::rc::rc_account_index )

FC_REFLECT( morphene::plugins::rc::rc_delegation_pool_object,
   (id)
   (account)
   (rc_pool_manabar)
   )
CHAINBASE_SET_INDEX_TYPE( morphene::plugins::rc::rc_delegation_pool_object, morphene::plugins::rc::rc_delegation_pool_index )

FC_REFLECT( morphene::plugins::rc::rc_indel_edge_object,
   (id)
   (from_account)
   (to_pool)
   (amount)
   )
CHAINBASE_SET_INDEX_TYPE( morphene::plugins::rc::rc_indel_edge_object, morphene::plugins::rc::rc_indel_edge_index )

FC_REFLECT( morphene::plugins::rc::rc_outdel_drc_edge_object,
   (id)
   (from_pool)
   (to_account)
   (drc_manabar)
   (drc_max_mana)
   )
CHAINBASE_SET_INDEX_TYPE( morphene::plugins::rc::rc_outdel_drc_edge_object, morphene::plugins::rc::rc_outdel_drc_edge_index )
