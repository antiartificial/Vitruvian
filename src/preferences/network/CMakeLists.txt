Application(Network
	SOURCES
	InterfaceAddressView.cpp
	InterfaceListItem.cpp
	InterfaceView.cpp
	IPAddressControl.cpp
	Network.cpp
	NetworkProfile.cpp
	NetworkSettingsAddOn.cpp
	NetworkWindow.cpp
	ServiceListItem.cpp
	ServiceView.cpp
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi/MockNetworkBackend.cpp
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi/NMNetworkBackend.cpp
	LIBS bnetapi shared localestub
	INCLUDES
	${CMAKE_SOURCE_DIR}/src/kits/network/libnetapi
	RDEF Network.rdef InterfaceIcons.rdef
)
