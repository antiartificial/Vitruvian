Application(test_network_backend
	SOURCES
	test_backend.cpp
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi/MockNetworkBackend.cpp
	LIBS bnetapi shared
	INCLUDES
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi
)
