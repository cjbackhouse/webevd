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
  fTempDir = "/tmp/webevd_XXXXXX";
  mkdtemp(fTempDir.data());
}

template<class T> WebEVDServer<T>::~WebEVDServer()
{
  // I'm nervous about removing files based on a wildcard, so we explicitly
  // track everything we put in the tmp directory and clean it up here.
  for(const std::string& f: fCleanup) unlink(f.c_str());
  if(rmdir(fTempDir.c_str()) != 0){
    std::cout << "Failed to clean up temporary directory " << fTempDir << std::endl;
  }
}

template<class T> void WebEVDServer<T>::serve()
{
  std::cout << "Temp dir: " << fTempDir << std::endl;

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

    const int status = system(TString::Format("busybox httpd -f -p %d -h %s", port, fTempDir.c_str()).Data());
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

int RoundUpToPowerOfTwo(int x)
{
  int ret = 1;
  while(ret < x) ret *= 2;
  return ret;
}


// Larger than this doesn't seem to work in the browser. Smaller won't fit
// the number of ticks we have.
const int kArenaSize = 8192;

const int kBlockSize = 64;

// Square because seems to be necessary for mipmapping
struct PNGArena
{
  PNGArena(const std::string& n) : name(n), extent(kArenaSize), nviews(0)
  {
  }

  png_byte& operator()(int i, int x, int y, int c)
  {
    return data[i][(y*extent+x)*4+c];
  }

  const png_byte& operator()(int i, int x, int y, int c) const
  {
    return data[i][(y*extent+x)*4+c];
  }

  png_byte* NewBlock()
  {
    const int nfitx = extent/kBlockSize;
    const int nfity = extent/kBlockSize;

    int ix = nviews%nfitx;
    int iy = nviews/nfitx;

    if(data.empty() || iy >= nfity){
      ix = 0;
      iy = 0;
      nviews = 0;
      data.emplace_back(4*extent*extent);
    }

    ++nviews;

    return &data.back()[(iy*extent + ix)*kBlockSize*4];
  }

  std::string name;
  int extent;
  int elemx, elemy;
  int nviews;

  std::vector<std::vector<png_byte>> data;
};


struct PNGView
{
  PNGView(PNGArena& a, int w, int h)
    : arena(a), width(w), height(h), blocks(w/kBlockSize+1, std::vector<png_byte*>(h/kBlockSize+1, 0))
  {
  }

  inline png_byte& operator()(int x, int y, int c)
  {
    const int ix = x/kBlockSize;
    const int iy = y/kBlockSize;
    if(!blocks[ix][iy]) blocks[ix][iy] = arena.NewBlock();
    return blocks[ix][iy][((y-iy*kBlockSize)*arena.extent+(x-ix*kBlockSize))*4+c];
  }

  inline png_byte operator()(int x, int y, int c) const
  {
    const int ix = x/kBlockSize;
    const int iy = y/kBlockSize;
    if(!blocks[ix][iy]) return 0;
    return blocks[ix][iy][((y-iy*kBlockSize)*arena.extent+(x-ix*kBlockSize))*4+c];
  }

protected:
  template<class T> friend T& operator<<(T&, const PNGView&);

  PNGArena& arena;
  int width, height;
  std::vector<std::vector<png_byte*>> blocks;
};


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
         << "dx: " << ix*kBlockSize << ", "
         << "dy: " << iy*kBlockSize << ", "
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


void MipMap(PNGArena& bytes, int newdim)
{
  // The algorithm here is only really suitable for the specific way we're
  // encoding hits, not for general images.
  //
  // The alpha channel is set to the max of any of the source pixels. The
  // colour channels are averaged, weighted by the alpha values, and then
  // scaled so that the most intense colour retains its same maximum intensity
  // (this is relevant for green, where we use "dark green", 128).
  for(unsigned int d = 0; d < bytes.data.size(); ++d){
    for(int y = 0; y < newdim; ++y){
      for(int x = 0; x < newdim; ++x){
        double totc[3] = {0,};
        double maxtotc = 0;
        png_byte maxc[3] = {0,};
        png_byte maxmaxc = 0;
        png_byte maxa = 0;
        for(int dy = 0; dy <= 1; ++dy){
          for(int dx = 0; dx <= 1; ++dx){
            const png_byte va = bytes(d, x*2+dx, y*2+dy, 3); // alpha value
            maxa = std::max(maxa, va);

            for(int c = 0; c < 3; ++c){
              const png_byte vc = bytes(d, x*2+dx, y*2+dy, c); // colour value
              totc[c] += vc * va;
              maxc[c] = std::max(maxc[c], vc);
              maxtotc = std::max(maxtotc, totc[c]);
              maxmaxc = std::max(maxmaxc, maxc[c]);
            } // end for c
          } // end for dx
        } // end for dy

        for(int c = 0; c < 3; ++c) bytes(d, x, y, c) = maxtotc ? maxmaxc*totc[c]/maxtotc : 0;
        bytes(d, x, y, 3) = maxa;
      } // end for x
    } // end for y
  } // end for d
}

void AnalyzeArena(const PNGArena& bytes)
{
  for(int blockSize = 1; blockSize <= bytes.extent; blockSize *= 2){
    long nfilled = 0;
    for(unsigned int d = 0; d < bytes.data.size(); ++d){
      for(int iy = 0; iy < bytes.extent/blockSize; ++iy){
        for(int ix = 0; ix < bytes.extent/blockSize; ++ix){
          bool filled = false;
          for(int y = 0; y < blockSize && !filled; ++y){
            for(int x = 0; x < blockSize; ++x){
              if(bytes(d, ix*blockSize+x, iy*blockSize+y, 3) > 0){
                filled = true;
                break;
              }
            }
          } // end for y
          if(filled) ++nfilled;
        }
      }
    }

    std::cout << "With block size = " << blockSize << " " << double(nfilled)/((bytes.extent*bytes.extent)/(blockSize*blockSize)*bytes.data.size()) << " of the blocks are filled" << std::endl;
  }
}

void WriteToPNG(const std::string& prefix, const PNGArena& bytes,
                std::vector<std::string>& cleanup, int mipmapdim = -1)
{
  if(mipmapdim == -1) mipmapdim = bytes.extent;

  for(unsigned int d = 0; d < bytes.data.size(); ++d){
    const std::string fname = TString::Format("%s_%d_mip%d.png", prefix.c_str(), d, mipmapdim).Data();
    cleanup.push_back(fname);

    FILE* fp = fopen(fname.c_str(), "wb");

    png_struct_def* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    auto info_ptr = png_create_info_struct(png_ptr);

    // Doesn't seem to have a huge effect. Setting zero generates huge files
    //  png_set_compression_level(png_ptr, 9);

    // Doesn't affect the file size, may be a small speedup
    png_set_filter(png_ptr, 0, PNG_FILTER_NONE);

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, mipmapdim, mipmapdim,
                 8/*bit_depth*/, PNG_COLOR_TYPE_RGBA/*GRAY*/, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    std::vector<png_byte*> pdatas(mipmapdim);
    for(int i = 0; i < mipmapdim; ++i) pdatas[i] = const_cast<png_byte*>(&bytes(d, 0, i, 0));
    png_set_rows(png_ptr, info_ptr, &pdatas.front());

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    fclose(fp);

    png_destroy_write_struct(&png_ptr, &info_ptr);
  }
}

// NB: destroys "bytes" in the process
void WriteToPNGWithMipMaps(const std::string& prefix, PNGArena& bytes,
                           std::vector<std::string>& cleanup)
{
  //  AnalyzeArena(bytes);

  for(int mipmapdim = bytes.extent; mipmapdim >= 1; mipmapdim /= 2){
    WriteToPNG(prefix, bytes, cleanup, mipmapdim);

    MipMap(bytes, mipmapdim/2);
  }
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

// TODO figure out a more-templatey way to do this
template<class T> struct HandleMaker
{
  static art::Handle<std::vector<T>> Handle(const art::Event&){return art::Handle<std::vector<T>>();}
  static gallery::Handle<std::vector<T>> Handle(const gallery::Event&){return gallery::Handle<std::vector<T>>();}
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

  auto pts = HandleMaker<recob::SpacePoint>::Handle(evt);
  evt.getByLabel("pandora"/*fSpacePointTag*/, pts);

  char webdir[PATH_MAX];
  realpath("web/", webdir);

  for(std::string tgt: {"evd.js", "index.html", "httpd.conf", /*, "three.js-master"*/}){
    symlink(TString::Format("%s/%s", webdir,           tgt.c_str()).Data(),
            TString::Format("%s/%s", fTempDir.c_str(), tgt.c_str()).Data());
    fCleanup.push_back(fTempDir+"/"+tgt);
  }

  std::ofstream outf(fTempDir+"/coords.js");
  fCleanup.push_back(fTempDir+"/coords.js");

  JSONFormatter json(outf);

  json << "var coords = [\n";
  for(const recob::SpacePoint& p: *pts){
    json << TVector3(p.XYZ()) << ",\n";
  }
  json << "];\n\n";

  auto digs = HandleMaker<raw::RawDigit>::Handle(evt);
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

  auto wires = HandleMaker<recob::Wire>::Handle(evt);
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

  auto hits = HandleMaker<recob::Hit>::Handle(evt);
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

  auto tracks = HandleMaker<recob::Track>::Handle(evt);
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


  auto parts = HandleMaker<simb::MCParticle>::Handle(evt);
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
  WriteToPNGWithMipMaps(fTempDir+"/"+arena.name, arena, fCleanup);

  // TODO use unique_ptr?
  for(auto it: plane_dig_imgs) delete it.second;
  for(auto it: plane_wire_imgs) delete it.second;
}

template class WebEVDServer<art::Event>;
template class WebEVDServer<gallery::Event>;

} // namespace

