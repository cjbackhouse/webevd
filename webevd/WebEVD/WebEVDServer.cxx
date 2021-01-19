// Chris Backhouse - bckhouse@fnal.gov

#include "webevd/WebEVD/WebEVDServer.h"

#include "webevd/WebEVD/PNGArena.h"

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
#include "lardataalg/DetectorInfo/DetectorPropertiesData.h"

#include <sys/types.h>
#include <sys/socket.h>
//#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "signal.h"

#include "zlib.h"

#include <thread>

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
  : fSock(0)
{
}

// ----------------------------------------------------------------------------
template<class T> WebEVDServer<T>::~WebEVDServer()
{
  if(fSock) close(fSock);
}

short swap_byte_order(short x)
{
  char* cx = (char*)&x;
  std::swap(cx[0], cx[1]);
  return x;
}

void write_ok200(int sock,
                 const std::string content = "text/html",
                 bool gzip = false)
{
  std::string str =
    "HTTP/1.0 200 OK\r\n"
    "Server: WebEVD/1.0.0\r\n"
    "Content-Type: "+content+"\r\n";

  if(gzip) str += "Content-Encoding: gzip\r\n";

  str += "\r\n";

  write(sock, &str.front(), str.size());
}

void write_notfound404(int sock)
{
  const char str[] =
    "HTTP/1.0 404 Not Found\r\n"
    "Server: WebEVD/1.0.0\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "404. Huh?\r\n";

  write(sock, str, strlen(str));
}

void write_unimp501(int sock)
{
  const char str[] =
    "HTTP/1.0 501 Not Implemented\r\n"
    "Server: WebEVD/1.0.0\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "I don't know how to do that\r\n";

  write(sock, str, strlen(str));
}

void write_file(const std::string& fname, int sock)
{
  std::cout << "Serving " << fname << std::endl;

  std::vector<char> buf(1024*1024);
  FILE* f = fopen(fname.c_str(), "r");

  if(!f){
    std::cout << fname << " not found!" << std::endl;
    return;
  }

  while(true){
    int nread = fread(&buf.front(), 1, buf.size(), f);
    if(nread <= 0){
      std::cout << "Done\n" << std::endl;
      fclose(f);
      return;
    }
    std::cout << "Writing " << nread << " bytes" << std::endl;
    write(sock, &buf.front(), nread);
  }
}

std::string read_all(int sock)
{
  std::string ret;

  std::vector<char> buf(1024*1024);
  while(true){
    const int nread = read(sock, &buf.front(), buf.size());
    if(nread == 0) return ret;
    ret.insert(ret.end(), buf.begin(), buf.begin()+nread);
    // Only handle GETs, so no need to wait for payload (for which we'd need to
    // check Content-Length too).
    if(ret.find("\r\n\r\n") != std::string::npos) return ret;
  }
}

EResult err(const char* call)
{
  std::cout << call << "() error " << errno << " = " << strerror(errno) << std::endl;
  return kERROR;
  //  return errno;
}

Result HandleCommand(std::string cmd, int sock)
{
  EResult code = kERROR;
  int run = -1, subrun = -1, evt = -1;

  if(cmd == "/QUIT") code = kQUIT;
  if(cmd == "/NEXT") code = kNEXT;
  if(cmd == "/PREV") code = kPREV;

  if(cmd.find("/seek/") == 0){
    code = kSEEK;
    char* ctx;
    strtok_r(cmd.data(), "/", &ctx); // consumes the "seek" text
    run    = atoi(strtok_r(0, "/", &ctx));
    subrun = atoi(strtok_r(0, "/", &ctx));
    evt    = atoi(strtok_r(0, "/", &ctx));
    // if this goes wrong we get zeros, which seems a reasonable fallback
  }

  write_ok200(sock, "text/html", false);

  const int delay = (code == kQUIT) ? 2000 : 0;
  const std::string txt = (code == kQUIT) ? "Goodbye!" : "Please wait...";

  const std::string msg = TString::Format("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><script>setTimeout(function(){window.location.replace('/');}, %d);</script></head><body style=\"background-color:black;color:white;\"><h1>%s</h1></body></html>", delay, txt.c_str()).Data();

  write(sock, msg.c_str(), msg.size());
  close(sock);

  if(code == kSEEK){
    return Result(kSEEK, run, subrun, evt);
  }
  else{
    return code;
  }
}

