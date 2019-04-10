
#pragma once

#include <morphene/protocol/base.hpp>
#include <morphene/protocol/block_header.hpp>
#include <morphene/protocol/asset.hpp>

#include <fc/utf8.hpp>

namespace morphene { namespace protocol {

inline bool is_asset_type( asset asset, asset_symbol_type symbol )
{
   return asset.symbol == symbol;
}

inline void validate_account_name( const string& name )
{
   FC_ASSERT( is_valid_account_name( name ), "Account name ${n} is invalid", ("n", name) );
}

} }
