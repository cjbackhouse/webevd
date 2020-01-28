include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

# TODO rationalize
set( evd_lib_list larcorealg_Geometry
                        ${ART_FRAMEWORK_CORE}
			${ART_FRAMEWORK_PRINCIPAL}
			${ART_FRAMEWORK_SERVICES_REGISTRY}
                        ${ART_FRAMEWORK_SERVICES_BASIC}
			${ART_ROOT_IO_TFILE_SUPPORT} ${ROOT_CORE}
                        ${ART_ROOT_IO}
                        ${ART_ROOT_IO_ROOTINPUT_SOURCE}
                        canvas
			cetlib
                        cetlib_except
                        ${Boost_SYSTEM_LIBRARY}
			${ROOT_BASIC_LIB_LIST}

			larcore_Geometry_Geometry_service
	                lardata_Utilities
			larevt_Filters
			lardataobj_RawData
			lardataobj_RecoBase
                        larsim_MCCheater_BackTrackerService_service
	                larsim_MCCheater_ParticleInventoryService_service
			lardata_ArtDataHelper
                        ${ART_FRAMEWORK_CORE}
			${ART_FRAMEWORK_PRINCIPAL}
			${ART_FRAMEWORK_SERVICES_REGISTRY}
			${ART_ROOT_IO_TFILE_SUPPORT} ${ROOT_CORE}
			${ART_ROOT_IO_TFILESERVICE_SERVICE}
			art_Persistency_Common canvas
			art_Persistency_Provenance canvas
			art_Utilities canvas
			${MF_MESSAGELOGGER}

	                ${FHICLCPP}
			cetlib cetlib_except
                        ${ROOT_GEOM}
                        ${ROOT_XMLIO}
                        ${ROOT_GDML}
			${ROOT_BASIC_LIB_LIST}
                        png
                        pthread
                        nusimdata_SimulationBase
                        gallery
                        )

art_make( BASENAME_ONLY
          LIB_LIBRARIES ${evd_lib_list}
          MODULE_LIBRARIES ${evd_lib_list} dune_EVD InputSeeker_service
          SERVICE_LIBRARIES ${evd_lib_list} dune_EVD
        )

install_headers()
install_fhicl()
install_source()

