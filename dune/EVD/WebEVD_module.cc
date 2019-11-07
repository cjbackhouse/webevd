// Christopher Backhouse - bckhouse@fnal.gov

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

  std::string fTempDir;
};

DEFINE_ART_MODULE(WebEVD)

// ---------------------------------------------------------------------------
WebEVD::WebEVD(const fhicl::ParameterSet& pset)
  : EDAnalyzer(pset),
    fSpacePointTag(art::InputTag(pset.get<std::string>("SpacePointLabel"),
                                 pset.get<std::string>("SpacePointInstanceLabel"))),
    fHitLabel(pset.get<std::string>("HitLabel"))
{
  fTempDir = "/tmp/webevd_XXXXXX";
  mkdtemp(fTempDir.data());
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
    const int status = system(TString::Format("busybox httpd -f -p %d -h %s", port, fTempDir.c_str()).Data());
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

  std::cout << fTempDir << std::endl;
}

int RoundUpToPowerOfTwo(int x)
{
  int ret = 1;
  while(ret < x) ret *= 2;
  return ret;
}

// Square because seems to be necessary for mipmapping
struct PNGArena
{
  PNGArena(const std::string& n, int e, int ex, int ey) : name(n), extent(e), elemx(ex), elemy(ey), nviews(0), data(e*e*4, 0)
  {
  }

  png_byte& operator()(int x, int y, int c)
  {
    return data[(y*extent+x)*4+c];
  }

  const png_byte& operator()(int x, int y, int c) const
  {
    return data[(y*extent+x)*4+c];
  }

  struct View
  {
    png_byte& operator()(int x, int y, int c)
    {
      return arena(x+dx, y+dy, c);
    }

    const png_byte& operator()(int x, int y, int c) const
    {
      return arena(x+dx, y+dy, c);
    }

    std::string ArenaName() const {return arena.name;}
    int OffsetX() const {return dx;}
    int OffsetY() const {return dy;}

  protected:
    friend class PNGArena;
    View(PNGArena& a, int _dx, int _dy) : arena(a), dx(_dx), dy(_dy)
    {
    }

    PNGArena& arena;
    int dx, dy;
  };

  bool Full() const
  {
    return nviews >= (extent/elemx)*(extent/elemy);
  }

  // TODO think about memory management
  View* NewView()
  {
    const int nfitx = extent/elemx;
    const int nfity = extent/elemy;

    const int ix = nviews%nfitx;
    const int iy = nviews/nfitx;

    if(iy >= nfity){
      std::cout << "Arena overflow!" << std::endl;
      ++nviews;
      std::cout << "  " << extent << " " << elemx << " " << elemy << " " << nviews << std::endl;
      //      return new View(*this, 0, 0);
      abort();
    }

    ++nviews;

    return new View(*this, ix*elemx, iy*elemy);
  }

  std::string name;
  int extent;
  int elemx, elemy;
  int nviews;
  std::vector<png_byte> data;
};

void MipMap(PNGArena& bytes, int newdim)
{
  // The algorithm here is only really suitable for the specific way we're
  // encoding hits, not for general images.
  //
  // The alpha channel is set to the max of any of the source pixels. The
  // colour channels are averaged, weighted by the alpha values, and then
  // scaled so that the most intense colour retains its same maximum intensity
  // (this is relevant for green, where we use "dark green", 128).
  for(int y = 0; y < newdim; ++y){
    for(int x = 0; x < newdim; ++x){
      double totc[3] = {0,};
      double maxtotc = 0;
      png_byte maxc[3] = {0,};
      png_byte maxmaxc = 0;
      png_byte maxa = 0;
      for(int dy = 0; dy <= 1; ++dy){
        for(int dx = 0; dx <= 1; ++dx){
          const png_byte va = bytes(x*2+dx, y*2+dy, 3); // alpha value
          maxa = std::max(maxa, va);

          for(int c = 0; c < 3; ++c){
            const png_byte vc = bytes(x*2+dx, y*2+dy, c); // colour value
            totc[c] += vc * va;
            maxc[c] = std::max(maxc[c], vc);
            maxtotc = std::max(maxtotc, totc[c]);
            maxmaxc = std::max(maxmaxc, maxc[c]);
          } // end for c
        } // end for dx
      } // end for dy

      for(int c = 0; c < 3; ++c) bytes(x, y, c) = maxtotc ? maxmaxc*totc[c]/maxtotc : 0;
      bytes(x, y, 3) = maxa;
    } // end for x
  } // end for y
}

