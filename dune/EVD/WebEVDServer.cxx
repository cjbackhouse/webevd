// Chris Backhouse - bckhouse@fnal.gov

// art v3_03 (soon?) will inlude evt.getInputTags<>(). Until then, we can hack it in this dreadful way

// These guys are dragged in and don't like private->public
#include <any>
#include <sstream>
#include <mutex>

#define private public
#include "art/Framework/Principal/Event.h"
#undef private

std::vector<art::InputTag>
getInputTags(const art::Principal& p,
             art::ModuleContext const& mc,
             art::WrappedTypeID const& wrapped,
             art::SelectorBase const& sel,
             art::ProcessTag const& processTag)
{
  std::vector<art::InputTag> tags;
  cet::transform_all(p.findGroupsForProduct(mc, wrapped, sel, processTag, false), back_inserter(tags), [](auto const g) {
      return g.result()->productDescription().inputTag();
    });
  return tags;
}


template <typename PROD> std::vector<art::InputTag>
getInputTags(const art::Event& evt)
{
  return getInputTags(evt.principal_, evt.mc_, art::WrappedTypeID::make<PROD>(), art::MatchAllSelector{}, art::ProcessTag{"", evt.md_.processName()});
}

// end getInputTags hack


#include "dune/EVD/WebEVDServer.h"

#include "dune/EVD/PNGArena.h"

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

#include "gallery/Event.h"

#include <png.h>

