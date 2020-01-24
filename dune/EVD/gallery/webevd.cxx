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
  std::cout << "Usage: webevd [-d DET] [-e [[RUN:]SUBRUN:]EVT] events.root [more_events.root...]" << std::endl;
  exit(1);
}

// We use a function try block to catch and report on all exceptions.
int main(int argc, char** argv)
{
  if(argc == 1) usage();
  --argc;
  ++argv;

  bool detKnown = false;
  bool isFD = false;

  int tgt_run = -1, tgt_subrun = -1, tgt_evt = -1;

  while(argc >= 1 && argv[0][0] == '-'){
    if(std::string(argv[0]) == "-d"){
      if(argc < 2) usage();

      const std::string d(argv[1]);

      detKnown = true;

      if(d == "fd" || d == "fardet" || d == "dune10kt") isFD = true;
      else if(d == "pd" || d == "protodune" || d == "np04") isFD = false;
      else{
        std::cout << "Unrecognized detector '" << d << "'" << std::endl;
        return 1;
      }

      argc -= 2;
      argv += 2;
    }

    if(std::string(argv[0]) == "-e"){
      if(argc < 2) usage();

      std::vector<int> toks;

      char* ptok = strtok(argv[1], ":");
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
  } // end while options remain

  const std::vector<std::string> filenames(argv, argv + argc);

  if(!detKnown){
    if(filenames[0].find("dune10kt") != std::string::npos ||
       filenames[0].find("1x2x6") != std::string::npos){
      isFD = true;
      detKnown = true;
    }
    else if(filenames[0].find("np04") != std::string::npos){
      isFD = false;
      detKnown = true;
    }
    else{
      std::cout << "Unable to auto-detect detector from filename. Please specify it explicitly with -d" << std::endl;
      return 1;
    }

    std::cout << "Auto-detected geometry as ";
    if(isFD) std::cout << "far detector"; else std::cout << "ProtoDUNE SP";
    std::cout << std::endl;
  }

  // Prototype for automatically configuring the geometry below. Dumps the
  // input file geometry configuration. Don't know how to do this in code yet
  // system(("config_dumper -S -f Geometry "+filenames[0]).c_str());

  evd::WebEVDServer<gallery::Event> server;

  std::string fclConfig = "#include \"services_dune.fcl\"\n";
  if(isFD){
    fclConfig +=
      "services:{@table::dunefd_services}\n"
      "services.Geometry.GDML: \"dune10kt_v1_1x2x6.gdml\"\n";
    // TODO why is it necessary to manually specify the GDML?
  }
  else{
    fclConfig +=
      "services:{@table::protodune_services}\n";
  }

  fclConfig +=
    "services.BackTrackerService: @erase       \n"
    "services.PhotonBackTrackerService: @erase \n"
    "services.LArFFT: @erase                   \n"
    "services.TFileService: @erase             \n";

  ArtServiceHelper::load_services(fclConfig);

  const geo::GeometryCore* geom = art::ServiceHandle<geo::Geometry>()->provider();
  const detinfo::DetectorProperties* detprop = art::ServiceHandle<detinfo::DetectorPropertiesService>()->provider();

  std::cout << geom->GDMLFile() << std::endl;

  for(gallery::Event evt(filenames); !evt.atEnd();){
    const art::EventAuxiliary& aux = evt.eventAuxiliary();

    if((tgt_run    >= 0 && aux.run()    != tgt_run   ) ||
       (tgt_subrun >= 0 && aux.subRun() != tgt_subrun) ||
       (tgt_evt    >= 0 && aux.event()  != tgt_evt   )) continue;

    std::cout << "\nDisplaying event " << aux.run() << ":" << aux.subRun() << ":" << aux.event() << std::endl << std::endl;

    const evd::EResult res = server.serve(evt, geom, detprop);

    if(res == evd::kNEXT){
      std::cout << "Next event" << std::endl;
      evt.next();
    }
    else if(res == evd::kPREV){
      std::cout << "Previous event" << std::endl;
      evt.previous();
    }
    else if(res == evd::kQUIT){
      std::cout << "Quit" << std::endl;
      return 0;
    }
    else if(res == evd::kERROR){
      std::cout << "Error" << std::endl;
      return 1;
    }
  }

  std::cout << "End of file" << std::endl;
  return 0;
}
