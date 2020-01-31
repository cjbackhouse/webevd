#ifndef WEBEVD_PNGARENA_H
#define WEBEVD_PNGARENA_H

#include "dune/EVD/Temporaries.h"

#include <string>
#include <vector>

typedef unsigned char png_byte;

namespace evd
{
  class PNGArena
  {
  public:
    // Square because seems to be necessary for mipmapping. Larger than this
    // doesn't seem to work in the browser. Smaller won't fit the number of
    // ticks we have.
    static const int kArenaSize = 4096;//8192;
    static const int kBlockSize = 64;

    PNGArena(const std::string& name);

    inline png_byte& operator()(int i, int x, int y, int c)
    {
      return data[i][(y*extent+x)*4+c];
    }

    inline const png_byte& operator()(int i, int x, int y, int c) const
    {
      return data[i][(y*extent+x)*4+c];
    }

    png_byte* NewBlock();
    
    std::string name;
    int extent;
    int elemx, elemy;
    int nviews;

    std::vector<std::vector<png_byte>> data;
  };


  class PNGView
  {
  public:
    PNGView(PNGArena& a, int w, int h);

    inline png_byte& operator()(int x, int y, int c)
    {
      const int ix = x/PNGArena::kBlockSize;
      const int iy = y/PNGArena::kBlockSize;
      if(!blocks[ix][iy]) blocks[ix][iy] = arena.NewBlock();
      return blocks[ix][iy][((y-iy*PNGArena::kBlockSize)*arena.extent+(x-ix*PNGArena::kBlockSize))*4+c];
    }

    inline png_byte operator()(int x, int y, int c) const
    {
      const int ix = x/PNGArena::kBlockSize;
      const int iy = y/PNGArena::kBlockSize;
      if(!blocks[ix][iy]) return 0;
      return blocks[ix][iy][((y-iy*PNGArena::kBlockSize)*arena.extent+(x-ix*PNGArena::kBlockSize))*4+c];
    }

  protected:
    template<class T> friend T& operator<<(T&, const PNGView&);

    PNGArena& arena;
    int width, height;
    std::vector<std::vector<png_byte*>> blocks;
  };


  void MipMap(PNGArena& bytes, int newdim);

  void AnalyzeArena(const PNGArena& bytes);

  void WriteToPNG(Temporaries& tmp,
                  const std::string& prefix,
                  const PNGArena& bytes,
                  int mipmapdim = -1);

  // NB: destroys "bytes" in the process
  void WriteToPNGWithMipMaps(Temporaries& tmp,
                             const std::string& prefix, PNGArena& bytes);

} // namespace

#endif
