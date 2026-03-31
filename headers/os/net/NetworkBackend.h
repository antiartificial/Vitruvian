/*
 * Copyright 2024-2026 Vitruvian OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NETWORK_BACKEND_H
#define _NETWORK_BACKEND_H


#include <cstdint>
#include <string>
#include <vector>

// When building within Vitruvian, status_t comes from SupportDefs.h.
// For standalone/host-side test builds, provide a minimal definition.
#ifdef __VOS__
#	include <SupportDefs.h>
#else
#	ifndef _NETWORK_BACKEND_COMPAT_TYPES
#	define _NETWORK_BACKEND_COMPAT_TYPES
		typedef int32_t status_t;
#		ifndef B_OK
#			define B_OK 0
#		endif
#		ifndef B_ERROR
#			define B_ERROR (-1)
#		endif
#		ifndef B_NAME_NOT_FOUND
#			include <limits.h>
#			define B_GENERAL_ERROR_BASE INT_MIN
#			define B_NAME_NOT_FOUND (B_GENERAL_ERROR_BASE + 9)
#		endif
#		ifndef B_NOT_SUPPORTED
#			include <errno.h>
#			define B_NOT_SUPPORTED (-EOPNOTSUPP)
#		endif
#		ifndef B_TIMED_OUT
#			include <errno.h>
#			define B_TIMED_OUT (-ETIMEDOUT)
#		endif
#	endif
#endif


// Interface type, mirrors BNetworkInterfaceType where applicable
enum network_interface_type {
	B_NETWORK_INTERFACE_TYPE_ETHERNET	= 0,
	B_NETWORK_INTERFACE_TYPE_WIFI		= 1,
	B_NETWORK_INTERFACE_TYPE_LOOPBACK	= 2,
	B_NETWORK_INTERFACE_TYPE_OTHER		= 3
};

// Connection state reported by the backend
enum network_connection_state {
	B_NETWORK_STATE_UNKNOWN				= 0,
	B_NETWORK_STATE_NO_LINK				= 1,
	B_NETWORK_STATE_LINK_NO_CONFIG		= 2,
	B_NETWORK_STATE_CONNECTING			= 3,
	B_NETWORK_STATE_CONNECTED			= 4,
	B_NETWORK_STATE_DISCONNECTING		= 5,
	B_NETWORK_STATE_FAILED				= 6
};


struct network_interface_info {
	std::string					name;
	std::string					hwAddress;
	network_interface_type		type;
	network_connection_state	state;
	std::string					ipv4Address;
	std::string					ipv4Mask;
	std::string					ipv4Gateway;
	std::string					ipv6Address;
	int32_t						ipv6PrefixLength;
	bool						enabled;
};


struct wireless_network_info {
	std::string					name;		// SSID
	std::string					bssid;
	uint8_t						signalStrength;	// 0-100
	uint32_t					frequency;		// MHz
	uint32_t					authMode;		// B_NETWORK_AUTHENTICATION_*
	bool						isConnected;
};


struct network_connection_info {
	std::string					id;			// NM connection path or UUID
	std::string					name;		// human-readable name
	std::string					interface;	// bound interface name, if any
	network_interface_type		type;
	bool						active;
};


class INetworkBackend {
public:
	virtual						~INetworkBackend() {}

	// Interface enumeration
	virtual	status_t			GetInterfaces(
									std::vector<network_interface_info>&
										interfaces) = 0;
	virtual	status_t			GetInterfaceInfo(const char* name,
									network_interface_info& info) = 0;

	// Interface control
	virtual	status_t			EnableInterface(const char* name) = 0;
	virtual	status_t			DisableInterface(const char* name) = 0;

	// Wireless
	virtual	status_t			GetWirelessNetworks(const char* interface,
									std::vector<wireless_network_info>&
										networks) = 0;
	virtual	status_t			RequestWirelessScan(
									const char* interface) = 0;

	// Connection management
	virtual	status_t			GetConnections(
									std::vector<network_connection_info>&
										connections) = 0;
	virtual	status_t			ActivateConnection(const char* id,
									const char* interface) = 0;
	virtual	status_t			DeactivateConnection(
									const char* id) = 0;

	// Wireless connect with credentials
	virtual	status_t			ConnectWireless(const char* interface,
									const char* ssid,
									const char* password = NULL) = 0;
	virtual	status_t			Disconnect(const char* interface) = 0;

	// Address configuration
	virtual	status_t			SetStaticConfig(const char* interface,
									const char* ipAddress,
									const char* netmask,
									const char* gateway) = 0;
	virtual	status_t			SetDHCP(const char* interface) = 0;

	// Overall state
	virtual	network_connection_state
								GetState() = 0;
};


#endif	// _NETWORK_BACKEND_H
