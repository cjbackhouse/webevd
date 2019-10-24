// Christopher Backhouse - bckhouse@fnal.gov

#include <string>

#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art_root_io/TFileService.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "canvas/Persistency/Common/Ptr.h"

#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/SpacePoint.h"
#include "lardataobj/RecoBase/Wire.h"
#include "lardataobj/RecoBase/Track.h"

#include "larsim/MCCheater/BackTrackerService.h"

#include "lardataobj/RawData/RawDigit.h"
#include "lardataobj/RawData/raw.h" // Uncompress()

#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "TGraph.h"
#include "TPad.h"

#include <png.h>

namespace reco3d
{

class WebEVD: public art::EDAnalyzer
{
public:
  explicit WebEVD(const fhicl::ParameterSet& pset);

  void analyze(const art::Event& evt);
  void endJob() override;

protected:
  art::InputTag fSpacePointTag;

  std::string fHitLabel;
};

DEFINE_ART_MODULE(WebEVD)

// ---------------------------------------------------------------------------
WebEVD::WebEVD(const fhicl::ParameterSet& pset)
  : EDAnalyzer(pset),
    fSpacePointTag(art::InputTag(pset.get<std::string>("SpacePointLabel"),
                                 pset.get<std::string>("SpacePointInstanceLabel"))),
    fHitLabel(pset.get<std::string>("HitLabel"))
{
}

void WebEVD::endJob()
{
  char host[1024];
  gethostname(host, 1024);
  char* user = getlogin();

  std::cout << "\n------------------------------------------------------------\n" << std::endl;
  // std::cout << "firefox localhost:1080 &" << std::endl;
  // std::cout << "ssh -L 1080:localhost:8000 ";
  // std::cout << host << std::endl << std::endl;
  // std::cout << "Press Ctrl-C here when done" << std::endl;
  // system("busybox httpd -f -p 8000 -h web/");

  // E1071 is DUNE :)
  int port = 1071;

  // Search for an open port up-front
  while(system(TString::Format("ss -an | grep -q %d", port).Data()) == 0) ++port;
  
  while(true){
    std::cout << "firefox localhost:" << port << " &" << std::endl;
    std::cout << "ssh -L "
              << port << ":localhost:" << port << " "
              << user << "@" << host << std::endl << std::endl;
    std::cout << "Press Ctrl-C here when done" << std::endl;
    const int status = system(TString::Format("busybox httpd -f -p %d -h web/", port).Data());
  // system("cd web; python -m SimpleHTTPServer 8000");
  // system("cd web; python3 -m http.server 8000");

    std::cout << "\nStatus: " << status << std::endl;

    if(status == 256){
      // Deal with race condition by trying another port
      ++port;
      continue;
    }
    else{
      break;
    }
  }
}

struct PNGBytes
{
  PNGBytes(int w, int h) : width(w), height(h)
  {
    // Scale up to the next power of two
    //    for(width = 1; width < w; width *= 2);
    //    for(height = 1; height < h; height *= 2);
    //    std::cout << w << "x" << h << " -> " << width << "x" << height << std::endl;

    data = new png_byte*[height];
    for(int i = 0; i < height; ++i){
      data[i] = new png_byte[width*4];
      for(int j = 0; j < width*4; ++j){
        data[i][j] = 0;
        if(i >= h || j >= w*4) data[i][j] = 128;
      }
    }
  }

  png_byte& operator()(int x, int y, int c)
  {
    return data[y][x*4+c];
  }

  ~PNGBytes()
  {
    for(int i = 0; i < height; ++i) delete[] data[i];
    delete[] data;
  }

