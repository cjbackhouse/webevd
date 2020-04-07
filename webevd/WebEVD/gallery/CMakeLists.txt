include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

# TODO why is this list necessary here but not eg in the directory above?
link_directories(
                  $ENV{CANVAS_LIB}
                  $ENV{CETLIB_LIB}
                  $ENV{CETLIB_EXCEPT_LIB}
                  $ENV{GALLERY_LIB}
                  $ENV{NUSIMDATA_LIB}
                  $ENV{LARDATAOBJ_LIB}
                  $ENV{MRB_BUILDDIR}/webevd/lib/
                  )

link_libraries( pthread
                canvas
                cetlib
                cetlib_except
                gallery
                nusimdata_SimulationBase
                lardataobj_RecoBase
                webevd_ArtSupport
                png
                webevd_WebEVD
                )

add_executable(webevd webevd.cxx)

install_headers()
install_source()
