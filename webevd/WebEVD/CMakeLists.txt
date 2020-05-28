include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

# TODO rationalize
set( evd_lib_list ${ART_FRAMEWORK_CORE}
                  ${ART_FRAMEWORK_PRINCIPAL}
                  ${ART_ROOT_IO}
                  ${ART_ROOT_IO_ROOTINPUT_SOURCE}
                  ${ROOT_BASIC_LIB_LIST}
                  canvas
                  cetlib
                  cetlib_except
                  larcorealg_Geometry
                  lardataobj_RawData
                  lardataobj_RecoBase
                  lardataalg_DetectorInfo
                  nusimdata_SimulationBase
                  art_Persistency_Common
                  art_Persistency_Provenance
                  ${MF_MESSAGELOGGER}
                  png
                  pthread
                  gallery
                  )

art_make( BASENAME_ONLY
          LIB_LIBRARIES ${evd_lib_list}
          MODULE_LIBRARIES ${evd_lib_list} webevd_WebEVD InputSeeker_service
          SERVICE_LIBRARIES ${evd_lib_list} webevd_WebEVD
        )

install_headers()
install_fhicl()
install_source()

set( ${product}_webevd_dir "${product}/${version}/webevd/" )
file(GLOB webevd_files web/* )
install( FILES ${webevd_files} DESTINATION ${${product}_webevd_dir} )

ADD_SUBDIRECTORY(gallery)
