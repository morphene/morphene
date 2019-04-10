
#include <morphene/chain/compound.hpp>
#include <morphene/protocol/asset.hpp>
#include <morphene/protocol/types.hpp>

#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <iostream>
#include <vector>

#define PRODUCER_OFF    1
#define REWARD_TYPES    2

using morphene::protocol::legacy_asset;
using morphene::protocol::share_type;
using morphene::protocol::calc_percent_reward_per_block;

/*
Explanation of output

{"rvec":["929159090641","8360617424769","929159090641","8360617424769","197985103985","1780051544865","195077031513","1755693283617","179687790278","1615357001502"],"b":68585000,"s":"24303404786580"}

rvec shows total number of MORPH satoshis created since genesis for:

- Producer rewards

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

   for( int i=0; i<REWARD_TYPES; i++ )
   {
      reward_delta.emplace_back();
      reward_total.emplace_back();
   }

   auto block_inflation_model = [&]( uint32_t block_num, share_type& current_supply )
   {
      int64_t start_inflation_rate = int64_t( MORPHENE_INFLATION_RATE_START_PERCENT );
      int64_t inflation_rate_adjustment = int64_t( block_num / MORPHENE_INFLATION_NARROWING_PERIOD );
      int64_t inflation_rate_floor = int64_t( MORPHENE_INFLATION_RATE_STOP_PERCENT );
      int64_t current_inflation_rate = std::max( start_inflation_rate - inflation_rate_adjustment, inflation_rate_floor );

      auto new_morph = ( current_supply * current_inflation_rate ) / ( int64_t( MORPHENE_100_PERCENT ) * int64_t( MORPHENE_BLOCKS_PER_YEAR ) );
      reward_delta[ PRODUCER_OFF ] = std::max(new_morph, MORPHENE_MIN_PRODUCER_REWARD.amount);

      current_supply += reward_delta[PRODUCER_OFF];
      reward_total[PRODUCER_OFF] += reward_delta[PRODUCER_OFF];

      return;
   };

   share_type current_supply = MORPHENE_INIT_SUPPLY;

   for( uint32_t b=1; b<20*MORPHENE_BLOCKS_PER_YEAR; b++ )
   {
      block_inflation_model( b, current_supply );
      if( b%1000 == 0 || b==1 )
      {
         fc::mutable_variant_object mvo;
         mvo("rvec", reward_total)("b", b)("s", current_supply);
         std::cout << fc::json::to_string(mvo) << std::endl;
      }
   }

   return 0;
}
