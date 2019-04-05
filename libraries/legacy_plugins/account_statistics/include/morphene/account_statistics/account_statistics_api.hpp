#pragma once

#include <morphene/account_statistics/account_statistics_plugin.hpp>

#include <fc/api.hpp>

namespace morphene { namespace app {
   struct api_context;
} }

namespace morphene { namespace account_statistics {

namespace detail
{
   class account_statistics_api_impl;
}

class account_statistics_api
{
   public:
      account_statistics_api( const morphene::app::api_context& ctx );

      void on_api_startup();

   private:
      std::shared_ptr< detail::account_statistics_api_impl > _my;
};

} } // morphene::account_statistics

FC_API( morphene::account_statistics::account_statistics_api, )