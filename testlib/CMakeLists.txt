include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

set(testlib_SRC 
	utils.cpp 
	dummytorrentcreator.cpp
)

kde4_add_library(testlib STATIC ${testlib_SRC})
