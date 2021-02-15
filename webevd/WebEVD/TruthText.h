#ifndef WEBEVD_TRUTHTEXT_H
#define WEBEVD_TRUTHTEXT_H

#include <string>

namespace simb{class MCTruth;}

namespace evd
{
  std::string MCTruthShortText(const simb::MCTruth& truth);
}

#endif
