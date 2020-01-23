#include "dune/EVD/Temporaries.h"

#include "TString.h"

namespace evd
{
  // --------------------------------------------------------------------------
  Temporaries::Temporaries()
  {
    fTempDir = "/tmp/webevd_XXXXXX";
    mkdtemp(fTempDir.data());
  }

  // --------------------------------------------------------------------------
  Temporaries::~Temporaries()
  {
    for(const std::string& f: fCleanup) unlink((fTempDir+"/"+f).c_str());

    if(rmdir(fTempDir.c_str()) != 0){
      std::cout << "Failed to clean up temporary directory " << fTempDir << std::endl;
    }
  }

  // --------------------------------------------------------------------------
  FILE* Temporaries::fopen(const std::string& fname, const char* mode)
  {
    fCleanup.push_back(fname);
    return ::fopen((fTempDir+"/"+fname).c_str(), mode);
  }

  // --------------------------------------------------------------------------
  std::ofstream Temporaries::ofstream(const std::string& fname)
  {
    fCleanup.push_back(fname);
    return std::ofstream(fTempDir+"/"+fname);
  }

  // --------------------------------------------------------------------------
  int Temporaries::symlink(const std::string& dir, const std::string& fname)
  {
    fCleanup.push_back(fname);
    return ::symlink(TString::Format("%s/%s", dir.c_str(),      fname.c_str()).Data(),
                     TString::Format("%s/%s", fTempDir.c_str(), fname.c_str()).Data());
  }

  // --------------------------------------------------------------------------
  std::string Temporaries::compress(const std::string& fname)
  {
    const std::string tgt = fname+".gz";
    // Don't destroy original, use fastest compression
    system(("gzip -c -1 " + fTempDir+"/"+fname+" > " + fTempDir+"/"+tgt).c_str());
    fCleanup.push_back(tgt);
    return fTempDir+"/"+tgt;
  }
}
