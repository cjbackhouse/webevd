set( evd_lib_list pthread
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

cet_make_exec( webevd
               SOURCE webevd.cxx
               LIBRARIES ${evd_lib_list}
               )

cet_make_exec( webevd_browser
               SOURCE webevd_browser.cxx
               LIBRARIES ${evd_lib_list}
               )

install_headers()
install_source()
