/*
 * Copyright (c) 2016 Boone Development, and contributors.
 */
#pragma once
#include <morphene/protocol/hardfork.hpp>

// WARNING!
// Every symbol defined here needs to be handled appropriately in get_config.cpp
// This is checked by get_config_check.sh called from Dockerfile

#ifdef IS_TEST_NET
#define MORPHENE_BLOCKCHAIN_VERSION              ( version(0, 2, 0) )

#define MORPHENE_INIT_PRIVATE_KEY                (fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("init_key"))))
#define MORPHENE_INIT_PUBLIC_KEY_STR             (std::string( morphene::protocol::public_key_type(MORPHENE_INIT_PRIVATE_KEY.get_public_key()) ))
#define MORPHENE_CHAIN_ID 												(fc::sha256::hash("morphene-test"))
#define MORPHENE_ADDRESS_PREFIX                  "TST"

#define MORPHENE_GENESIS_TIME                    (fc::time_point_sec(1553990399))

#define MORPHENE_MIN_ACCOUNT_CREATION_FEE          0
#define MORPHENE_MAX_ACCOUNT_CREATION_FEE          int64_t(1000000000)

#define MORPHENE_OWNER_AUTH_RECOVERY_PERIOD                  fc::seconds(60)
#define MORPHENE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD  fc::seconds(12)
#define MORPHENE_OWNER_UPDATE_LIMIT                          fc::seconds(0)
#define MORPHENE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM 1

#define MORPHENE_INIT_SUPPLY                     (int64_t( 1000 ) * int64_t( 1000000 ) * int64_t( 1000 ))
#define MORPHENE_RECENT_CLAIMS									  (fc::uint128_t(uint64_t(0ull)))

/// Allows to limit number of total produced blocks.
#define TESTNET_BLOCK_LIMIT                   (3000000)

#else // IS LIVE Morphene NETWORK

#define MORPHENE_BLOCKCHAIN_VERSION              ( version(0, 1, 0) )

#define MORPHENE_INIT_PUBLIC_KEY_STR             "MPH7rLx8tBP1cxc1tqjfw7GEExeFFyyfZPfhKz68dLXs4PSZTxNwZ"
#define MORPHENE_CHAIN_ID 												(fc::sha256::hash("morphene-main"))
#define MORPHENE_ADDRESS_PREFIX                  "MPH"

#define MORPHENE_GENESIS_TIME                    (fc::time_point_sec(1553990399))

#define MORPHENE_MIN_ACCOUNT_CREATION_FEE           1
#define MORPHENE_MAX_ACCOUNT_CREATION_FEE           int64_t(1000000000)

#define MORPHENE_OWNER_AUTH_RECOVERY_PERIOD                  fc::days(30)
#define MORPHENE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD  fc::days(1)
#define MORPHENE_OWNER_UPDATE_LIMIT                          fc::minutes(60)
#define MORPHENE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM 3186477

#define MORPHENE_INIT_SUPPLY                     int64_t(0)
#define MORPHENE_RECENT_CLAIMS									  (fc::uint128_t(uint64_t(1000000000000ull)))

#endif

#define VESTS_SYMBOL  (morphene::protocol::asset_symbol_type::from_asset_num( MORPHENE_ASSET_NUM_VESTS ) )
#define MORPH_SYMBOL  (morphene::protocol::asset_symbol_type::from_asset_num( MORPHENE_ASSET_NUM_MORPH ) )

#define MORPHENE_BLOCKCHAIN_HARDFORK_VERSION     ( hardfork_version( MORPHENE_BLOCKCHAIN_VERSION ) )

#define MORPHENE_BLOCK_INTERVAL                  3
#define MORPHENE_BLOCKS_PER_YEAR                 (365*24*60*60/MORPHENE_BLOCK_INTERVAL)
#define MORPHENE_BLOCKS_PER_DAY                  (24*60*60/MORPHENE_BLOCK_INTERVAL)

#define MORPHENE_INIT_WITNESS_NAME               "initwitness"
#define MORPHENE_NUM_INIT_WITNESSES              1
#define MORPHENE_INIT_TIME                       (fc::time_point_sec());

