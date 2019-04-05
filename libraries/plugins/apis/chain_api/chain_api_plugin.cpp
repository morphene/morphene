#include <morphene/plugins/chain_api/chain_api_plugin.hpp>
#include <morphene/plugins/chain_api/chain_api.hpp>


namespace morphene { namespace plugins { namespace chain {

chain_api_plugin::chain_api_plugin() {}
chain_api_plugin::~chain_api_plugin() {}

void chain_api_plugin::set_program_options( options_description& cli, options_description& cfg ) {}

void chain_api_plugin::plugin_initialize( const variables_map& options )
{
   api = std::make_shared< chain_api >();
}

void chain_api_plugin::plugin_startup() {}
void chain_api_plugin::plugin_shutdown() {}

} } } // morphene::plugins::chain
