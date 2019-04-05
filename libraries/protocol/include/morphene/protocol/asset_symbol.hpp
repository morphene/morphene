#pragma once

#include <fc/io/raw.hpp>
#include <morphene/protocol/types_fwd.hpp>

#define MORPHENE_ASSET_SYMBOL_PRECISION_BITS    4
#define MORPHENE_MAX_NAI                          99999999
#define MORPHENE_MIN_NAI                          1
#define MORPHENE_MIN_NON_RESERVED_NAI             10000000
#define MORPHENE_ASSET_SYMBOL_NAI_LENGTH        10
#define MORPHENE_ASSET_SYMBOL_NAI_STRING_LENGTH ( MORPHENE_ASSET_SYMBOL_NAI_LENGTH + 2 )

#define MORPHENE_PRECISION_MORPH (3)
#define MORPHENE_PRECISION_VESTS (6)

// One's place is used for check digit, which means NAI 0-9 all have NAI data of 0 which is invalid
// This space is safe to use because it would alwasys result in failure to convert from NAI
#define MORPHENE_NAI_MORPH (2)
#define MORPHENE_NAI_VESTS (3)

#define MORPHENE_ASSET_NUM_MORPH \
  (((MORPHENE_MAX_NAI + MORPHENE_NAI_MORPH) << MORPHENE_ASSET_SYMBOL_PRECISION_BITS) | MORPHENE_PRECISION_MORPH)
#define MORPHENE_ASSET_NUM_VESTS \
  (((MORPHENE_MAX_NAI + MORPHENE_NAI_VESTS) << MORPHENE_ASSET_SYMBOL_PRECISION_BITS) | MORPHENE_PRECISION_VESTS)

#ifdef IS_TEST_NET

#define VESTS_SYMBOL_U64  (uint64_t('V') | (uint64_t('E') << 8) | (uint64_t('S') << 16) | (uint64_t('T') << 24) | (uint64_t('S') << 32))
#define MORPH_SYMBOL_U64  (uint64_t('T') | (uint64_t('E') << 8) | (uint64_t('S') << 16) | (uint64_t('T') << 24) | (uint64_t('S') << 32))

#else

#define VESTS_SYMBOL_U64  (uint64_t('V') | (uint64_t('E') << 8) | (uint64_t('S') << 16) | (uint64_t('T') << 24) | (uint64_t('S') << 32))
#define MORPH_SYMBOL_U64  (uint64_t('S') | (uint64_t('T') << 8) | (uint64_t('E') << 16) | (uint64_t('E') << 24) | (uint64_t('M') << 32))

#endif

#define VESTS_SYMBOL_SER  (uint64_t(6) | (VESTS_SYMBOL_U64 << 8)) ///< VESTS|VESTS with 6 digits of precision
#define MORPH_SYMBOL_SER  (uint64_t(3) | (MORPH_SYMBOL_U64 << 8)) ///< MORPH|TESTS with 3 digits of precision

#define MORPHENE_ASSET_MAX_DECIMALS 12

#define MORPHENE_ASSET_NUM_PRECISION_MASK   0xF
#define MORPHENE_ASSET_NUM_CONTROL_MASK     0x10
#define MORPHENE_ASSET_NUM_VESTING_MASK     0x20

namespace morphene { namespace protocol {

class asset_symbol_type
{
   public:
      enum asset_symbol_space
      {
         legacy_space = 1,
         smt_nai_space = 2
      };

      asset_symbol_type() {}

      // buf must have space for MORPHENE_ASSET_SYMBOL_MAX_LENGTH+1
      static asset_symbol_type from_string( const std::string& str );
      static asset_symbol_type from_nai_string( const char* buf, uint8_t decimal_places );
      static asset_symbol_type from_asset_num( uint32_t asset_num )
      {   asset_symbol_type result;   result.asset_num = asset_num;   return result;   }
      static uint32_t asset_num_from_nai( uint32_t nai, uint8_t decimal_places );
      static asset_symbol_type from_nai( uint32_t nai, uint8_t decimal_places )
      {   return from_asset_num( asset_num_from_nai( nai, decimal_places ) );          }

      std::string to_string()const;

      void to_nai_string( char* buf )const;
      std::string to_nai_string()const
      {
         char buf[ MORPHENE_ASSET_SYMBOL_NAI_STRING_LENGTH ];
         to_nai_string( buf );
         return std::string( buf );
      }

      uint32_t to_nai()const;

