#ifndef WEBEVDSERVER_H
#define WEBEVDSERVER_H

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
    std::string fTempDir;
    std::vector<std::string> fCleanup;
  };
}

#endif
