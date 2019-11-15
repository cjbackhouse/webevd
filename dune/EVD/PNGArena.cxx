#include "dune/EVD/PNGArena.h"

#include "TString.h"

#include <png.h>

namespace evd
{
  // --------------------------------------------------------------------------
  PNGArena::PNGArena(const std::string& n) : name(n), extent(kArenaSize), nviews(0)
  {
  }

  // --------------------------------------------------------------------------
  png_byte* PNGArena::NewBlock()
  {
    const int nfitx = extent/kBlockSize;
    const int nfity = extent/kBlockSize;

    int ix = nviews%nfitx;
    int iy = nviews/nfitx;

    if(data.empty() || iy >= nfity){
      ix = 0;
      iy = 0;
      nviews = 0;
      data.emplace_back(4*extent*extent);
    }

    ++nviews;

    return &data.back()[(iy*extent + ix)*kBlockSize*4];
  }

  // --------------------------------------------------------------------------
  PNGView::PNGView(PNGArena& a, int w, int h)
    : arena(a), width(w), height(h), blocks(w/PNGArena::kBlockSize+1, std::vector<png_byte*>(h/PNGArena::kBlockSize+1, 0))
  {
  }

  // --------------------------------------------------------------------------
  void MipMap(PNGArena& bytes, int newdim)
  {
    // The algorithm here is only really suitable for the specific way we're
    // encoding hits, not for general images.
    //
    // The alpha channel is set to the max of any of the source pixels. The
    // colour channels are averaged, weighted by the alpha values, and then
    // scaled so that the most intense colour retains its same maximum
    // intensity (this is relevant for green, where we use "dark green", 128).
    for(unsigned int d = 0; d < bytes.data.size(); ++d){
      for(int y = 0; y < newdim; ++y){
        for(int x = 0; x < newdim; ++x){
          double totc[3] = {0,};
          double maxtotc = 0;
          png_byte maxc[3] = {0,};
          png_byte maxmaxc = 0;
          png_byte maxa = 0;
          for(int dy = 0; dy <= 1; ++dy){
            for(int dx = 0; dx <= 1; ++dx){
              const png_byte va = bytes(d, x*2+dx, y*2+dy, 3); // alpha value
              maxa = std::max(maxa, va);

              for(int c = 0; c < 3; ++c){
                const png_byte vc = bytes(d, x*2+dx, y*2+dy, c); // colour value
                totc[c] += vc * va;
                maxc[c] = std::max(maxc[c], vc);
                maxtotc = std::max(maxtotc, totc[c]);
                maxmaxc = std::max(maxmaxc, maxc[c]);
              } // end for c
            } // end for dx
          } // end for dy

          for(int c = 0; c < 3; ++c) bytes(d, x, y, c) = maxtotc ? maxmaxc*totc[c]/maxtotc : 0;
          bytes(d, x, y, 3) = maxa;
        } // end for x
      } // end for y
    } // end for d
  }

  // --------------------------------------------------------------------------
  void AnalyzeArena(const PNGArena& bytes)
  {
    for(int blockSize = 1; blockSize <= bytes.extent; blockSize *= 2){
      long nfilled = 0;
      for(unsigned int d = 0; d < bytes.data.size(); ++d){
        for(int iy = 0; iy < bytes.extent/blockSize; ++iy){
          for(int ix = 0; ix < bytes.extent/blockSize; ++ix){
            bool filled = false;
            for(int y = 0; y < blockSize && !filled; ++y){
              for(int x = 0; x < blockSize; ++x){
                if(bytes(d, ix*blockSize+x, iy*blockSize+y, 3) > 0){
                  filled = true;
                  break;
                }
              }
            } // end for y
            if(filled) ++nfilled;
          }
        }
      }

      std::cout << "With block size = " << blockSize << " " << double(nfilled)/((bytes.extent*bytes.extent)/(blockSize*blockSize)*bytes.data.size()) << " of the blocks are filled" << std::endl;
    }
  }

  // --------------------------------------------------------------------------
  void WriteToPNG(Temporaries& tmp,
                  const std::string& prefix,
                  const PNGArena& bytes,
                  int mipmapdim)
  {
    if(mipmapdim == -1) mipmapdim = bytes.extent;

    for(unsigned int d = 0; d < bytes.data.size(); ++d){
      const std::string fname = TString::Format("%s_%d_mip%d.png", prefix.c_str(), d, mipmapdim).Data();

      FILE* fp = tmp.fopen(fname.c_str(), "wb");

      png_struct_def* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
      auto info_ptr = png_create_info_struct(png_ptr);

      // Doesn't seem to have a huge effect. Setting zero generates huge files
      //  png_set_compression_level(png_ptr, 9);

      // Doesn't affect the file size, may be a small speedup
      png_set_filter(png_ptr, 0, PNG_FILTER_NONE);

      png_init_io(png_ptr, fp);
      png_set_IHDR(png_ptr, info_ptr, mipmapdim, mipmapdim,
                   8/*bit_depth*/, PNG_COLOR_TYPE_RGBA/*GRAY*/, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

      std::vector<png_byte*> pdatas(mipmapdim);
      for(int i = 0; i < mipmapdim; ++i) pdatas[i] = const_cast<png_byte*>(&bytes(d, 0, i, 0));
      png_set_rows(png_ptr, info_ptr, &pdatas.front());

      png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

      fclose(fp);

      png_destroy_write_struct(&png_ptr, &info_ptr);
    }
  }

  // --------------------------------------------------------------------------
  void WriteToPNGWithMipMaps(Temporaries& tmp,
                             const std::string& prefix, PNGArena& bytes)
  {
    //  AnalyzeArena(bytes);

    for(int mipmapdim = bytes.extent; mipmapdim >= 1; mipmapdim /= 2){
      WriteToPNG(tmp, prefix, bytes, mipmapdim);
      MipMap(bytes, mipmapdim/2);
    }
  }

} //namespace