void WriteToPNG(const std::string& fname, /*const*/ PNGArena& bytes)
{
  for(int mipmapdim = bytes.extent; mipmapdim >= 1; mipmapdim /= 2){
    //    FILE* fp = fopen(fname.c_str()+".png", "wb");

    FILE* fp = fopen(TString::Format("%s_%d.png", fname.c_str(), mipmapdim).Data(), "wb");

    png_struct_def* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    auto info_ptr = png_create_info_struct(png_ptr);

    // Doesn't seem to have a huge effect
    //  png_set_compression_level(png_ptr, 9);

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, mipmapdim, mipmapdim,
                 8/*bit_depth*/, PNG_COLOR_TYPE_RGBA/*GRAY*/, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    std::vector<png_byte*> pdatas(mipmapdim);
    for(int i = 0; i < mipmapdim; ++i) pdatas[i] = const_cast<png_byte*>(&bytes(0, i, 0));
    png_set_rows(png_ptr, info_ptr, &pdatas.front());

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    fclose(fp);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    MipMap(bytes, mipmapdim/2);
  }
}


struct PNGBytes
{
  PNGBytes(int w, int h)
    : width(RoundUpToPowerOfTwo(w)),
      height(RoundUpToPowerOfTwo(h)),
      data(width*height*4, 0)
  {
    // Scale up to the next power of two
    //    for(width = 1; width < w; width *= 2);
    //    for(height = 1; height < h; height *= 2);
    std::cout << w << "x" << h << " -> " << width << "x" << height << std::endl;
  }

  png_byte& operator()(int x, int y, int c)
  {
    return data[(y*width+x)*4+c];
  }

  const png_byte& operator()(int x, int y, int c) const
  {
    return data[(y*width+x)*4+c];
  }

  void GetExtents(int& minx, int& maxx, int& miny, int& maxy,
                  int threshold = 0) const
  {
    // HACK the extents off for now
    minx = miny = 0; maxx = width-1; maxy = height-1;
    return;

    minx = width-1;
    maxx = 0;
    miny = height-1;
    maxy = 0;

    for(int i = 0; i < width; ++i){
      for(int j = 0; j < height; ++j){
        if((*this)(i, j, 3) > threshold){ // check the alpha
          minx = std::min(i, minx);
          maxx = std::max(i, maxx);
          miny = std::min(j, miny);
          maxy = std::max(j, maxy);
        }
      }
    }
  }


  int width, height;
  std::vector<png_byte> data;
};

PNGBytes MipMap(const PNGBytes& b)
{
  PNGBytes ret(b.width/2, b.height/2);

  for(int i = 0; i < b.width/2; ++i){
    for(int j = 0; j < b.height/2; ++j){
      for(int c = 0; c < 3; ++c){
        // TODO think about this algorith a bit
        ret(i, j, c) = (b(i*2,   j*2, c) + b(i*2+1, j*2,   c) +
                        b(i*2+1, j*2, c) + b(i*2+1, j*2+1, c))/4;
      }
      ret(i, j, 3) = std::max(std::max(b(i*2,   j*2, 3), b(i*2+1, j*2,   3)),
                              std::max(b(i*2+1, j*2, 3), b(i*2+1, j*2+1, 3)));
    }
  }

  return ret;
}

