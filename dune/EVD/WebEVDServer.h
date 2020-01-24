#ifndef WEBEVDSERVER_H
#define WEBEVDSERVER_H

#include "dune/EVD/Temporaries.h"

#include <string>
#include <vector>

namespace geo{class GeometryCore;}
namespace detinfo{class DetectorProperties;}

namespace evd
{
  enum EResult{kNEXT, kPREV, kQUIT, kERROR};

  template<class T> class WebEVDServer
  {
  public:
    WebEVDServer();
    ~WebEVDServer();

    EResult serve(const T& evt,
                  const geo::GeometryCore* geom,
                  const detinfo::DetectorProperties* detprop);

  protected:
    template<class PROD> using HandleT = typename T::template HandleT<std::vector<PROD>>;

    void WriteFiles(const T& evt,
                    const geo::GeometryCore* geom,
                    const detinfo::DetectorProperties* detprop,
                    Temporaries& tmp);

    EResult do_serve(Temporaries& tmp);

    int EnsureListen();

    int fSock;
  };
}

#endif
