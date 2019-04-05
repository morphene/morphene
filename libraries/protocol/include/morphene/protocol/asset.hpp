#pragma once
#include <morphene/protocol/types.hpp>
#include <morphene/protocol/config.hpp>
#include <morphene/protocol/asset_symbol.hpp>

namespace morphene { namespace protocol {

   struct asset
   {
      asset( const asset& _asset, asset_symbol_type id )
      :amount( _asset.amount ),symbol(id){}

      asset( share_type a, asset_symbol_type id )
         :amount(a),symbol(id){}

      asset()
         :amount(0),symbol(MORPH_SYMBOL){}

      share_type        amount;
      asset_symbol_type symbol;

      void validate()const;

      asset& operator += ( const asset& o )
      {
         FC_ASSERT( symbol == o.symbol );
         amount += o.amount;
         return *this;
      }

      asset& operator -= ( const asset& o )
      {
         FC_ASSERT( symbol == o.symbol );
         amount -= o.amount;
         return *this;
      }
      asset operator -()const { return asset( -amount, symbol ); }

      friend bool operator == ( const asset& a, const asset& b )
      {
         return std::tie( a.symbol, a.amount ) == std::tie( b.symbol, b.amount );
      }
      friend bool operator < ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return a.amount < b.amount;
      }

      friend bool operator <= ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return a.amount <= b.amount;
      }

      friend bool operator != ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return a.amount != b.amount;
      }

      friend bool operator > ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return a.amount > b.amount;
      }

      friend bool operator >= ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return a.amount >= b.amount;
      }

      friend asset operator - ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return asset( a.amount - b.amount, a.symbol );
      }
      friend asset operator + ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return asset( a.amount + b.amount, a.symbol );
      }

      friend asset operator * ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.symbol == b.symbol );
         return asset( a.amount * b.amount, a.symbol );
      }
   };

  struct legacy_asset
  {
     public:
        legacy_asset( const legacy_asset& _asset, asset_symbol_type id )
        :amount( _asset.amount ),symbol(id){}

        legacy_asset( share_type a, asset_symbol_type id )
           :amount(a),symbol(id){}

        legacy_asset()
           :amount(0),symbol(MORPH_SYMBOL){}

        asset to_asset()const
        {
           return asset( amount, symbol );
        }

        operator asset()const { return to_asset(); }

        static legacy_asset from_asset( const asset& a )
        {
           legacy_asset leg;
           leg.amount = a.amount;
           leg.symbol = a.symbol;
           return leg;
        }

        string to_string()const;
        static legacy_asset from_string( const string& from );

        share_type                       amount;
        asset_symbol_type                symbol = MORPH_SYMBOL;

        void validate()const;

        legacy_asset& operator += ( const legacy_asset& o )
        {
           FC_ASSERT( symbol == o.symbol );
           amount += o.amount;
           return *this;
        }

        legacy_asset& operator -= ( const legacy_asset& o )
        {
           FC_ASSERT( symbol == o.symbol );
           amount -= o.amount;
           return *this;
        }
        legacy_asset operator -()const { return legacy_asset( -amount, symbol ); }

        friend bool operator == ( const legacy_asset& a, const legacy_asset& b )
        {
           return std::tie( a.symbol, a.amount ) == std::tie( b.symbol, b.amount );
        }
        friend bool operator < ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return a.amount < b.amount;
        }

        friend bool operator <= ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return a.amount <= b.amount;
        }

        friend bool operator != ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return a.amount != b.amount;
        }

        friend bool operator > ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return a.amount > b.amount;
        }

        friend bool operator >= ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return a.amount >= b.amount;
        }

        friend legacy_asset operator - ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return legacy_asset( a.amount - b.amount, a.symbol );
        }
        friend legacy_asset operator + ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return legacy_asset( a.amount + b.amount, a.symbol );
        }

        friend legacy_asset operator * ( const legacy_asset& a, const legacy_asset& b )
        {
           FC_ASSERT( a.symbol == b.symbol );
           return legacy_asset( a.amount * b.amount, a.symbol );
        }
  };

   /** Represents quotation of the relative value of asset against another asset.
       Similar to 'currency pair' used to determine value of currencies.

       For example:
       1 EUR / 1.25 USD where:
       1 EUR is an asset specified as a base
       1.25 USD us an asset specified as a qute

       can determine value of EUR against USD.
   */
   struct price
   {
      /** Even non-single argument, lets make it an explicit one to avoid implicit calls for
          initialization lists.

          \param base  - represents a value of the price object to be expressed relatively to quote
                         asset. Cannot have amount == 0 if you want to build valid price.
          \param quote - represents an relative asset. Cannot have amount == 0, otherwise
                         asertion fail.

        Both base and quote shall have different symbol defined, since it also results in
        creation of invalid price object. \see validate() method.
      */
      explicit price(const legacy_asset& base, const legacy_asset& quote) : base(base),quote(quote)
      {
          /// Call validate to verify passed arguments. \warning It throws on error.
          validate();
      }

      /** Default constructor is needed because of fc::variant::as method requirements.
      */
      price() = default;

      legacy_asset base;
      legacy_asset quote;

      static price max(asset_symbol_type base, asset_symbol_type quote );
      static price min(asset_symbol_type base, asset_symbol_type quote );

      price max()const { return price::max( base.symbol, quote.symbol ); }
      price min()const { return price::min( base.symbol, quote.symbol ); }

      bool is_null()const;
      void validate()const;

   }; /// price

   price operator / ( const legacy_asset& base, const legacy_asset& quote );
   inline price operator~( const price& p ) { return price{p.quote,p.base}; }

   bool  operator <  ( const legacy_asset& a, const legacy_asset& b );
   bool  operator <= ( const legacy_asset& a, const legacy_asset& b );
   bool  operator <  ( const price& a, const price& b );
   bool  operator <= ( const price& a, const price& b );
   bool  operator >  ( const price& a, const price& b );
   bool  operator >= ( const price& a, const price& b );
   bool  operator == ( const price& a, const price& b );
   bool  operator != ( const price& a, const price& b );
   legacy_asset operator *  ( const legacy_asset& a, const price& b );

   legacy_asset from_string( const string& from );

} } // morphene::protocol

namespace fc {
  void to_variant( const morphene::protocol::asset& var,  fc::variant& vo );
  void from_variant( const fc::variant& var,  morphene::protocol::asset& vo );
  inline void to_variant( const morphene::protocol::legacy_asset& a, fc::variant& var )
  {
    var = a.to_string();
  }
  inline void from_variant( const fc::variant& var, morphene::protocol::legacy_asset& a )
  {
    a = morphene::protocol::legacy_asset::from_string( var.as_string() );
  }
}

FC_REFLECT( morphene::protocol::asset, (amount)(symbol) )
FC_REFLECT( morphene::protocol::price, (base)(quote) )

FC_REFLECT( morphene::protocol::legacy_asset,
   (amount)
   (symbol)
   )