// ----------------------------------------------------------------------------
std::string FindWebDir()
{
  std::string webdir;

  // For development purposes we prefer to serve the files from the source
  // directory, which allows them to be live-edited with just a refresh of the
  // browser window to see them.
  if(getenv("MRB_SOURCE")) cet::search_path("MRB_SOURCE").find_file("webevd/webevd/WebEVD/web/", webdir);
  // Otherwise, serve the files from where they get installed
  if(webdir.empty() && getenv("PRODUCTS") && getenv("WEBEVD_VERSION")) cet::search_path("PRODUCTS").find_file("webevd/"+std::string(getenv("WEBEVD_VERSION"))+"/webevd/", webdir);

  if(webdir.empty()){
    std::cout << "Unable to find webevd files under $MRB_SOURCE or $PRODUCTS" << std::endl;
    abort();
  }

  return webdir;
}

// ----------------------------------------------------------------------------
void _HandleGetPNG(std::string doc, int sock, PNGArena* arena)
{
  const std::string mime = "image/png";

  write_ok200(sock, mime, false);

  // Parse the filename
  char* ctx;
  strtok_r(&doc.front(), "_", &ctx); // consume the "arena" text
  const int imgIdx = atoi(strtok_r(0, "_", &ctx));
  const int dim = atoi(strtok_r(0, ".", &ctx));

  FILE* f = fdopen(sock, "wb");
  arena->WritePNGBytes(f, imgIdx, dim);
  fclose(f);
}

// ----------------------------------------------------------------------------
void gzip_buffer(unsigned char* src,
                 int length,
                 std::vector<unsigned char>& dest,
                 int level)
{
  // C++20 will allow to use designated initializers here
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  strm.next_in = src;
  strm.avail_in = length;

  // The 16 here is the secret sauce to get gzip header and trailer for some
  // reason...
  deflateInit2(&strm, level, Z_DEFLATED, 15 | 16, 9, Z_DEFAULT_STRATEGY);

  // If we allocate a big enough buffer we can deflate in one pass
  dest.resize(deflateBound(&strm, length));

  strm.next_out = dest.data();
  strm.avail_out = dest.size();

  deflate(&strm, Z_FINISH);

  dest.resize(dest.size() - strm.avail_out);

  deflateEnd(&strm);
}

// ----------------------------------------------------------------------------
void write_compressed_buffer(unsigned char* src,
                             int length,
                             int sock,
                             int level)
{
  std::vector<unsigned char> dest;
  gzip_buffer(src, length, dest, level);

  std::cout << "Writing " << length << " bytes (compressed to " << dest.size() << ")\n" << std::endl;

  write(sock, dest.data(), dest.size());
}

// ----------------------------------------------------------------------------
void write_compressed_file(const std::string& loc, int fd_out, int level)
{
  int fd_in = open(loc.c_str(), O_RDONLY);

  // Map in the whole file
  struct stat st;
  fstat(fd_in, &st);
  unsigned char* src = (unsigned char*)mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd_in, 0);

  write_compressed_buffer(src, st.st_size, fd_out, level);

  munmap(src, st.st_size);

  close(fd_in);
}

