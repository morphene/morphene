
#include <morphene/chain/compound.hpp>
#include <morphene/protocol/asset.hpp>
#include <morphene/protocol/types.hpp>

#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <iostream>
#include <vector>

#define CURATE_OFF      0
#define VCURATE_OFF     1
#define CONTENT_OFF     2
#define VCONTENT_OFF    3
#define PRODUCER_OFF    4
#define VPRODUCER_OFF   5
#define REWARD_TYPES    6

using morphene::protocol::asset;
using morphene::protocol::share_type;
using morphene::protocol::calc_percent_reward_per_block;
using morphene::protocol::calc_percent_reward_per_round;
using morphene::protocol::calc_percent_reward_per_hour;

/*
Explanation of output

{"rvec":["929159090641","8360617424769","929159090641","8360617424769","197985103985","1780051544865","195077031513","1755693283617","179687790278","1615357001502"],"b":68585000,"s":"24303404786580"}

rvec shows total number of MORPH satoshis created since genesis for:

- Curation rewards
- Vesting rewards balancing curation rewards
- Content rewards
- Vesting rewards balancing content rewards
- Producer rewards
- Vesting rewards balancing producer rewards

b is block number
s is total supply


Some possible sources of inaccuracy, the direction and estimated relative sizes of these effects:

- Missed blocks not modeled (lowers MORPH supply, small)
- Lost / forgotten private keys / wallets and deliberate burning of MORPH not modeled (lowers MORPH supply, unknown but likely small)
- Possible bugs or mismatches with implementation (unknown)

*/

int main( int argc, char** argv, char** envp )
{
   std::vector< share_type > reward_delta;
   std::vector< share_type > reward_total;

/*
#define MORPHENE_GENESIS_TIME                    (fc::time_point_sec(1458835200))
#define MORPHENE_FIRST_CASHOUT_TIME              (fc::time_point_sec(1467590400))  /// July 4th
*/

   for( int i=0; i<REWARD_TYPES; i++ )
   {
      reward_delta.emplace_back();
      reward_total.emplace_back();
   }

   auto block_inflation_model = [&]( uint32_t block_num, share_type& current_supply )
   {
      uint32_t vesting_factor = (block_num < MORPHENE_START_VESTING_BLOCK) ? 0 : 9;

      share_type curate_reward   = calc_percent_reward_per_block< MORPHENE_CURATE_APR_PERCENT >( current_supply );
      reward_delta[ CURATE_OFF ] = std::max( curate_reward, MORPHENE_MIN_CURATE_REWARD.amount );
      reward_delta[ VCURATE_OFF ] = reward_delta[ CURATE_OFF ] * vesting_factor;

      share_type content_reward  = calc_percent_reward_per_block< MORPHENE_CONTENT_APR_PERCENT >( current_supply );
      reward_delta[ CONTENT_OFF ] = std::max( content_reward, MORPHENE_MIN_CONTENT_REWARD.amount );
      reward_delta[ VCONTENT_OFF ] = reward_delta[ CONTENT_OFF ] * vesting_factor;

      share_type producer_reward = calc_percent_reward_per_block< MORPHENE_PRODUCER_APR_PERCENT >( current_supply );
      reward_delta[ PRODUCER_OFF ] = std::max( producer_reward, MORPHENE_MIN_PRODUCER_REWARD.amount );
      reward_delta[ VPRODUCER_OFF ] = reward_delta[ PRODUCER_OFF ] * vesting_factor;

      current_supply += reward_delta[CURATE_OFF] + reward_delta[VCURATE_OFF] + reward_delta[CONTENT_OFF] + reward_delta[VCONTENT_OFF] + reward_delta[PRODUCER_OFF] + reward_delta[VPRODUCER_OFF];
      // supply for above is computed by using pre-updated supply for computing all 3 amounts.
      // supply for below reward types is basically a self-contained event which updates the supply immediately before the next reward type's computation.

      for( int i=0; i<REWARD_TYPES; i++ )
      {
         reward_total[i] += reward_delta[i];
      }

      return;
   };

   share_type current_supply = 0;

   for( uint32_t b=1; b<10*MORPHENE_BLOCKS_PER_YEAR; b++ )
   {
      block_inflation_model( b, current_supply );
      if( b%1000 == 0 )
      {
         fc::mutable_variant_object mvo;
         mvo("rvec", reward_total)("b", b)("s", current_supply);
         std::cout << fc::json::to_string(mvo) << std::endl;
      }
   }

   return 0;
}
