Application(NetworkStatus
	SOURCES
	NetworkStatus.cpp
	NetworkStatusView.cpp
	NetworkStatusWindow.cpp
	RadioView.cpp
	WirelessNetworkMenuItem.cpp
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi/MockNetworkBackend.cpp
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi/NMNetworkBackend.cpp
	LIBS shared localestub bnetapi
	INCLUDES
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi
	RDEF NetworkStatus.rdef NetworkStatusIcons.rdef
)
#UsePrivateHeaders(NetworkStatus interface net)
