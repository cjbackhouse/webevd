// Chris Backhouse - bckhouse@fnal.gov

#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/EDAnalyzer.h"

#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "larcore/Geometry/Geometry.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "dune/EVD/WebEVDServer.h"

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
};

DEFINE_ART_MODULE(WebEVD)

// ---------------------------------------------------------------------------
WebEVD::WebEVD(const fhicl::ParameterSet& pset)
  : EDAnalyzer(pset), fDetProp(art::ServiceHandle<detinfo::DetectorPropertiesService>()->provider())
{
}

void WebEVD::endJob()
{
  std::cout << "Ran out of events. Goodbye!" << std::endl;
}

void WebEVD::analyze(const art::Event& evt)
{
  const EResult res = fServer.serve(evt, fGeom.get(), fDetProp);

  if(res == kNEXT){
    std::cout << "Next clicked in GUI. Going to next event" << std::endl;
    // nothing, fall through to next
  }
  else if(res == kPREV){
    std::cout << "Previous button unimplemented - doing Next" << std::endl;
  }
  else if(res == kQUIT){
    // TODO cleanups
    std::cout << "Quit clicked in GUI. Goodbye!" << std::endl;
    exit(0);
  }
  else if(res == kERROR){
    std::cout << "Error. Quitting" << std::endl;
    exit(1);
  }
}

} // namespace
