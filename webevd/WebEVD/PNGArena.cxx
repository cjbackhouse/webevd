#include "webevd/WebEVD/PNGArena.h"

#include "TString.h"

#include <iostream>

#include <png.h>
#include <thread>
#include <zlib.h> // for Z_RLE

namespace evd
{
  // --------------------------------------------------------------------------
  PNGArena::PNGArena(const std::string& n) : name(n), nviews(0)
  {
  }

  // --------------------------------------------------------------------------
  png_byte* PNGArena::NewBlock()
  {
    const int nfitx = kArenaSize/kBlockSize;
    const int nfity = kArenaSize/kBlockSize;

    int ix = nviews%nfitx;
    int iy = nviews/nfitx;

    if(data.empty() || iy >= nfity){
      ix = 0;
      iy = 0;
      nviews = 0;
      data.push_back(std::make_unique<std::array<png_byte, kTotBytes>>());
      data.back()->fill(0);
      fHasMIP.push_back(false);
      fMIPLocks.push_back(new std::mutex);
    }

    ++nviews;

    return &(*data.back())[(iy*kArenaSize + ix)*kBlockSize*4];
  }

  // --------------------------------------------------------------------------
  void PNGArena::FillMipMaps(int d)// const
  {
    png_byte* src = data[d]->data();
    png_byte* dest = data[d]->data() + kArenaSize*kArenaSize*4;

    for(int newdim = kArenaSize/2; newdim >= 1; newdim /= 2){
      const int olddim = newdim*2;

      // The alpha channel is set to twice the average of the source
      // pixels. The intuition is that a linear feature should remain equally
      // as bright, but would be expected to only hit 2 of the 4 source
      // pixels. The colour channels are averaged, weighted by the alpha
      // values.
      for(int y = 0; y < newdim; ++y){
        for(int x = 0; x < newdim; ++x){
          int totc[3] = {0,};
          int tota = 0;
          for(int dy = 0; dy <= 1; ++dy){
            for(int dx = 0; dx <= 1; ++dx){
              const int i = (y*2+dy)*olddim + (x*2+dx);

              const png_byte va = src[i*4+3]; // alpha value
              tota += va;

              for(int c = 0; c < 3; ++c){
                const png_byte vc = src[i*4+c]; // colour value
                totc[c] += vc * va;
              } // end for c
            } // end for dx
          } // end for dy

          const int i = y*newdim+x;
          for(int c = 0; c < 3; ++c) dest[i*4+c] = tota > 0 ? std::min(totc[c]/tota, 255) : 0;
          dest[i*4+3] = std::min(tota/2, 255);
        } // end for x
      } // end for y

      src = dest;
      dest += 4*newdim*newdim;
    } // end for newdim
  }

  // This is essentially what the libpng default version would be anyway. But
  // by writing it ourselves we can just ignore failure (probably the browser
  // hung up), rather than getting entangled in libpng's error handling.
  void webevd_png_write_fn(png_struct_def* png_ptr,
                           png_byte* buffer,
                           long unsigned int nbytes)
  {
    FILE* fout = (FILE*)png_get_io_ptr(png_ptr);
    const size_t ret = fwrite(buffer, 1, nbytes, fout);

    if(ret != nbytes){
      std::cout << "Error writing " << nbytes << " bytes of png -- returned " << ret << std::endl;
      perror(0);
      std::cout << std::endl;
    }
  }

  void webevd_png_flush_fn(png_struct_def* png_ptr)
  {
    // Nothing to do here, but libpng requires we provide this
  }

  // --------------------------------------------------------------------------
  void PNGArena::WritePNGBytes(FILE* fout, int imgIdx, int dim)
  {
    if(imgIdx >= int(data.size())){
      std::cout << "Bad index " << imgIdx << std::endl;
      return; // TODO return 404 instead
    }

    if(dim != kArenaSize){ // might need MIPMaps generated
      std::lock_guard<std::mutex> guard(*fMIPLocks[imgIdx]);
      if(!fHasMIP[imgIdx]){
        FillMipMaps(imgIdx);
        fHasMIP[imgIdx] = true;
      }
    }

    // Figure out offset
    png_byte* src = &(*data[imgIdx])[MipMapOffset(dim, kArenaSize)];

    if(src + 4*dim*dim - 1 > &(*data[imgIdx]).back()){ // would run off end of data
      std::cout << "Bad mipmap size " << dim << std::endl;
      return; // TODO return 404 instead
    }

    png_struct_def* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    auto info_ptr = png_create_info_struct(png_ptr);

    // I'm trying to optimize the compression speed, without blowing the file
    // size up. It's far from clear these are the best parameters, but they do
    // seem to be an improvement on the defaults.
    png_set_compression_level(png_ptr, 1);
    png_set_compression_strategy(png_ptr, Z_RLE);

    png_set_IHDR(png_ptr, info_ptr, dim, dim,
                 8/*bit_depth*/, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    std::vector<png_byte*> pdatas(dim);
    for(int i = 0; i < dim; ++i) pdatas[i] = src + i*dim*4;

    png_set_rows(png_ptr, info_ptr, pdatas.data());

    // This works fine until libpng throws a fatal error when the browser hangs
    // up
    // png_init_io(png_ptr, fout);

    // So set our own write function that does the same thing but ignores
    // errors
    png_set_write_fn(png_ptr, fout, webevd_png_write_fn, webevd_png_flush_fn);

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
  }

  // --------------------------------------------------------------------------
  PNGView::PNGView(PNGArena& a, int w, int h)
    : arena(a), width(w), height(h), blocks(w/PNGArena::kBlockSize+1, std::vector<png_byte*>(h/PNGArena::kBlockSize+1, 0))
  {
  }

  // --------------------------------------------------------------------------
  void AnalyzeArena(const PNGArena& bytes)
  {
    for(int blockSize = 1; blockSize <= PNGArena::kArenaSize; blockSize *= 2){
      long nfilled = 0;
      for(unsigned int d = 0; d < bytes.data.size(); ++d){
        for(int iy = 0; iy < PNGArena::kArenaSize/blockSize; ++iy){
          for(int ix = 0; ix < PNGArena::kArenaSize/blockSize; ++ix){
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

      std::cout << "With block size = " << blockSize << " " << double(nfilled)/((PNGArena::kArenaSize*PNGArena::kArenaSize)/(blockSize*blockSize)*bytes.data.size()) << " of the blocks are filled" << std::endl;
    }
  }

} //namespace
