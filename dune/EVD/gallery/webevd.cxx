#include <iostream>
#include <string>
#include <vector>

#include "canvas/Utilities/InputTag.h"
#include "gallery/Event.h"

#include "dune/ArtSupport/ArtServiceHelper.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "larcore/Geometry/Geometry.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

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

  const std::vector<std::string> filenames(argv + 1, argv + argc);

  // Dump the input file geometry configuration. Don't know how to do this in
  // code yet
  system(("config_dumper -S -f Geometry "+filenames[0]).c_str());


  evd::WebEVDServer<gallery::Event> server;

  // TODO why is it necessary to manually specify the GDML?
  // TODO can we find a way to extract it from the job configuration stored in the file?
  ArtServiceHelper::load_services(
"#include \"services_dune.fcl\"            \n"
"services:{@table::dunefd_services}        \n"
"services.BackTrackerService: @erase       \n"
"services.PhotonBackTrackerService: @erase \n"
"services.LArFFT: @erase                   \n"
"services.TFileService: @erase             \n"
"services.Geometry.GDML: \"dune10kt_v1_1x2x6.gdml\" "
);

  const geo::GeometryCore* geom = art::ServiceHandle<geo::Geometry>()->provider();
  const detinfo::DetectorProperties* detprop = art::ServiceHandle<detinfo::DetectorPropertiesService>()->provider();

  std::cout << geom->GDMLFile() << std::endl;

  for(gallery::Event evt(filenames); !evt.atEnd(); evt.next()){
    const art::EventAuxiliary& aux = evt.eventAuxiliary();

    if((tgt_run >= 0 && aux.run() != tgt_run) ||
       (tgt_subrun >= 0 && aux.subRun() != tgt_subrun) ||
       (tgt_evt >= 0 && aux.event() != tgt_evt)) continue;

    server.analyze(evt, geom, detprop);

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
