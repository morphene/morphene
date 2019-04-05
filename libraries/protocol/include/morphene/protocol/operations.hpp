#pragma once

#include <morphene/protocol/types.hpp>

#include <morphene/protocol/operation_util.hpp>
#include <morphene/protocol/morphene_operations.hpp>
#include <morphene/protocol/morphene_virtual_operations.hpp>

namespace morphene { namespace protocol {

   /** NOTE: do not change the order of any operations prior to the virtual operations
    * or it will trigger a hardfork.
    */
   typedef fc::static_variant<
            transfer_operation,
            transfer_to_vesting_operation,
            withdraw_vesting_operation,

            account_create_operation,
            account_update_operation,

            witness_update_operation,
            account_witness_vote_operation,
            account_witness_proxy_operation,

            custom_operation,
            custom_json_operation,
            custom_binary_operation,

            set_withdraw_vesting_route_operation,
            claim_account_operation,
            create_claimed_account_operation,
            request_account_recovery_operation,
            recover_account_operation,
            change_recovery_account_operation,
            escrow_transfer_operation,
            escrow_dispute_operation,
            escrow_release_operation,
            escrow_approve_operation,
            reset_account_operation,
            set_reset_account_operation,
            delegate_vesting_shares_operation,
            account_create_with_delegation_operation,
            witness_set_properties_operation,

            /// virtual operations below this point
            fill_vesting_withdraw_operation,
            shutdown_witness_operation,
            hardfork_operation,
            return_vesting_delegation_operation,
            producer_reward_operation,
            clear_null_account_balance_operation
         > operation;

   /*void operation_get_required_authorities( const operation& op,
                                            flat_set<string>& active,
                                            flat_set<string>& owner,
                                            flat_set<string>& posting,
                                            vector<authority>&  other );

   void operation_validate( const operation& op );*/

   bool is_market_operation( const operation& op );

   bool is_virtual_operation( const operation& op );

} } // morphene::protocol

/*namespace fc {
    void to_variant( const morphene::protocol::operation& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  morphene::protocol::operation& vo );
}*/

MORPHENE_DECLARE_OPERATION_TYPE( morphene::protocol::operation )
FC_REFLECT_TYPENAME( morphene::protocol::operation )
