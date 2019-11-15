#ifndef WEBEVD_TEMPORARIES_H
#define WEBEVD_TEMPORARIES_H

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace evd
{
  class Temporaries
  {
  public:
    Temporaries();
    ~Temporaries();

    std::string DirectoryName() const {return fTempDir;}

    FILE* fopen(const std::string& fname, const char* mode);
    std::ofstream ofstream(const std::string& fname);
    int symlink(const std::string& dir, const std::string& fname);

  protected:
    std::string fTempDir;
    std::vector<std::string> fCleanup;
  };
}

#endif
