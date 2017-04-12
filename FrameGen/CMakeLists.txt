link_directories(${ZLIB_LIB}) 


art_make_library(LIBRARY_NAME framegen 
		SOURCE	FrameGen.cpp
		LIBRARIES z
)

art_make_exec  (NAME framegen-test
		SOURCE main.cpp	 
		LIBRARIES framegen z
)

install_headers()
install_source()
install_fhicl()
