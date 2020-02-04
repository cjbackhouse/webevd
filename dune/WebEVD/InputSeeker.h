#ifndef INPUTSEEKER_H
#define INPUTSEEKER_H

#include "art/Framework/Services/Registry/ServiceMacros.h"

namespace art{class InputSource; class RootInput; class Worker;}

namespace evd
{
  /// This is obviously a hack, but it's modeled on what EventDisplayBase does
  class InputSeeker
  {
  public:
    InputSeeker(const fhicl::ParameterSet& pset, art::ActivityRegistry& reg);

    void seekToEvent(int offset);
    void seekToEvent(art::EventID evt);

  protected:
    void postBeginJobWorkers(art::InputSource* src,
                             const std::vector<art::Worker*>& workers);

    art::RootInput* fSrc;
  };

}

DECLARE_ART_SERVICE(evd::InputSeeker, LEGACY)

#endif
