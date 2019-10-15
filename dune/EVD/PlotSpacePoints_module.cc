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

#include "larsim/MCCheater/BackTrackerService.h"

#include "lardataobj/RawData/RawDigit.h"
#include "lardataobj/RawData/raw.h" // Uncompress()

#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "TGraph.h"
#include "TPad.h"

#include <png.h>

namespace reco3d
{

class PlotSpacePoints: public art::EDAnalyzer
{
public:
  explicit PlotSpacePoints(const fhicl::ParameterSet& pset);

  void analyze(const art::Event& evt);
  void endJob() override;

protected:
  void Plot(const std::vector<recob::SpacePoint>& pts,
            const std::string& suffix) const;

  void Plot3D(const std::vector<recob::SpacePoint>& pts,
              const std::string& suffix) const;

  std::vector<recob::SpacePoint> TrueSpacePoints(art::Handle<std::vector<recob::Hit>> hits) const;

  art::InputTag fSpacePointTag;

  std::string fHitLabel;

  std::string fSuffix;

  bool fPlots;
  bool fPlots3D;
  bool fPlotsTrue;
};

DEFINE_ART_MODULE(PlotSpacePoints)

// ---------------------------------------------------------------------------
PlotSpacePoints::PlotSpacePoints(const fhicl::ParameterSet& pset)
  : EDAnalyzer(pset),
    fSpacePointTag(art::InputTag(pset.get<std::string>("SpacePointLabel"),
                                 pset.get<std::string>("SpacePointInstanceLabel"))),
    fHitLabel(pset.get<std::string>("HitLabel")),
    fSuffix(pset.get<std::string>("Suffix")),
    fPlots(pset.get<bool>("Plots")),
    fPlots3D(pset.get<bool>("Plots3D")),
    fPlotsTrue(pset.get<bool>("PlotsTrue"))
{
  if(!fSuffix.empty()) fSuffix = "_"+fSuffix;
}

void PlotSpacePoints::endJob()
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

// ---------------------------------------------------------------------------
void PlotSpacePoints::Plot(const std::vector<recob::SpacePoint>& pts,
                           const std::string& suffix) const
{
  TGraph gZX;
  TGraph gYX;
  TGraph gZY;

  gZX.SetTitle(";z;x");
  gYX.SetTitle(";y;x");
  gZY.SetTitle(";z;y");

  for(const recob::SpacePoint& pt: pts){
    const double* xyz = pt.XYZ();
    const double x = xyz[0];
    const double y = xyz[1];
    const double z = xyz[2];
    gZX.SetPoint(gZX.GetN(), z, x);
    gYX.SetPoint(gYX.GetN(), y, x);
    gZY.SetPoint(gZY.GetN(), z, y);
  }

  if(gZX.GetN() == 0) gZX.SetPoint(0, 0, 0);
  if(gYX.GetN() == 0) gYX.SetPoint(0, 0, 0);
  if(gZY.GetN() == 0) gZY.SetPoint(0, 0, 0);

  gZX.Draw("ap");
  gPad->Print(("plots/evd"+suffix+".png").c_str());

  gYX.Draw("ap");
  gPad->Print(("plots/evd_ortho"+suffix+".png").c_str());
  gZY.Draw("ap");
  gPad->Print(("plots/evd_zy"+suffix+".png").c_str());
}

// ---------------------------------------------------------------------------
std::vector<recob::SpacePoint> PlotSpacePoints::
TrueSpacePoints(art::Handle<std::vector<recob::Hit>> hits) const
{
  std::vector<recob::SpacePoint> pts_true;

  const double err[6] = {0,};

  art::ServiceHandle<cheat::BackTrackerService const> bt_serv;
  for(unsigned int i = 0; i < hits->size(); ++i){
    try{
      const std::vector<double> xyz = bt_serv->HitToXYZ(art::Ptr<recob::Hit>(hits, i));
      pts_true.emplace_back(&xyz[0], err, 0);
    }
    catch(...){} // some hits have no electrons?
  }

  return pts_true;
}

// ---------------------------------------------------------------------------
void PlotSpacePoints::Plot3D(const std::vector<recob::SpacePoint>& pts,
                             const std::string& suffix) const
{
  int frame = 0;
  for(int phase = 0; phase < 4; ++phase){
    const int Nang = 20;
    for(int iang = 0; iang < Nang; ++iang){
      const double ang = M_PI/2*iang/double(Nang);

      TGraph g;

      for(const recob::SpacePoint& p: pts){
        const double* xyz = p.XYZ();

        double x, y;
        if(phase == 0){
          x = cos(ang)*xyz[1]+sin(ang)*xyz[2];
          y = xyz[0];
        }
        if(phase == 1){
          x = xyz[2];
          y = cos(ang)*xyz[0]+sin(ang)*xyz[1];
        }
        if(phase == 2){
          x = cos(ang)*xyz[2]-sin(ang)*xyz[0];
          y = xyz[1];
        }
        if(phase == 3){
          x = -cos(ang)*xyz[0] + sin(ang)*xyz[1];
          y = cos(ang)*xyz[1] + sin(ang)*xyz[0];
        }

        //        const double phi = phase/3.*M_PI/2 + ang/3;
        const double phi = 0;
        g.SetPoint(g.GetN(), cos(phi)*x+sin(phi)*y, cos(phi)*y-sin(phi)*x);
      }

      std::string fname = TString::Format(("anim/evd3d"+suffix+"_%03d.png").c_str(), frame++).Data();
      g.SetTitle(fname.c_str());
      if(g.GetN()) g.Draw("ap");
      gPad->Print(fname.c_str());
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

    data = new png_byte*[height];
    for(int i = 0; i < height; ++i){
      data[i] = new png_byte[width];
      for(int j = 0; j < width; ++j) data[i][j] = 0;
    }
  }

  ~PNGBytes()
  {
    for(int i = 0; i < height; ++i) delete[] data[i];
    delete data;
  }

  int width, height;
  png_byte** data;
};

void WriteToPNG(const std::string& fname, const PNGBytes& bytes)
{
  FILE* fp = fopen(fname.c_str(), "wb");

  auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  auto info_ptr = png_create_info_struct(png_ptr);
  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, bytes.width, bytes.height,
               8/*bit_depth*/, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  png_set_rows(png_ptr, info_ptr, bytes.data);
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
  fclose(fp);
}

void PlotSpacePoints::analyze(const art::Event& evt)
{
  //  const int width = 480; // TODO remove // max wire ID 512*8;
  const int height = 4492; // TODO somewhere to look up number of ticks?

  //  PNGBytes dig_bytes_pos(width, height);
  //  PNGBytes dig_bytes_neg(width, height);
  //  PNGBytes wire_bytes(width, height);

  std::map<geo::PlaneID, PNGBytes*> plane_dig_imgs_pos;
  std::map<geo::PlaneID, PNGBytes*> plane_dig_imgs_neg;

  std::map<geo::PlaneID, PNGBytes*> plane_wire_imgs;

  //  if(fPlots){
    art::Handle<std::vector<recob::SpacePoint>> pts;
    evt.getByLabel(fSpacePointTag, pts);

    const std::string suffix = TString::Format("%s_%d", fSuffix.c_str(), evt.event()).Data();

    //    if(!pts->empty()){
    //      Plot(*pts, suffix);

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

          if(plane_dig_imgs_pos.count(plane) == 0){
            //            std::cout << "Create " << plane << " with " << Nw << std::endl;
            plane_dig_imgs_pos[plane] = new PNGBytes(Nw, height);
            plane_dig_imgs_neg[plane] = new PNGBytes(Nw, height);
          }

          //          std::cout << "Look up " << plane << std::endl;

          PNGBytes& bytes_pos = *plane_dig_imgs_pos[plane];
          PNGBytes& bytes_neg = *plane_dig_imgs_neg[plane];

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

            if(adc > 0){
              //              dig_bytes_pos.data[tick][digIdx] = std::min(+adc, 255);
              bytes_pos.data[tick][wire.Wire-w0.Wire] = std::min(+adc, 255);
            }
            else if(adc < 0){
              bytes_neg.data[tick][wire.Wire-w0.Wire] = std::min(-adc, 255);
            }
            //            else bytes.data[tick][wire.Wire-w0.Wire] = 128;
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
            bytes.data[tick][wire.Wire-w0.Wire] = std::max(0, std::min(int(10*adcs[tick]), 255));
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

      if(fPlots3D) Plot3D(*pts, suffix);
      //    }
      //  }

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
  for(auto it: plane_dig_imgs_pos){
    const geo::PlaneID plane = it.first;
    const geo::PlaneGeo& planegeo = geom->Plane(plane);
    const int view = planegeo.View();
    const unsigned int nwires = planegeo.Nwires();
    const double pitch = planegeo.WirePitch();
    const TVector3 c = planegeo.GetCenter();
    //    const auto d = planegeo.GetIncreasingWireDirection();

    const int nticks = height; // HACK from earlier
    const double tick_pitch = detprop->ConvertTicksToX(1, plane) - detprop->ConvertTicksToX(0, plane);

    outf << "  \"" << plane << "\": {"
         << "view: " << view << ", "
         << "nwires: " << nwires << ", "
         << "pitch: " << pitch << ", "
         << "nticks: " << nticks << ", "
         << "tick_pitch: " << tick_pitch << ", "
         << "center: [" << c.X() << ", " << c.Y() << ", " << c.Z() << "], "
         << "hits: [";
    for(const recob::Hit& hit: plane_hits[plane]){
      outf << "{wire: " << geo::WireID(hit.WireID()).Wire
           << ", tick: " << hit.PeakTime() << "}, ";
    }

    outf << "]}," << std::endl;
  }
  outf << "};" << std::endl;

  for(auto it: plane_dig_imgs_pos) WriteToPNG("digits/"+it.first.toString()+"_pos.png", *it.second);
  for(auto it: plane_dig_imgs_neg) WriteToPNG("digits/"+it.first.toString()+"_neg.png", *it.second);

  for(auto it: plane_wire_imgs) WriteToPNG("wires/"+it.first.toString()+".png", *it.second);

  //  WriteToPNG("digits_pos.png", dig_bytes_pos);
  //  WriteToPNG("digits_neg.png", dig_bytes_neg);
  //  WriteToPNG("wires.png", wire_bytes);

  return;

  if(fPlotsTrue){
    art::Handle<std::vector<recob::Hit>> hits;
    evt.getByLabel(fHitLabel, hits);
    const std::vector<recob::SpacePoint> pts = TrueSpacePoints(hits);

    const std::string suffix = TString::Format("%s_true_%d", fSuffix.c_str(), evt.event()).Data();

    Plot(pts, suffix);

    if(fPlots3D) Plot3D(pts, suffix);
  }
}

} // namespace