#define MORPHENE_MAX_VOTED_WITNESSES             1 /// elected
#define MORPHENE_MAX_MINER_WITNESSES             19  /// miner
#define MORPHENE_MAX_RUNNER_WITNESSES            1  /// timeshare
#define MORPHENE_MAX_WITNESSES                   (MORPHENE_MAX_VOTED_WITNESSES+MORPHENE_MAX_MINER_WITNESSES+MORPHENE_MAX_RUNNER_WITNESSES) /// 21 is more than enough

#define MORPHENE_HARDFORK_REQUIRED_WITNESSES     17 // 17 of the 21 dpos witnesses (20 elected and 1 virtual time) required for hardfork. This guarantees 75% participation on all subsequent rounds.
#define MORPHENE_MAX_TIME_UNTIL_EXPIRATION       (60*60) // seconds,  aka: 1 hour
#define MORPHENE_MAX_MEMO_SIZE                   2048
#define MORPHENE_MAX_PROXY_RECURSION_DEPTH       4
#define MORPHENE_VESTING_WITHDRAW_INTERVALS      7 /// 1 week total
#define MORPHENE_VESTING_WITHDRAW_INTERVAL_SECONDS (60*60*24) /// 1 day per interval
#define MORPHENE_MAX_WITHDRAW_ROUTES             10
#define MORPHENE_VOTING_MANA_REGENERATION_SECONDS (5*60*60*24) // 5 day

#define MORPHENE_MAX_ACCOUNT_WITNESS_VOTES       30

#define MORPHENE_100_PERCENT                     10000
#define MORPHENE_1_PERCENT                       (MORPHENE_100_PERCENT/100)

#define MORPHENE_INFLATION_RATE_START_PERCENT    (978) // Fixes block 7,000,000 to 9.5%
#define MORPHENE_INFLATION_RATE_STOP_PERCENT     (95) // 0.95%
#define MORPHENE_INFLATION_NARROWING_PERIOD      (250000) // Narrow 0.01% every 250k blocks

#define MORPHENE_MAX_RATION_DECAY_RATE           (1000000)

#define MORPHENE_BANDWIDTH_AVERAGE_WINDOW_SECONDS (60*60*24*7) ///< 1 week
#define MORPHENE_BANDWIDTH_PRECISION             (uint64_t(1000000)) ///< 1 million

#define MORPHENE_CREATE_ACCOUNT_WITH_MORPHENE_MODIFIER 30
#define MORPHENE_CREATE_ACCOUNT_DELEGATION_RATIO    5
#define MORPHENE_CREATE_ACCOUNT_DELEGATION_TIME     fc::days(30)

#define MORPHENE_MIN_PRODUCER_REWARD             legacy_asset( 1000, MORPH_SYMBOL )
#define MORPHENE_MIN_POW_REWARD									 MORPHENE_MIN_PRODUCER_REWARD

#define MORPHENE_MINING_REWARD                   asset( 1000, STEEM_SYMBOL )
#define MORPHENE_EQUIHASH_N                      140
#define MORPHENE_EQUIHASH_K                      6

#define MORPHENE_ACTIVE_CHALLENGE_FEE            legacy_asset( 2000, MORPH_SYMBOL )
#define MORPHENE_OWNER_CHALLENGE_FEE             legacy_asset( 30000, MORPH_SYMBOL )
#define MORPHENE_ACTIVE_CHALLENGE_COOLDOWN       fc::days(1)
#define MORPHENE_OWNER_CHALLENGE_COOLDOWN        fc::days(1)

#define MORPHENE_CONTENT_CONSTANT                (uint128_t(uint64_t(2000000000000ll)))
// note, if redefining these constants make sure calculate_claims doesn't overflow

