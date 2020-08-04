#include <iostream>
#include <string>
#include <vector>

#include "canvas/Utilities/InputTag.h"
#include "gallery/Event.h"

#include "webevd/ArtSupport/ArtServiceHelper.h"

#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "larcore/Geometry/Geometry.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "webevd/WebEVD/WebEVDServer.h"
#include "webevd/WebEVD/ColorRamp.h"

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
      continue;
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
      continue;
    }

    if(std::string(argv[0]) == "--help" ||
       std::string(argv[0]) == "-h") usage();

    // Didn't match any of the conditions above
    std::cout << "Unknown argument " << argv[0] << std::endl;
    usage();
  } // end while options remain

  if(argc == 0){
    std::cout << "Must specify at least one input file" << std::endl;
    usage();
  }

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

  std::string fclConfig =
    "#include \"services_dune.fcl\"\n"
    "#include \"ColorRamp.fcl\"\n";

  if(isFD){
    fclConfig +=
      "@table::dunefd_services\n"
      "Geometry.GDML: \"dune10kt_v1_1x2x6.gdml\"\n";
    // TODO why is it necessary to manually specify the GDML?
  }
  else{
    fclConfig +=
      "@table::protodune_services\n";
  }

  fclConfig +=
    "ColorRamp: @local::standard_colorramp \n"
    "BackTrackerService: @erase       \n"
    "PhotonBackTrackerService: @erase \n"
    "LArFFT: @erase                   \n"
    "TFileService: @erase             \n";

  ArtServiceHelper::load_services(fclConfig);

  const geo::GeometryCore* geom = art::ServiceHandle<geo::Geometry>()->provider();
  const detinfo::DetectorPropertiesData detprop = art::ServiceHandle<detinfo::DetectorPropertiesService>()->DataForJob();
  const evd::ColorRamp* ramp = art::ServiceHandle<evd::ColorRamp>().get();

  std::cout << geom->GDMLFile() << std::endl;

  std::cout << "Filling index of event numbers..." << std::endl;
  std::map<art::EventID, std::pair<long long, long long>> seek_index;
  for(gallery::Event evt(filenames); !evt.atEnd(); evt.next()){
    seek_index[evt.eventAuxiliary().eventID()] = std::make_pair(evt.fileEntry(), evt.eventEntry());
  }
  std::cout << "Done" << std::endl;

  for(gallery::Event evt(filenames); !evt.atEnd();){
    const art::EventAuxiliary& aux = evt.eventAuxiliary();

    if(tgt_run >= 0 || tgt_subrun >= 0 || tgt_evt >= 0){
      if((tgt_run    >= 0 && int(aux.run())    != tgt_run   ) ||
         (tgt_subrun >= 0 && int(aux.subRun()) != tgt_subrun) ||
         (tgt_evt    >= 0 && int(aux.event())  != tgt_evt   )){
        evt.next();
        continue;
      }
    }
    else{
      // We must have arrived, stop seeking
      tgt_run = tgt_subrun = tgt_evt = -1;
    }

    std::cout << "\nDisplaying event " << aux.run() << ":" << aux.subRun() << ":" << aux.event() << std::endl << std::endl;

    const evd::Result res = server.serve(evt, geom, detprop, ramp);

    switch(res.code){
    case evd::kNEXT:
      std::cout << "Next event" << std::endl;
      evt.next();
      break;

    case evd::kPREV:
      std::cout << "Previous event" << std::endl;
      evt.previous();
      break;

    case evd::kSEEK:
      std::cout << "User requested seek to " << res.run << ":"<< res.subrun << ":" << res.event << std::endl;
      {
        const art::EventID tgt(res.run, res.subrun, res.event);
        if(seek_index.find(tgt) == seek_index.end()){
          std::cout << tgt << " not found in event index! Abort." << std::endl;
          return 1;
        }

        while(std::make_pair(evt.fileEntry(), evt.eventEntry()) < seek_index[tgt]) evt.next();
        while(std::make_pair(evt.fileEntry(), evt.eventEntry()) > seek_index[tgt]) evt.previous();
      }
      continue;

    case evd::kQUIT:
      std::cout << "Quit" << std::endl;
      return 0;

    case evd::kERROR:
      std::cout << "Error" << std::endl;
      return 1;

    default:
      std::cout << "Unrecognized result code " << res.code << "!" << std::endl;
      abort();
    }
  }

  std::cout << "End of file" << std::endl;
  return 0;
}
