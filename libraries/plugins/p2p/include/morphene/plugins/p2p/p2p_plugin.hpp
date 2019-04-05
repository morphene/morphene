#pragma once

#include <morphene/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

#define MORPHENE_P2P_PLUGIN_NAME "p2p"

namespace morphene { namespace plugins { namespace p2p {
namespace bpo = boost::program_options;

namespace detail { class p2p_plugin_impl; }

class p2p_plugin : public appbase::plugin<p2p_plugin> {
public:
   APPBASE_PLUGIN_REQUIRES((plugins::chain::chain_plugin))

   p2p_plugin();
   virtual ~p2p_plugin();

   virtual void set_program_options(bpo::options_description &,
                                    bpo::options_description &config_file_options) override;

   static const std::string& name() { static std::string name = MORPHENE_P2P_PLUGIN_NAME; return name; }

   virtual void plugin_initialize(const bpo::variables_map& options) override;
   virtual void plugin_startup() override;
   virtual void plugin_shutdown() override;

   void broadcast_block( const morphene::protocol::signed_block& block );
   void broadcast_transaction( const morphene::protocol::signed_transaction& tx );
   void set_block_production( bool producing_blocks );

private:
   std::unique_ptr< detail::p2p_plugin_impl > my;
};

} } } // morphene::plugins::p2p