// 5ccc e802 de5f
// int(expm1( log1p( 1 ) / BLOCKS_PER_YEAR ) * 2**MORPHENE_APR_PERCENT_SHIFT_PER_BLOCK / 100000 + 0.5)
// we use 100000 here instead of 10000 because we end up creating an additional 9x for vesting
#define MORPHENE_APR_PERCENT_MULTIPLY_PER_BLOCK          ( (uint64_t( 0x5ccc ) << 0x20) \
                                                        | (uint64_t( 0xe802 ) << 0x10) \
                                                        | (uint64_t( 0xde5f )        ) \
                                                        )
// chosen to be the maximal value such that MORPHENE_APR_PERCENT_MULTIPLY_PER_BLOCK * 2**64 * 100000 < 2**128
#define MORPHENE_APR_PERCENT_SHIFT_PER_BLOCK             87

#define MORPHENE_APR_PERCENT_MULTIPLY_PER_ROUND          ( (uint64_t( 0x79cc ) << 0x20 ) \
                                                        | (uint64_t( 0xf5c7 ) << 0x10 ) \
                                                        | (uint64_t( 0x3480 )         ) \
                                                        )

#define MORPHENE_APR_PERCENT_SHIFT_PER_ROUND             83

// We have different constants for hourly rewards
// i.e. hex(int(math.expm1( math.log1p( 1 ) / HOURS_PER_YEAR ) * 2**MORPHENE_APR_PERCENT_SHIFT_PER_HOUR / 100000 + 0.5))
#define MORPHENE_APR_PERCENT_MULTIPLY_PER_HOUR           ( (uint64_t( 0x6cc1 ) << 0x20) \
                                                        | (uint64_t( 0x39a1 ) << 0x10) \
                                                        | (uint64_t( 0x5cbd )        ) \
                                                        )

// chosen to be the maximal value such that MORPHENE_APR_PERCENT_MULTIPLY_PER_HOUR * 2**64 * 100000 < 2**128
#define MORPHENE_APR_PERCENT_SHIFT_PER_HOUR              77

// These constants add up to GRAPHENE_100_PERCENT.  Each GRAPHENE_1_PERCENT is equivalent to 1% per year APY
// *including the corresponding 9x vesting rewards*
#define MORPHENE_PRODUCER_APR_PERCENT            9000
#define MORPHENE_POW_APR_PERCENT                 1000

#define MORPHENE_MIN_ACCOUNT_NAME_LENGTH          3
#define MORPHENE_MAX_ACCOUNT_NAME_LENGTH         16

#define MORPHENE_MIN_PERMLINK_LENGTH             0
#define MORPHENE_MAX_PERMLINK_LENGTH             256
#define MORPHENE_MAX_WITNESS_URL_LENGTH          2048

#define MORPHENE_MAX_SHARE_SUPPLY                int64_t(1000000000000000ll)
#define MORPHENE_MAX_SATOSHIS                    int64_t(4611686018427387903ll)
#define MORPHENE_MAX_SIG_CHECK_DEPTH             2
#define MORPHENE_MAX_SIG_CHECK_ACCOUNTS          125

#define MORPHENE_MIN_TRANSACTION_SIZE_LIMIT      1024
#define MORPHENE_SECONDS_PER_YEAR                (uint64_t(60*60*24*365ll))

#define MORPHENE_MAX_TRANSACTION_SIZE            (1024*64)
#define MORPHENE_MIN_BLOCK_SIZE_LIMIT            (MORPHENE_MAX_TRANSACTION_SIZE)
#define MORPHENE_MAX_BLOCK_SIZE                  (MORPHENE_MAX_TRANSACTION_SIZE*MORPHENE_BLOCK_INTERVAL*2000)
#define MORPHENE_SOFT_MAX_BLOCK_SIZE             (2*1024*1024)
#define MORPHENE_MIN_BLOCK_SIZE                  115
#define MORPHENE_BLOCKS_PER_HOUR                 (60*60/MORPHENE_BLOCK_INTERVAL)

#define MORPHENE_MIN_UNDO_HISTORY                10
#define MORPHENE_MAX_UNDO_HISTORY                10000

#define MORPHENE_MIN_TRANSACTION_EXPIRATION_LIMIT (MORPHENE_BLOCK_INTERVAL * 5) // 5 transactions per block
#define MORPHENE_BLOCKCHAIN_PRECISION            uint64_t( 1000 )

