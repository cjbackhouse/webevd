#ifndef WEBEVDSERVER_H
#define WEBEVDSERVER_H

#include <string>
#include <vector>

namespace geo{class GeometryCore;}
namespace detinfo{class DetectorPropertiesData;}

namespace evd
{
  class PNGArena;
  class PNGServer;

  enum EResult{kNEXT, kPREV, kQUIT, kERROR, kSEEK};
  struct Result{
    Result(EResult c) : code(c) {}
    Result(EResult c, int r, int s, int e) : code(c), run(r), subrun(s), event(e) {}
    EResult code; int run, subrun, event;
  };

  template<class T> class WebEVDServer
  {
  public:
    WebEVDServer();
    ~WebEVDServer();

    Result serve(const T& evt,
                 const geo::GeometryCore* geom,
                 const detinfo::DetectorPropertiesData& detprop);

  protected:
    template<class PROD> using HandleT = typename T::template HandleT<std::vector<PROD>>;

    void FillCoordsAndArena(const T& evt,
                            const geo::GeometryCore* geom,
                            const detinfo::DetectorPropertiesData& detprop,
                            PNGArena& arena);

    Result do_serve(PNGArena& arena);

    int EnsureListen();

    int fSock;

    std::string fCoords;
  };
}

#endif