// ----------------------------------------------------------------------------
void _HandleGet(std::string doc, int sock, PNGArena* arena, std::string* coords, std::string* jsonsp, std::string* jsonhits, std::string* jsontrks, std::string* jsontrajs)
{
  if(doc == "/") doc = "/index.html";

  if(doc.find(".png") != std::string::npos){
    _HandleGetPNG(doc, sock, arena);
    return;
  }

  // TODO - more sophisticated MIME type handling
  std::string mime = "text/html";
  if(doc.find(".js") != std::string::npos) mime = "application/javascript";
  if(doc.find(".css") != std::string::npos) mime = "text/css";
  if(doc.find(".ico") != std::string::npos) mime = "image/vnd.microsoft.icon";

  // Index of files that we have as strings in memory
  const std::map<std::string, std::string*> toc =
    {{"/coords.js",        coords},
     {"/spacepoints.json", jsonsp},
     {"/hits.json",        jsonhits},
     {"/tracks.json",      jsontrks},
     {"/trajs.json",       jsontrajs}};

  auto it = toc.find(doc);
  if(it != toc.end()){
    const std::string* s = it->second;

    write_ok200(sock, mime, true);
    write_compressed_buffer((unsigned char*)s->data(), s->size(), sock, Z_DEFAULT_COMPRESSION);
  }
  else{
    // Otherwise it must be a physical file

    // Don't accidentally serve any file we shouldn't
    const std::set<std::string> whitelist = {"/evd.css", "/evd.js", "/favicon.ico", "/index.html"};

    if(whitelist.count(doc)){
      write_ok200(sock, mime, true);
      write_compressed_file(FindWebDir()+doc, sock, Z_DEFAULT_COMPRESSION);
    }
    else{
      write_notfound404(sock);
    }
  }

  close(sock);
}

// ----------------------------------------------------------------------------
template<class T> int WebEVDServer<T>::EnsureListen()
{
  if(fSock != 0) return 0;

  char host[1024];
  gethostname(host, 1024);
  char* user = getlogin();

  std::cout << "\n------------------------------------------------------------\n" << std::endl;

  // E1071 is DUNE :)
  int port = 1071;

  // Search for an open port up-front
  while(system(TString::Format("ss -an | grep -q %d", port).Data()) == 0) ++port;


  fSock = socket(AF_INET, SOCK_STREAM, 0);
  if(fSock == -1) return err("socket");

  // Reuse port immediately even if a previous instance just aborted.
  const int one = 1;
  if(setsockopt(fSock, SOL_SOCKET, SO_REUSEADDR,
                &one, sizeof(one)) != 0) return err("setsockopt");

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = swap_byte_order(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if(bind(fSock, (sockaddr*)&addr, sizeof(addr)) != 0) return err("bind");

  if(listen(fSock, 128/*backlog*/) != 0) return err("listen");


  std::cout << "First run" << std::endl;
  std::cout << "ssh -L "
            << port << ":localhost:" << port << " "
            << user << "@" << host << std::endl << std::endl;
  std::cout << "and then navigate to localhost:" << port << " in your favorite browser." << std::endl << std::endl;
  //  std::cout << "Press Ctrl-C here when done." << std::endl;

  return 0;
}

// ----------------------------------------------------------------------------
template<class T> Result WebEVDServer<T>::do_serve(PNGArena& arena)
{
  if(EnsureListen() != 0) return kERROR;

  std::list<std::thread> threads;

  while(true){
    int sock = accept(fSock, 0, 0);
    if(sock == -1) return err("accept");

    std::string req = read_all(sock);

    std::cout << req << std::endl;

    char* ctx;
    char* verb = strtok_r(&req.front(), " ", &ctx);

    if(verb && std::string(verb) == "GET"){
      char* freq = strtok_r(0, " ", &ctx);
      std::string sreq(freq);

      if(sreq == "/NEXT" ||
         sreq == "/PREV" ||
         sreq == "/QUIT" ||
         sreq.find("/seek/") == 0){
        for(std::thread& t: threads) t.join();
        return HandleCommand(sreq, sock);
      }
      else{
        threads.emplace_back(_HandleGet, sreq, sock, &arena, &fCoords, &fSpacePointJSON, &fHitsJSON, &fTracksJSON, &fTrajsJSON);
      }
    }
    else{
      write_unimp501(sock);
      close(sock);
    }
  }

  // unreachable
}

// ----------------------------------------------------------------------------
class JSONFormatter
{
public:
  JSONFormatter(std::ostream& os) : fStream(os) {}

  template<class T> JSONFormatter& operator<<(const T& x)
  {
    static_assert(std::is_arithmetic_v<T> ||
                  std::is_enum_v<T> ||
                  std::is_same_v<T, std::string> ||
                  std::is_same_v<T, geo::OpDetID>);
    fStream << x;
    return *this;
  }

  JSONFormatter& operator<<(double x)
  {
    if(isnan(x)) fStream << "NaN";
    else if(isinf(x)) fStream << "Infinity";
    else fStream << x;
    return *this;
  }

  JSONFormatter& operator<<(float x){return *this << double(x);}

  JSONFormatter& operator<<(const char* x)
  {
    fStream << x;
    return *this;
  }

  template<class T> JSONFormatter& operator<<(const std::vector<T>& v)
  {
    fStream << "[";
    for(const T& x: v){
      (*this) << x;
      if(&x != &v.back()) (*this) << ", ";
    }
    fStream << "]";
    return *this;
  }

  template<class T, class U>
  JSONFormatter& operator<<(const std::map<T, U>& m)
  {
    fStream << "{\n";
    unsigned int n = 0;
    for(auto& it: m){
      (*this) << "  " << it.first << ": " << it.second;
      ++n;
      if(n != m.size()) (*this) << ",\n";
    }
    fStream << "\n}";
    return *this;
  }

  JSONFormatter& operator<<(const TVector3& v)
  {
    *this << "["
          << v.X() << ", "
          << v.Y() << ", "
          << v.Z() << "]";
    return *this;
  }

  JSONFormatter& operator<<(const art::InputTag& t)
  {
    fStream << "\"" << t.label();
    if(!t.instance().empty()) fStream << ":" << t.instance();
    if(!t.process().empty()) fStream << ":" << t.process();
    fStream << "\"";
    return *this;
  }

  JSONFormatter& operator<<(const geo::OpDetID& id)
  {
    fStream << "\"" << id << "\"";
    return *this;
  }

protected:
  std::ostream& fStream;
};

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const geo::PlaneID& plane)
{
  return json << "\"" << std::string(plane) << "\"";
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const recob::Hit& hit)
{
  return json << "{\"wire\": " << geo::WireID(hit.WireID()).Wire
              << ", \"tick\": " << hit.PeakTime()
              << ", \"rms\": " << hit.RMS() << "}";
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

  return json << "{ \"positions\": " << pts << " }";
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const simb::MCParticle& part)
{
  const int apdg = abs(part.PdgCode());
  if(apdg == 12 || apdg == 14 || apdg == 16) return json << "{ \"pdg\": " << apdg << ", \"positions\": [] }"; // decay neutrinos
  std::vector<TVector3> pts;
  for(unsigned int j = 0; j < part.NumberTrajectoryPoints(); ++j){
    pts.emplace_back(part.Vx(j), part.Vy(j), part.Vz(j));
  }

  return json << "{ \"pdg\": " << apdg << ", \"positions\": " << pts << " }";
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const geo::CryostatGeo& cryo)
{
  const TVector3 r0(cryo.MinX(), cryo.MinY(), cryo.MinZ());
  const TVector3 r1(cryo.MaxX(), cryo.MaxY(), cryo.MaxZ());
  return json << "{ min: "  << r0 << ", max: " << r1 << " }";
}

// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& json, const geo::OpDetGeo& opdet)
{
  return json << "{ name: " << opdet.ID() << ", "
              << "center: " << TVector3(opdet.GetCenter().X(),
                                        opdet.GetCenter().Y(),
                                        opdet.GetCenter().Z()) << ", "
              << "length: " << opdet.Length() << ", "
              << "width: " << opdet.Width() << ", "
              << "height: " << opdet.Height() << " }";
}


// ----------------------------------------------------------------------------
JSONFormatter& operator<<(JSONFormatter& os, const PNGView& v)
{
  os << "{blocks: [";
  for(unsigned int ix = 0; ix < v.blocks.size(); ++ix){
    for(unsigned int iy = 0; iy < v.blocks[ix].size(); ++iy){
      const png_byte* b = v.blocks[ix][iy];
      if(!b) continue;

      int dataidx = 0;
      for(unsigned int d = 0; d < v.arena.data.size(); ++d){
        if(b >= &v.arena.data[d]->front() &&
           b <  &v.arena.data[d]->front() + 4*PNGArena::kArenaSize*PNGArena::kArenaSize){
          dataidx = d;
          break;
        }
      }

      const int texdx = ((b-&v.arena.data[dataidx]->front())/4)%PNGArena::kArenaSize;
      const int texdy = ((b-&v.arena.data[dataidx]->front())/4)/PNGArena::kArenaSize;

      os << "{"
         << "x: " << ix*PNGArena::kBlockSize << ", "
         << "y: " << iy*PNGArena::kBlockSize << ", "
         << "dx: " << PNGArena::kBlockSize << ", "
         << "dy: " << PNGArena::kBlockSize << ", "
         << "fname: \"" << v.arena.name << "_" << dataidx << "\", "
         << "texdim: " << PNGArena::kArenaSize << ", "
         << "u: " << texdx << ", "
         << "v: " << texdy << ", "
         << "du: " << PNGArena::kBlockSize << ", "
         << "dv: " << PNGArena::kBlockSize
         << "}, ";
    }
  }
  os << "]}";
  return os;
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

  try{
    evt.getValidHandle<std::vector<T>>(art::InputTag(label));
  }
  catch(...){
    std::cout << "...but \"" << label << "\" not found in file" << std::endl;
    return {};
  }

  return {art::InputTag(label)};
}

