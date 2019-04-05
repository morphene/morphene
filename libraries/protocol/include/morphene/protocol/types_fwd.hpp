#pragma once
#include <cstdint>
#include <fc/uint128.hpp>

namespace fc {
class variant;
} // fc

namespace morphene { namespace protocol {
template< typename Storage = fc::uint128 >
class fixed_string_impl;

class asset_symbol_type;
class legacy_morph_asset_symbol_type;
struct legacy_morph_asset;
} } // morphene::protocol

namespace fc { namespace raw {

template< typename Stream, typename Storage >
inline void pack( Stream& s, const morphene::protocol::fixed_string_impl< Storage >& u );
template< typename Stream, typename Storage >
inline void unpack( Stream& s, morphene::protocol::fixed_string_impl< Storage >& u, uint32_t depth = 0 );

template< typename Stream >
inline void pack( Stream& s, const morphene::protocol::asset_symbol_type& sym );
template< typename Stream >
inline void unpack( Stream& s, morphene::protocol::asset_symbol_type& sym, uint32_t depth = 0 );

template< typename Stream >
inline void pack( Stream& s, const morphene::protocol::legacy_morph_asset_symbol_type& sym );
template< typename Stream >
inline void unpack( Stream& s, morphene::protocol::legacy_morph_asset_symbol_type& sym, uint32_t depth = 0 );

} // raw

template< typename Storage >
inline void to_variant( const morphene::protocol::fixed_string_impl< Storage >& s, fc::variant& v );
template< typename Storage >
inline void from_variant( const variant& v, morphene::protocol::fixed_string_impl< Storage >& s );

inline void to_variant( const morphene::protocol::asset_symbol_type& sym, fc::variant& v );

inline void from_variant( const fc::variant& v, morphene::protocol::legacy_morph_asset& leg );
inline void to_variant( const morphene::protocol::legacy_morph_asset& leg, fc::variant& v );

} // fc
