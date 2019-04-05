#pragma once

#include <morphene/chain/util/asset.hpp>
#include <morphene/chain/morphene_objects.hpp>

#include <morphene/protocol/asset.hpp>
#include <morphene/protocol/config.hpp>
#include <morphene/protocol/types.hpp>
#include <morphene/protocol/misc_utilities.hpp>

#include <fc/reflect/reflect.hpp>

#include <fc/uint128.hpp>

namespace morphene { namespace chain { namespace util {

using morphene::protocol::asset;
using morphene::protocol::price;
using morphene::protocol::share_type;

using fc::uint128_t;

inline uint128_t get_content_constant_s()
{
   return MORPHENE_CONTENT_CONSTANT; // looking good for posters
}

uint128_t evaluate_reward_curve( const uint128_t& rshares, const protocol::curve_id& curve = protocol::quadratic, const uint128_t& content_constant = MORPHENE_CONTENT_CONSTANT );

} } } // morphene::chain::util
