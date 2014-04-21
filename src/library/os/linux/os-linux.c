#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <sys/socket.h>
#include <netpacket/packet.h>

#include <paths.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "../os.h"
#include "../../base/public-key.h"

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

static int ifname_position(char *ifnames, char * ifname, int ifnames_max) {
	int i, position;
	position = -1;
	for (i = 0; i < ifnames_max; i++) {
		if (strcmp(ifname, &ifnames[i * NI_MAXHOST]) == 0) {
			position = i;
			break;
		}
	}
	return position;

}

FUNCTION_RETURN getAdapterInfos(AdapterInfo * adapterInfos,
		size_t * adapter_info_size) {

	FUNCTION_RETURN f_return = OK;
	struct ifaddrs *ifaddr, *ifa;
	int family, i, s, n, if_name_position;
	unsigned int if_num, if_max;
	char host[NI_MAXHOST];
	char *ifnames;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		return ERROR;
	}
	/* count the maximum number of interfaces */
	for (ifa = ifaddr, if_max = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}
		if_max++;
	}

	/* allocate space for names */
	ifnames = (char*) malloc(NI_MAXHOST * if_max);
	memset(ifnames, 0, NI_MAXHOST * if_max);
	/* Walk through linked list, maintaining head pointer so we
	 can free list later */
	for (ifa = ifaddr, n = 0, if_num = 0; ifa != NULL;
			ifa = ifa->ifa_next, n++) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}
		if_name_position = ifname_position(ifnames, ifa->ifa_name, if_num);
		//interface name not seen en advance
		if (if_name_position < 0) {
			strncpy(&ifnames[if_num * NI_MAXHOST], ifa->ifa_name, NI_MAXHOST);
			if (adapterInfos != NULL && if_num < *adapter_info_size) {
				strncpy(adapterInfos[if_num].description, ifa->ifa_name,
						NI_MAXHOST);
			}
			if_name_position = if_num;
			if_num++;
			if (adapterInfos == NULL) {
				continue;
			}
		}
		family = ifa->ifa_addr->sa_family;
		/* Display interface name and family (including symbolic
		 form of the latter for the common families) */
#ifdef _DEBUG
		printf("%-8s %s (%d)\n", ifa->ifa_name,
				(family == AF_PACKET) ? "AF_PACKET" :
				(family == AF_INET) ? "AF_INET" :
				(family == AF_INET6) ? "AF_INET6" : "???", family);
#endif
		/* For an AF_INET* interface address, display the address
		 * || family == AF_INET6*/
		if (family == AF_INET) {
			/*
			 s = getnameinfo(ifa->ifa_addr,
			 (family == AF_INET) ?
			 sizeof(struct sockaddr_in) :
			 sizeof(struct sockaddr_in6), host, NI_MAXHOST,
			 NULL, 0, NI_NUMERICHOST);
			 */
#ifdef _DEBUG
			s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
			NI_MAXHOST,
			NULL, 0, NI_NUMERICHOST);
			if (s != 0) {
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
			}
			printf("\t\taddress: <%s>\n", host);
#endif

			if (adapterInfos != NULL && if_name_position < *adapter_info_size) {
				struct sockaddr_in *s1 = (struct sockaddr_in*) ifa->ifa_addr;
				in_addr_t iaddr = s1->sin_addr.s_addr;
				adapterInfos[if_name_position].ipv4_address[0] = (iaddr
						& 0x000000ff);
				adapterInfos[if_name_position].ipv4_address[1] = (iaddr
						& 0x0000ff00) >> 8;
				adapterInfos[if_name_position].ipv4_address[2] = (iaddr
						& 0x00ff0000) >> 16;
				adapterInfos[if_name_position].ipv4_address[3] = (iaddr
						& 0xff000000) >> 24;
			}
		} else if (family == AF_PACKET && ifa->ifa_data != NULL) {
			struct sockaddr_ll *s1 = (struct sockaddr_ll*) ifa->ifa_addr;
			if (adapterInfos != NULL && if_name_position < *adapter_info_size) {
				for (i = 0; i < 6; i++) {
					adapterInfos[if_name_position].mac_address[i] =
							s1->sll_addr[i];
#ifdef _DEBUG
					printf("%02x:", s1->sll_addr[i]);
#endif
				}
#ifdef _DEBUG
					printf("\t %s\n", ifa->ifa_name);
#endif

			}
		}
	}

	*adapter_info_size = if_num;
	if (adapterInfos == NULL) {
		f_return = OK;
	} else if (*adapter_info_size < if_num) {
		f_return = BUFFER_TOO_SMALL;
	}
	freeifaddrs(ifaddr);
	return f_return;
}

FUNCTION_RETURN getDiskInfos(DiskInfo * diskInfos, size_t * disk_info_size) {
	struct stat filename_stat, mount_stat;
	static char discard[1024];
	char device[64], name[64], type[64];
	FILE *mounts = fopen(_PATH_MOUNTED, "r");
	if (mounts == NULL) {
		return ERROR;
	}

	while (fscanf(mounts, "%64s %64s %64s %1024[^\n]", device, name, type,
			discard) != EOF) {
		if (stat(device, &mount_stat) != 0)
			continue;
		if (filename_stat.st_dev == mount_stat.st_rdev) {
			fprintf(stderr, "device: %s; name: %s; type: %s\n", device, name,
					type);
		}
	}

	return ERROR;
}

void os_initialize() {
	static int initialized = 0;
	if (initialized == 0) {
		initialized = 1;
		ERR_load_ERR_strings();
		ERR_load_crypto_strings();
		OpenSSL_add_all_algorithms();
	}
}

VIRTUALIZATION getVirtualization() {
//http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
//
//bool rc = true;
	/*__asm__ (
	 "push   %edx\n"
	 "push   %ecx\n"
	 "push   %ebx\n"
	 "mov    %eax, 'VMXh'\n"
	 "mov    %ebx, 0\n" // any value but not the MAGIC VALUE
	 "mov    %ecx, 10\n"// get VMWare version
	 "mov    %edx, 'VX'\n"// port number
	 "in     %eax, dx\n"// read port on return EAX returns the VERSION
	 "cmp    %ebx, 'VMXh'\n"// is it a reply from VMWare?
	 "setz   [rc] \n"// set return value
	 "pop    %ebx \n"
	 "pop    %ecx \n"
	 "pop    %edx \n"
	 );*/

	return NONE;
}