// ----------------------------------------------------------------------------
template<class TProd, class TEvt> void
SerializeProduct(const TEvt& evt,
                 JSONFormatter& json,
                 const std::string& label)
{
  if(!label.empty()) json << "var " << label << " = {\n"; else json << "{";

  const std::vector<art::InputTag> tags = getInputTags<TProd>(evt);
  for(const art::InputTag& tag: tags){
    json << "  " << tag << ": ";

    typename TEvt::template HandleT<std::vector<TProd>> prods; // deduce handle type
    evt.getByLabel(tag, prods);

    json << *prods;

    if(tag != tags.back()){
      json << ",";
    }
    json << "\n";
  }
  if(!label.empty()) json << "};\n\n"; else json << "}";
}

// ----------------------------------------------------------------------------
template<class TProd, class TEvt> void
SerializeProductByLabel(const TEvt& evt,
                        const std::string& in_label,
                        JSONFormatter& json)
                                                    //                        const std::string& out_label)
{
  typename TEvt::template HandleT<std::vector<TProd>> prods; // deduce handle type
  evt.getByLabel(in_label, prods);

  //  json << "var " << out_label << " = ";
  if(prods.isValid()){
    json << *prods;// << ";\n\n";
  }
  else{
    json << "[]";//;\n\n";
  }
}

// ----------------------------------------------------------------------------
template<class TEvt>
unsigned long HandleDigits(const TEvt& evt, const geo::GeometryCore* geom,
                           PNGArena& arena, JSONFormatter& json)
{
  unsigned long maxTick = 0;

  std::map<art::InputTag, std::map<geo::PlaneID, PNGView>> imgs;

  for(art::InputTag tag: getInputTags<raw::RawDigit>(evt)){
    typename TEvt::template HandleT<std::vector<raw::RawDigit>> digs; // deduce handle type
    evt.getByLabel(tag, digs);

    for(const raw::RawDigit& dig: *digs) maxTick = std::max(maxTick, (unsigned long)dig.Samples());

    for(const raw::RawDigit& dig: *digs){
      for(geo::WireID wire: geom->ChannelToWire(dig.Channel())){
        //        const geo::TPCID tpc(wire);
        const geo::PlaneID plane(wire);

        const geo::WireID w0 = geom->GetBeginWireID(plane);
        const unsigned int Nw = geom->Nwires(plane);

        if(imgs[tag].count(plane) == 0){
          imgs[tag].emplace(plane, PNGView(arena, Nw, maxTick));
        }

        PNGView& bytes = imgs[tag].find(plane)->second;

        raw::RawDigit::ADCvector_t adcs(dig.Samples());
        raw::Uncompress(dig.ADCs(), adcs, dig.Compression());

        for(unsigned int tick = 0; tick < adcs.size(); ++tick){
          const int adc = adcs[tick] ? int(adcs[tick])-dig.GetPedestal() : 0;

          if(adc != 0){
            // alpha
            bytes(wire.Wire-w0.Wire, tick, 3) = std::min(abs(adc), 255);
            if(adc > 0){
              // red
              bytes(wire.Wire-w0.Wire, tick, 0) = 255;
            }
            else{
              // blue
              bytes(wire.Wire-w0.Wire, tick, 2) = 255;
            }
          }
        } // end for tick
      } // end for wire
    } // end for dig
  } // end for tag

  json << "var xdigs = " << imgs << ";\n\n";

  return maxTick;
}

