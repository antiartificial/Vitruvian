/*
 * Copyright 2024-2026 Vitruvian OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "MockNetworkBackend.h"

#ifdef __VOS__
#	include <Errors.h>
#endif

#include <algorithm>


MockNetworkBackend::MockNetworkBackend()
	:
	fNextError(B_OK)
{
}


MockNetworkBackend::~MockNetworkBackend()
{
}


status_t
MockNetworkBackend::_CheckFail()
{
	status_t err = fNextError;
	fNextError = B_OK;
	return err;
}


// #pragma mark - Interface enumeration


status_t
MockNetworkBackend::GetInterfaces(
	std::vector<network_interface_info>& interfaces)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	interfaces.clear();
	for (InterfaceMap::const_iterator it = fInterfaces.begin();
			it != fInterfaces.end(); ++it) {
		interfaces.push_back(it->second);
	}
	return B_OK;
}


status_t
MockNetworkBackend::GetInterfaceInfo(const char* name,
	network_interface_info& info)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	InterfaceMap::const_iterator it = fInterfaces.find(name);
	if (it == fInterfaces.end())
		return B_NAME_NOT_FOUND;

	info = it->second;
	return B_OK;
}


// #pragma mark - Interface control


status_t
MockNetworkBackend::EnableInterface(const char* name)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	InterfaceMap::iterator it = fInterfaces.find(name);
	if (it == fInterfaces.end())
		return B_NAME_NOT_FOUND;

	it->second.enabled = true;
	if (it->second.state == B_NETWORK_STATE_NO_LINK)
		it->second.state = B_NETWORK_STATE_CONNECTING;
	return B_OK;
}


status_t
MockNetworkBackend::DisableInterface(const char* name)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	InterfaceMap::iterator it = fInterfaces.find(name);
	if (it == fInterfaces.end())
		return B_NAME_NOT_FOUND;

	it->second.enabled = false;
	it->second.state = B_NETWORK_STATE_NO_LINK;
	return B_OK;
}


// #pragma mark - Wireless


status_t
MockNetworkBackend::GetWirelessNetworks(const char* interface,
	std::vector<wireless_network_info>& networks)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	WirelessMap::const_iterator it = fWirelessNetworks.find(interface);
	if (it == fWirelessNetworks.end()) {
		networks.clear();
		return B_OK;
	}

	networks = it->second;
	return B_OK;
}


status_t
MockNetworkBackend::RequestWirelessScan(const char* interface)
{
	return _CheckFail();
}


// #pragma mark - Connection management


status_t
MockNetworkBackend::GetConnections(
	std::vector<network_connection_info>& connections)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	connections = fConnections;
	return B_OK;
}


status_t
MockNetworkBackend::ActivateConnection(const char* id, const char* interface)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	for (size_t i = 0; i < fConnections.size(); i++) {
		if (fConnections[i].id == id) {
			fConnections[i].active = true;
			fConnections[i].interface = interface ? interface : "";
			return B_OK;
		}
	}
	return B_NAME_NOT_FOUND;
}


status_t
MockNetworkBackend::DeactivateConnection(const char* id)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	for (size_t i = 0; i < fConnections.size(); i++) {
		if (fConnections[i].id == id) {
			fConnections[i].active = false;
			return B_OK;
		}
	}
	return B_NAME_NOT_FOUND;
}


// #pragma mark - Wireless connect/disconnect


status_t
MockNetworkBackend::ConnectWireless(const char* interface, const char* ssid,
	const char* password)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	InterfaceMap::iterator ifIt = fInterfaces.find(interface);
	if (ifIt == fInterfaces.end())
		return B_NAME_NOT_FOUND;

	// Mark matching wireless network as connected
	WirelessMap::iterator wIt = fWirelessNetworks.find(interface);
	if (wIt != fWirelessNetworks.end()) {
		for (size_t i = 0; i < wIt->second.size(); i++) {
			wIt->second[i].isConnected = (wIt->second[i].name == ssid);
		}
	}

	ifIt->second.state = B_NETWORK_STATE_CONNECTED;
	return B_OK;
}


status_t
MockNetworkBackend::Disconnect(const char* interface)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	InterfaceMap::iterator it = fInterfaces.find(interface);
	if (it == fInterfaces.end())
		return B_NAME_NOT_FOUND;

	it->second.state = B_NETWORK_STATE_NO_LINK;

	// Clear wireless connected flags
	WirelessMap::iterator wIt = fWirelessNetworks.find(interface);
	if (wIt != fWirelessNetworks.end()) {
		for (size_t i = 0; i < wIt->second.size(); i++)
			wIt->second[i].isConnected = false;
	}

	return B_OK;
}


// #pragma mark - Address configuration


status_t
MockNetworkBackend::SetStaticConfig(const char* interface,
	const char* ipAddress, const char* netmask, const char* gateway)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	InterfaceMap::iterator it = fInterfaces.find(interface);
	if (it == fInterfaces.end())
		return B_NAME_NOT_FOUND;

	it->second.ipv4Address = ipAddress ? ipAddress : "";
	it->second.ipv4Mask = netmask ? netmask : "";
	it->second.ipv4Gateway = gateway ? gateway : "";
	it->second.state = B_NETWORK_STATE_CONNECTED;
	return B_OK;
}


status_t
MockNetworkBackend::SetDHCP(const char* interface)
{
	status_t err = _CheckFail();
	if (err != B_OK)
		return err;

	InterfaceMap::iterator it = fInterfaces.find(interface);
	if (it == fInterfaces.end())
		return B_NAME_NOT_FOUND;

	it->second.ipv4Address = "192.168.1.100";
	it->second.ipv4Mask = "255.255.255.0";
	it->second.ipv4Gateway = "192.168.1.1";
	it->second.state = B_NETWORK_STATE_CONNECTED;
	return B_OK;
}


// #pragma mark - Overall state


network_connection_state
MockNetworkBackend::GetState()
{
	for (InterfaceMap::const_iterator it = fInterfaces.begin();
			it != fInterfaces.end(); ++it) {
		if (it->second.state == B_NETWORK_STATE_CONNECTED)
			return B_NETWORK_STATE_CONNECTED;
		if (it->second.state == B_NETWORK_STATE_CONNECTING)
			return B_NETWORK_STATE_CONNECTING;
	}
	return B_NETWORK_STATE_NO_LINK;
}


// #pragma mark - Mock control


void
MockNetworkBackend::AddInterface(const network_interface_info& info)
{
	fInterfaces[info.name] = info;
}


void
MockNetworkBackend::RemoveInterface(const char* name)
{
	fInterfaces.erase(name);
	fWirelessNetworks.erase(name);
}


void
MockNetworkBackend::SetInterfaceState(const char* name,
	network_connection_state state)
{
	InterfaceMap::iterator it = fInterfaces.find(name);
	if (it != fInterfaces.end())
		it->second.state = state;
}


void
MockNetworkBackend::AddWirelessNetwork(const char* interface,
	const wireless_network_info& network)
{
	fWirelessNetworks[interface].push_back(network);
}


void
MockNetworkBackend::ClearWirelessNetworks(const char* interface)
{
	fWirelessNetworks.erase(interface);
}


void
MockNetworkBackend::AddConnection(const network_connection_info& conn)
{
	fConnections.push_back(conn);
}


void
MockNetworkBackend::SetFailNextCall(status_t error)
{
	fNextError = error;
}