#define MORPHENE_BLOCKCHAIN_PRECISION_DIGITS     3
#define MORPHENE_MAX_INSTANCE_ID                 (uint64_t(-1)>>16)
/** NOTE: making this a power of 2 (say 2^15) would greatly accelerate fee calcs */
#define MORPHENE_MAX_AUTHORITY_MEMBERSHIP        40
#define MORPHENE_MAX_ASSET_WHITELIST_AUTHORITIES 10
#define MORPHENE_MAX_URL_LENGTH                  127

#define MORPHENE_IRREVERSIBLE_THRESHOLD          (75 * MORPHENE_1_PERCENT)

#define MORPHENE_VIRTUAL_SCHEDULE_LAP_LENGTH  ( fc::uint128::max_value() )

#define MORPHENE_DELEGATION_RETURN_PERIOD  (MORPHENE_VOTING_MANA_REGENERATION_SECONDS)

#define MORPHENE_RD_MIN_DECAY_BITS               6
#define MORPHENE_RD_MAX_DECAY_BITS              32
#define MORPHENE_RD_DECAY_DENOM_SHIFT           36
#define MORPHENE_RD_MAX_POOL_BITS               64
#define MORPHENE_RD_MAX_BUDGET_1                ((uint64_t(1) << (MORPHENE_RD_MAX_POOL_BITS + MORPHENE_RD_MIN_DECAY_BITS - MORPHENE_RD_DECAY_DENOM_SHIFT))-1)
#define MORPHENE_RD_MAX_BUDGET_2                ((uint64_t(1) << (64-MORPHENE_RD_DECAY_DENOM_SHIFT))-1)
#define MORPHENE_RD_MAX_BUDGET_3                (uint64_t( std::numeric_limits<int32_t>::max() ))
#define MORPHENE_RD_MAX_BUDGET                  (int32_t( std::min( { MORPHENE_RD_MAX_BUDGET_1, MORPHENE_RD_MAX_BUDGET_2, MORPHENE_RD_MAX_BUDGET_3 } )) )
#define MORPHENE_RD_MIN_DECAY                   (uint32_t(1) << MORPHENE_RD_MIN_DECAY_BITS)
#define MORPHENE_RD_MIN_BUDGET                  1
#define MORPHENE_RD_MAX_DECAY                   (uint32_t(0xFFFFFFFF))

#define MORPHENE_ACCOUNT_SUBSIDY_PRECISION      (MORPHENE_100_PERCENT)

// We want the global subsidy to run out first in normal (Poisson)
// conditions, so we boost the per-witness subsidy a little.
#define MORPHENE_WITNESS_SUBSIDY_BUDGET_PERCENT (125 * MORPHENE_1_PERCENT)

// Since witness decay only procs once per round, multiplying the decay
// constant by the number of witnesses means the per-witness pools have
// the same effective decay rate in real-time terms.
#define MORPHENE_WITNESS_SUBSIDY_DECAY_PERCENT  (MORPHENE_MAX_WITNESSES * MORPHENE_100_PERCENT)

// 347321 corresponds to a 5-day halflife
#define MORPHENE_DEFAULT_ACCOUNT_SUBSIDY_DECAY  (347321)
// Default rate is 0.5 accounts per block
#define MORPHENE_DEFAULT_ACCOUNT_SUBSIDY_BUDGET (797)
#define MORPHENE_DECAY_BACKSTOP_PERCENT         (90 * MORPHENE_1_PERCENT)

/**
 *  Reserved Account IDs with special meaning
 */
///@{
/// Represents the canonical account with NO authority (nobody can access funds in null account)
#define MORPHENE_NULL_ACCOUNT                    "null"
/// Represents the canonical account with WILDCARD authority (anybody can access funds in temp account)
#define MORPHENE_TEMP_ACCOUNT                    "temp"
/// Represents the canonical account for specifying you will vote for directly (as opposed to a proxy)
#define MORPHENE_PROXY_TO_SELF_ACCOUNT           ""
///@}
