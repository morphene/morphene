
#pragma once

#include <morphene/schema/abstract_schema.hpp>
#include <morphene/schema/schema_impl.hpp>

#include <morphene/protocol/asset_symbol.hpp>

namespace morphene { namespace schema { namespace detail {

//////////////////////////////////////////////
// asset_symbol_type                        //
//////////////////////////////////////////////

struct schema_asset_symbol_type_impl
   : public abstract_schema
{
   MORPHENE_SCHEMA_CLASS_BODY( schema_asset_symbol_type_impl )
};

}

template<>
struct schema_reflect< morphene::protocol::asset_symbol_type >
{
   typedef detail::schema_asset_symbol_type_impl           schema_impl_type;
};

} }
