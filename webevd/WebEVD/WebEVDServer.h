#ifndef WEBEVDSERVER_H
#define WEBEVDSERVER_H

#include <string>
#include <vector>

namespace geo{class GeometryCore;}
namespace detinfo{class DetectorPropertiesData;}

namespace evd
{
  class ColorRamp;
  class PNGArena;

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
                 const detinfo::DetectorPropertiesData& detprop,
                 const evd::ColorRamp* ramp);

  protected:
    template<class PROD> using HandleT = typename T::template HandleT<std::vector<PROD>>;

    int EnsureListen();

    int fSock;
  };
}

#endif
