#include "dune/WebEVD/InputSeeker.h"

#include "art/Framework/Services/Registry/ActivityRegistry.h"

#include "art_root_io/RootInput.h"

namespace evd
{
  //--------------------------------------------------------------------
  InputSeeker::InputSeeker(const fhicl::ParameterSet&,
                           art::ActivityRegistry& reg)
  {
    reg.sPostBeginJobWorkers.watch(this, &InputSeeker::postBeginJobWorkers);
  }

  //--------------------------------------------------------------------
  void InputSeeker::postBeginJobWorkers(art::InputSource* src,
                                        const std::vector<art::Worker*>&)
  {
    fSrc = dynamic_cast<art::RootInput*>(src);
    if(!fSrc){
      std::cout << "InputSource is not RootInput -- will not be able to seek backward" << std::endl;
    }
  }

  //--------------------------------------------------------------------
  void InputSeeker::seekToEvent(int offset)
  {
    if(!fSrc){
      std::cout << "Unable to seek" << std::endl;
      return;
    }
    fSrc->seekToEvent(offset);
  }

  //--------------------------------------------------------------------
  void InputSeeker::seekToEvent(art::EventID evt)
  {
    if(!fSrc){
      std::cout << "Unable to seek" << std::endl;
      return;
    }
    fSrc->seekToEvent(evt);
  }
}

DEFINE_ART_SERVICE(evd::InputSeeker)