// ----------------------------------------------------------------------------
template<class TEvt>
unsigned long HandleWires(const TEvt& evt, const geo::GeometryCore* geom,
                          PNGArena& arena, JSONFormatter& json)
{
  unsigned long maxTick = 0;

  std::map<art::InputTag, std::map<geo::PlaneID, PNGView>> imgs;

  for(art::InputTag tag: getInputTags<recob::Wire>(evt)){
    typename TEvt::template HandleT<std::vector<recob::Wire>> wires; // deduce handle type
    evt.getByLabel(tag, wires);

    for(const recob::Wire& wire: *wires) maxTick = std::max(maxTick, wire.NSignal());

    for(const recob::Wire& rbwire: *wires){
      for(geo::WireID wire: geom->ChannelToWire(rbwire.Channel())){
        //        const geo::TPCID tpc(wire);
        const geo::PlaneID plane(wire);

        const geo::WireID w0 = geom->GetBeginWireID(plane);
        const unsigned int Nw = geom->Nwires(plane);

        if(imgs[tag].count(plane) == 0){
          imgs[tag].emplace(plane, PNGView(arena, Nw, maxTick));
        }

        PNGView& bytes = imgs[tag].find(plane)->second;

        const auto adcs = rbwire.Signal();
        for(unsigned int tick = 0; tick < adcs.size(); ++tick){
          if(adcs[tick] <= 0) continue;

          // green channel
          bytes(wire.Wire-w0.Wire, tick, 1) = 128; // dark green
          // alpha channel
          bytes(wire.Wire-w0.Wire, tick, 3) = std::max(0, std::min(int(10*adcs[tick]), 255));
        } // end for tick
      } // end for wire
    } // end for rbwire
  } // end for tag

  json << "var xwires = " << imgs << ";\n\n";

  return maxTick;
}

// ----------------------------------------------------------------------------
template<class TEvt>
void HandleHits(const TEvt& evt, const geo::GeometryCore* geom,
                JSONFormatter& json)
{
  std::map<art::InputTag, std::map<geo::PlaneID, std::vector<recob::Hit>>> plane_hits;

  for(art::InputTag tag: getInputTags<recob::Hit>(evt)){
    typename TEvt::template HandleT<std::vector<recob::Hit>> hits; // deduce handle type
    evt.getByLabel(tag, hits);

    for(const recob::Hit& hit: *hits){
      // Would possibly be right for disambiguated hits?
      //    const geo::WireID wire(hit.WireID());

      for(geo::WireID wire: geom->ChannelToWire(hit.Channel())){
        const geo::PlaneID plane(wire);

        // Correct for disambiguated hits
        //      plane_hits[plane].push_back(hit);

        // Otherwise we have to update the wire number
        plane_hits[tag][plane].emplace_back(hit.Channel(), hit.StartTick(), hit.EndTick(), hit.PeakTime(), hit.SigmaPeakTime(), hit.RMS(), hit.PeakAmplitude(), hit.SigmaPeakAmplitude(), hit.SummedADC(), hit.Integral(), hit.SigmaIntegral(), hit.Multiplicity(), hit.LocalIndex(), hit.GoodnessOfFit(), hit.DegreesOfFreedom(), hit.View(), hit.SignalType(), wire);
      }
    }
  } // end for tag

  //  json << "var xhits = " << plane_hits << ";\n\n";
  json << plane_hits;
}