      /**Returns true when symbol represents vesting variant of the token,
       * false for liquid one.
       */
      bool is_vesting() const;
      /**Returns vesting symbol when called from liquid one
       * and liquid symbol when called from vesting one.
       */
      asset_symbol_type get_paired_symbol() const;
      /**Returns asset_num stripped of precision holding bits. */
      uint32_t get_stripped_precision_smt_num() const
      { 
         return asset_num & ~( MORPHENE_ASSET_NUM_PRECISION_MASK );
      }

      asset_symbol_space space()const;
      uint8_t decimals()const
      {  return uint8_t( asset_num & 0x0F );    }
      void validate()const;

      friend bool operator == ( const asset_symbol_type& a, const asset_symbol_type& b )
      {  return (a.asset_num == b.asset_num);   }
      friend bool operator != ( const asset_symbol_type& a, const asset_symbol_type& b )
      {  return (a.asset_num != b.asset_num);   }
      friend bool operator <  ( const asset_symbol_type& a, const asset_symbol_type& b )
      {  return (a.asset_num <  b.asset_num);   }
      friend bool operator >  ( const asset_symbol_type& a, const asset_symbol_type& b )
      {  return (a.asset_num >  b.asset_num);   }
      friend bool operator <= ( const asset_symbol_type& a, const asset_symbol_type& b )
      {  return (a.asset_num <= b.asset_num);   }
      friend bool operator >= ( const asset_symbol_type& a, const asset_symbol_type& b )
      {  return (a.asset_num >= b.asset_num);   }

      uint32_t asset_num = 0;
};

} } // morphene::protocol

FC_REFLECT(morphene::protocol::asset_symbol_type, (asset_num))

namespace fc { namespace raw {

// Legacy serialization of assets
// 0000pppp aaaaaaaa bbbbbbbb cccccccc dddddddd eeeeeeee ffffffff 00000000
// Symbol = abcdef
//
// NAI serialization of assets
// aaa1pppp bbbbbbbb cccccccc dddddddd
// NAI = (MSB to LSB) dddddddd cccccccc bbbbbbbb aaa
//
// NAI internal storage of legacy assets

template< typename Stream >
inline void pack( Stream& s, const morphene::protocol::asset_symbol_type& sym )
{
   switch( sym.space() )
   {
      case morphene::protocol::asset_symbol_type::legacy_space:
      {
         uint64_t ser = 0;
         switch( sym.asset_num )
         {
            case MORPHENE_ASSET_NUM_MORPH:
               ser = MORPH_SYMBOL_SER;
               break;
            case MORPHENE_ASSET_NUM_VESTS:
               ser = VESTS_SYMBOL_SER;
               break;
            default:
               FC_ASSERT( false, "Cannot serialize unknown asset symbol" );
         }
         pack( s, ser );
         break;
      }
      case morphene::protocol::asset_symbol_type::smt_nai_space:
         pack( s, sym.asset_num );
         break;
      default:
         FC_ASSERT( false, "Cannot serialize unknown asset symbol" );
   }
}

template< typename Stream >
inline void unpack( Stream& s, morphene::protocol::asset_symbol_type& sym, uint32_t )
{
   uint64_t ser = 0;
   s.read( (char*) &ser, 4 );

   switch( ser )
   {
      case MORPH_SYMBOL_SER & 0xFFFFFFFF:
         s.read( ((char*) &ser)+4, 4 );
         FC_ASSERT( ser == MORPH_SYMBOL_SER, "invalid asset bits" );
         sym.asset_num = MORPHENE_ASSET_NUM_MORPH;
         break;
      case VESTS_SYMBOL_SER & 0xFFFFFFFF:
         s.read( ((char*) &ser)+4, 4 );
         FC_ASSERT( ser == VESTS_SYMBOL_SER, "invalid asset bits" );
         sym.asset_num = MORPHENE_ASSET_NUM_VESTS;
         break;
      default:
         sym.asset_num = uint32_t( ser );
   }
   sym.validate();
}

} // fc::raw

inline void to_variant( const morphene::protocol::asset_symbol_type& sym, fc::variant& var )
{
   try
   {
      std::vector< variant > v( 2 );
      v[0] = sym.decimals();
      v[1] = sym.to_nai_string();
   } FC_CAPTURE_AND_RETHROW()
}

inline void from_variant( const fc::variant& var, morphene::protocol::asset_symbol_type& sym )
{
   try
   {
      auto v = var.as< std::vector< variant > >();
      FC_ASSERT( v.size() == 2, "Expected tuple of length 2." );

      sym = morphene::protocol::asset_symbol_type::from_nai_string( v[1].as< std::string >().c_str(), v[0].as< uint8_t >() );
   } FC_CAPTURE_AND_RETHROW()
}

} // fc
