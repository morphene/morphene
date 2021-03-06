#pragma once

#include <morphene/protocol/morphene_operations.hpp>

#include <morphene/chain/evaluator.hpp>

namespace morphene { namespace chain {

using namespace morphene::protocol;

MORPHENE_DEFINE_EVALUATOR( account_create )
MORPHENE_DEFINE_EVALUATOR( account_update )
MORPHENE_DEFINE_EVALUATOR( transfer )
MORPHENE_DEFINE_EVALUATOR( transfer_to_vesting )
MORPHENE_DEFINE_EVALUATOR( witness_update )
MORPHENE_DEFINE_EVALUATOR( account_witness_vote )
MORPHENE_DEFINE_EVALUATOR( account_witness_proxy )
MORPHENE_DEFINE_EVALUATOR( withdraw_vesting )
MORPHENE_DEFINE_EVALUATOR( set_withdraw_vesting_route )
MORPHENE_DEFINE_EVALUATOR( pow )
MORPHENE_DEFINE_EVALUATOR( claim_account )
MORPHENE_DEFINE_EVALUATOR( create_claimed_account )
MORPHENE_DEFINE_EVALUATOR( request_account_recovery )
MORPHENE_DEFINE_EVALUATOR( recover_account )
MORPHENE_DEFINE_EVALUATOR( change_recovery_account )
MORPHENE_DEFINE_EVALUATOR( reset_account )
MORPHENE_DEFINE_EVALUATOR( set_reset_account )
MORPHENE_DEFINE_EVALUATOR( delegate_vesting_shares )
MORPHENE_DEFINE_EVALUATOR( witness_set_properties )
MORPHENE_DEFINE_EVALUATOR( create_auction )
MORPHENE_DEFINE_EVALUATOR( update_auction )
MORPHENE_DEFINE_EVALUATOR( delete_auction )
MORPHENE_DEFINE_EVALUATOR( place_bid )

} } // morphene::chain
