/*
 * Copyright 2024-2026 Vitruvian OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef _MOCK_NETWORK_BACKEND_H
#define _MOCK_NETWORK_BACKEND_H


#include <NetworkBackend.h>

#include <map>
#include <string>


class MockNetworkBackend : public INetworkBackend {
public:
								MockNetworkBackend();
	virtual						~MockNetworkBackend();

	// INetworkBackend interface
	virtual	status_t			GetInterfaces(
									std::vector<network_interface_info>&
										interfaces);
	virtual	status_t			GetInterfaceInfo(const char* name,
									network_interface_info& info);

	virtual	status_t			EnableInterface(const char* name);
	virtual	status_t			DisableInterface(const char* name);

	virtual	status_t			GetWirelessNetworks(const char* interface,
									std::vector<wireless_network_info>&
										networks);
	virtual	status_t			RequestWirelessScan(const char* interface);

	virtual	status_t			GetConnections(
									std::vector<network_connection_info>&
										connections);
	virtual	status_t			ActivateConnection(const char* id,
									const char* interface);
	virtual	status_t			DeactivateConnection(const char* id);

	virtual	status_t			ConnectWireless(const char* interface,
									const char* ssid,
									const char* password = NULL);
	virtual	status_t			Disconnect(const char* interface);

	virtual	status_t			SetStaticConfig(const char* interface,
									const char* ipAddress,
									const char* netmask,
									const char* gateway);
	virtual	status_t			SetDHCP(const char* interface);

	virtual	network_connection_state
								GetState();

	// Mock control -- configure the fake state
			void				AddInterface(
									const network_interface_info& info);
			void				RemoveInterface(const char* name);
			void				SetInterfaceState(const char* name,
									network_connection_state state);

			void				AddWirelessNetwork(const char* interface,
									const wireless_network_info& network);
			void				ClearWirelessNetworks(const char* interface);

			void				AddConnection(
									const network_connection_info& conn);

			void				SetFailNextCall(status_t error);

private:
			status_t			_CheckFail();

	typedef std::map<std::string, network_interface_info> InterfaceMap;
	typedef std::map<std::string, std::vector<wireless_network_info>>
								WirelessMap;

			InterfaceMap		fInterfaces;
			WirelessMap			fWirelessNetworks;
			std::vector<network_connection_info>
								fConnections;
			status_t			fNextError;
};


#endif	// _MOCK_NETWORK_BACKEND_H
