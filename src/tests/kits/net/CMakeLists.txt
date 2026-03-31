UsePrivateHeaders(GLOBAL net)

add_subdirectory(service)
add_subdirectory(network_backend)

Test(NetAddressTest SOURCES NetAddressTest.cpp)
Test (NetEndpointTest SOURCES NetEndpointTest.cpp)
