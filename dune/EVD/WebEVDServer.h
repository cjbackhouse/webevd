#ifndef WEBEVDSERVER_H
#define WEBEVDSERVER_H

#include "dune/EVD/Temporaries.h"

#include <string>
#include <vector>

namespace geo{class GeometryCore;}
namespace detinfo{class DetectorProperties;}

namespace evd
{
  template<class T> class WebEVDServer
  {
  public:
    WebEVDServer();
    ~WebEVDServer();
    void analyze(const T& evt,
                 const geo::GeometryCore* geom,
                 const detinfo::DetectorProperties* detprop);
    void serve();
  protected:
    Temporaries fTmp;
  };
}

#endif
