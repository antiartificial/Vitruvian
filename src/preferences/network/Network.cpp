/*
 * Copyright 2004-2015 Haiku Inc. All rights reserved.
 * Copyright 2024-2026 Vitruvian OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include <Alert.h>
#include <Application.h>
#include <Catalog.h>
#include <Locale.h>
#include <Window.h>

#include <cstring>

#include <NetworkBackend.h>
#include "MockNetworkBackend.h"
#include "NetworkWindow.h"


static const char* kSignature = "application/x-vnd.Haiku-Network";


class Application : public BApplication {
public:
								Application(INetworkBackend* backend);

public:
	virtual	void				ReadyToRun();

private:
			INetworkBackend*	fBackend;
};


Application::Application(INetworkBackend* backend)
	:
	BApplication(kSignature),
	fBackend(backend)
{
}


void
Application::ReadyToRun()
{
	NetworkWindow* window = new NetworkWindow(fBackend);
	window->Show();
}


static INetworkBackend*
CreateMockBackend()
{
	MockNetworkBackend* mock = new MockNetworkBackend();

	network_interface_info eth;
	eth.name = "eth0";
	eth.hwAddress = "aa:bb:cc:dd:ee:ff";
	eth.type = B_NETWORK_INTERFACE_TYPE_ETHERNET;
	eth.state = B_NETWORK_STATE_CONNECTED;
	eth.ipv4Address = "192.168.1.100";
	eth.ipv4Mask = "255.255.255.0";
	eth.ipv4Gateway = "192.168.1.1";
	eth.enabled = true;
	mock->AddInterface(eth);

	network_interface_info wifi;
	wifi.name = "wlan0";
	wifi.hwAddress = "11:22:33:44:55:66";
	wifi.type = B_NETWORK_INTERFACE_TYPE_WIFI;
	wifi.state = B_NETWORK_STATE_NO_LINK;
	wifi.enabled = true;
	mock->AddInterface(wifi);

	wireless_network_info ap1;
	ap1.name = "HomeNetwork";
	ap1.bssid = "00:11:22:33:44:55";
	ap1.signalStrength = 85;
	ap1.frequency = 2437;
	ap1.authMode = 3;
	ap1.isConnected = false;
	mock->AddWirelessNetwork("wlan0", ap1);

	wireless_network_info ap2;
	ap2.name = "CoffeeShop_Free";
	ap2.bssid = "aa:bb:cc:dd:ee:00";
	ap2.signalStrength = 42;
	ap2.frequency = 5180;
	ap2.authMode = 0;
	ap2.isConnected = false;
	mock->AddWirelessNetwork("wlan0", ap2);

	return mock;
}


// #pragma mark -


int
main(int argc, char** argv)
{
	INetworkBackend* backend = NULL;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--mock") == 0)
			backend = CreateMockBackend();
	}

	Application* app = new Application(backend);
	app->Run();
	delete app;
	return 0;
}
