/*
 * Copyright 2024-2026 Vitruvian OS contributors.
 * Distributed under the terms of the MIT License.
 *
 * Test harness for INetworkBackend / MockNetworkBackend.
 * Follows the existing testharness pattern: standalone executable,
 * returns 0 on success, non-zero on failure.
 */


#include "MockNetworkBackend.h"

#ifdef __VOS__
#	include <Errors.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>


static int sTestsPassed = 0;
static int sTestsFailed = 0;


#define TEST(name) \
	static void name(); \
	static struct _Register_##name { \
		_Register_##name() { _RunTest(#name, name); } \
	} _reg_##name; \
	static void name()

static void
_RunTest(const char* name, void (*fn)())
{
	printf("  %-50s ", name);
	try {
		fn();
		printf("[PASS]\n");
		sTestsPassed++;
	} catch (...) {
		printf("[FAIL]\n");
		sTestsFailed++;
	}
}


static void
_Assert(bool condition, const char* expr, const char* file, int line)
{
	if (!condition) {
		fprintf(stderr, "    ASSERT FAILED: %s (%s:%d)\n", expr, file, line);
		throw "assertion failed";
	}
}

#define ASSERT(expr) _Assert((expr), #expr, __FILE__, __LINE__)
#define ASSERT_EQ(a, b) _Assert((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define ASSERT_NE(a, b) _Assert((a) != (b), #a " != " #b, __FILE__, __LINE__)
#define ASSERT_OK(expr) _Assert((expr) == B_OK, #expr " == B_OK", __FILE__, __LINE__)


// Helper to create a typical ethernet interface
static network_interface_info
MakeEthernet(const char* name, network_connection_state state)
{
	network_interface_info info;
	info.name = name;
	info.hwAddress = "aa:bb:cc:dd:ee:ff";
	info.type = B_NETWORK_INTERFACE_TYPE_ETHERNET;
	info.state = state;
	info.ipv4Address = "";
	info.ipv4Mask = "";
	info.ipv4Gateway = "";
	info.ipv6Address = "";
	info.ipv6PrefixLength = 0;
	info.enabled = true;
	return info;
}


// Helper to create a wifi interface
static network_interface_info
MakeWifi(const char* name, network_connection_state state)
{
	network_interface_info info = MakeEthernet(name, state);
	info.type = B_NETWORK_INTERFACE_TYPE_WIFI;
	info.hwAddress = "11:22:33:44:55:66";
	return info;
}


static wireless_network_info
MakeAP(const char* ssid, uint8_t strength, bool connected)
{
	wireless_network_info ap;
	ap.name = ssid;
	ap.bssid = "00:11:22:33:44:55";
	ap.signalStrength = strength;
	ap.frequency = 2437;
	ap.authMode = 3; // WPA2
	ap.isConnected = connected;
	return ap;
}


// #pragma mark - Tests


TEST(test_empty_backend)
{
	MockNetworkBackend backend;
	std::vector<network_interface_info> ifaces;
	ASSERT_OK(backend.GetInterfaces(ifaces));
	ASSERT_EQ(ifaces.size(), 0u);
	ASSERT_EQ(backend.GetState(), B_NETWORK_STATE_NO_LINK);
}


TEST(test_add_interface)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTED));

	std::vector<network_interface_info> ifaces;
	ASSERT_OK(backend.GetInterfaces(ifaces));
	ASSERT_EQ(ifaces.size(), 1u);
	ASSERT_EQ(ifaces[0].name, "eth0");
	ASSERT_EQ(ifaces[0].type, B_NETWORK_INTERFACE_TYPE_ETHERNET);
}


TEST(test_get_interface_info)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTED));

	network_interface_info info;
	ASSERT_OK(backend.GetInterfaceInfo("eth0", info));
	ASSERT_EQ(info.state, B_NETWORK_STATE_CONNECTED);

	ASSERT_EQ(backend.GetInterfaceInfo("eth1", info), B_NAME_NOT_FOUND);
}


TEST(test_multiple_interfaces)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTED));
	backend.AddInterface(MakeWifi("wlan0", B_NETWORK_STATE_NO_LINK));

	std::vector<network_interface_info> ifaces;
	ASSERT_OK(backend.GetInterfaces(ifaces));
	ASSERT_EQ(ifaces.size(), 2u);
}


TEST(test_remove_interface)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTED));
	backend.RemoveInterface("eth0");

	std::vector<network_interface_info> ifaces;
	ASSERT_OK(backend.GetInterfaces(ifaces));
	ASSERT_EQ(ifaces.size(), 0u);
}


TEST(test_enable_disable)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTED));

	ASSERT_OK(backend.DisableInterface("eth0"));
	network_interface_info info;
	ASSERT_OK(backend.GetInterfaceInfo("eth0", info));
	ASSERT(!info.enabled);
	ASSERT_EQ(info.state, B_NETWORK_STATE_NO_LINK);

	ASSERT_OK(backend.EnableInterface("eth0"));
	ASSERT_OK(backend.GetInterfaceInfo("eth0", info));
	ASSERT(info.enabled);
	ASSERT_EQ(info.state, B_NETWORK_STATE_CONNECTING);

	ASSERT_EQ(backend.EnableInterface("nonexistent"), B_NAME_NOT_FOUND);
}