namespace evd
{

template<class T> WebEVDServer<T>::WebEVDServer()
{
}

template<class T> WebEVDServer<T>::~WebEVDServer()
{
}

template<class T> void WebEVDServer<T>::serve()
{
  std::cout << "Temp dir: " << fTmp.DirectoryName() << std::endl;

  char host[1024];
  gethostname(host, 1024);
  char* user = getlogin();

  std::cout << "\n------------------------------------------------------------\n" << std::endl;

  // E1071 is DUNE :)
  int port = 1071;

  // Search for an open port up-front
  while(system(TString::Format("ss -an | grep -q %d", port).Data()) == 0) ++port;
  
  while(true){
    std::cout << "First run" << std::endl;
    std::cout << "ssh -L "
              << port << ":localhost:" << port << " "
              << user << "@" << host << std::endl << std::endl;
    std::cout << "and then navigate to localhost:" << port << " in your favorite browser." << std::endl << std::endl;
    std::cout << "Press Ctrl-C here when done." << std::endl;

    const int status = system(TString::Format("busybox httpd -f -p %d -h %s", port, fTmp.DirectoryName().c_str()).Data());
    // Alternative ways to start an HTTP server
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

template<class T> T& operator<<(T& os, const PNGView& v)
{
  os << "{blocks: [";
  for(unsigned int ix = 0; ix < v.blocks.size(); ++ix){
    for(unsigned int iy = 0; iy < v.blocks[ix].size(); ++iy){
      const png_byte* b = v.blocks[ix][iy];
      if(!b) continue;

      int dataidx = 0;
      for(unsigned int d = 0; d < v.arena.data.size(); ++d){
        if(b >= &v.arena.data[d].front() &&
           b <  &v.arena.data[d].front() + 4*v.arena.extent*v.arena.extent){
          dataidx = d;
          break;
        }
      }

      const int texdx = ((b-&v.arena.data[dataidx].front())/4)%v.arena.extent;
      const int texdy = ((b-&v.arena.data[dataidx].front())/4)/v.arena.extent;

      os << "{"
         << "dx: " << ix*PNGArena::kBlockSize << ", "
         << "dy: " << iy*PNGArena::kBlockSize << ", "
         << "fname: \"" << v.arena.name << "_" << dataidx << "\", "
         << "texdim: " << v.arena.extent << ", "
         << "texdx: " << texdx << ", "
         << "texdy: " << texdy
         << "}, ";
    }
  }
  os << "]}";
  return os;
}


class JSONFormatter
{
public:
  JSONFormatter(std::ofstream& os) : fStream(os) {}

  template<class T> JSONFormatter& operator<<(const T& x){fStream << x; return *this;}

  JSONFormatter& operator<<(const TVector3& v)
  {
    fStream << "["
            << v.X() << ", "
            << v.Y() << ", "
            << v.Z() << "]";
    return *this;
  }

protected:
  std::ofstream& fStream;
};


template<class T, class U> struct HandleType;

template<class U> struct HandleType<art::Event, U>
{
  typedef art::Handle<std::vector<U>> type;
};

template<class U> struct HandleType<gallery::Event, U>
{
  typedef gallery::Handle<std::vector<U>> type;
};


template<class T> void WebEVDServer<T>::
analyze(const T& evt,
        const geo::GeometryCore* geom,
        const detinfo::DetectorProperties* detprop)
{
  /*
  std::cout << getInputTags<std::vector<recob::Track>>(evt).size() << " tracks "
            << getInputTags<std::vector<recob::Wire>>(evt).size() << " wires"
            << std::endl;

  for(auto x: getInputTags<std::vector<recob::Track>>(evt)) std::cout << x << std::endl;
  */

  // Needs art v3_03 (soon?)
  //  std::vector<art::InputTag> tags = evt.getInputTags<recob::Track>();
  //  evt.getInputTags<recob::Track>();

  // Alternate syntax
  //  auto const tokens = evt.getProductTokens<recob::Track>();
  //  for (auto const& token : tokens) {
  //    auto const h = event.getValidHandle(token);
  //  }

  const int height = 4492; // TODO somewhere to look up number of ticks?

  PNGArena arena("arena");

  std::map<geo::PlaneID, PNGView*> plane_dig_imgs;
  std::map<geo::PlaneID, PNGView*> plane_wire_imgs;

  typename HandleType<T, recob::SpacePoint>::type pts;
  evt.getByLabel("pandora"/*fSpacePointTag*/, pts);

  char webdir[PATH_MAX];
  realpath("web/", webdir);

  fTmp.symlink(webdir, "evd.js");
  fTmp.symlink(webdir, "index.html");
  fTmp.symlink(webdir, "httpd.conf");

  std::ofstream outf = fTmp.ofstream("coords.js");

  JSONFormatter json(outf);

  json << "var coords = [\n";
  for(const recob::SpacePoint& p: *pts){
    json << TVector3(p.XYZ()) << ",\n";
  }
  json << "];\n\n";

  typename HandleType<T, raw::RawDigit>::type digs;
  evt.getByLabel("daq", digs);

  for(unsigned int digIdx = 0; digIdx < digs->size(); ++digIdx){
    const raw::RawDigit& dig = (*digs)[digIdx];

    for(geo::WireID wire: geom->ChannelToWire(dig.Channel())){
      const geo::TPCID tpc(wire);
      const geo::PlaneID plane(wire);

      const geo::WireID w0 = geom->GetBeginWireID(plane);
      const unsigned int Nw = geom->Nwires(plane);

      if(plane_dig_imgs.count(plane) == 0){
        plane_dig_imgs[plane] = new PNGView(arena, Nw, height);
      }

      PNGView& bytes = *plane_dig_imgs[plane];

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

  typename HandleType<T, recob::Wire>::type wires;
  evt.getByLabel("caldata", wires);

  for(unsigned int wireIdx = 0; wireIdx < wires->size(); ++wireIdx){
    for(geo::WireID wire: geom->ChannelToWire((*wires)[wireIdx].Channel())){
      const geo::TPCID tpc(wire);
      const geo::PlaneID plane(wire);

      const geo::WireID w0 = geom->GetBeginWireID(plane);
      const unsigned int Nw = geom->Nwires(plane);

      if(plane_wire_imgs.count(plane) == 0){
        plane_wire_imgs[plane] = new PNGView(arena, Nw, height);
      }

      PNGView& bytes = *plane_wire_imgs[plane];

      const auto adcs = (*wires)[wireIdx].Signal();
      for(unsigned int tick = 0; tick < std::min(adcs.size(), size_t(height)); ++tick){
        if(adcs[tick] <= 0) continue;

        // green channel
        bytes(wire.Wire-w0.Wire, tick, 1) = 128; // dark green
        // alpha channel
        bytes(wire.Wire-w0.Wire, tick, 3) = std::max(0, std::min(int(10*adcs[tick]), 255));
      }
    }
  }

  typename HandleType<T, recob::Hit>::type hits;
  evt.getByLabel("gaushit", hits);

  std::map<geo::PlaneID, std::vector<recob::Hit>> plane_hits;
  for(const recob::Hit& hit: *hits){
    // Would possibly be right for disambiguated hits?
    //    const geo::WireID wire(hit.WireID());

    for(geo::WireID wire: geom->ChannelToWire(hit.Channel())){
      const geo::PlaneID plane(wire);

      // Correct for disambiguated hits
      //      plane_hits[plane].push_back(hit);

      // Otherwise we have to update the wire number
      plane_hits[plane].emplace_back(hit.Channel(), hit.StartTick(), hit.EndTick(), hit.PeakTime(), hit.SigmaPeakTime(), hit.RMS(), hit.PeakAmplitude(), hit.SigmaPeakAmplitude(), hit.SummedADC(), hit.Integral(), hit.SigmaIntegral(), hit.Multiplicity(), hit.LocalIndex(), hit.GoodnessOfFit(), hit.DegreesOfFreedom(), hit.View(), hit.SignalType(), wire);
    }
  }

  json << "planes = {\n";
  for(auto it: plane_dig_imgs){
    const geo::PlaneID plane = it.first;
    const geo::PlaneGeo& planegeo = geom->Plane(plane);
    const int view = planegeo.View();
    const unsigned int nwires = planegeo.Nwires();
    const double pitch = planegeo.WirePitch();
    const TVector3 c = planegeo.GetCenter();
    const PNGView* dig_view = it.second;

    const auto d = planegeo.GetIncreasingWireDirection();
    const TVector3 n = planegeo.GetNormalDirection();

    const int nticks = height; // HACK from earlier
    const double tick_pitch = detprop->ConvertTicksToX(1, plane) - detprop->ConvertTicksToX(0, plane);

    PNGView* wire_view = plane_wire_imgs.count(plane) ? plane_wire_imgs[plane] : 0;

    json << "  \"" << plane << "\": {"
         << "view: " << view << ", "
         << "nwires: " << nwires << ", "
         << "pitch: " << pitch << ", "
         << "nticks: " << nticks << ", "
         << "tick_pitch: " << tick_pitch << ", "
         << "center: " << c << ", "
         << "across: " << d << ", "
         << "normal: " << n << ", ";

    json << "digs: " << *dig_view << ", ";
    if(wire_view) json << "wires: " << *wire_view << ", ";

    json << "hits: [";
    for(const recob::Hit& hit: plane_hits[plane]){
      json << "{wire: " << geo::WireID(hit.WireID()).Wire
           << ", tick: " << hit.PeakTime()
           << ", rms: " << hit.RMS() << "}, ";
    }

    json << "]},\n";
  }
  json << "};\n";

  typename HandleType<T, recob::Track>::type tracks;
  evt.getByLabel("pandora", tracks);

  json << "tracks = [\n";
  for(const recob::Track& track: *tracks){
    json << "  [\n    ";
    const recob::TrackTrajectory& traj = track.Trajectory();
    for(unsigned int j = traj.FirstValidPoint(); j <= traj.LastValidPoint(); ++j){
      if(!traj.HasValidPoint(j)) continue;
      const geo::Point_t pt = traj.LocationAtPoint(j);
      json << "[" << pt.X() << ", " << pt.Y() << ", " << pt.Z() << "], ";
    }
    json << "\n  ],\n";
  }
  json << "];\n";


  typename HandleType<T, simb::MCParticle>::type parts;
  evt.getByLabel("largeant", parts);

  json << "truth_trajs = [\n";
  for(const simb::MCParticle& part: *parts){
    const int apdg = abs(part.PdgCode());
    if(apdg == 12 || apdg == 14 || apdg == 16) continue; // decay neutrinos
    json << "  [\n    ";
    for(unsigned int j = 0; j < part.NumberTrajectoryPoints(); ++j){
      json << "[" << part.Vx(j) << ", " << part.Vy(j) << ", " << part.Vz(j) << "], ";
    }
    json << "\n  ],\n";
  }
  json << "];\n";

  std::cout << "Writing " << arena.name << std::endl;
  WriteToPNGWithMipMaps(fTmp, arena.name, arena);

  // TODO use unique_ptr?
  for(auto it: plane_dig_imgs) delete it.second;
  for(auto it: plane_wire_imgs) delete it.second;
}

template class WebEVDServer<art::Event>;
template class WebEVDServer<gallery::Event>;

} // namespace

