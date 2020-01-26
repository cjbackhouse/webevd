 # `root-config --cflags` \

CFLAGS := -I${GALLERY_INC} \
	  -I${CANVAS_INC} \
	  -I${BOOST_INC} \
	  -I${HEP_CONCURRENCY_INC} \
	  -I${CETLIB_EXCEPT_INC} \
	  -I${CETLIB_INC} \
	  -I${FHICLCPP_INC} \
	  -I${ART_INC} \
	  -I${CANVAS_ROOT_IO_INC} \
	  -I${ROOT_INC} \
	  -I${LARDATAOBJ_INC} \
	  -I${LARCOREALG_INC} \
	  -I${LARCOREOBJ_INC} \
	  -I${LARCORE_INC} \
	  -I${CLHEP_INC} \
	  -I${LARDATAALG_INC} \
	  -I${LARDATA_INC} \
	  -I${NUSIMDATA_INC} \
	  -I${DUNETPC_DIR} \
	  -DBOOST_SYSTEM_NO_DEPRECATED

LIBDIRS := `root-config --libs` \
	   -L${ROOTSYS}/lib \
	   -L${CANVAS_LIB} \
	   -L${CETLIB_EXCEPT_LIB} \
	   -L${CETLIB_LIB} \
	   -L${GALLERY_LIB} \
	   -L${ART_LIB} \
	   -L${NUSIMDATA_LIB} \
	   -L${LARCOREOBJ_LIB} \
	   -L${LARCOREALG_LIB} \
	   -L${LARDATAOBJ_LIB} \
	   -L${LARDATAALG_LIB} \
	   -L${FHICLCPP_LIB} \
	   -L${MRB_BUILDDIR}/dunetpc/lib/

LIBS := -lpthread -lcanvas -lcetlib_except -lcetlib -lgallery -lnusimdata_SimulationBase -llarcoreobj_SummaryData -llardataobj_RecoBase -ldune_ArtSupport -lart_Framework_Services_Registry -lpng -lfhiclcpp -ldune_EVD # -lCore -lRIO -lNet -lHist -lGraf -lGraf3d -lGpad -lTree -lRint -lPostscript -lMatrix -lPhysics -lMathCore -lThread -lMultiProc

all:
	g++ -O3 -g -pthread -std=c++1z webevd.cxx -owebevd ${CFLAGS} ${LIBDIRS} ${LIBS}
#	strip webevd
