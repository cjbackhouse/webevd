// Chris Backhouse - bckhouse@fnal.gov

#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/EDAnalyzer.h"

#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "larcore/Geometry/Geometry.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "dune/EVD/WebEVDServer.h"
#include "dune/EVD/InputSeeker.h"

namespace evd
{

class WebEVD: public art::EDAnalyzer
{
public:
  explicit WebEVD(const fhicl::ParameterSet& pset);

  void analyze(const art::Event& evt) override;
  void endJob() override;

protected:
  evd::WebEVDServer<art::Event> fServer;

  art::ServiceHandle<geo::Geometry> fGeom;
  const detinfo::DetectorProperties* fDetProp;

  bool fSeeking;
  unsigned int fSeekRun, fSeekSubRun, fSeekEvent;
};

DEFINE_ART_MODULE(WebEVD)

// ---------------------------------------------------------------------------
WebEVD::WebEVD(const fhicl::ParameterSet& pset)
  : EDAnalyzer(pset), fDetProp(art::ServiceHandle<detinfo::DetectorPropertiesService>()->provider()), fSeeking(false)
{
}

void WebEVD::endJob()
{
  std::cout << "Ran out of events. Goodbye!" << std::endl;
}

void WebEVD::analyze(const art::Event& evt)
{
  if(fSeeking){
    if(fSeekRun != evt.run() ||
       fSeekSubRun != evt.subRun() ||
       fSeekEvent != evt.event()) return;
    fSeeking = false; // found it
  }

  const Result res = fServer.serve(evt, fGeom.get(), fDetProp);

  switch(res.code){
  case kNEXT:
    std::cout << "Next clicked in GUI. Going to next event" << std::endl;
    // nothing, allow art to proceed to next
    return;

  case kPREV:
    std::cout << "Prev clicked in GUI. Going to previous event" << std::endl;
    // Because we will automatically increment by one
    art::ServiceHandle<InputSeeker>()->seekToEvent(-2);
    return;

  case kQUIT:
    // TODO give fServer a chance to clean up
    std::cout << "Quit clicked in GUI. Goodbye!" << std::endl;
    exit(0);

  case kERROR:
    std::cout << "Error. Quitting" << std::endl;
    exit(1);

  case kSEEK:
    std::cout << "User requested seek to " << res.run << ":"<< res.subrun << ":" << res.event << std::endl;
    fSeeking = true;
    fSeekRun = res.run;
    fSeekSubRun = res.subrun;
    fSeekEvent = res.event;
    return;

  default:
    std::cout << "Unhandled result code " << res.code << "!" << std::endl;
    abort();
  }
}

} // namespace
