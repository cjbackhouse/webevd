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

void usage()
{
  std::cout << "Usage: webevd [-e [[RUN:]SUBRUN:]EVT] events.root [more_events.root...]" << std::endl;
  exit(1);
}

// We use a function try block to catch and report on all exceptions.
int main(int argc, char** argv)
{
  if(argc == 1) usage();

  int tgt_run = -1, tgt_subrun = -1, tgt_evt = -1;
  if(argc >= 2 && std::string(argv[1]) == "-e"){
    if(argc <= 3) usage();

    std::vector<int> toks;

    char* ptok = strtok(argv[2], ":");
    while(ptok){
      toks.push_back(atoi(ptok));
      ptok = strtok(0, ":");
    }

    if(toks.empty() || toks.size() > 3) usage();

    tgt_evt = toks[toks.size()-1];
    std::cout << "Will look for event " << tgt_evt;
    if(toks.size() > 1){
      tgt_subrun = toks[toks.size()-2];
      std::cout << " in subrun " << tgt_subrun;
    }
    if(toks.size() > 2){
      tgt_run = toks[toks.size()-3];
      std::cout << " in run " << tgt_run;
    }
    std::cout << std::endl;

    argc -= 2;
    argv += 2;
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

    if((tgt_run >= 0 && aux.run() != tgt_run) ||
       (tgt_subrun >= 0 && aux.subRun() != tgt_subrun) ||
       (tgt_evt >= 0 && aux.event() != tgt_evt)) continue;

    server.analyze(evt, &geom, &detprop);

    std::cout << "\nDisplaying event " << aux.run() << ":" << aux.subRun() << ":" << aux.event() << std::endl << std::endl;

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
