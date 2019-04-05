#pragma once

#include <morphene/chain/evaluator.hpp>

#include <morphene/private_message/private_message_operations.hpp>
#include <morphene/private_message/private_message_plugin.hpp>

namespace morphene { namespace private_message {

MORPHENE_DEFINE_PLUGIN_EVALUATOR( private_message_plugin, morphene::private_message::private_message_plugin_operation, private_message )

} }
