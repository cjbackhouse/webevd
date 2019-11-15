#include <iostream>
#include <string>
#include <vector>

#include "canvas/Utilities/InputTag.h"
#include "gallery/Event.h"

#include "larcorealg/Geometry/GeometryCore.h"
#include "larcorealg/Geometry/GeometryBuilderStandard.h"
#include "dune/Geometry/DuneApaChannelMapAlg.h"
#include "larcorealg/Geometry/GeoObjectSorterStandard.h"
#include "dune/Geometry/GeoObjectSorterAPA.h"

#include "lardataalg/DetectorInfo/DetectorProperties.h"

#include "lardataalg/DetectorInfo/DetectorPropertiesStandard.h"

#include "dune/EVD/WebEVDServer.h"

// We use a function try block to catch and report on all exceptions.
int main(int argc, char** argv)
{
  if(argc == 1){
    std::cout << "Please specify the name of one or more art/ROOT input file(s) to read.\n";
    return 1;
  }

  evd::WebEVDServer<gallery::Event> server;

  fhicl::ParameterSet pset;
  pset.put("SurfaceY", 0);
  pset.put("Name", "dummy");
  geo::GeometryCore geom(pset);

  geo::GeometryBuilderStandard::Config cfg;
  geo::GeometryBuilderStandard builder(cfg);
  // TODO extract the gdml path from the input file
  geom.LoadGeometryFile("dummy", "/dune/app/users/bckhouse/dev/evd/build_slf6.x86_64/dunetpc/gdml/dune10kt_v1_1x2x6.gdml", builder);

  pset.put("ChannelsPerOpDet", 0);
  const geo::GeoObjectSorterAPA sorter(pset);
  std::shared_ptr<geo::DuneApaChannelMapAlg> cmap(new geo::DuneApaChannelMapAlg(pset));
  cmap->setSorter(sorter);
  geom.ApplyChannelMap(cmap);

  //  detinfo::DetectorPropertiesStandard detprop;
  // lar::DetectorPropertiesStandard::providers_type providers;
  // providers.set(lar::providerFrom<geo::Geometry>());
  // providers.set(lar::providerFrom<detinfo::LArPropertiesService>());
  // providers.set(lar::providerFrom<detinfo::DetectorClocksService>());
  // detprop->Setup(providers);

  // I haven't been able to figure out how to make a properly configured
  // DetectorProperties. For now just mock one up with the only function we
  // actually use.
  struct DummyProperties: public detinfo::DetectorProperties
  {
    double ConvertTicksToX(double ticks, const geo::PlaneID& planeid) const override {return 0.0802814*ticks;}

    double Efield(unsigned int planegap=0) const override {abort();}
    double DriftVelocity(double efield=0., double temperature=0.) const override {abort();}
    double BirksCorrection(double dQdX) const override {abort();}
    double ModBoxCorrection(double dQdX) const override {abort();}
    double ElectronLifetime() const override {abort();}
    double Density(double temperature) const override {abort();}
    double Temperature() const override {abort();}

    double Eloss(double mom, double mass, double tcut) const override {abort();}
    double ElossVar(double mom, double mass) const override {abort();}
    double       SamplingRate()      const override {abort();}
    double       ElectronsToADC()    const override {abort();}
    unsigned int NumberTimeSamples() const override {abort();}
    unsigned int ReadOutWindowSize() const override {abort();}
    int          TriggerOffset()     const override {abort();}
    double       TimeOffsetU()       const override {abort();}
    double       TimeOffsetV()       const override {abort();}
    double       TimeOffsetZ()       const override {abort();}
    double       ConvertXToTicks(double X, int p, int t, int c) const override {abort();}
    double       ConvertXToTicks(double X, geo::PlaneID const& planeid) const override {abort();}
    double       ConvertTicksToX(double ticks, int p, int t, int c) const override {abort();}
    double       GetXTicksOffset(int p, int t, int c) const override {abort();}
    double       GetXTicksOffset(geo::PlaneID const& planeid) const override {abort();}
    double       GetXTicksCoefficient() const override {abort();}
    double       GetXTicksCoefficient(int t, int c) const override {abort();}
    double       GetXTicksCoefficient(geo::TPCID const& tpcid) const override {abort();}
    double       ConvertTDCToTicks(double tdc) const override {abort();}
    double       ConvertTicksToTDC(double ticks) const override {abort();}
    bool SimpleBoundary()     const override {abort();}
  };

  const DummyProperties detprop;


  const std::vector<std::string> filenames(argv + 1, argv + argc);

  for(gallery::Event evt(filenames); !evt.atEnd(); evt.next()){
    const art::EventAuxiliary& aux = evt.eventAuxiliary();

    // Gallery presents the events in a different order for some reason
    if(aux.run() != 20000001 || aux.subRun() != 24 || aux.event() != 2302) continue;

    server.analyze(evt, &geom, &detprop);
    server.serve();

    /*
    art::ProcessHistory ph = evt.processHistory();
    std::cout << ph.size() << std::endl;
    for(const art::ProcessConfiguration& it: ph){
      //      int x = it;
      const fhicl::ParameterSetID psetid = it.parameterSetID();
      std::cout << psetid << std::endl;
      //      std::cout << it << std::endl;
      //      for(auto it2: it){
      //        std::cout << "  " << it2 << std::endl;
      //      }
    }
    //    fhicl::ParameterSet ps;
    //    evt.getProcessParameterSet("foo", ps);
    */
  }
 }
