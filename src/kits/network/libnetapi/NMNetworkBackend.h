/*
 * Copyright 2024-2026 Vitruvian OS contributors.
 * Distributed under the terms of the MIT License.
 *
 * NetworkManager D-Bus backend for INetworkBackend.
 * Communicates with org.freedesktop.NetworkManager over D-Bus.
 */
#ifndef _NM_NETWORK_BACKEND_H
#define _NM_NETWORK_BACKEND_H


#include <NetworkBackend.h>


class NMNetworkBackend : public INetworkBackend {
public:
								NMNetworkBackend();
	virtual						~NMNetworkBackend();

			status_t			Init();

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

private:
	// D-Bus connection handle (sd-bus or GDBus -- TBD)
			void*				fBusConnection;
};


#endif	// _NM_NETWORK_BACKEND_H
