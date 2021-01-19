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

#include "webevd/WebEVD/IEventSource.h"

class GalleryEventSource: public evd::IEventSource<gallery::Event>
{
public:
  GalleryEventSource() : fEvt(0) {}

  virtual ~GalleryEventSource() {delete fEvt;}

  virtual const gallery::Event* GetEvent(const std::string& fname,
                                         art::EventID tgt,
                                         art::EventID* next = 0,
                                         art::EventID* prev = 0) override
  {
    EnsureFile(fname);

    auto it = fSeekIdx.find(tgt);
    if(it == fSeekIdx.end()){
      std::cout << tgt << " not found in event index!" << std::endl;
      return 0;
    }

    fEvt->goToEntry(it->second);

    if(next && std::next(it) != fSeekIdx.end()) *next = std::next(it)->first;
    if(prev && it != fSeekIdx.begin())          *prev = std::prev(it)->first;

    return fEvt;
  }

  virtual const gallery::Event* FirstEvent(const std::string& fname,
                                           art::EventID* next = 0) override
  {
    EnsureFile(fname);
    return GetEvent(fname, fSeekIdx.begin()->first, next, 0);
  }

protected:
  void EnsureFile(const std::string& fname)
  {
    if(fEvt && fEvt->getTFile()->GetName() == fname) return;

    delete fEvt;
    fSeekIdx.clear();
    std::cout << "Opening " << fname << "..." << std::endl;
    fEvt = new gallery::Event({fname});
    FillIndex();
  }

  void FillIndex()
  {
    std::cout << "Filling index of event numbers..." << std::endl;

    fEvt->toBegin();
    for(; !fEvt->atEnd(); fEvt->next()){
      fSeekIdx[fEvt->eventAuxiliary().eventID()] = fEvt->eventEntry();
    }
    fEvt->toBegin();
    std::cout << "Done" << std::endl;
  }

  gallery::Event* fEvt;

  std::map<art::EventID, long long> fSeekIdx;
};

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

  if(filenames.size() > 1){
    std::cout << "TODO TODO used to handle multiple input files, now don't" << std::endl;
    return 1;
  }

  GalleryEventSource src;

  // Prototype for automatically configuring the geometry below. Dumps the
  // input file geometry configuration. Don't know how to do this in code yet
  // system(("config_dumper -S -f Geometry "+filenames[0]).c_str());

  evd::WebEVDServer<gallery::Event> server;

  std::string fclConfig = "#include \"services_dune.fcl\"\n";
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
    "BackTrackerService: @erase       \n"
    "PhotonBackTrackerService: @erase \n"
    "LArFFT: @erase                   \n"
    "TFileService: @erase             \n";

  ArtServiceHelper::load_services(fclConfig);

  const geo::GeometryCore* geom = art::ServiceHandle<geo::Geometry>()->provider();
  const detinfo::DetectorPropertiesData detprop = art::ServiceHandle<detinfo::DetectorPropertiesService>()->DataForJob();

  std::cout << geom->GDMLFile() << std::endl;

  std::cout << "Filling index of event numbers..." << std::endl;
  std::map<art::EventID, std::pair<long long, long long>> seek_index;
  for(gallery::Event evt(filenames); !evt.atEnd(); evt.next()){
    seek_index[evt.eventAuxiliary().eventID()] = std::make_pair(evt.fileEntry(), evt.eventEntry());
  }
  std::cout << "Done" << std::endl;

  // TODO used to be able to handle unspecified components
  const gallery::Event* evt = 0;
  const art::EventID kInvalidID(-1, -1, -1);
  art::EventID prev = kInvalidID, next = kInvalidID;
  if(tgt_run > 0)
    evt = src.GetEvent(filenames[0], art::EventID(tgt_run, tgt_subrun, tgt_evt), &next, &prev);
  else
    evt = src.FirstEvent(filenames[0], &next);

  while(evt){
    const evd::Result res = server.serve(*evt, geom, detprop);

    switch(res.code){
    case evd::kNEXT:
      std::cout << "Next event" << std::endl;
      evt = src.GetEvent(filenames[0], next, &next, &prev);
      break;

    case evd::kPREV:
      std::cout << "Previous event" << std::endl;
      evt = src.GetEvent(filenames[0], prev, &next, &prev);
      break;

    case evd::kSEEK:
      std::cout << "User requested seek to " << res.run << ":"<< res.subrun << ":" << res.event << std::endl;
      evt = src.GetEvent(filenames[0], art::EventID(res.run, res.subrun, res.event), &next, &prev);
      break;

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