// ----------------------------------------------------------------------------
template<class TEvt>
void HandlePlanes(const TEvt& evt, const geo::GeometryCore* geom,
                  const detinfo::DetectorPropertiesData& detprop,
                  JSONFormatter& json, unsigned long maxTick)
{
  json << "var planes = {\n";
  for(geo::PlaneID plane: geom->IteratePlaneIDs()){
    const geo::PlaneGeo& planegeo = geom->Plane(plane);
    const int view = planegeo.View();
    const unsigned int nwires = planegeo.Nwires();
    const double pitch = planegeo.WirePitch();
    const TVector3 c = planegeo.GetCenter();

    const TVector3 d = planegeo.GetIncreasingWireDirection();
    const TVector3 n = planegeo.GetNormalDirection();

    const TVector3 wiredir = planegeo.GetWireDirection();
    const double depth = planegeo.Depth(); // really height

    const double tick_origin = detprop.ConvertTicksToX(0, plane);
    const double tick_pitch = detprop.ConvertTicksToX(1, plane) - tick_origin;

    json << "  " << plane << ": {"
         << "view: " << view << ", "
         << "nwires: " << nwires << ", "
         << "pitch: " << pitch << ", "
         << "nticks: " << maxTick << ", "
         << "tick_origin: " << tick_origin << ", "
         << "tick_pitch: " << tick_pitch << ", "
         << "center: " << c << ", "
         << "across: " << d << ", "
         << "wiredir: " << wiredir << ", "
         << "depth: " << depth << ", "
         << "normal: " << n << "},\n";
  }
  json << "};\n\n";
}

// ----------------------------------------------------------------------------
template<class T> void WebEVDServer<T>::
FillCoordsAndArena(const T& evt,
                   const geo::GeometryCore* geom,
                   const detinfo::DetectorPropertiesData& detprop,
                   PNGArena& arena)
{
  std::stringstream outf;
  std::stringstream outfsp;
  std::stringstream outfhits;
  std::stringstream outftrks;
  std::stringstream outftrajs;

  JSONFormatter json(outf);
  JSONFormatter jsonsp(outfsp);
  JSONFormatter jsonhits(outfhits);
  JSONFormatter jsontrk(outftrks);
  JSONFormatter jsontraj(outftrajs);

  if constexpr (std::is_same_v<T, art::Event>){
    // art
    json << "var run = " << evt.run() << ";\n"
         << "var subrun = " << evt.subRun() << ";\n"
         << "var evt  = " << evt.event() << ";\n\n";
  }
  else{
    // gallery
    const art::EventAuxiliary& aux = evt.eventAuxiliary();
    json << "var run = " << aux.run() << ";\n"
         << "var subrun = " << aux.subRun() << ";\n"
         << "var evt  = " << aux.event() << ";\n\n";
  }

  unsigned long maxTick = 0;
  maxTick = std::max(maxTick, HandleDigits(evt, geom, arena, json));
  maxTick = std::max(maxTick, HandleWires (evt, geom, arena, json));

  HandleHits(evt, geom, jsonhits);

  HandlePlanes(evt, geom, detprop, json, maxTick);

  SerializeProduct<recob::Track>(evt, jsontrk, "");//"tracks");

  SerializeProduct<recob::SpacePoint>(evt, jsonsp, "");//, "spacepoints");

  SerializeProduct<recob::Vertex>(evt, json, "reco_vtxs");

  SerializeProductByLabel<simb::MCParticle>(evt, "largeant", jsontraj);//"truth_trajs");

  json << "var cryos = [\n";
  for(const geo::CryostatGeo& cryo: geom->IterateCryostats()){
    json << "  " << cryo << ",\n";
  }
  json << "];\n\n";

  json << "var opdets = [\n";
  for(unsigned int i = 0; i < geom->NOpDets(); ++i){
    json << "  " << geom->OpDetGeoFromOpDet(i);
    if(i != geom->NOpDets()-1) json << ",\n"; else json << "\n";
  }
  json << "];\n";

  fCoords = outf.str();

  fSpacePointJSON = outfsp.str();
  fHitsJSON = outfhits.str();
  fTracksJSON = outftrks.str();
  fTrajsJSON = outftrajs.str();
}

// ----------------------------------------------------------------------------
template<class T> Result WebEVDServer<T>::
serve(const T& evt,
      const geo::GeometryCore* geom,
      const detinfo::DetectorPropertiesData& detprop)
{
  // Don't want a sigpipe signal when the browser hangs up on us. This way we
  // will get an error return from the write() call instead.
  signal(SIGPIPE, SIG_IGN);

  PNGArena arena("arena");
  FillCoordsAndArena(evt, geom, detprop, arena);
  return do_serve(arena);
}

template class WebEVDServer<art::Event>;
template class WebEVDServer<gallery::Event>;

} // namespace
