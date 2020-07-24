#ifndef WEBEVD_PNGARENA_H
#define WEBEVD_PNGARENA_H

#include "webevd/WebEVD/Temporaries.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

typedef unsigned char png_byte;

namespace evd
{
  class JSONFormatter;

  static constexpr int MipMapOffset(int dim, int maxdim)
  {
    if(dim >= maxdim) return 0;
    return MipMapOffset(dim*2, maxdim) + (dim*2)*(dim*2)*4;
  }

  class PNGArena
  {
  public:
    friend class PNGView;
    friend void AnalyzeArena(const PNGArena&);

    enum{
      // Square because seems to be necessary for mipmapping. Larger than this
      // doesn't seem to work in the browser.
      kArenaSize = 4096,
      // We have APAs with 480 and 1148 wires. This fits them in 1 and 3 blocks
      // without being too wasteful.
      kBlockSize = 512,
      kTotBytes = MipMapOffset(1, kArenaSize)+4 // leave space for 1x1 as well
    };

    PNGArena(const std::string& name);

    inline png_byte& operator()(int i, int x, int y, int c)
    {
      return (*data[i])[(y*kArenaSize+x)*4+c];
    }

    inline const png_byte& operator()(int i, int x, int y, int c) const
    {
      return (*data[i])[(y*kArenaSize+x)*4+c];
    }

    void WritePNGBytes(FILE* fout, int imgIdx, int dim);

    png_byte* NewBlock();

    //  protected:
    std::string name;
    int elemx, elemy;
    int nviews;

    std::vector<std::unique_ptr<std::array<png_byte, kTotBytes>>> data;

  protected:
    void FillMipMaps(int d);

    std::vector<std::mutex*> fMIPLocks;
    std::vector<bool> fHasMIP;
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
      return blocks[ix][iy][((y-iy*PNGArena::kBlockSize)*PNGArena::kArenaSize+(x-ix*PNGArena::kBlockSize))*4+c];
    }

    inline png_byte operator()(int x, int y, int c) const
    {
      const int ix = x/PNGArena::kBlockSize;
      const int iy = y/PNGArena::kBlockSize;
      if(!blocks[ix][iy]) return 0;
      return blocks[ix][iy][((y-iy*PNGArena::kBlockSize)*PNGArena::kArenaSize+(x-ix*PNGArena::kBlockSize))*4+c];
    }

  protected:
    friend JSONFormatter& operator<<(JSONFormatter&, const PNGView&);

    PNGArena& arena;
    int width, height;
    std::vector<std::vector<png_byte*>> blocks;
  };

  void AnalyzeArena(const PNGArena& bytes);

} // namespace

#endif
