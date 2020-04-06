include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

# TODO rationalize
set( evd_lib_list larcorealg_Geometry
                  ${ART_FRAMEWORK_CORE}
                  ${ART_FRAMEWORK_PRINCIPAL}
                  ${ART_FRAMEWORK_SERVICES_REGISTRY}
                  ${ART_FRAMEWORK_SERVICES_BASIC}
                  ${ART_ROOT_IO}
                  ${ART_ROOT_IO_ROOTINPUT_SOURCE}
                  ${ART_ROOT_IO_TFILESERVICE_SERVICE}
                  ${ART_ROOT_IO_TFILE_SUPPORT}
                  ${ROOT_BASIC_LIB_LIST}
                  ${ROOT_CORE}
                  ${ROOT_GEOM}
                  ${ROOT_XMLIO}
                  ${ROOT_GDML}
                  canvas
                  cetlib
                  cetlib_except
                  ${Boost_SYSTEM_LIBRARY}
                  larcore_Geometry_Geometry_service
                  lardata_Utilities
                  larevt_Filters
                  lardataobj_RawData
                  lardataobj_RecoBase
                  larsim_MCCheater_BackTrackerService_service
                  larsim_MCCheater_ParticleInventoryService_service
                  lardata_ArtDataHelper
                  nusimdata_SimulationBase
                  art_Persistency_Common
                  art_Persistency_Provenance
                  art_Utilities
                  ${MF_MESSAGELOGGER}
                  ${FHICLCPP}
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
