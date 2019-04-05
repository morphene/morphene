#pragma once

#include <morphene/protocol/types.hpp>

#include <fc/reflect/reflect.hpp>

namespace morphene { namespace protocol {
struct signed_transaction;
} } // morphene::protocol

namespace morphene { namespace plugins { namespace rc {

using morphene::protocol::account_name_type;
using morphene::protocol::signed_transaction;

account_name_type get_resource_user( const signed_transaction& tx );

} } } // morphene::plugins::rc