  int width, height;
  png_byte** data;
};

void WriteToPNG(const std::string& fname, const PNGBytes& bytes)
{
  FILE* fp = fopen(fname.c_str(), "wb");

  png_struct_def* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  auto info_ptr = png_create_info_struct(png_ptr);

  // Doesn't seem to have a huge effect
  //  png_set_compression_level(png_ptr, 9);

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, bytes.width, bytes.height,
               8/*bit_depth*/, PNG_COLOR_TYPE_RGBA/*GRAY*/, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  png_set_rows(png_ptr, info_ptr, bytes.data);
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  fclose(fp);

  png_destroy_write_struct(&png_ptr, &info_ptr);
}

void WebEVD::analyze(const art::Event& evt)
{
  //  const int width = 480; // TODO remove // max wire ID 512*8;
  const int height = 4492; // TODO somewhere to look up number of ticks?

  std::map<geo::PlaneID, PNGBytes*> plane_dig_imgs;
  std::map<geo::PlaneID, PNGBytes*> plane_wire_imgs;

  //  art::Handle<std::vector<recob::SpacePoint>> pts;
  //evt.getByLabel(fSpacePointTag, pts);
  std::vector<recob::SpacePoint>* pts = new std::vector<recob::SpacePoint>;

  std::ofstream outf("coords.js");
  outf << "var coords = [" << std::endl;
  for(const recob::SpacePoint& p: *pts){
    const double* xyz = p.XYZ();
    outf << "  [" << xyz[0] << ", " << xyz[1] << ", " << xyz[2] << "]," << std::endl;
  }
  outf << "];" << std::endl;

  //      outf << "var waves = [" << std::endl;

  art::Handle<std::vector<raw::RawDigit>> digs;
  evt.getByLabel("daq", digs);

  /*
    for(const raw::RawDigit& dig: *digs){
    //        std::cout << dig.NADC() << " " << dig.Samples() << " " << dig.Compression() << " " << dig.GetPedestal() << std::endl;
    
    // ChannelID_t Channel();

    raw::RawDigit::ADCvector_t adcs(dig.Samples());
    raw::Uncompress(dig.ADCs(), adcs, dig.Compression());

    outf << "  [ ";
    for(auto x: adcs) outf << (x ? x-dig.GetPedestal() : 0) << ", ";
    outf << " ]," << std::endl;
    }

    outf << "];" << std::endl;
  */

  art::ServiceHandle<geo::Geometry> geom;
  const detinfo::DetectorProperties* detprop = art::ServiceHandle<detinfo::DetectorPropertiesService>()->provider();

  for(unsigned int digIdx = 0; digIdx < digs->size()/*std::min(digs->size(), size_t(width))*/; ++digIdx){
    const raw::RawDigit& dig = (*digs)[digIdx];


    for(geo::WireID wire: geom->ChannelToWire(dig.Channel())){
      //          if(geom->SignalType(wire) != geo::kCollection) continue; // for now

      const geo::TPCID tpc(wire);
      const geo::PlaneID plane(wire);

      const geo::WireID w0 = geom->GetBeginWireID(plane);
      const unsigned int Nw = geom->Nwires(plane);

      if(plane_dig_imgs.count(plane) == 0){
        //            std::cout << "Create " << plane << " with " << Nw << std::endl;
        plane_dig_imgs[plane] = new PNGBytes(Nw, height);
      }

      //          std::cout << "Look up " << plane << std::endl;

      PNGBytes& bytes = *plane_dig_imgs[plane];

      //          std::cout << dig.Samples() << " and " << wire.Wire << std::endl;
      //        }
      //          if(geo::TPCID(wire) == tpc){
      //            xpos = detprop->ConvertTicksToX(hit->PeakTime(), wire);
      //          if (geom->SignalType(wire) == geo::kCollection) xpos += fXHitOffset;
      
      //          const geo::WireID w0 = geom->GetBeginWireID(tpc);
      //          const geo::WireID w0 = geom->GetBeginWireID(plane);

      raw::RawDigit::ADCvector_t adcs(dig.Samples());
      raw::Uncompress(dig.ADCs(), adcs, dig.Compression());

      for(unsigned int tick = 0; tick < std::min(adcs.size(), size_t(height)); ++tick){
        const int adc = adcs[tick] ? int(adcs[tick])-dig.GetPedestal() : 0;

        if(adc != 0){
          // alpha
          bytes(wire.Wire-w0.Wire, tick, 3) = std::min(abs(4*adc), 255);
          if(adc > 0){
            // red
            bytes(wire.Wire-w0.Wire, tick, 0) = 255;
          }
          else{
            // blue
            bytes(wire.Wire-w0.Wire, tick, 2) = 255;
          }
        }
      }
    }
  }

  art::Handle<std::vector<recob::Wire>> wires;
  evt.getByLabel("caldata", wires);

  //      std::cout << wires->size() << std::endl;
  for(unsigned int wireIdx = 0; wireIdx < wires->size(); ++wireIdx){
    for(geo::WireID wire: geom->ChannelToWire((*wires)[wireIdx].Channel())){
      //          if(geom->SignalType(wire) != geo::kCollection) continue; // for now

      const geo::TPCID tpc(wire);
      const geo::PlaneID plane(wire);

      const geo::WireID w0 = geom->GetBeginWireID(plane);
      const unsigned int Nw = geom->Nwires(plane);

      if(plane_wire_imgs.count(plane) == 0){
        plane_wire_imgs[plane] = new PNGBytes(Nw, height);
      }

      PNGBytes& bytes = *plane_wire_imgs[plane];

      const auto adcs = (*wires)[wireIdx].Signal();
      //        std::cout << "  " << adcs.size() << std::endl;
      for(unsigned int tick = 0; tick < std::min(adcs.size(), size_t(height)); ++tick){
        // green channel
        bytes(wire.Wire-w0.Wire, tick, 1) = 128; // dark green
        // alpha channel
        bytes(wire.Wire-w0.Wire, tick, 3) = std::max(0, std::min(int(10*adcs[tick]), 255));
      }
    }
  }

  /*
    outf << "var wires = [" << std::endl;
    
    for(const recob::Wire& wire: *wires){
    outf << "  [ ";
    for(auto x: wire.SignalROI()) outf << x << ", ";
    outf << " ]," << std::endl;
    }

    outf << "];" << std::endl;
  */

  art::Handle<std::vector<recob::Hit>> hits;
  evt.getByLabel("gaushit", hits);

  std::map<geo::PlaneID, std::vector<recob::Hit>> plane_hits;
  for(const recob::Hit& hit: *hits){
    const geo::WireID wire(hit.WireID());
    if(geom->SignalType(wire) != geo::kCollection) continue; // for now

    const geo::PlaneID plane(wire);

    plane_hits[plane].push_back(hit);
  }

  outf << "planes = {" << std::endl;
  for(auto it: plane_dig_imgs){
    const geo::PlaneID plane = it.first;
    const geo::PlaneGeo& planegeo = geom->Plane(plane);
    const int view = planegeo.View();
    const unsigned int nwires = planegeo.Nwires();
    const double pitch = planegeo.WirePitch();
    const TVector3 c = planegeo.GetCenter();

    const auto d = planegeo.GetIncreasingWireDirection();
    //    const auto dwire = planegeo.GetWireDirection();
    const TVector3 n = planegeo.GetNormalDirection();

    const int nticks = height; // HACK from earlier
    const double tick_pitch = detprop->ConvertTicksToX(1, plane) - detprop->ConvertTicksToX(0, plane);

    outf << "  \"" << plane << "\": {"
         << "view: " << view << ", "
         << "nwires: " << nwires << ", "
         << "pitch: " << pitch << ", "
         << "nticks: " << nticks << ", "
         << "tick_pitch: " << tick_pitch << ", "
         << "center: [" << c.X() << ", " << c.Y() << ", " << c.Z() << "], "
         << "across: [" << d.X() << ", " << d.Y() << ", " << d.Z() << "], "
         << "normal: [" << n.X() << ", " << n.Y() << ", " << n.Z() << "], "
         << "hits: [";
    for(const recob::Hit& hit: plane_hits[plane]){
      outf << "{wire: " << geo::WireID(hit.WireID()).Wire
           << ", tick: " << hit.PeakTime() << "}, ";
    }

    outf << "]}," << std::endl;
  }
  outf << "};" << std::endl;

  art::Handle<std::vector<recob::Track>> tracks;
  evt.getByLabel("pandora", tracks);

  outf << "tracks = [" << std::endl;
  for(const recob::Track& track: *tracks){
    outf << "  [\n    ";
    const recob::TrackTrajectory& traj = track.Trajectory();
    for(unsigned int j = traj.FirstValidPoint(); j <= traj.LastValidPoint(); ++j){
      const geo::Point_t pt = traj.LocationAtPoint(j);
      outf << "[" << pt.X() << ", " << pt.Y() << ", " << pt.Z() << "], ";
    }
    outf << "\n  ]," << std::endl;
  }
  outf << "];" << std::endl;


  art::Handle<std::vector<simb::MCParticle>> parts;
  evt.getByLabel("largeant", parts);

  outf << "truth_trajs = [" << std::endl;
  for(const simb::MCParticle& part: *parts){
    const int apdg = abs(part.PdgCode());
    if(apdg == 12 || apdg == 14 || apdg == 16) continue; // decay neutrinos
    outf << "  [\n    ";
    for(unsigned int j = 0; j < part.NumberTrajectoryPoints(); ++j){
      outf << "[" << part.Vx(j) << ", " << part.Vy(j) << ", " << part.Vz(j) << "], ";
    }
    outf << "\n  ]," << std::endl;
  }
  outf << "];" << std::endl;


  // Very cheesy speedup to parallelize png making
  for(auto it: plane_dig_imgs){
    std::cout << "Writing digits/" << it.first.toString() << std::endl;
    if(fork() == 0){
      WriteToPNG("digits/"+it.first.toString()+".png", *it.second);
      delete it.second;
      exit(0);
    }
  }

  for(auto it: plane_wire_imgs){
    std::cout << "Writing wires/" << it.first.toString() << std::endl;
    if(fork() == 0){
      WriteToPNG("wires/"+it.first.toString()+".png", *it.second);
      delete it.second;
      exit(0);
    }
  }
}

} // namespace
