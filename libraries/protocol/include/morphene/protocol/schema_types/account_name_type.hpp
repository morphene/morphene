
#pragma once

#include <morphene/schema/abstract_schema.hpp>
#include <morphene/schema/schema_impl.hpp>

#include <morphene/protocol/types.hpp>

namespace morphene { namespace schema { namespace detail {

//////////////////////////////////////////////
// account_name_type                        //
//////////////////////////////////////////////

struct schema_account_name_type_impl
   : public abstract_schema
{
   MORPHENE_SCHEMA_CLASS_BODY( schema_account_name_type_impl )
};

}

template<>
struct schema_reflect< morphene::protocol::account_name_type >
{
   typedef detail::schema_account_name_type_impl           schema_impl_type;
};

} }

namespace fc {

template<>
struct get_typename< morphene::protocol::account_name_type >
{
   static const char* name()
   {
      return "morphene::protocol::account_name_type";
   }
};

}
