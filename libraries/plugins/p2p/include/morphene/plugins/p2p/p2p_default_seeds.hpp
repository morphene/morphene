#pragma once

#include <vector>

namespace morphene{ namespace plugins { namespace p2p {

#ifdef IS_TEST_NET
const std::vector< std::string > default_seeds;
#else
const std::vector< std::string > default_seeds = {
   "localhost:2001",  // localhost
};
#endif

} } } // morphene::plugins::p2p
