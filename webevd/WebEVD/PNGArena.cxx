#include "webevd/WebEVD/PNGArena.h"

#include "TString.h"

#include <png.h>
#include <thread>
#include <zlib.h>

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
    // The alpha channel is set to twice the average of the source pixels. The
    // intuition is that a linear feature should remain equally as bright, but
    // would be expected to only hit 2 of the 4 source pixels. The colour
    // channels are averaged, weighted by the alpha values.
    for(unsigned int d = 0; d < bytes.data.size(); ++d){
      for(int y = 0; y < newdim; ++y){
        for(int x = 0; x < newdim; ++x){
          int totc[3] = {0,};
          int tota = 0;
          for(int dy = 0; dy <= 1; ++dy){
            for(int dx = 0; dx <= 1; ++dx){
              const png_byte va = bytes(d, x*2+dx, y*2+dy, 3); // alpha value
              tota += va;

              for(int c = 0; c < 3; ++c){
                const png_byte vc = bytes(d, x*2+dx, y*2+dy, c); // colour value
                totc[c] += vc * va;
              } // end for c
            } // end for dx
          } // end for dy

          for(int c = 0; c < 3; ++c) bytes(d, x, y, c) = tota > 0 ? std::min(totc[c]/tota, 255) : 0;
          bytes(d, x, y, 3) = std::min(tota/2, 255);
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

  void _WriteToPNG(Temporaries* tmp,
                   const std::string prefix,
                   const PNGArena* bytes,
                   int mipmapdim,
                   int d)
  {
    const std::string fname = TString::Format("%s_%d_mip%d.png", prefix.c_str(), d, mipmapdim).Data();

    FILE* fp = tmp->fopen(fname.c_str(), "wb");

    png_struct_def* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    auto info_ptr = png_create_info_struct(png_ptr);

    // I'm trying to optimize the compression speed, without blowing the file
    // size up. It's far from clear these are the best parameters, but they do
    // seem to be an improvement on the defaults.
    png_set_compression_level(png_ptr, 1);
    png_set_compression_strategy(png_ptr, Z_RLE);

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, mipmapdim, mipmapdim,
                 8/*bit_depth*/, PNG_COLOR_TYPE_RGBA/*GRAY*/, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    std::vector<png_byte*> pdatas(mipmapdim);
    for(int i = 0; i < mipmapdim; ++i) pdatas[i] = const_cast<png_byte*>(&(*bytes)(d, 0, i, 0));
    png_set_rows(png_ptr, info_ptr, &pdatas.front());

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    fclose(fp);

    png_destroy_write_struct(&png_ptr, &info_ptr);
  }

  // --------------------------------------------------------------------------
  void WriteToPNG(Temporaries& tmp,
                  const std::string& prefix,
                  const PNGArena& bytes,
                  int mipmapdim)
  {
    if(mipmapdim == -1) mipmapdim = bytes.extent;

    std::vector<std::thread> threads;
    threads.reserve(bytes.data.size());

    for(unsigned int d = 0; d < bytes.data.size(); ++d){
      threads.emplace_back(_WriteToPNG, &tmp, prefix, &bytes, mipmapdim, d);
    }

    for(std::thread& t: threads) t.join();
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
