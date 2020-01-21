// Chris Backhouse - bckhouse@fnal.gov

#include "dune/EVD/WebEVDServer.h"

#include "dune/EVD/PNGArena.h"

#include <string>

#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Principal/Handle.h"

#include "art/Framework/Principal/Event.h"
#include "gallery/Event.h"

#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/SpacePoint.h"
#include "lardataobj/RecoBase/Wire.h"
#include "lardataobj/RecoBase/Track.h"
#include "lardataobj/RecoBase/Vertex.h"

#include "nusimdata/SimulationBase/MCParticle.h"

#include "lardataobj/RawData/RawDigit.h"
#include "lardataobj/RawData/raw.h" // Uncompress()

#include "larcorealg/Geometry/GeometryCore.h"
#include "lardataalg/DetectorInfo/DetectorProperties.h"

namespace std{
  bool operator<(const art::InputTag& a, const art::InputTag& b)
  {
    return (std::make_tuple(a.label(), a.instance(), a.process()) <
            std::make_tuple(b.label(), b.instance(), b.process()));
  }
}

namespace evd
{

// ----------------------------------------------------------------------------
template<class T> WebEVDServer<T>::WebEVDServer()
{
}

// ----------------------------------------------------------------------------
template<class T> WebEVDServer<T>::~WebEVDServer()
{
}

// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
class JSONFormatter
{
public:
  JSONFormatter(std::ofstream& os) : fStream(os) {}

  template<class T> JSONFormatter& operator<<(const T& x)
  {
    static_assert(std::is_arithmetic_v<T> ||
                  std::is_enum_v<T> ||
                  std::is_same_v<T, std::string>);
    fStream << x;
    return *this;
  }

  JSONFormatter& operator<<(const char* x)
  {
    fStream << x;
    return *this;
  }

  template<class T> JSONFormatter& operator<<(const std::vector<T>& v)
  {
    fStream << "[";
    for(const T& x: v) (*this) << x << ", ";
    fStream << "]";
    return *this;
  }

  JSONFormatter& operator<<(const TVector3& v)
  {
    fStream << "["
            << v.X() << ", "
            << v.Y() << ", "
            << v.Z() << "]";
    return *this;
  }

  JSONFormatter& operator<<(const art::InputTag& t)
  {
    fStream << t.label();
    if(!t.instance().empty()) fStream << "_" << t.instance();
    if(!t.process().empty()) fStream << "_" << t.process();
    return *this;
  }

protected:
  std::ofstream& fStream;
};

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const geo::PlaneID& plane)
{
  return json << "\"" << std::string(plane) << "\"";
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const recob::Vertex& vtx)
{
  return json << TVector3(vtx.position().x(),
                          vtx.position().y(),
                          vtx.position().z());
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const recob::SpacePoint& sp)
{
  return json << TVector3(sp.XYZ());
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const recob::Track& track)
{
  std::vector<TVector3> pts;

  const recob::TrackTrajectory& traj = track.Trajectory();
  for(unsigned int j = traj.FirstValidPoint(); j <= traj.LastValidPoint(); ++j){
    if(!traj.HasValidPoint(j)) continue;
    const geo::Point_t pt = traj.LocationAtPoint(j);
    pts.emplace_back(pt.X(), pt.Y(), pt.Z());
  }

  return json << pts;
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const simb::MCParticle& part)
{
  const int apdg = abs(part.PdgCode());
  if(apdg == 12 || apdg == 14 || apdg == 16) return json << "[]"; // decay neutrinos
  std::vector<TVector3> pts;
  for(unsigned int j = 0; j < part.NumberTrajectoryPoints(); ++j){
    pts.emplace_back(part.Vx(j), part.Vy(j), part.Vz(j));
  }

  return json << pts;
}

// ----------------------------------------------------------------------------
template<class T> std::vector<art::InputTag>
getInputTags(const art::Event& evt)
{
  return evt.getInputTags<std::vector<T>>();
}

// ----------------------------------------------------------------------------
template<class T> std::vector<art::InputTag>
getInputTags(const gallery::Event& evt)
{
  std::string label = "pandora";
  if constexpr(std::is_same_v<T, recob::Hit>) label = "gaushit";

  std::cout << "Warning: getInputTags() not supported by gallery (https://cdcvs.fnal.gov/redmine/issues/23615) defaulting to \"" << label << "\"" << std::endl;
  return {art::InputTag(label)};
}

