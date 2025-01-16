#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>

int get_ip(const char *device,  char *result)
{
    struct ifaddrs *ifap, *ifa;

    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
	if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET) {
	    printf("Check: %s vs %s\n", ifa->ifa_name, device);

	    if (strcmp(ifa->ifa_name, device) == 0) {
		struct sockaddr_in *sa = (struct sockaddr_in *) ifa->ifa_addr;
		result = inet_ntoa(sa->sin_addr);
		printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, result);
		freeifaddrs(ifap);
		return 0;
	    }
	}
    }
    freeifaddrs(ifap);
    return -1;
}
