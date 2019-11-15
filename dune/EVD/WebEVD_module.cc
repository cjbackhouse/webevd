// Chris Backhouse - bckhouse@fnal.gov

#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/EDAnalyzer.h"

#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "larcore/Geometry/Geometry.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "dune/EVD/WebEVDServer.h"

namespace reco3d
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
  fServer.serve();
}

void WebEVD::analyze(const art::Event& evt)
{
  fServer.analyze(evt, fGeom.get(), fDetProp);
}

} // namespace