TEST(test_wireless_networks)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeWifi("wlan0", B_NETWORK_STATE_NO_LINK));
	backend.AddWirelessNetwork("wlan0", MakeAP("HomeWifi", 80, false));
	backend.AddWirelessNetwork("wlan0", MakeAP("CoffeeShop", 45, false));

	std::vector<wireless_network_info> networks;
	ASSERT_OK(backend.GetWirelessNetworks("wlan0", networks));
	ASSERT_EQ(networks.size(), 2u);
	ASSERT_EQ(networks[0].name, "HomeWifi");
	ASSERT_EQ(networks[1].signalStrength, 45);
}


TEST(test_wireless_connect)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeWifi("wlan0", B_NETWORK_STATE_NO_LINK));
	backend.AddWirelessNetwork("wlan0", MakeAP("HomeWifi", 80, false));

	ASSERT_OK(backend.ConnectWireless("wlan0", "HomeWifi", "password123"));

	network_interface_info info;
	ASSERT_OK(backend.GetInterfaceInfo("wlan0", info));
	ASSERT_EQ(info.state, B_NETWORK_STATE_CONNECTED);

	std::vector<wireless_network_info> networks;
	ASSERT_OK(backend.GetWirelessNetworks("wlan0", networks));
	ASSERT(networks[0].isConnected);
}


TEST(test_disconnect)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeWifi("wlan0", B_NETWORK_STATE_CONNECTED));
	backend.AddWirelessNetwork("wlan0", MakeAP("HomeWifi", 80, true));

	ASSERT_OK(backend.Disconnect("wlan0"));

	network_interface_info info;
	ASSERT_OK(backend.GetInterfaceInfo("wlan0", info));
	ASSERT_EQ(info.state, B_NETWORK_STATE_NO_LINK);

	std::vector<wireless_network_info> networks;
	ASSERT_OK(backend.GetWirelessNetworks("wlan0", networks));
	ASSERT(!networks[0].isConnected);
}


TEST(test_static_config)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_NO_LINK));

	ASSERT_OK(backend.SetStaticConfig("eth0",
		"10.0.0.5", "255.255.255.0", "10.0.0.1"));

	network_interface_info info;
	ASSERT_OK(backend.GetInterfaceInfo("eth0", info));
	ASSERT_EQ(info.ipv4Address, "10.0.0.5");
	ASSERT_EQ(info.ipv4Mask, "255.255.255.0");
	ASSERT_EQ(info.ipv4Gateway, "10.0.0.1");
	ASSERT_EQ(info.state, B_NETWORK_STATE_CONNECTED);
}


TEST(test_dhcp)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_NO_LINK));

	ASSERT_OK(backend.SetDHCP("eth0"));

	network_interface_info info;
	ASSERT_OK(backend.GetInterfaceInfo("eth0", info));
	ASSERT(!info.ipv4Address.empty());
	ASSERT_EQ(info.state, B_NETWORK_STATE_CONNECTED);
}


TEST(test_overall_state)
{
	MockNetworkBackend backend;
	ASSERT_EQ(backend.GetState(), B_NETWORK_STATE_NO_LINK);

	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTING));
	ASSERT_EQ(backend.GetState(), B_NETWORK_STATE_CONNECTING);

	backend.SetInterfaceState("eth0", B_NETWORK_STATE_CONNECTED);
	ASSERT_EQ(backend.GetState(), B_NETWORK_STATE_CONNECTED);
}


TEST(test_error_injection)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTED));

	backend.SetFailNextCall(B_TIMED_OUT);
	std::vector<network_interface_info> ifaces;
	ASSERT_EQ(backend.GetInterfaces(ifaces), B_TIMED_OUT);

	// Next call should succeed (error is one-shot)
	ASSERT_OK(backend.GetInterfaces(ifaces));
	ASSERT_EQ(ifaces.size(), 1u);
}


TEST(test_connections)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeEthernet("eth0", B_NETWORK_STATE_CONNECTED));

	network_connection_info conn;
	conn.id = "/org/freedesktop/NetworkManager/Settings/1";
	conn.name = "Wired connection 1";
	conn.interface = "eth0";
	conn.type = B_NETWORK_INTERFACE_TYPE_ETHERNET;
	conn.active = true;
	backend.AddConnection(conn);

	std::vector<network_connection_info> connections;
	ASSERT_OK(backend.GetConnections(connections));
	ASSERT_EQ(connections.size(), 1u);
	ASSERT(connections[0].active);

	ASSERT_OK(backend.DeactivateConnection(conn.id.c_str()));
	ASSERT_OK(backend.GetConnections(connections));
	ASSERT(!connections[0].active);

	ASSERT_OK(backend.ActivateConnection(conn.id.c_str(), "eth0"));
	ASSERT_OK(backend.GetConnections(connections));
	ASSERT(connections[0].active);
}


TEST(test_clear_wireless_networks)
{
	MockNetworkBackend backend;
	backend.AddInterface(MakeWifi("wlan0", B_NETWORK_STATE_NO_LINK));
	backend.AddWirelessNetwork("wlan0", MakeAP("Net1", 50, false));
	backend.AddWirelessNetwork("wlan0", MakeAP("Net2", 70, false));
	backend.ClearWirelessNetworks("wlan0");

	std::vector<wireless_network_info> networks;
	ASSERT_OK(backend.GetWirelessNetworks("wlan0", networks));
	ASSERT_EQ(networks.size(), 0u);
}


// #pragma mark - main


int
main()
{
	printf("\nNetworkBackend test harness\n");
	printf("==========================\n\n");
	printf("Results: %d passed, %d failed\n\n", sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