// ----------------------------------------------------------------------------
template<class TProd, class TEvt> void
SerializeProduct(const TEvt& evt,
                 JSONFormatter& json,
                 const std::string& label)
{
  json << "var " << label << " = {\n";
  for(const art::InputTag& tag: getInputTags<TProd>(evt)){
    json << "  " << tag << ": ";

    typename TEvt::template HandleT<std::vector<TProd>> prods; // deduce handle type
    evt.getByLabel(tag, prods);

    json << *prods;

    json << ",\n";
  }
  json << "};\n\n";
}

// ----------------------------------------------------------------------------
template<class TProd, class TEvt> void
SerializeProductByLabel(const TEvt& evt,
                        const std::string& in_label,
                        JSONFormatter& json,
                        const std::string& out_label)
{
  typename TEvt::template HandleT<std::vector<TProd>> prods; // deduce handle type
  evt.getByLabel(in_label, prods);

  json << "var " << out_label << " = ";
  if(prods.isValid()){
    json << *prods << ";\n\n";
  }
  else{
    json << "[];\n\n";
  }
}

// ----------------------------------------------------------------------------
template<class T> void WebEVDServer<T>::
analyze(const T& evt,
        const geo::GeometryCore* geom,
        const detinfo::DetectorProperties* detprop)
{
  PNGArena arena("arena");

  std::map<geo::PlaneID, PNGView*> plane_dig_imgs;
  std::map<geo::PlaneID, PNGView*> plane_wire_imgs;

  char webdir[PATH_MAX];
  realpath("web/", webdir);

  fTmp.symlink(webdir, "evd.js");
  fTmp.symlink(webdir, "index.html");
  fTmp.symlink(webdir, "httpd.conf");

  std::ofstream outf = fTmp.ofstream("coords.js");

  JSONFormatter json(outf);

  HandleT<raw::RawDigit> digs;
  evt.getByLabel("daq", digs);

  HandleT<recob::Wire> wires;
  evt.getByLabel("caldata", wires);

  // Find out empirically how many samples we took
  unsigned long maxTick = 0;
  if(digs.isValid()){
    for(const raw::RawDigit& dig: *digs) maxTick = std::max(maxTick, (unsigned long)dig.Samples());
  }
  if(wires.isValid()){
    for(const recob::Wire& wire: *wires) maxTick = std::max(maxTick, wire.NSignal());
  }
  std::cout << "Number of ticks " << maxTick << std::endl;

  for(unsigned int digIdx = 0; digIdx < (digs.isValid() ? digs->size() : 0); ++digIdx){
    const raw::RawDigit& dig = (*digs)[digIdx];

    for(geo::WireID wire: geom->ChannelToWire(dig.Channel())){
      const geo::TPCID tpc(wire);
      const geo::PlaneID plane(wire);

      const geo::WireID w0 = geom->GetBeginWireID(plane);
      const unsigned int Nw = geom->Nwires(plane);

      if(plane_dig_imgs.count(plane) == 0){
        plane_dig_imgs[plane] = new PNGView(arena, Nw, maxTick);
      }

      PNGView& bytes = *plane_dig_imgs[plane];

      raw::RawDigit::ADCvector_t adcs(dig.Samples());
      raw::Uncompress(dig.ADCs(), adcs, dig.Compression());

      for(unsigned int tick = 0; tick < adcs.size(); ++tick){
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

  for(unsigned int wireIdx = 0; wireIdx < wires->size(); ++wireIdx){
    for(geo::WireID wire: geom->ChannelToWire((*wires)[wireIdx].Channel())){
      const geo::TPCID tpc(wire);
      const geo::PlaneID plane(wire);

      const geo::WireID w0 = geom->GetBeginWireID(plane);
      const unsigned int Nw = geom->Nwires(plane);

      if(plane_wire_imgs.count(plane) == 0){
        plane_wire_imgs[plane] = new PNGView(arena, Nw, maxTick);
      }

      PNGView& bytes = *plane_wire_imgs[plane];

      const auto adcs = (*wires)[wireIdx].Signal();
      for(unsigned int tick = 0; tick < adcs.size(); ++tick){
        if(adcs[tick] <= 0) continue;

        // green channel
        bytes(wire.Wire-w0.Wire, tick, 1) = 128; // dark green
        // alpha channel
        bytes(wire.Wire-w0.Wire, tick, 3) = std::max(0, std::min(int(10*adcs[tick]), 255));
      }
    }
  }

  std::map<geo::PlaneID, std::map<art::InputTag, std::vector<recob::Hit>>> plane_hits;

  for(art::InputTag tag: getInputTags<recob::Hit>(evt)){
    HandleT<recob::Hit> hits;
    evt.getByLabel(tag, hits);

    for(const recob::Hit& hit: *hits){
      // Would possibly be right for disambiguated hits?
      //    const geo::WireID wire(hit.WireID());

      for(geo::WireID wire: geom->ChannelToWire(hit.Channel())){
        const geo::PlaneID plane(wire);

        // Correct for disambiguated hits
        //      plane_hits[plane].push_back(hit);

        // Otherwise we have to update the wire number
        plane_hits[plane][tag].emplace_back(hit.Channel(), hit.StartTick(), hit.EndTick(), hit.PeakTime(), hit.SigmaPeakTime(), hit.RMS(), hit.PeakAmplitude(), hit.SigmaPeakAmplitude(), hit.SummedADC(), hit.Integral(), hit.SigmaIntegral(), hit.Multiplicity(), hit.LocalIndex(), hit.GoodnessOfFit(), hit.DegreesOfFreedom(), hit.View(), hit.SignalType(), wire);
      }
    }
  }

  json << "planes = {\n";
  for(geo::PlaneID plane: geom->IteratePlaneIDs()){
    const geo::PlaneGeo& planegeo = geom->Plane(plane);
    const int view = planegeo.View();
    const unsigned int nwires = planegeo.Nwires();
    const double pitch = planegeo.WirePitch();
    const TVector3 c = planegeo.GetCenter();
    const PNGView* dig_view = plane_dig_imgs[plane];

    const auto d = planegeo.GetIncreasingWireDirection();
    const TVector3 n = planegeo.GetNormalDirection();

    const double tick_pitch = detprop->ConvertTicksToX(1, plane) - detprop->ConvertTicksToX(0, plane);

    PNGView* wire_view = plane_wire_imgs.count(plane) ? plane_wire_imgs[plane] : 0;

    // Skip completely empty planes (eg the wall-facing collection wires)
    if(!dig_view && !wire_view) continue;

    json << "  " << plane << ": {"
         << "view: " << view << ", "
         << "nwires: " << nwires << ", "
         << "pitch: " << pitch << ", "
         << "nticks: " << maxTick << ", "
         << "tick_pitch: " << tick_pitch << ", "
         << "center: " << c << ", "
         << "across: " << d << ", "
         << "normal: " << n << ", ";

    if(dig_view) json << "digs: " << *dig_view << ", ";
    if(wire_view) json << "wires: " << *wire_view << ", ";

    json << "hits: {";
    for(auto it: plane_hits[plane]){
      json << it.first << ": [";
      for(const recob::Hit& hit: it.second){
        json << "{wire: " << geo::WireID(hit.WireID()).Wire
             << ", tick: " << hit.PeakTime()
             << ", rms: " << hit.RMS() << "}, ";
      }
      json << "], ";
    }

    json << "}},\n";
  }
  json << "};\n";

  SerializeProduct<recob::Track>(evt, json, "tracks");

  SerializeProduct<recob::SpacePoint>(evt, json, "spacepoints");

  SerializeProduct<recob::Vertex>(evt, json, "reco_vtxs");

  SerializeProductByLabel<simb::MCParticle>(evt, "largeant", json, "truth_trajs");

  json << "cryos = [\n";
  for(auto it = geom->begin_cryostat(); it != geom->end_cryostat(); ++it){
    const TVector3 r0(it->MinX(), it->MinY(), it->MinZ());
    const TVector3 r1(it->MaxX(), it->MaxY(), it->MaxZ());
    json << "  { min: "  << r0 << ", max: " << r1 << " },\n";
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