void WriteToPNG(const std::string& fname, const PNGBytes& bytes)
{
  int minx, maxx, miny, maxy;
  // Don't allow a little bit of noise to dominate the sizing. Still seems
  // to be the case often for the digits though.
  bytes.GetExtents(minx, maxx, miny, maxy, 8);

  // Just don't write the image if it's completely empty
  if(miny > maxy || minx > maxx) return;

  const int neww = maxx-minx+1;
  const int newh = maxy-miny+1;

  //  std::cout << bytes.width << "x" << bytes.height << " -> " << neww << "x" << newh << " (" << minx << "-" << maxx << ", " << miny << "-" << maxy << ")" << std::endl;

  FILE* fp = fopen(fname.c_str(), "wb");

  png_struct_def* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  auto info_ptr = png_create_info_struct(png_ptr);

  // Doesn't seem to have a huge effect
  //  png_set_compression_level(png_ptr, 9);

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, neww, newh, //bytes.width, bytes.height,
               8/*bit_depth*/, PNG_COLOR_TYPE_RGBA/*GRAY*/, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  std::vector<png_byte*> pdatas(newh);
  for(int i = 0; i < newh; ++i) pdatas[i] = const_cast<png_byte*>(&bytes(minx, miny+i, 0));
  png_set_rows(png_ptr, info_ptr, &pdatas.front());

  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  fclose(fp);

  png_destroy_write_struct(&png_ptr, &info_ptr);
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

void WebEVD::analyze(const art::Event& evt)
{
  std::cout << getInputTags<std::vector<recob::Track>>(evt).size() << " tracks "
            << getInputTags<std::vector<recob::Wire>>(evt).size() << " wires"
            << std::endl;

  for(auto x: getInputTags<std::vector<recob::Track>>(evt)) std::cout << x << std::endl;

  // Needs art v3_03 (soon?)
  //  std::vector<art::InputTag> tags = evt.getInputTags<recob::Track>();
  //  evt.getInputTags<recob::Track>();

  // Alternate syntax
  //  auto const tokens = evt.getProductTokens<recob::Track>();
  //  for (auto const& token : tokens) {
  //    auto const h = event.getValidHandle(token);
  //  }

  //  const int width = 480; // TODO remove // max wire ID 512*8;
  const int height = 4492; // TODO somewhere to look up number of ticks?

  // Larger than this doesn't seem to work in the browser. Smaller won't fit
  // the number of ticks we have.
  const int kArenaSize = 8192;

  std::map<std::pair<int, int>, std::vector<PNGArena*>> arenas;

  std::map<geo::PlaneID, PNGArena::View*> plane_dig_imgs;
  std::map<geo::PlaneID, PNGArena::View*> plane_wire_imgs;

  art::Handle<std::vector<recob::SpacePoint>> pts;
  evt.getByLabel(fSpacePointTag, pts);

  char webdir[PATH_MAX];
  realpath("web/", webdir);

  for(std::string tgt: {"evd.js", "index.html", "three.js-master"}){
    symlink(TString::Format("%s/%s", webdir,           tgt.c_str()).Data(),
            TString::Format("%s/%s", fTempDir.c_str(), tgt.c_str()).Data());
  }

  std::ofstream outf(fTempDir+"/coords.js");
  JSONFormatter json(outf);

  json << "var coords = [\n";
  for(const recob::SpacePoint& p: *pts){
    json << TVector3(p.XYZ()) << ",\n";
  }
  json << "];\n\n";

  //      json << "var waves = [" << std::endl;

  art::Handle<std::vector<raw::RawDigit>> digs;
  evt.getByLabel("daq", digs);

  /*
    for(const raw::RawDigit& dig: *digs){
    //        std::cout << dig.NADC() << " " << dig.Samples() << " " << dig.Compression() << " " << dig.GetPedestal() << std::endl;
    
    // ChannelID_t Channel();

    raw::RawDigit::ADCvector_t adcs(dig.Samples());
    raw::Uncompress(dig.ADCs(), adcs, dig.Compression());

    json << "  [ ";
    for(auto x: adcs) json << (x ? x-dig.GetPedestal() : 0) << ", ";
    json << " ]," << std::endl;
    }

    json << "];" << std::endl;
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
        const auto key = std::make_pair(Nw, height);
        if(arenas[key].empty() || arenas[key].back()->Full()){
          const std::string name = TString::Format("arena_%d_%d_%lu", Nw, height, arenas[key].size()).Data();
          arenas[key].push_back(new PNGArena(name, kArenaSize, Nw, height));
        }

        plane_dig_imgs[plane] = arenas[key].back()->NewView();
      }

      //          std::cout << "Look up " << plane << std::endl;

      PNGArena::View& bytes = *plane_dig_imgs[plane];

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
        const auto key = std::make_pair(Nw, height);
        if(arenas[key].empty() || arenas[key].back()->Full()){
          const std::string name = TString::Format("arena_%d_%d_%lu", Nw, height, arenas[key].size()).Data();
          arenas[key].push_back(new PNGArena(name, kArenaSize, Nw, height));
        }
        plane_wire_imgs[plane] = arenas[key].back()->NewView();
      }

      PNGArena::View& bytes = *plane_wire_imgs[plane];

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
    json << "var wires = [" << std::endl;
    
    for(const recob::Wire& wire: *wires){
    json << "  [ ";
    for(auto x: wire.SignalROI()) json << x << ", ";
    json << " ]," << std::endl;
    }

    json << "];" << std::endl;
  */

  art::Handle<std::vector<recob::Hit>> hits;
  evt.getByLabel("gaushit", hits);

  std::map<geo::PlaneID, std::vector<recob::Hit>> plane_hits;
  for(const recob::Hit& hit: *hits){
    // Would possibly be right for disambiguated hits?
    //    const geo::WireID wire(hit.WireID());

    for(geo::WireID wire: geom->ChannelToWire(hit.Channel())){
      //    if(geom->SignalType(wire) != geo::kCollection) continue; // for now
      // TODO loop over possible wires
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
    const PNGArena::View* dig_view = it.second;

    const auto d = planegeo.GetIncreasingWireDirection();
    //    const auto dwire = planegeo.GetWireDirection();
    const TVector3 n = planegeo.GetNormalDirection();

    const int nticks = height; // HACK from earlier
    const double tick_pitch = detprop->ConvertTicksToX(1, plane) - detprop->ConvertTicksToX(0, plane);

    PNGArena::View* wire_view = plane_wire_imgs.count(plane) ? plane_wire_imgs[plane] : 0;

    json << "  \"" << plane << "\": {"
         << "view: " << view << ", "
         << "nwires: " << nwires << ", "
         << "pitch: " << pitch << ", "
         << "nticks: " << nticks << ", "
         << "tick_pitch: " << tick_pitch << ", "
         << "center: " << c << ", "
         << "across: " << d << ", "
         << "normal: " << n << ", ";

    json << "digs: {"
         << "fname: \"" << dig_view->ArenaName() << "\", "
         << "texdim: " << kArenaSize << ", "
         << "texdx: " << dig_view->OffsetX() << ", "
         << "texdy: " << dig_view->OffsetY() << "}, ";

    if(wire_view){
      json << "wires: {"
           << "fname: \"" << wire_view->ArenaName() << "\", "
           << "texdim: " << kArenaSize << ", "
           << "texdx: " << wire_view->OffsetX() << ", "
           << "texdy: " << wire_view->OffsetY() << "}, ";
    }

    json << "hits: [";
    for(const recob::Hit& hit: plane_hits[plane]){
      json << "{wire: " << geo::WireID(hit.WireID()).Wire
           << ", tick: " << hit.PeakTime()
           << ", rms: " << hit.RMS() << "}, ";
    }

    json << "]},\n";
  }
  json << "};\n";

  art::Handle<std::vector<recob::Track>> tracks;
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


  art::Handle<std::vector<simb::MCParticle>> parts;
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

  for(auto it: arenas){
    for(unsigned int i = 0; i < it.second.size(); ++i){
      std::cout << "Writing " << it.second[i]->name << std::endl;
      WriteToPNG(fTempDir+"/"+it.second[i]->name, *it.second[i]);
      delete it.second[i];
    }
  }

  /*
  // Very cheesy speedup to parallelize png making
  for(auto it: plane_dig_imgs){
    std::cout << "Writing digits/" << it.first.toString() << std::endl;
    if(true){//fork() == 0){
      WriteToPNG("digits/"+it.first.toString()+".png", *it.second);

      //      WriteToPNG("digits/"+it.first.toString()+"_m.png", MipMap(MipMap(MipMap(*it.second))));

      delete it.second;
      //      exit(0);
    }
  }

  for(auto it: plane_wire_imgs){
    std::cout << "Writing wires/" << it.first.toString() << std::endl;
    if(true){//fork() == 0){
      WriteToPNG("wires/"+it.first.toString()+".png", *it.second);
      delete it.second;
      //      exit(0);
    }
  }
  */
}

} // namespace
