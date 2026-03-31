/*
 * Copyright 2024-2026 Vitruvian OS contributors.
 * Distributed under the terms of the MIT License.
 *
 * NetworkManager D-Bus backend stub.
 *
 * TODO: Implement using sd-bus or GDBus to communicate with
 * org.freedesktop.NetworkManager on the system bus.
 *
 * Key NM D-Bus interfaces:
 *   org.freedesktop.NetworkManager
 *     - GetDevices() -> ao
 *     - ActivateConnection(oo) -> o
 *     - DeactivateConnection(o)
 *     - State -> u (NM_STATE_*)
 *
 *   org.freedesktop.NetworkManager.Device
 *     - properties: Interface, DeviceType, State, Ip4Config, ...
 *
 *   org.freedesktop.NetworkManager.Device.Wireless
 *     - GetAccessPoints() -> ao
 *     - RequestScan(a{sv})
 *
 *   org.freedesktop.NetworkManager.AccessPoint
 *     - properties: Ssid, HwAddress, Strength, Frequency, WpaFlags, ...
 *
 *   org.freedesktop.NetworkManager.Settings
 *     - ListConnections() -> ao
 *
 *   org.freedesktop.NetworkManager.Settings.Connection
 *     - GetSettings() -> a{sa{sv}}
 */


#include "NMNetworkBackend.h"

#include <Errors.h>


NMNetworkBackend::NMNetworkBackend()
	:
	fBusConnection(NULL)
{
}


NMNetworkBackend::~NMNetworkBackend()
{
	// TODO: close D-Bus connection
}


status_t
NMNetworkBackend::Init()
{
	// TODO: open system D-Bus connection
	// TODO: verify org.freedesktop.NetworkManager is reachable
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::GetInterfaces(
	std::vector<network_interface_info>& interfaces)
{
	// TODO: call GetDevices(), iterate device objects,
	// read properties for each device
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::GetInterfaceInfo(const char* name,
	network_interface_info& info)
{
	// TODO: find device by Interface property, read all props
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::EnableInterface(const char* name)
{
	// TODO: set Managed property or use nmcli equivalent
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::DisableInterface(const char* name)
{
	// TODO: disconnect + set unmanaged, or disable via NM
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::GetWirelessNetworks(const char* interface,
	std::vector<wireless_network_info>& networks)
{
	// TODO: find wireless device, call GetAccessPoints(),
	// read AP properties (Ssid, Strength, Frequency, WpaFlags)
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::RequestWirelessScan(const char* interface)
{
	// TODO: call RequestScan({}) on the wireless device
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::GetConnections(
	std::vector<network_connection_info>& connections)
{
	// TODO: call Settings.ListConnections(), read each
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::ActivateConnection(const char* id, const char* interface)
{
	// TODO: call NM.ActivateConnection(connPath, devPath, "/")
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::DeactivateConnection(const char* id)
{
	// TODO: call NM.DeactivateConnection(activeConnPath)
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::ConnectWireless(const char* interface, const char* ssid,
	const char* password)
{
	// TODO: build connection settings dict, call AddAndActivateConnection
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::Disconnect(const char* interface)
{
	// TODO: find active connection for device, deactivate
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::SetStaticConfig(const char* interface,
	const char* ipAddress, const char* netmask, const char* gateway)
{
	// TODO: modify connection settings, reactivate
	return B_NOT_SUPPORTED;
}


status_t
NMNetworkBackend::SetDHCP(const char* interface)
{
	// TODO: set method=auto in ipv4 settings, reactivate
	return B_NOT_SUPPORTED;
}


network_connection_state
NMNetworkBackend::GetState()
{
	// TODO: read NM State property, map NM_STATE_* to our enum
	// NM_STATE_CONNECTED_GLOBAL (70) -> B_NETWORK_STATE_CONNECTED
	// NM_STATE_CONNECTING (40)       -> B_NETWORK_STATE_CONNECTING
	// NM_STATE_DISCONNECTED (20)     -> B_NETWORK_STATE_NO_LINK
	return B_NETWORK_STATE_UNKNOWN;
}
