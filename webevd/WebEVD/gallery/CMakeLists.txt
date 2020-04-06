include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

# TODO why is this list necessary here but not eg in the directory above?
link_directories( $ENV{LARCOREALG_LIB}
                  $ENV{CANVAS_LIB}
                  $ENV{CETLIB_LIB}
                  $ENV{CETLIB_EXCEPT_LIB}
                  $ENV{GALLERY_LIB}
                  $ENV{ART_LIB}
                  $ENV{NUSIMDATA_LIB}
                  $ENV{LARCOREOBJ_LIB}
                  $ENV{LARCOREALG_LIB}
                  $ENV{LARDATAOBJ_LIB}
                  $ENV{LARDATAALG_LIB}
                  $ENV{FHICLCPP_LIB}
                  $ENV{DUNETPC_LIB} # For dune_artSupport
                  $ENV{MRB_BUILDDIR}/webevd/lib/
                  )

link_libraries( pthread
                canvas
                cetlib_except
                cetlib
                gallery
                nusimdata_SimulationBase
                larcoreobj_SummaryData
                lardataobj_RecoBase
                dune_ArtSupport
                art_Framework_Services_Registry
                png
                fhiclcpp
                webevd_WebEVD
                )

add_executable(webevd webevd.cxx)

install_headers()
install_source()
