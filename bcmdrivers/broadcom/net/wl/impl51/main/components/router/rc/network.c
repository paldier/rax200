/*
 * Network services
 *
 * Copyright (C) 2018, Broadcom. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: network.c 766522 2018-08-06 05:05:21Z $
 */

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <signal.h>

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

#include <linux/sockios.h>
#include <linux/types.h>
#include <linux/ethtool.h>
#ifdef BCA_HNDROUTER
#include <linux/if_bridge.h>
#endif /* BCA_HNDROUTER */

#include <bcmnvram.h>
#include <netconf.h>
#include <shutils.h>
#include <wlutils.h>
#include <nvparse.h>
#include <rc.h>
#include <bcmutils.h>
#include <etutils.h>
#include <bcmparams.h>
#include <security_ipc.h>
#include <wlif_utils.h>
#include <dpsta_linux.h>

#ifdef BCA_HNDROUTER
#include "ambitCfg.h"
#endif

bool emf_enabled = FALSE;

static int
add_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];

	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {
		dprintf("add %s\n", word);

		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;
		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		dprintf("add %s\n", ifname);

		route_add(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}

	return 0;
}

static int
del_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];

	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {
		dprintf("add %s\n", word);

		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;
		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		dprintf("add %s\n", ifname);

		route_del(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}

	return 0;
}

static int
add_lan_routes(char *lan_ifname)
{
	return add_routes("lan_", "route", lan_ifname);
}

static int
del_lan_routes(char *lan_ifname)
{
	return del_routes("lan_", "route", lan_ifname);
}

/* Set initial QoS mode for all et interfaces that are up. */

static void
set_et_qos_mode(void)
{
	int i, s, qos;
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return;

	qos = (strcmp(nvram_safe_get("wl_wme"), "off") != 0);

	for (i = 1; i <= DEV_NUMIFS; i ++) {
		ifr.ifr_ifindex = i;
		if (ioctl(s, SIOCGIFNAME, &ifr))
			continue;
		if (ioctl(s, SIOCGIFHWADDR, &ifr))
			continue;
		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
			continue;
		if (ioctl(s, SIOCGIFFLAGS, &ifr))
			continue;
		if (!(ifr.ifr_flags & IFF_UP))
			continue;
		/* Set QoS for et & bcm57xx devices */
		memset(&info, 0, sizeof(info));
		info.cmd = ETHTOOL_GDRVINFO;
		ifr.ifr_data = (caddr_t)&info;
		if (ioctl(s, SIOCETHTOOL, &ifr) < 0)
			continue;
		if ((strncmp(info.driver, "et", 2) != 0) &&
		    (strncmp(info.driver, "bcm57", 5) != 0))
			continue;
		ifr.ifr_data = (caddr_t)&qos;
		(void) ioctl(s, SIOCSETCQOS, &ifr);
	}

	close(s);
}

/*
 * Carry out a socket request including openning and closing the socket
 * Return -1 if failed to open socket (and perror); otherwise return
 * result of ioctl
 */
static int
soc_req(const char *name, int action, struct ifreq *ifr)
{
	int s;
	int rv = 0;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}
	if (strlen(name) >= sizeof(ifr->ifr_name)) {
		fprintf(stderr, "soc_req: ifr name is too long\n");
		close(s);
		return -1;
	}
	strncpy(ifr->ifr_name, name, sizeof(ifr->ifr_name));
	rv = ioctl(s, action, ifr);
	close(s);

	return rv;
}

static int
wl_send_dif_event(const char *ifname, uint32 event)
{
	static int s = -1;
	int len, n;
	struct sockaddr_in to;
	char data[IFNAMSIZ + sizeof(uint32)];

	/* create a socket to receive dynamic i/f events */
	if (s < 0) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) {
			perror("socket");
			return -1;
		}
	}

	/* Init the message contents to send to eapd. Specify the interface
	 * and the event that occured on the interface.
	 */
	strncpy(data, ifname, IFNAMSIZ);
	*(uint32 *)(data + IFNAMSIZ) = event;
	len = IFNAMSIZ + sizeof(uint32);

	/* send to eapd */
	to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
	to.sin_family = AF_INET;
	to.sin_port = htons(EAPD_WKSP_DIF_UDP_PORT);

	n = sendto(s, data, len, 0, (struct sockaddr *)&to,
	           sizeof(struct sockaddr_in));

	if (n != len) {
		perror("udp send failed\n");
		return -1;
	}

	dprintf("hotplug_net(): sent event %d\n", event);

	return n;
}
/* Check NVRam to see if "name" is explicitly enabled */
static inline int
wl_vif_enabled(const char *name, char *tmp)
{
	return (atoi(nvram_safe_get(strcat_r(name, "_bss_enabled", tmp))));
}

/* Set the HW address for interface "name" if present in NVRam */
static void
wl_vif_hwaddr_set(const char *name)
{
	int rc;
	char *ea;
	char hwaddr[20];
	struct ifreq ifr;
	int retry = 0;
	unsigned char comp_mac_address[ETHER_ADDR_LEN];
	int was_up;

	snprintf(hwaddr, sizeof(hwaddr), "%s_hwaddr", name);
	ea = nvram_get(hwaddr);
	if (ea == NULL) {
		fprintf(stderr, "NET: No hw addr found for %s\n", name);
		return;
	}


	fprintf(stderr, "NET: Setting %s hw addr to %s\n", name, ea);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	ether_atoe(ea, (unsigned char *)ifr.ifr_hwaddr.sa_data);
	ether_atoe(ea, comp_mac_address);

	wl_ioctl(name, WLC_GET_UP, &was_up, sizeof(was_up));
	if (was_up)
		wl_ioctl(name, WLC_DOWN, NULL, 0);

	if ((rc = soc_req(name, SIOCSIFHWADDR, &ifr)) < 0) {
		fprintf(stderr, "NET: Error setting hw for %s; returned %d\n", name, rc);
	}
	memset(&ifr, 0, sizeof(ifr));
	while (retry < 100) { /* maximum 100 millisecond waiting */
		usleep(1000); /* 1 ms sleep */
		if ((rc = soc_req(name, SIOCGIFHWADDR, &ifr)) < 0) {
			fprintf(stderr, "NET: Error Getting hw for %s; returned %d\n", name, rc);
		}
		if (memcmp(comp_mac_address, (unsigned char *)ifr.ifr_hwaddr.sa_data,
			ETHER_ADDR_LEN) == 0) {
			break;
		}
		retry++;
	}
	if (retry >= 100) {
		fprintf(stderr, "Unable to check if mac was set properly for  %s\n", name);
	}

	if (was_up)
		wl_ioctl(name, WLC_UP, NULL, 0);
}

#ifdef __CONFIG_EMF__
void
emf_mfdb_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *mgrp, *ifname;

	/* Add/Delete MFDB entries corresponding to new interface */
	foreach(word, nvram_safe_get("emf_entry"), next) {
		ifname = word;
		mgrp = strsep(&ifname, ":");

		if ((mgrp == 0) || (ifname == 0))
			continue;

		/* Add/Delete MFDB entry using the group addr and interface */
		if (strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"),
			     "mfdb", lan_ifname, mgrp, ifname);
		}
	}

	return;
}

void
emf_uffp_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *ifname;

	/* Add/Delete UFFP entries corresponding to new interface */
	foreach(word, nvram_safe_get("emf_uffp_entry"), next) {
		ifname = word;

		/* Add/Delete UFFP entry for the interface */
		if (strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"),
			     "uffp", lan_ifname, ifname);
		}
	}

	return;
}

void
emf_rtport_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *ifname;

	/* Add/Delete RTPORT entries corresponding to new interface */
	foreach(word, nvram_safe_get("emf_rtport_entry"), next) {
		ifname = word;

		/* Add/Delete RTPORT entry for the interface */
		if (strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"),
			     "rtport", lan_ifname, ifname);
		}
	}

	return;
}

void
start_emf(char *lan_ifname)
{
	char word[256], *next;
	char *mgrp, *ifname;

#if defined(BCA_HNDROUTER) && defined(MCPD_PROXY)
	/* Disable EMF.
	 * Since Runner is involved in Ethernet side when MCPD is enabled
	 */
	if (nvram_match("emf_enable", "1")) {
		nvram_set("emf_enable", "0");
		nvram_commit();
	}

	return;
#endif /* BCA_HNDROUTER && MCPD_PROXY */

	if (!nvram_match("emf_enable", "1"))
		return;

	/* Start EMF */
	eval("emf", "start", lan_ifname);

	/* Add the static MFDB entries */
	foreach(word, nvram_safe_get("emf_entry"), next) {
		ifname = word;
		mgrp = strsep(&ifname, ":");

		if ((mgrp == 0) || (ifname == 0))
			continue;

		/* Add MFDB entry using the group addr and interface */
		eval("emf", "add", "mfdb", lan_ifname, mgrp, ifname);
	}

	/* Add the UFFP entries */
	foreach(word, nvram_safe_get("emf_uffp_entry"), next) {
		ifname = word;

		/* Add UFFP entry for the interface */
		eval("emf", "add", "uffp", lan_ifname, ifname);
	}

	/* Add the RTPORT entries */
	foreach(word, nvram_safe_get("emf_rtport_entry"), next) {
		ifname = word;

		/* Add RTPORT entry for the interface */
		eval("emf", "add", "rtport", lan_ifname, ifname);
	}

	return;
}

void
load_emf(void)
{
	/* Load the EMF & IGMP Snooper modules */
	eval("insmod", "emf");
	eval("insmod", "igs");

	emf_enabled = TRUE;

	return;
}

void
unload_emf(void)
{
	if (!emf_enabled)
		return;

	/* Unload the EMF & IGMP Snooper modules */
	eval("rmmod", "igs");
	eval("rmmod", "emf");

	emf_enabled = FALSE;

	return;
}
#endif /* __CONFIG_EMF__ */

static int
dpsta_ioctl(char *name, void *buf, int len)
{
	struct ifreq ifr;
	int ret = 0;
	int s;

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return errno;
	}

	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
	ifr.ifr_data = (caddr_t)buf;
	if ((ret = ioctl(s, SIOCDEVPRIVATE, &ifr)) < 0)
		perror(ifr.ifr_name);

	/* cleanup */
	close(s);
	return ret;
}

static bool
l2bridge_no_ipaddr(const char *br_ifname)
{
	char l2bridge[NVRAM_MAX_PARAM_LEN];

	snprintf(l2bridge, sizeof(l2bridge), "%s_l2bridge_mode", br_ifname);

	/* For now, brX_l2bridge_mode only has 1 mode of On/Off for bridge IP address
	 * but it could be expanded to have other modes/flags in the future if needed
	 */
	return (nvram_match(l2bridge, "1") ? TRUE : FALSE);
}

static void
setup_lan(char *name, char *lan_ifname, char *hwaddr, int *dpsta, int ifidx)
{
	int s = 0;
	char tmp[100];
	char buf[255], *ptr;
	struct ifreq ifr;
#ifdef __CONFIG_DHDAP__
	int is_dhd;
#endif /* __CONFIG_DHDAP__ */

	/* Bring up interface.Ignore any bogus/unknown
	 * Interfaces on the NVRAM list
	 */

	if (ifconfig(name, IFUP | IFF_ALLMULTI, NULL, NULL)) {
		perror("ifconfig");
		return;
	}
	else {
		/* Set the logical bridge address to that of the first interface */
		if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
			perror("socket");
			return;
		}
		strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name)-1);
		ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';
		if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0) {
			if (memcmp(hwaddr, "\0\0\0\0\0\0", ETHER_ADDR_LEN) == 0) {
				memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
			}
		}
		close(s);

		/* if not a wl i/f then simply add it to the bridge */
		snprintf(tmp, sizeof(tmp), "wlconf %s up", name);
		if (system(tmp)) {
			/* Foxconn modify start, Max Ding, 01/16/2017 */
			/* Fix issue: restart_all_processes will cause IGMP can't work on eth3/eth4(port 3/4)
			 * Root cause: we didn't do "brctl delif" in stop_lan(), so "brctl addif" here will fail when restart_all_processes
			 *             But the interface is included in br0 now, so treat it as normal case.
			 */
			if (eval("brctl", "addif", lan_ifname, name)) {
				perror("brctl");
				//return;
			}
			//else {
				snprintf(tmp, sizeof(tmp), "br%x_ifnames", ifidx);
				ptr = nvram_get(tmp);
				if (ptr)
					snprintf(buf, sizeof(buf), "%s %s", ptr, name);
				else
					snprintf(buf, sizeof(buf), "%s", name);
				nvram_set(tmp, buf);
			//}
			/* Foxconn modify end, Max Ding, 01/16/2017 */
#ifdef __CONFIG_EMF__
			if (nvram_match("emf_enable", "1"))
				eval("emf", "add", "iface", lan_ifname, name);
#endif /* __CONFIG_EMF__ */
		}
		else {
			char mode[] = "wlXXXXXXXXXX_mode";
			int unit = -1;
			int subunit = -1;
			char urembss[] = "wlXXXXXXXXXX_ure_mbss";
			bool ure_mbss = FALSE;

			/* get the instance number of the wl i/f */
			wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit));

			if (!strcmp(nvram_safe_get("ure_disable"), "0")) {
				snprintf(urembss, sizeof(urembss), "wl%d_ure_mbss", unit);
				if (!strcmp(nvram_safe_get(urembss), "1")) {
					ure_mbss = TRUE;
				}
			}

			if (!ure_mbss) {
				subunit = 0;
				snprintf(mode, sizeof(mode), "wl%d_mode", unit);
			}
			else {
				if (strncmp(name, "eth", 3) == 0) {
					subunit = 0;
					snprintf(mode, sizeof(mode), "wl%d_mode", unit);
				}
				else {
					snprintf(mode, sizeof(mode), "%s_mode", name);
				}
			}

			/* WET specific configurations */
			if (nvram_match(mode, "wet")) {
				/* Receive all multicast frames in WET mode */
				ifconfig(name, IFUP | IFF_ALLMULTI, NULL, NULL);

				/* Enable host DHCP relay */
				if (nvram_match("lan_dhcp", "1"))
				{
					wet_host_t wh;
					if (ure_mbss && subunit) {
						if (get_ifname_unit(name, NULL, &subunit) != 0)
							goto bypasswet;
					}

					if (subunit < 0)
						goto bypasswet;

					memset(&wh, 0, sizeof(wet_host_t));
					wh.bssidx = subunit;
					memcpy(&wh.buf, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
#ifdef __CONFIG_DHDAP__
					is_dhd = !dhd_probe(name);
					if (is_dhd) {
						dhd_iovar_set(name, "wet_host_mac", &wh,
							sizeof(wet_host_t));
					}
					else
#endif /* __CONFIG_DHDAP__ */
					{
						wl_iovar_set(name, "wet_host_mac", &wh,
							sizeof(wet_host_t));
					}
				}
			}
bypasswet:
			/* Dont attach the main wl i/f in wds */
			if ((strncmp(name, "wl", 2) != 0) && (nvram_match(mode, "wds"))) {
				/* Save this interface name in unbridged_ifnames
				* This behaviour is consistent with BEARS release
				*/
				ptr = nvram_get("unbridged_ifnames");
				if (ptr)
					snprintf(buf, sizeof(buf), "%s %s", ptr, name);
				else
					snprintf(buf, sizeof(buf), "%s", name);
				nvram_set("unbridged_ifnames", buf);
				return;
			}

			/* Dont add main wl i/f when proxy sta is
			* enabled in both bands. Instead add the
			* dpsta interface.
			*/
			if (strstr(nvram_safe_get("dpsta_ifnames"), name)) {
				strcpy(name, !(*dpsta) ? "dpsta" : "");
				(*dpsta)++;

				/* Assign hw address */
				if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
					strncpy(ifr.ifr_name, "dpsta", IFNAMSIZ);
					if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0 &&
						memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0",
						ETHER_ADDR_LEN) == 0) {
							ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
							memcpy(ifr.ifr_hwaddr.sa_data, hwaddr,
								ETHER_ADDR_LEN);
							if (ioctl(s, SIOCSIFHWADDR, &ifr)) {
								close(s);
								return;
							}
					}
					close(s);
				}
			}

			/* In 3GMAC mode, skip wl interfaces that avail of hw switching.
			 *
			 * Do not add these wl interfaces to the LAN bridge as they avail of
			 * HW switching. Misses in the HW switch's ARL will be forwarded via vlan1
			 * to br0 (i.e. via the network GMAC#2).
			 */
			if (et_capable(NULL, "gmac3") &&
			    find_in_list(nvram_get("fwd_wlandevs"), name))
				goto gmac3_no_swbr;

			eval("brctl", "addif", lan_ifname, name);
gmac3_no_swbr:

#ifdef __CONFIG_EMF__
			if (nvram_match("emf_enable", "1"))
				eval("emf", "add", "iface", lan_ifname, name);
#endif /* __CONFIG_EMF__ */

			snprintf(tmp, sizeof(tmp), "br%x_ifnames", ifidx);
			ptr = nvram_get(tmp);
			if (ptr)
				snprintf(buf, sizeof(buf), "%s %s", ptr, name);
			else {
				strncpy(buf, name, sizeof(buf));
				buf[sizeof(buf)-1] = '\0';
			}

			nvram_set(tmp, buf);
		}
	}
}
void
start_lan(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char br_ifname[80];
	char name[80], *next;
	char tmp[100];
	int i, s, dpsta = 0;
	struct ifreq ifr;
	char buf[255], *ptr;
	char lan_stp[10];
	char *lan_ifnames;
	char lan_dhcp[10];
	char lan_ipaddr[15];
	char lan_netmask[15];
	char lan_hwaddr[15];
	char hwaddr[ETHER_ADDR_LEN];
	dpsta_enable_info_t info = { 0 };

	/* The NVRAM variable lan_ifnames contains all the available interfaces.
	 * This is used to build the unbridged interface list. Once the unbridged list
	 * is built lan_interfaces is rebuilt with only the interfaces in the bridge
	 *
	 * In 3GMAC mode, primary wl interfaces that avail of HW switching are
	 * excluded from lan_ifnames, and appear as LAN-like ports on the integrated
	 * switch, appearing to the Linux bridge as vlan1.
	 */

	dprintf("%s\n", lan_ifname);

	/* Foxconn, added by MJ., for DLAN AUTO IP, 2010.05.18 */
#ifdef DLNA
#ifdef DLNA_DEBUG
	char auto_ip[8];
    strcpy(auto_ip, acosNvramConfig_get("dlna_auto_ip"));
	cprintf("dlna_auto_ip: %s. \n", auto_ip);
#endif
	if(nvram_match("dlna_auto_ip", "1"))
    {
	    if(nvram_match("auto_ip_backup", "0"))
	    {/* dlna_auto_ip changed from 0 to 1. */
			nvram_set("auto_ip_backup", "1");
			/* Set default Auto IP values. */
			nvram_set("tmp_lan_ipaddr", nvram_get("lan_ipaddr"));
			nvram_set("lan_ipaddr", "169.254.146.254");

			nvram_set("tmp_lan_netmask", nvram_get("lan_netmask"));
			nvram_set("lan_netmask", "255.255.0.0");

			nvram_set("tmp_lan_proto", nvram_get("lan_proto"));
            nvram_set("lan_proto", "static");

			nvram_set("tmp_rip_enable", nvram_get("rip_enable"));
            nvram_set("rip_enable", "0");

			nvram_commit();
	    }
    }else{/* dlna_auto_ip = 0 */
		if(nvram_match("auto_ip_backup", "1"))
		{/* dlan_auto_ip changed from 1 to 0. */
			/* If user had changed the value, don't use tmp values.*/
			if(!nvram_match("tmp_lan_netmask", "null")&&
				nvram_match(nvram_get("lan_netmask"), "255.255.0.0")){
				nvram_set("lan_netmask", nvram_get("tmp_lan_netmask"));
				nvram_set("tmp_lan_netmask", "null");
			}
			if(!nvram_match("tmp_lan_ipaddr", "null")&& 
				nvram_match(nvram_get("lan_ipaddr"), "169.254.146.254")){
				nvram_set("lan_ipaddr", nvram_get("tmp_lan_ipaddr"));
				nvram_set("tmp_lan_ipaddr", "null");
			}
			if(!nvram_match("tmp_lan_proto", "null")&&
				nvram_match(nvram_get("lan_proto"), "static")){
				nvram_set("lan_proto", nvram_get("tmp_lan_proto"));
				nvram_set("tmp_lan_proto", "null");
			}
			if(!nvram_match("tmp_rip_enable", "null")&&
				nvram_match(nvram_get("rip_enable"), "0")){
				nvram_set("rip_enable", nvram_get("tmp_rip_enable"));
				nvram_set("tmp_rip_enable", "null");
			}
			nvram_set("auto_ip_backup", "0");
			nvram_commit();
		}
	}
#ifdef DLNA_DEBUG
	cprintf("-> netmask: %s\n", nvram_get("lan_netmask"));
	cprintf("-> lan ip: %s\n", nvram_get("lan_ipaddr"));
	cprintf("-> dhcp server: %s\n", nvram_get("lan_proto"));
	cprintf("-> rip: %s\n", nvram_get("rip_enable"));	
#endif


	//nvram_commit();
#endif
	/* Foxconn, ended by MJ., for DLAN AUTO IP, 2010.05.18 */


	/* Create links */
	symlink("/sbin/rc", "/tmp/ldhclnt");


/*Foxconn modified start, edward zhang, 2013/07/03*/
#ifdef VLAN_SUPPORT
	nvram_unset("unbridged_ifnames");
    /*unset some bridge nvram*/
    char br_ifname_tag[16] = "";
    char br_ifnames_tag[64] = "";
    
    for(i=0; i<7; i++)
    {
        sprintf(br_ifname_tag,"br%d_ifname",i);
        sprintf(br_ifnames_tag,"br%d_ifnames",i);
        nvram_unset(br_ifname_tag);
        nvram_unset(br_ifnames_tag);
    }
#else
	nvram_unset("br0_ifname");
	nvram_unset("br1_ifname");
	nvram_unset("unbridged_ifnames");
	nvram_unset("br0_ifnames");
	nvram_unset("br1_ifnames");
#endif
/*Foxconn modified end, edward zhang, 2013/07/03*/
#if defined(__CONFIG_EXTACS__) || defined(__CONFIG_WL_ACI__)
	nvram_unset("acs_ifnames");
#endif /* defined(_CONFIG_EXTACS__) || defined(__CONFIG_WL_ACI__ */
	/* If we're a travel router... then we need to make sure we get
	 * the primary wireless interface up before trying to attach slave
	 * interface(s) to the bridge
	 */
	if (nvram_match("ure_disable", "0") && nvram_match("router_disable", "0"))
	{
		snprintf(tmp, sizeof(tmp), "wlconf %s up", nvram_get("wan0_ifname"));
		system(tmp);
	}

	/* In 3GMAC mode, bring up the GMAC forwarding interfaces.
	 * Then bringup the primary wl interfaces that avail of hw switching.
	 */
	if (et_capable(NULL, "gmac3")) {
		/* Bring up GMAC#0 and GMAC#1 forwarder(s) */
		foreach(name, nvram_safe_get("fwddevs"), next) {
			ifconfig(name, 0, NULL, NULL);
			ifconfig(name, IFUP | IFF_ALLMULTI | IFF_PROMISC, NULL, NULL);
		}
	}

	/* Bring up bridged interfaces */
	for (i = 0; i < MAX_NO_BRIDGE; i++) {
		if (!i) {
			lan_ifname = nvram_safe_get("lan_ifname");
			snprintf(lan_stp, sizeof(lan_stp), "lan_stp");
			snprintf(lan_dhcp, sizeof(lan_dhcp), "lan_dhcp");
			snprintf(lan_ipaddr, sizeof(lan_ipaddr), "lan_ipaddr");
			snprintf(lan_hwaddr, sizeof(lan_hwaddr), "lan_hwaddr");
			snprintf(lan_netmask, sizeof(lan_netmask), "lan_netmask");
			lan_ifnames = nvram_safe_get("lan_ifnames");
		}
		else {
			snprintf(tmp, sizeof(tmp), "lan%x_ifname", i);
			lan_ifname = nvram_safe_get(tmp);
			snprintf(lan_stp, sizeof(lan_stp), "lan%x_stp", i);
			snprintf(lan_dhcp, sizeof(lan_dhcp), "lan%x_dhcp", i);
			snprintf(lan_ipaddr, sizeof(lan_ipaddr), "lan%x_ipaddr", i);
			snprintf(lan_hwaddr, sizeof(lan_hwaddr), "lan%x_hwaddr", i);
			snprintf(lan_netmask, sizeof(lan_netmask), "lan%x_netmask", i);
			snprintf(tmp, sizeof(tmp), "lan%x_ifnames", i);
			lan_ifnames = nvram_safe_get(tmp);
		}
		if (strncmp(lan_ifname, "br", 2) == 0) {
			/* Set the bridge ifname in brX_ifname */
			snprintf(br_ifname, sizeof(br_ifname), "br%d_ifname", i);
			nvram_set(br_ifname, lan_ifname);

			eval("brctl", "addbr", lan_ifname);
			
			/* Bob added start to avoid sending router solicitation packets, 09/03/2009 */
#ifdef INCLUDE_IPV6
			sprintf(buf, "echo 0 > /proc/sys/net/ipv6/conf/%s/router_solicitations", lan_ifname);
			system(buf);
#endif
			/* Bob added end to avoid sending router solicitation packets, 09/03/2009 */ 
			
			eval("brctl", "setfd", lan_ifname, "0");
			/* Foxconn modified start pling 12/05/2007, enable STP only in repeater mode */
			//if (nvram_match(lan_stp, "0")) 
			if (nvram_invmatch("wla_repeater", "1"))
			/* Foxconn modified end pling 12/05/2007 */

				eval("brctl", "stp", lan_ifname, "off");
			else
				eval("brctl", "stp", lan_ifname, "on");
			if (!i && strnlen(nvram_safe_get("dpsta_ifnames"), 32)) {
				eval("brctl", "setbridgeprio", lan_ifname, "65535");
			}
#ifdef __CONFIG_EMF__
			if (nvram_match("emf_enable", "1")) 
			{
				if( !strcmp(lan_ifname, "br0") )
				{
					/* Add br0 to emf/igs only if IGMP proxy is enabled*/
					if (nvram_match("igmp_proxying_enable", "1"))
					{
				eval("emf", "add", "bridge", lan_ifname);
				eval("igs", "add", "bridge", lan_ifname);
			}
				}
				else
				{
				eval("emf", "add", "bridge", lan_ifname);
				eval("igs", "add", "bridge", lan_ifname);
			}
			}
#endif /* __CONFIG_EMF__ */
			memset(hwaddr, 0, sizeof(hwaddr));

			foreach(name, lan_ifnames, next) {
				if (strncmp(name, "wl", 2) == 0) {
					if (!(strcmp(nvram_safe_get("wl_mode"), "psr"))) {
						wl_vif_hwaddr_set(name);
						setup_lan(name, lan_ifname, hwaddr, &dpsta, i);
						continue;
					} else if (!wl_vif_enabled(name, tmp)) {
						continue; /* Ignore dsiabled WL VIF */
					}
					wl_vif_hwaddr_set(name);
				}

                /*Set lan mac */
                char macstr[20],  mac[6];	

                acosNvramConfig_read("et0macaddr", macstr, sizeof (macstr));
                ether_atoe(macstr, mac);
        		if(strncmp(name,LAN1_IF_NAME_NUM,4)==0 || strncmp(name,LAN2_IF_NAME_NUM,4)==0 || \
        		    strncmp(name,LAN3_IF_NAME_NUM,4)==0 || strncmp(name,LAN4_IF_NAME_NUM,4)==0 
#if defined(AX6000) || defined(AX11000)
                     || strncmp(name,LAN5_IF_NAME_NUM,4)==0
#endif
        		    ) /*Do it for ethx*/ {
        		    //cprintf("name: %s ================================================\n", name);
        	        set_eth_mac(name, mac);
        	    }
    
				setup_lan(name, lan_ifname, hwaddr, &dpsta, i);
				//setup_lan(name, lan_ifname, hwaddr, &dpsta, i);
			} /* foreach().... */
			/* setup br mac here */
			if (memcmp(hwaddr, "\0\0\0\0\0\0", ETHER_ADDR_LEN) &&
			    (s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
				strncpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
				ifr.ifr_name[IFNAMSIZ-1] = '\0';
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				memcpy(ifr.ifr_hwaddr.sa_data, hwaddr, ETHER_ADDR_LEN);
				ioctl(s, SIOCSIFHWADDR, &ifr);
				close(s);
			}
		} /* if (strncmp(lan_ifname.... */
		/* specific non-bridged lan i/f */
		else if (strcmp(lan_ifname, "")) {
			/* Bring up interface */
			ifconfig(lan_ifname, IFUP, NULL, NULL);
			/* config wireless i/f */
			snprintf(tmp, sizeof(tmp), "wlconf %s up", lan_ifname);
			system(tmp);
		}
		else
			continue ; /* lanX_ifname is empty string , so donot do anything */

		/* Get current LAN hardware address */
        snprintf(tmp, sizeof(tmp), "br%x_ifnames", i);
        if ((strncmp(lan_ifname, "br", 2) != 0) || nvram_get(tmp)) {
			if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
				char eabuf[32];
				strncpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
				ifr.ifr_name[IFNAMSIZ-1] = '\0';
				if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0)
					nvram_set(lan_hwaddr,
					ether_etoa((unsigned char *)ifr.ifr_hwaddr.sa_data, eabuf));
				close(s);
			}
        } else
            nvram_unset(lan_hwaddr);

        /* Foxconn added start pling 03/18/2011 */
        /* Remove bridge DNS hijack module first.
         * If we are in AP mode, this module will be
         * inserted later again.
         */
#ifdef AP_MODE
        system("/sbin/rmmod br_dns_hijack 2>/dev/null");
#endif
        /* Foxconn added end pling 03/18/2011 */

		if (l2bridge_no_ipaddr(lan_ifname)) {
			ifconfig(lan_ifname, IFUP, NULL, NULL);
		/* Launch DHCP client - AP only */
		} else if (nvram_match("router_disable", "1") && nvram_match(lan_dhcp, "1")) {
			char *dhcp_argv[] = {
				"udhcpc",
				"-i", lan_ifname,
				"-p", (sprintf(tmp, "/var/run/udhcpc-%s.pid", lan_ifname), tmp),
				"-s", "/tmp/ldhclnt",
				NULL
			};
			int pid;

			/* Start dhcp daemon */
			_eval(dhcp_argv, ">/dev/console", 0, &pid);
		}
/* Foxconn add start, Jenny Zhao, 03/07/2011  @Spec 2.0:AP Mode*/
        /* Auto IP mode. Now according to Router Spec 2.0,
         * this is not related to UPnP/DLNA anymore, 
         * but related to "AP mode".
         */
#ifdef AP_MODE
        else if (nvram_match("enable_ap_mode", "1")) {

            /* Foxconn added start pling 03/18/2011 */
            /* In AP mode, we need to hijack some domain names,
             * e.g. www.routerlogin.net
             *      readyshare.routerlogin.net.
             *      etc
             * we use 'br_dns_hijack.ko for this purpose.
             */
            char command[128];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 13))
            system("/bin/mknod -m 755 /dev/acos_nat_cli c 100 0");
#ifdef SAMBA_ENABLE
            sprintf(command, "/sbin/insmod %s/br_dns_hijack.ko readyshare_dev=%s", \
                KERNEL_MODULE_PATH, nvram_safe_get("smb_host_name"));

#else
            sprintf(command, "/sbin/insmod %s/br_dns_hijack.ko readyshare_dev=\"\"", \
                KERNEL_MODULE_PATH);
#endif            
#else            
#ifdef SAMBA_ENABLE
            sprintf(command, "/sbin/insmod "
                    "/lib/modules/2.6.22/kernel/lib/br_dns_hijack.ko "
                    "readyshare_dev=%s",
                    nvram_safe_get("smb_host_name"));
#else
            sprintf(command, "/sbin/insmod "
                    "/lib/modules/2.6.22/kernel/lib/br_dns_hijack.ko "
                    "readyshare_dev=\"\"");
#endif
#endif
            printf("command = '%s'\n", command);
            system(command);

            /* Insert acos_nat for logging purpose */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 13))
            system("/bin/mknod -m 755 /dev/acos_nat_cli c 100 0");
            sprintf(command, "/sbin/insmod %s/acos_nat.ko", KERNEL_MODULE_PATH);
            system(command);
#elif (defined LINUX26)
            system("/bin/mknod -m 755 /dev/acos_nat_cli c 100 0");
            system("/sbin/insmod /lib/modules/2.6.22/kernel/lib/acos_nat.ko");
#else
            system("/sbin/insmod /lib/modules/2.4.20/kernel/net/ipv4/acos_nat/acos_nat.o");
#endif
            /* Foxconn added end pling 03/18/2011 */

            /* Foxconn added start, Wins, 03/12/2011, @SamKnows */
//#ifdef ISP_SK
            char cmd[64];
            /* Bring up WAN interface. */
            sprintf(cmd, "ifconfig %s up", nvram_get("wan_ifname"));
            system(cmd);
            /* Bridge WAN interface into br0. */
            sprintf(cmd, "brctl addif %s %s", nvram_get("lan_ifname"), nvram_get("wan_ifname"));
            system(cmd);
            /* Turn spanning tree for br0. */
#ifdef BCA_HNDROUTER
            eval("brctl", "stp", nvram_safe_get("lan_ifname"), "on");
#else /* !BCA_HNDROUTER */
            sprintf(cmd, "brctl stp %s on", nvram_get("lan_ifname"));
            system(cmd);
#endif /* !BCA_HNDROUTER */
//#endif /* ISP_SK */
            /* Foxconn added end, Wins, 03/12/2011, @SamKnows */
			/*Foxconn add start by Hank 01/11/2013*/
			/*Fix can not see IPTV in AP mode*/
			eval("emf", "add", "iface", lan_ifname, "vlan2");
			/*Foxconn add end by Hank 01/11/2013*/
            
            /* Removed to start_services function in ap/acos/services.c */
            /* We should start autoipd in start_services(this function called 
               after acos_init), because 'ntpclient' be called in autoipd and 
               'set_system_time' be called in acos_init,If acos_init execute 
               after autoipd, ntp time will be recoverd by set_system_time 
            */
#if 0
            if (nvram_match("ap_dyn_ip", "1")) {
                /* Foxconn added start pling 03/16/2011 */
                /* Clear the NVRAM, so that heartbeat can show
                 * the Internet LED correctly.
                 */
                nvram_set("lan_ipaddr", "0.0.0.0");
                /* Foxconn added end pling 03/16/2011 */
                eval("autoipd");
            }
            else
#endif
                if (!nvram_match("ap_dyn_ip", "1")) {
                char command[128];
                FILE *fp;
                char tmp[100]; 
                char word[100], *next;
                char line[100];            
                /* Use user-defined DNS servers if necessary */
                char dns[256];
                
                //use static settings from GUI
                ifconfig(lan_ifname, IFUP, nvram_get("lan_ipaddr"),
                            nvram_get("lan_netmask"));  /* Foxconn modified pling 03/24/2011 */
                sprintf (command, "route add default gw %s", nvram_get("apmode_gateway"));
                system (command);            
                    
                //Add dns
                strcpy(dns, nvram_get("apmode_dns1"));
                /* Open resolv.conf to read */
                if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
                    perror("/tmp/resolv.conf");
                    return errno;
                }
                foreach(word, dns, next) 
                {
       
                    fprintf(fp, "nameserver %s\n", word);
                }
                fclose(fp);
        }
    }
#endif
/* Foxconn add end, Jenny Zhao, 03/07/2011 */
        /* Foxconn added start pling 03/01/2012 */
        /* In station mode, we insert br_dns_hijack
         * module to make GUI management, USB access
         * possible.
         */
#ifdef STA_MODE
        else if (nvram_match("enable_sta_mode", "1")) {

            char command[128];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 13))
            system("/bin/mknod -m 755 /dev/acos_nat_cli c 100 0");
#ifdef SAMBA_ENABLE
            sprintf(command, "/sbin/insmod %s/br_dns_hijack.ko readyshare_dev=%s", \
                KERNEL_MODULE_PATH, nvram_safe_get("smb_host_name"));

#else
            sprintf(command, "/sbin/insmod %s/br_dns_hijack.ko readyshare_dev=\"\"", \
                KERNEL_MODULE_PATH);
#endif            
#else    
#ifdef SAMBA_ENABLE
            sprintf(command, "/sbin/insmod "
                    "/lib/modules/2.6.22/kernel/lib/br_dns_hijack.ko "
                    "readyshare_dev=%s",
                    nvram_safe_get("smb_host_name"));
#else
            sprintf(command, "/sbin/insmod "
                    "/lib/modules/2.6.22/kernel/lib/br_dns_hijack.ko "
                    "readyshare_dev=\"\"");
#endif
#endif
            system(command);

            /* Foxconn added start pling 03/06/2012 */
            /* Fix station mode static IP not work issue */
            if (!nvram_match("ap_dyn_ip", "1")) {
                char command[128];
                FILE *fp;
                char tmp[100]; 
                char word[100], *next;
                char line[100];            
                char dns[256];
                
                /* use static settings from GUI */
                ifconfig(lan_ifname, IFUP, nvram_get("lan_ipaddr"),
                            nvram_get("lan_netmask"));
                sprintf(command, "route add default gw %s", 
                            nvram_get("apmode_gateway"));
                system(command);            
                    
                /* Add dns */
                strcpy(dns, nvram_get("apmode_dns1"));
                /* Open resolv.conf to read */
                if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
                    perror("/tmp/resolv.conf");
                    return errno;
                }
                foreach(word, dns, next) {
                    fprintf(fp, "nameserver %s\n", word);
                }
                fclose(fp);
            }
            /* Foxconn added end pling 03/06/2012 */
        }
#endif /* STA_MODE */
        /* Foxconn added end pling 03/01/2012 */
        		
		/* Handle static IP address - AP or Router */
		else {
			/* Bring up and configure LAN interface */
			ifconfig(lan_ifname, IFUP,
				nvram_safe_get(lan_ipaddr), nvram_safe_get(lan_netmask));
			/* We are done configuration */
			lan_up(lan_ifname);
		}

#ifdef __CONFIG_EMF__
		/* Start the EMF for this LAN */
		start_emf(lan_ifname);
#endif /* __CONFIG_EMF__ */
	} /* For loop */

	/* Configure dpsta module */
	if (dpsta) {
		int32 i = 0;

		/* Enable and set the policy to in-band and cross-band
		 * forwarding policy.
		 */
		info.enable = 1;
		info.policy = atoi(nvram_safe_get("dpsta_policy"));
		info.lan_uif = atoi(nvram_safe_get("dpsta_lan_uif"));
		foreach(name, nvram_safe_get("dpsta_ifnames"), next) {
			strcpy((char *)info.upstream_if[i], name);
			i++;
		}
		dpsta_ioctl("dpsta", &info, sizeof(dpsta_enable_info_t));

		/* Bring up dpsta interface */
		ifconfig("dpsta", IFUP, NULL, NULL);
	}

	/* Set initial QoS mode for LAN ports. */
	set_et_qos_mode();

	/* start syslogd if either log_ipaddr or log_ram_enable is set */
	if (nvram_invmatch("log_ipaddr", "") || nvram_match("log_ram_enable", "1")) {
#if !defined(__CONFIG_BUSYBOX__) || defined(BB_SYSLOGD)
		char *argv[] = {
			"syslogd",
			NULL,		/* -C */
			NULL, NULL,	/* -R host */
			NULL
		};
		int pid;
		int argc = 1;

		if (nvram_match("log_ram_enable", "1"))
			argv[argc++] = "-C";

		if (nvram_invmatch("log_ipaddr", "")) {
			argv[argc++] = "-R";
			argv[argc++] = nvram_get("log_ipaddr");
		}

		_eval(argv, NULL, 0, &pid);
#else /* Busybox configured w/o syslogd */
		cprintf("Busybox configured w/o syslogd\n");
#endif
	}

	dprintf("%s %s\n",
		nvram_safe_get("lan_ipaddr"),
		nvram_safe_get("lan_netmask"));

}

void
stop_lan(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char name[80], *next, signal[] = "XXXXXXXX";
	char br_prefix[20];
	char tmp[20];
	int i = 0;
	char* lan_ifnames;

	dprintf("%s\n", lan_ifname);

	/* Stop the syslogd daemon */
	eval("killall", "syslogd");
	/* release the DHCP address and kill the client */
	snprintf(signal, sizeof(signal), "-%d", SIGUSR2);
	eval("killall", signal, "udhcpc");
	eval("killall", "udhcpc");

	/* Remove static routes */
	del_lan_routes(lan_ifname);

	if (et_capable(NULL, "gmac3")) {
		/* Bring down GMAC#0 and GMAC#1 forwarder(s) */
		foreach(name, nvram_safe_get("fwddevs"), next) {
			ifconfig(name, 0, NULL, NULL);
		}
	}

	/* Bring down unbridged interfaces,if any */
	foreach(name, nvram_safe_get("unbridged_ifnames"), next) {
		eval("wlconf", name, "down");
		ifconfig(name, 0, NULL, NULL);
	}

	for (i = 0; i < MAX_NO_BRIDGE; i++) {
		if (!i) {
			lan_ifname = nvram_safe_get("br0_ifname");
			snprintf(br_prefix, sizeof(br_prefix), "br0_ifnames");
		}
		else {
			snprintf(tmp, sizeof(tmp), "br%x_ifname", i);
			lan_ifname = nvram_safe_get(tmp);
			snprintf(br_prefix, sizeof(br_prefix), "br%x_ifnames", i);
		}
		if (!strcmp(lan_ifname, ""))
			continue;

		/* Bring down LAN interface */
#if 0 /* foxconn wklin removed start, 03/24/2011 */
		ifconfig(lan_ifname, 0, NULL, NULL);

		/* Bring down bridged interfaces */
		if (strncmp(lan_ifname, "br", 2) == 0) {
			lan_ifnames = nvram_safe_get(br_prefix);
			foreach(name, lan_ifnames, next) {
				if (!strcmp(name, "dpsta")) {
					char dp_uif[80], *dpnext;
					foreach(dp_uif, nvram_safe_get("dpsta_ifnames"),
					        dpnext) {
						eval("wlconf", dp_uif, "down");
						ifconfig(dp_uif, 0, NULL, NULL);
					}
				}
				eval("wlconf", name, "down");
				ifconfig(name, 0, NULL, NULL);

				/* List of primary WLAN interfaces that avail of HW switching. */
				/* In 3GMAC mode, each wl interfaces in "fwd_wlandevs" don't
				 * attach to the bridge.
				 */
				if (et_capable(NULL, "gmac3") &&
				    find_in_list(nvram_get("fwd_wlandevs"), name))
					goto gmac3_no_swbr;

				eval("brctl", "delif", lan_ifname, name);
gmac3_no_swbr:
				;

#ifdef __CONFIG_EMF__
				/* Remove ifface from emf */
				if (nvram_match("emf_enable", "1"))
					eval("emf", "del", "iface", lan_ifname, name);
#endif /* __CONFIG_EMF__ */
			}
#ifdef __CONFIG_EMF__
			/* Stop the EMF for this LAN */
			eval("emf", "stop", lan_ifname);
			/* Remove Bridge from igs */
			eval("igs", "del", "bridge", lan_ifname);
			eval("emf", "del", "bridge", lan_ifname);
#endif /* __CONFIG_EMF__ */
			eval("brctl", "delbr", lan_ifname);
		}
		/* Bring down specific interface */
		else if (strcmp(lan_ifname, ""))
			eval("wlconf", lan_ifname, "down");
#endif /* foxconn wklin removed end, 03/24/2011 */

        /* Foxconn add start, Jenny Zhao, 03/29/2011  @AP Mode*/
        /* We should delete eth0 from br0 for router mode */
        if (nvram_match("enable_ap_mode", "0")) {
            char cmd[64];
            /* Delete WAN interface from br0. */
            sprintf(cmd, "brctl delif %s %s", nvram_get("lan_ifname"), nvram_get("wan_ifname"));
            system(cmd);
        }
        /* Foxconn add end, Jenny Zhao, 03/29/2011 */
	}

	unlink("/tmp/ldhclnt");

	dprintf("done\n");
}

void
start_wl(void)
{
	int i;
    /* Foxconn modified start pling 11/26/2009 */
	//char *lan_ifname = nvram_safe_get("lan_ifname");
	char lan_ifname[128];   /* Bob modifed 08/08/2014, increase buffer size for R8000 */
    /* Foxconn modified end pling 11/26/2009 */
	char name[80], *next;
	char tmp[100];
	/* Foxconn modified start pling 11/26/2009 */
	//char *lan_ifnames;
	char lan_ifnames[128];  /* Bob modifed 08/08/2014, increase buffer size for R8000 */
    /* Foxconn modified end pling 11/26/2009 */
	char command[256];
#if defined(BCA_HNDROUTER)
	char *wla_ifname, *wlg_ifname;
	wla_ifname = nvram_safe_get("wl0_ifname");
	wlg_ifname = nvram_safe_get("wl1_ifname");
#if defined(INCLULDE_2ND_5G_RADIO)
	char *wlh_ifname;
	wlh_ifname = nvram_safe_get("wl2_ifname");
#endif
#endif

    /* Initialize DFS parameter */
    
    /*foxconn Han moved, 08/09/2018 per comment from CS5613362 all DFS parameters should be config before wlconf start.*/
    /*foxconn Han edited start, 07/31/2018 per HW request add DFS for all country*/
    {
        #if 0 /*debug*/
        system("echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ > /dev/console;");
        system("wl -i eth7 isup > /dev/console");
        system("wl -i eth7 status > /dev/console");
        system("wl -i eth7 radar > /dev/console");
        system("wl -i eth7 dfs_status > /dev/console");
        system("wl -i eth7 counters | grep txbcn > /dev/console");
        system("echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ > /dev/console;");
        #endif

        system("wl -i eth7 radar 1");
        //system("wl -i eth7 radarargs 2 6 54832 0 690 0x6a8 0x20 0x6419 0x7f09 8 700 2000 244 61708 2000 0 0x1e 0x8258 30528 65282 34406 7 5 0x31 16 12000000 50000000 2 12 0xa800  0x6a8 0x30);
		#if defined(AX11000) 
        //system("wl -i eth7 radarthrs 0x6a0 0x30 0x6a0 0x00 0x6a0 0x30 0x6a0 0x00 0x6a0 0x30 0x6a4 0x30 0x6b8 0x30 0x6b8 0x30");
		#else        
        system("wl -i eth7 radarthrs 0x6a0 0x30 0x6a0 0x30 0x6a0 0x30 0x6a0 0x30 0x6a0 0x30 0x6a4 0x30 0x6b8 0x30 0x6b8 0x30");
		#endif
#if defined(INCLULDE_2ND_5G_RADIO)
        system("wl -i eth8 radar 1");
        #if defined(AX11000) 
        //system("wl -i eth8 radarthrs 0x6a0 0x30 0x6a0 0x00 0x6a0 0x30 0x6a0 0x00 0x6a0 0x30 0x6a4 0x30 0x6b8 0x30 0x6b8 0x30");
		#else
        system("wl -i eth8 radarthrs 0x6a0 0x30 0x6a0 0x30 0x6a0 0x30 0x6a0 0x30 0x6a0 0x30 0x6a4 0x30 0x6b8 0x30 0x6b8 0x30");
        #endif
#endif

        #if 0 /*debug*/
        system("echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ > /dev/console;");
        system("wl -i eth7 isup > /dev/console");
        system("wl -i eth7 status > /dev/console");
        system("wl -i eth7 radar > /dev/console");
        system("wl -i eth7 dfs_status > /dev/console");
        system("wl -i eth7 counters | grep txbcn > /dev/console");
        system("echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ > /dev/console;");
        #endif 
    }
    /*foxconn Han edited end, 07/31/2018 per HW request add DFS for all country*/

	/* If we're a travel router... then we need to make sure we get
	 * the primary wireless interface up before trying to attach slave
	 * interface(s) to the bridge
	 */
	if (nvram_match("ure_disable", "0") && nvram_match("router_disable", "0")) {
		/* start wlireless */
		snprintf(tmp, sizeof(tmp), "wlconf %s start", nvram_get("wan0_ifname"));
		system(tmp);
	}

	/* Bring up bridged interfaces */
	for (i = 0; i < MAX_NO_BRIDGE; i++) {
		if (!i) {
            /* Foxconn modified start pling 11/26/2009 */
            /* Use char array to keep the nvram value instead of
             *  using pointers. 
             */
#if 0
			lan_ifname = nvram_safe_get("lan_ifname");
			lan_ifnames = nvram_safe_get("lan_ifnames");
#endif
			strcpy(lan_ifname, nvram_safe_get("lan_ifname"));
			strcpy(lan_ifnames, nvram_safe_get("lan_ifnames"));
            /* Foxconn modified end pling 11/26/2009 */
		}
		else {
			snprintf(tmp, sizeof(tmp), "lan%x_ifname", i);
            /* Foxconn modified start pling 11/26/2009 */
            /* Use char array to keep the nvram value instead of
             *  using pointers. 
             */
			//lan_ifname = nvram_safe_get( tmp);
			strcpy(lan_ifname, nvram_safe_get( tmp));
			snprintf(tmp, sizeof(tmp), "lan%x_ifnames", i);
			//lan_ifnames = nvram_safe_get( tmp);
			strcpy(lan_ifnames, nvram_safe_get( tmp));
            /* Foxconn modified end pling 11/26/2009 */
		}
		if (strncmp(lan_ifname, "br", 2) == 0) {
			foreach(name, lan_ifnames, next) {
				if (strncmp(name, "wl", 2) == 0) {
					if (!wl_vif_enabled(name, tmp)) {
						continue; /* Ignore disabled WL VIF */
					}
				}
				/* If a wl i/f, start it */
				snprintf(tmp, sizeof(tmp), "wlconf %s start", name);
				system(tmp);

			} /* foreach().... */
		} /* if (strncmp(lan_ifname.... */
		/* specific non-bridged lan i/f */
		else if (strcmp(lan_ifname, "")) {
			/* start wireless i/f */
			snprintf(tmp, sizeof(tmp), "wlconf %s start", lan_ifname);
			system(tmp);
		}
	} /* For loop */

#if defined(BCA_HNDROUTER)
        #if 0 /*debug*/
        system("echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ > /dev/console;");
        system("wl -i eth7 isup > /dev/console");
        system("wl -i eth7 status > /dev/console");
        system("wl -i eth7 radar > /dev/console");
        system("wl -i eth7 dfs_status > /dev/console");
        system("wl -i eth7 counters | grep txbcn > /dev/console");
        system("echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ > /dev/console;");
        #endif

	wla_ifname = nvram_safe_get("wl0_ifname");
	if (!(strcmp(wla_ifname, "") == 0)) {
        /*foxconn Han edited 08/28/2018, per MH's request change txcore parameter for 2.4G only*/
		snprintf(tmp, sizeof(tmp), "wl -i %s txcore -k 0x0f -o 0x0f -s 1 -c 0x0f -s 2 -c 0x0f -s 3 -c 0x0f", wla_ifname);
		//snprintf(tmp, sizeof(tmp), "wl -i %s txcore -k 0xd -o 0xd -s 1 -c 0xd -s 2 -c 0xd -s 3 -c 0xd", wla_ifname);
		system(tmp);

		snprintf(tmp, sizeof(tmp), "wl -i %s interference 25", wla_ifname);
		system(tmp);

		if(nvram_match("wla_region","5") == 1) {
			printf("ACSD %s %d force edcrs == 1\n",__func__,__LINE__);
			snprintf(tmp, sizeof(tmp), "wl -i %s edcrs 1", wla_ifname);
			system(tmp);
		} else {
			printf("ACSD %s %d force edcrs == 0\n",__func__,__LINE__);
			snprintf(tmp, sizeof(tmp), "wl -i %s edcrs 0", wla_ifname);
			system(tmp);
		}
	}

	wlg_ifname = nvram_safe_get("wl1_ifname");
	if (!(strcmp(wlg_ifname, "") == 0)) {
		snprintf(tmp, sizeof(tmp), "wl -i %s txcore -k 0x0f -o 0x0f -s 1 -c 0x0f -s 2 -c 0x0f -s 3 -c 0x0f", wlg_ifname);
		system(tmp);

		snprintf(tmp, sizeof(tmp), "wl -i %s interference 25", wlg_ifname);
		system(tmp);
	}

#if defined(INCLULDE_2ND_5G_RADIO)
	wlh_ifname = nvram_safe_get("wl2_ifname");
	if (!(strcmp(wlh_ifname, "") == 0)) {
		snprintf(tmp, sizeof(tmp), "wl -i %s txcore -k 0x0f -o 0xf -s 1 -c 0xf -s 2 -c 0xf -s 3 -c 0xf", wlh_ifname);
		system(tmp);

		snprintf(tmp, sizeof(tmp), "wl -i %s interference 25", wlh_ifname);
		system(tmp);
	}
#endif
#else

	sprintf(command, "wl -i %s txcore -k 0xd -o 0xd -s 1 -c 0xd -s 2 -c 0xd -s 3 -c 0xd", wla_ifname);   /* Per NTGR SL suggestion for R8500 */
	system(command);
	sprintf(command, "wl -i %s txcore -o 0xf -s 1 -c 0xf -s 2 -c 0xf -s 3 -c 0xf", wlh_ifname);     /* Modified per NTGR SL suggestion on 20/26/2015 for R8500 */
	system(command);
	
	sprintf(command, "wl -i %s interference 25", wla_ifname);
	system(command);
	sprintf(command, "wl -i %s interference 25", wlg_ifname);
	system(command);
#if defined(INCLULDE_2ND_5G_RADIO)    
	sprintf(command, "wl -i %s interference 25", wlh_ifname);
	system(command);
#endif

    //system("wl -i eth1 edcrs 0"); /* Bob added on 07/29/2015 per NTGR Peiman/SL request */
    /*foxconn Han edited 12/04/2015, Netgear request when region == 5(europe) don't change edcrs value*/
    if(nvram_match("wla_region","5")==1)
    {
        printf("ACSD %s %d force edcrs == 1\n",__func__,__LINE__);
    	sprintf(command, "wl -i %s edcrs 1", wla_ifname); /* Bob added on 07/29/2015 per NTGR Peiman/SL request */
    	system(command);
    }
    else
    {
        printf("ACSD %s %d force edcrs == 0\n",__func__,__LINE__);
    	sprintf(command, "wl -i %s edcrs 0", wla_ifname); /* Bob added on 07/29/2015 per NTGR Peiman/SL request */
    	system(command);
    }
#endif

#if defined(BCA_HNDROUTER)
    if (acosNvramConfig_match("enable_atf", "1"))
    {
        //eval("wl", "-i", "eth5", "frameburst", "1");
        //eval("wl", "-i", "eth6", "frameburst", "1");
        system("wl atf 1");
        system("wl -i eth7 atf 1");
    }
    else
    {
        //eval("wl", "-i", "eth5", "frameburst", "1");
        //eval("wl", "-i", "eth6", "frameburst", "1");
        system("wl atf 0");
        system("wl -i eth7 atf 0");
    }
    /* Foxconn James Hsu added start, 2017/4/10 */
    int region = 0;
    region=atoi(nvram_get("wla_region"));
    if(acosNvramConfig_match("ce_dfs_ch_enable","1") && ((region == 5) || (region == 4)))
    {
#if 0 /*foxconn Han remove, 07/31/2018 remove previous DFS parameters.*/
        system("wl -i eth6 radar 1");
        system("wl -i eth6 radarargs 2 6 54832 6 690 0x6a8 0x20 0x6419 0x7f09 8 700 2000 244 61708 2000 3000000 0x1e 0x8258 30528 65282 33894 7 6 0x31 16 12000000 50000000 2 12 0xb000  0x6b4 0x30");
        system("wl -i eth6 radarthrs 0x6ac 0x20 0x6ac 0x20 0x6b4 0x24 0x6ac 0x20 0x6b4 0x20 0x6bc 0x2c 0x6b4 0x24 0x6bc 0x2c");

#if defined(INCLULDE_2ND_5G_RADIO)
        system("wl -i eth7 radar 1");
        system("wl -i eth7 radarargs 2 6 54832 6 690 0x6a8 0x20 0x6419 0x7f09 8 700 2000 244 61708 2000 3000000 0x1e 0x8258 30528 65282 33894 7 6 0x31 16 12000000 50000000 2 12 0xb000  0x6b4 0x30");
        system("wl -i eth7 radarthrs 0x6ac 0x20 0x6ac 0x20 0x6b4 0x24 0x6ac 0x20 0x6b4 0x20 0x6bc 0x2c 0x6b4 0x24 0x6bc 0x2c");
#endif
#endif
    }
    /* Foxconn James Hsu added end, 2017/4/10 */

#endif /*R7800*/

}

#ifdef __CONFIG_NAT__
static int
wan_prefix(char *ifname, char *prefix)
{
	int unit;

	if ((unit = wan_ifunit(ifname)) < 0)
		return -1;

	sprintf(prefix, "wan%d_", unit);
	return 0;
}

static int
add_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return add_routes(prefix, "route", wan_ifname);
}

static int
del_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return del_routes(prefix, "route", wan_ifname);
}

static int
wan_valid(char *ifname)
{
	char name[80], *next;

	foreach(name, nvram_safe_get("wan_ifnames"), next)
		if (ifname && !strcmp(ifname, name))
			return 1;
	return 0;
}

/* save pptp param in use to tmp file*/
void _dbg_dump_wan_pptp_vars(void)
{
	do {
	  FILE* fp = fopen("/tmp/pptp.dbg", "w");
	  if (fp != NULL) {
	    fprintf(fp, "wan_pptp_server_name %s\n",
	     nvram_safe_get("wan_pptp_server_name"));
	    fprintf(fp, "wan_pptp_static %s\n", nvram_safe_get("wan_pptp_static"));
	    fprintf(fp, "wan_pptp_ipaddr %s\n", nvram_safe_get("wan_pptp_ipaddr"));
	    fprintf(fp, "wan_pptp_netmask %s\n", nvram_safe_get("wan_pptp_netmask"));
	    fprintf(fp, "wan_pptp_gateway %s\n", nvram_safe_get("wan_pptp_gateway"));
	    fprintf(fp, "wan_pptp_dns0 %s\n", nvram_safe_get("wan_pptp_dns0"));
	    fprintf(fp, "wan_pptp_dns1 %s\n", nvram_safe_get("wan_pptp_dns1"));
	    fprintf(fp, "wan_pptp_dns2 %s\n", nvram_safe_get("wan_pptp_dns2"));
	    fprintf(fp, "wan_pptp_username %s\n", nvram_safe_get("wan_pptp_username"));
	    fprintf(fp, "wan_pptp_passwd %s\n", nvram_safe_get("wan_pptp_passwd"));
	    fprintf(fp, "wan_pptp_demand %s\n", nvram_safe_get("wan_pptp_demand"));
	    fprintf(fp, "wan_pptp_idletime %s\n", nvram_safe_get("wan_pptp_idletime"));
	    fprintf(fp, "wan_pptp_keepalive %s\n", nvram_safe_get("wan_pptp_keepalive"));
	    fprintf(fp, "wan_pptp_mtu %s\n", nvram_safe_get("wan_pptp_mtu"));
	    fprintf(fp, "wan_pptp_mru %s\n", nvram_safe_get("wan_pptp_mru"));
	    fclose(fp);
	  }

	} while (0);
}

/* print l2tp param in use */
void _dbg_dump_wan_l2tp_vars(void)
{
	do {
		FILE* fp = fopen("/tmp/l2tp.dbg", "w");
		if (fp != NULL) {
			fprintf(fp, "wan_l2tp_ipaddr %s\n", nvram_safe_get("wan_l2tp_ipaddr"));
			fprintf(fp, "wan_l2tp_netmask %s\n", nvram_safe_get("wan_l2tp_netmask"));
			fprintf(fp, "wan_l2tp_gateway %s\n", nvram_safe_get("wan_l2tp_gateway"));
			fprintf(fp, "wan_l2tp_static %s\n", nvram_safe_get("wan_l2tp_static"));
			fprintf(fp, "wan_l2tp_ifname %s\n", nvram_safe_get("wan_l2tp_ifname "));
			fprintf(fp, "wan_l2tp_username %s\n", nvram_safe_get("wan_l2tp_username"));
			fprintf(fp, "wan_l2tp_passwd %s\n", nvram_safe_get("wan_l2tp_passwd"));
			fprintf(fp, "wan_l2tp_demand %s\n", nvram_safe_get("wan_l2tp_demand"));
			fprintf(fp, "wan_l2tp_idletime %s\n", nvram_safe_get("wan_l2tp_idletime"));
			fprintf(fp, "wan_l2tp_mtu %s\n", nvram_safe_get("wan_l2tp_mtu"));
			fprintf(fp, "wan_l2tp_mru %s\n", nvram_safe_get("wan_l2tp_mru"));
			fprintf(fp, "wan_l2tp_server_name %s\n",
			 nvram_safe_get("wan_l2tp_server_name"));
			fprintf(fp, "wan_l2tp_dns0 %s\n", nvram_safe_get("wan_l2tp_dns0"));
			fprintf(fp, "wan_l2tp_dns1 %s\n", nvram_safe_get("wan_l2tp_dns1"));
			fprintf(fp, "wan_l2tp_dns2 %s\n", nvram_safe_get("wan_l2tp_dns2"));
			fprintf(fp, "wan_l2tp_keepalive %s\n",
			 nvram_safe_get("wan_l2tp_keepalive"));

			fclose(fp);
		}

	} while (0);
}


#ifdef __CONFIG_ACCEL_PPTP__
static bool pptp_kernel_module_loaded = FALSE;
void
load_pptp_kernel_module(void)
{
	if (pptp_kernel_module_loaded)
		return;
	/* Load the pptp module */
	eval("insmod", "pptp");
	pptp_kernel_module_loaded = TRUE;
}

void
unload_pptp_kernel_module(void)
{
	if (!pptp_kernel_module_loaded)
		return;

	/* Unload the pptp module */
	eval("rmmod", "pptp");
	pptp_kernel_module_loaded = FALSE;
}
#endif /* ! __CONFIG_ACCEL_PPTP__ */

#ifdef __CONFIG_ACCEL_L2TP__
static bool pppol2tp_kernel_module_loaded = FALSE;
void
load_pppol2tp_kernel_module(void)
{
	if (pppol2tp_kernel_module_loaded)
		return;

	/* Load the ppol2tp.ko module */
	eval("insmod", "pppol2tp");
	pppol2tp_kernel_module_loaded = TRUE;
}

void
unload_pppol2tp_kernel_module(void)
{
	if (!pppol2tp_kernel_module_loaded)
		return;

	/* Unload the pptp module */
	eval("rmmod", "pppol2tp");
	pppol2tp_kernel_module_loaded = FALSE;
}
#endif /* __CONFIG_ACCEL_L2TP__ */

void start_wan_pptp(char *wan_ifname, char *wan_proto)
{
                   char ttybuf[16], buf[256];
                   FILE *ppp_fp;
                   int i;
                   char cmd[128];

		   memset(cmd, 0, 128);

                   _dbg_dump_wan_pptp_vars(); /* save to tmp file */

                   system("mkdir -p /tmp/ppp");
                   sprintf(buf, "echo '%s * %s *'>/tmp/ppp/pap-secrets",
                    nvram_safe_get("wan_pptp_username"), nvram_safe_get("wan_pptp_passwd"));
                   system(buf);
                   sprintf(buf, "echo '%s * %s *'>/tmp/ppp/chap-secrets",
                    nvram_safe_get("wan_pptp_username"), nvram_safe_get("wan_pptp_passwd"));
                   system(buf);

                   if ((mkdir("/var/lock", 0777) < 0) && (errno != EEXIST)) {
			printf("Can't mkdir /var/lock \n");
			return; /* error */
                   }
                   if ((mkdir("/dev/pty", 0777) < 0) && (errno != EEXIST)) {
			printf("Can't mkdir /dev/pty \n");
			return; /* error */
                   }

                   for (i = 0; i < 256; i++) {
                     sprintf(ttybuf, "/dev/pty/s%d", i);
                     if ((mknod(ttybuf,
                          S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP |S_IWGRP | S_IROTH | S_IWOTH,
                           makedev(3, i)) < 0) && (errno != EEXIST)) {
			printf("Can't mknod %s \n", ttybuf);
			return; /* error */
                     }
                   }

                   if ((ppp_fp = fopen("/tmp/ppp/options", "w")) == NULL) {
                     printf("Can't write /tmp/ppp/options for pptp \n");
                     return;
                   }
                   else {
#ifdef __CONFIG_ACCEL_PPTP__
                     fprintf(ppp_fp, "plugin pptp.so\npptp_server %s\n",
                      nvram_safe_get("wan_pptp_server_name"));
#else
                     fprintf(ppp_fp, "pty \"/bin/pptp %s --nolaunchpppd\"\n",
                      nvram_safe_get("wan_pptp_server_name"));
#endif
                     /* fprintf(ppp_fp, "dump\n"); */
                     fprintf(ppp_fp, "defaultroute\n");
                     fprintf(ppp_fp, "noauth\n");
                     fprintf(ppp_fp, "noipdefault\n");
                     fprintf(ppp_fp, "lcp-echo-interval 10\n");
                     fprintf(ppp_fp, "lcp-echo-failure 6\n");
                      /* redial after disconnected 6*10=60 seconds */
                     fprintf(ppp_fp, "usepeerdns\n");
                     fprintf(ppp_fp, "proxyarp\n");
                     fprintf(ppp_fp, "user %s\n", nvram_safe_get("wan_pptp_username"));

                     if (atoi(nvram_safe_get("wan_pptp_demand")) == 0) {
                        fprintf(ppp_fp, "persist\n");
                        fprintf(ppp_fp, "maxfail 0\n"); /* redial for no limit times */

                        if (atoi(nvram_safe_get("wan_pptp_redialtime")) > 0)
                          fprintf(ppp_fp, "holdoff %s\n", nvram_safe_get("wan_pptp_redialtime"));
                     }
                     else if (atoi(nvram_safe_get("wan_pptp_demand")) == 1) {
                        fprintf(ppp_fp, "demand\n");
                        fprintf(ppp_fp, "idle %d\n", atoi(nvram_safe_get("wan_pptp_idletime")));
                                system("echo 1 > /proc/sys/net/ipv4/ip_forward");
                                system("echo 'nameserver 168.95.1.1' > /var/resolv.conf");
                                system("killall -1 dnsmasq");
                     }

                     fprintf(ppp_fp, "mtu %s\n", nvram_safe_get("wan_pptp_mtu"));
#ifndef __CONFIG_ACCEL_PPTP__
                     fprintf(ppp_fp, "lock\n");
#endif
                     fclose(ppp_fp);
                   }

                   nvram_set("wan0_pptp_ifname", "ppp0");

                   if (!strcmp(nvram_safe_get("wan_proto"), "pptp") &&
                        !strcmp(nvram_safe_get("wan_pptp_static"), "0")) {

                        fprintf(stderr, "PPTP and Get DHCP...\n");
                        /* eval_script("ifconfig", wan_ifname, "0.0.0.0", "up"); */
                        eval("ifconfig", wan_ifname, "0.0.0.0", "up");

                        if (strlen(nvram_safe_get("wan_hostname")) > 0) {

                          char host_name[260] = "";
                          char temp[260] = "";
                          sprintf(temp, "%s", nvram_safe_get("wan_hostname"));
                          /* process_qoute(temp); */
                          sprintf(host_name, "\"%s\"", temp);
                          /* eval_script("udhcpc", "-n", "-i", wan_ifname,
                             "-h", host_name, "-s", "/bin/udhcpc.sh", "&"); */
                          eval("udhcpc", "-n", "-i", wan_ifname, "-h", host_name, "-s", "/bin/udhcpc.sh");
                        }
                        else {
                          /* eval_script("udhcpc","-n","-i", wan_ifname, "-s", "/bin/udhcpc.sh","&"); */
                          eval("udhcpc", "-n", "-i", wan_ifname, "-s", "/bin/udhcpc.sh");
                        }
#ifdef __CONFIG_ACCEL_PPTP__
                        load_pptp_kernel_module();
#endif /* __CONFIG_ACCEL_PPTP__ */
                        eval("pppd");

                   }
                   else {
                        fprintf(stderr, "Pure PPTP...\n");
                        eval("ifconfig", wan_ifname, nvram_safe_get("wan_pptp_ipaddr"),
                             "netmask", nvram_safe_get("wan_pptp_netmask"), "up");
                        /* sprintf(cmd,"route add default gw %s",nvram_safe_get("wan_pptp_gateway"));
                           fprintf(stderr,"\n###pptp static cmd=%s###\n",cmd);
                         */
                        system(cmd);
#ifdef __CONFIG_ACCEL_PPTP__
                        load_pptp_kernel_module();
#endif /* __CONFIG_ACCEL_PPTP__ */
                        eval("pppd");
                   }
}

void start_wan_l2tp(char *wan_ifname, char *wan_proto)
{
                    char ttybuf[16];
                    char buf[256];
                    FILE* ppp_fp = NULL;
                    int i;
                    char cmd[128];

                    memset(cmd, 0, 128);

                    _dbg_dump_wan_l2tp_vars();

                    eval("mkdir", "-p", "/tmp/l2tp");
                    sprintf(buf, "echo '%s * %s *'>/tmp/l2tp/l2tp-secrets",
                               nvram_safe_get("wan_l2tp_username"), nvram_safe_get("wan_l2tp_passwd"));
                    system(buf);
                    sprintf(buf, "echo '%s * %s *'>/tmp/ppp/pap-secrets",
                              nvram_safe_get("wan_l2tp_username"), nvram_safe_get("wan_l2tp_passwd"));
                    system(buf);
                    sprintf(buf, "echo '%s * %s *'>/tmp/ppp/chap-secrets",
                             nvram_safe_get("wan_l2tp_username"), nvram_safe_get("wan_l2tp_passwd"));
                    system(buf);

                    if ((mkdir("/tmp/lock", 0777) < 0) && (errno != EEXIST)) {
			printf("Can't mkdir /tmp/lock \n");
			return; /* error */
                    }

#ifdef __CONFIG_ACCEL_L2TP__
                    /* Create openl2tpd.conf */
                    if ((ppp_fp = fopen("/tmp/l2tp/openl2tpd.conf", "w")) != NULL)
                    {
#ifdef __CONFIG_ACCEL_L2TP_DEBUG__
                      fprintf(ppp_fp, "ppp profile create profile_name=ppp0 sync_mode=async trace_flags=255\n");
                      fprintf(ppp_fp, "tunnel create tunnel_name=one dest_ipaddr=%s src_ipaddr=%s ",
                            nvram_safe_get("wan_l2tp_server_name"), nvram_safe_get("wan_l2tp_ipaddr"));
                      fprintf(ppp_fp, "our_udp_port=1701 secret=tigran trace_flags=15\n");
                      fprintf(ppp_fp,
                       "session create session_name=one tunnel_name=one user_name=%s user_password=%s",
                        nvram_safe_get("wan_l2tp_username"), nvram_safe_get("wan_l2tp_passwd"));
                      fprintf(ppp_fp, "trace_flags=2047 ppp_profile_name=ppp0\n");
#else   /* ! __CONFIG_ACCEL_L2TP_DEBUG__ */
                      fprintf(ppp_fp, "ppp profile create profile_name=ppp0 sync_mode=async\n");
                      fprintf(ppp_fp,
                       "tunnel create tunnel_name=one dest_ipaddr=%s src_ipaddr=%s our_udp_port=1701 secret=matt\n",
                        nvram_safe_get("wan_l2tp_server_name"), nvram_safe_get("wan_l2tp_ipaddr"));
                      fprintf(ppp_fp, "session create session_name=one tunnel_name=one user_name=%s user_password=%s ",
                        nvram_safe_get("wan_l2tp_username"), nvram_safe_get("wan_l2tp_passwd"));
                      fprintf(ppp_fp, "ppp_profile_name=ppp0\n");
#endif /* __CONFIG_ACCEL_L2TP_DEBUG__ */
                      fclose(ppp_fp);
                    }
                    else {
                      printf("Can't open /tmp/l2tp/openl2tpd.conf\n");
                      return; /* error */
                    }

#else /* ! __CONFIG_ACCEL_L2TP__ */
                    /* Create l2tpd.conf */
                    if ((ppp_fp = fopen("/tmp/l2tp/l2tpd.conf", "w")) == NULL)
                    {
                         printf("Can't write /tmp/l2tp/l2tpd.conf for l2tp\n");
                         return; /* error */
                    }
                    else
                    {
                         fprintf(ppp_fp, "[global]\n");
                         fprintf(ppp_fp, "port = 1701\n");
                         fprintf(ppp_fp, "\n");
                         fprintf(ppp_fp, "[lac]\n");
                         fprintf(ppp_fp, "lns = %s\n", nvram_safe_get("wan_l2tp_server_name"));
                         fprintf(ppp_fp, "autodial = yes\n");
                         fprintf(ppp_fp, "redial = yes\n");
                         fprintf(ppp_fp, "redial timeout = 5\n");
                         fprintf(ppp_fp, "max redials = 5\n");
                         if (strlen(nvram_safe_get("wan_hostname")) > 0)
                                 fprintf(ppp_fp, "hostname = %s\n", nvram_safe_get("wan_hostname"));
                         fclose(ppp_fp);
                    }

                    if ((ppp_fp = fopen("/tmp/ppp/options", "w")) == NULL)
                    {
                         printf("Can't write /tmp/ppp/options for pppoe \n");
                         return;
                    }
                    else
                    {
                         /* Fix Cisco7200 Compatibility issue */
                         fprintf(ppp_fp, "dump\n");
                         fprintf(ppp_fp, "noccp\n");
                         fprintf(ppp_fp, "novj\n");
                         fprintf(ppp_fp, "novjccomp\n");
                         fprintf(ppp_fp, "nopcomp\n");
                         fprintf(ppp_fp, "noaccomp\n");
                         fprintf(ppp_fp, "lcp-echo-interval 10\n");
                         fprintf(ppp_fp, "lcp-echo-failure 3\n");
                         /* redial after disconnected 3*10=30 seconds */
                         /* redial after disconnected 3*10=30 seconds */
                         /* Gemtek, Fix Cisco7200 Compatibility issue */
                         fprintf(ppp_fp, "lock\n");
                         fprintf(ppp_fp, "debug\n");
                         fprintf(ppp_fp, "logfd 2\n");
                         fprintf(ppp_fp, "passive\n");
                         fprintf(ppp_fp, "nodetach\n");
                         fprintf(ppp_fp, "defaultroute\n");

                         if (atoi(nvram_safe_get("wan_l2tp_demand")) == 0)
                         {
                                 fprintf(ppp_fp, "persist\n");
                                 fprintf(ppp_fp, "maxfail 0\n"); /* redial for no limit times */
                         }
                         fprintf(ppp_fp, "noipdefault\n");
                         fprintf(ppp_fp, "ipcp-accept-remote\n");
                         fprintf(ppp_fp, "usepeerdns\n");
                         fprintf(ppp_fp, "proxyarp\n");
                         fprintf(ppp_fp, "user \"%s\"\n", nvram_safe_get("wan_l2tp_username"));
                         fprintf(ppp_fp, "password \"%s\"\n", nvram_safe_get("wan_l2tp_passwd"));
                         fclose(ppp_fp);
                    }
#endif /* __CONFIG_ACCEL_L2TP__ */

                   nvram_set("wan0_l2tp_ifname", "ppp0");
                   /* eval_script("ln","-s","/sbin/rc","/tmp/ppp/ip-up"); */

                   if (!strcmp(nvram_safe_get("wan_proto"), "l2tp") &&
                        !strcmp(nvram_safe_get("wan_l2tp_static"), "0"))
                   {
                       fprintf(stderr, "L2TP and Get DHCP...\n");
                       /* eval_script("ifconfig", wan_ifname, "0.0.0.0","up" ); */
                       eval("ifconfig", wan_ifname, "0.0.0.0", "up");
                       if (strlen(nvram_safe_get("wan_hostname")) > 0)
                       {
                        char host_name[260] = "";
                        char temp[260] = "";
                        sprintf(temp, "%s", nvram_safe_get("wan_hostname"));
                        /* process_qoute(temp); */
                        sprintf(host_name, "\"%s\"",temp );

                        /* eval_script("udhcpc","-n","-i", wan_ifname,"-h", host_name,
                         * "-s", "/bin/udhcpc.sh","&");
                         */
                        eval("udhcpc", "-n", "-i", wan_ifname, "-h", host_name, "-s", "/bin/udhcpc.sh");
                       }
                       else
                       {
                        /* eval_script("udhcpc","-n","-i", wan_ifname, "-s", "/bin/udhcpc.sh","&"); */
                        eval("udhcpc", "-n", "-i", wan_ifname, "-s", "/bin/udhcpc.sh");
                       }
                   }
                   else
                   {
                     fprintf(stderr, "Pure L2TP...\n");
                     eval("ifconfig", wan_ifname, nvram_safe_get("wan_l2tp_ipaddr"),
                                      "netmask", nvram_safe_get("wan_l2tp_netmask"), "up");
                     sprintf(cmd, "route add default gw %s", nvram_safe_get("wan_l2tp_gateway"));
                     /* fprintf(stderr,"\n###l2tp static cmd=%s###\n",cmd); */
                     system(cmd);
                   }

                   if (atoi(nvram_safe_get("wan_l2tp_demand")) == 0)
                   {
#ifdef __CONFIG_ACCEL_L2TP__
                     /* load_pppol2tp_kernel_module(); */
                     system("killall openl2tpd");
#ifdef __CONFIG_ACCEL_L2TP_DEBUG__
                     system("openl2tpd -f -u 1701 -c /tmp/l2tp/openl2tpd.conf -D -d 0x7FF &");
#else /* ! __CONFIG_ACCEL_L2TP_DEBUG__ */
                     system("openl2tpd -f -u 1701 -c /tmp/l2tp/openl2tpd.conf &");
#endif /* __CONFIG_ACCEL_L2TP_DEBUG__ */

#else /* ! __CONFIG_ACCEL_L2TP__ */
                    system(
                      "l2tpd -D -c /tmp/l2tp/l2tpd.conf -s /tmp/l2tp/l2tp-secrets -p /tmp/run/l2tpd.pid &");
#endif /* __CONFIG_ACCEL_L2TP__ */
                   }
                   else
                   {
#ifdef __CONFIG_ACCEL_L2TP__
                     /* load_pppol2tp_kernel_module(); */
                     system("killall openl2tpd");
#ifdef __CONFIG_ACCEL_L2TP_DEBUG__
                     system("openl2tpd -f -u 1701 -c /tmp/l2tp/openl2tpd.conf -D -d 0x7FF &");
#else /* ! __CONFIG_ACCEL_L2TP_DEBUG__ */
                     system("openl2tpd -f -u 1701 -c /tmp/l2tp/openl2tpd.conf &");
#endif /* __CONFIG_ACCEL_L2TP_DEBUG__ */

#else /* ! __CONFIG_ACCEL_L2TP__ */
                    /* eval_script("killall", "l2tpd"); */
                    /* eval_script("l2tpd", "-D", "-d", "-c",
                     * "/tmp/l2tp/l2tpd.conf", "-s", "/tmp/l2tp/l2tp-secrets", "-p",
                     * "/tmp/run/l2tpd.pid", "&");
                     */
                    system("killall l2tpd");
                    system(
                      "l2tpd -D -d -c /tmp/l2tp/l2tpd.conf -s /tmp/l2tp/l2tp-secrets -p /tmp/run/l2tpd.pid &");
#endif /* __CONFIG_ACCEL_L2TP__ */
                   }
}


void
start_wan(void)
{
	char *wan_ifname;
	char *wan_proto;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char eabuf[32];
	int s;
	struct ifreq ifr;
	pid_t pid;

	/* check if we need to setup WAN */
	if (nvram_match("router_disable", "1"))
		return;

	/* start connection independent firewall */
	start_firewall();

	/* Create links */
	if ((mkdir("/tmp/ppp", 0777) < 0) && (errno != EEXIST)) {
		perror("Could not create /tmp/ppp directory.");
		fprintf(stderr, "%s:%d: Aborting...\n", __FUNCTION__, __LINE__);
		abort();
	}
	symlink("/sbin/rc", "/tmp/ppp/ip-up");
	symlink("/sbin/rc", "/tmp/ppp/ip-down");

	symlink("/sbin/rc", "/tmp/udhcpc");

	/* Start each configured and enabled wan connection and its undelying i/f */
	for (unit = 0; unit < MAX_NVPARSE; unit ++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		/* make sure the connection exists and is enabled */
		wan_ifname = nvram_get(strcat_r(prefix, "ifname", tmp));
		if (!wan_ifname)
			continue;
		wan_proto = nvram_get(strcat_r(prefix, "proto", tmp));
		if (!wan_proto || !strcmp(wan_proto, "disabled"))
			continue;

		/* disable the connection if the i/f is not in wan_ifnames */
		if (!wan_valid(wan_ifname)) {
			nvram_set(strcat_r(prefix, "proto", tmp), "disabled");
			continue;
		}

		/* Checking for size of wan_ifname */
		if (strlen(wan_ifname) > (sizeof(ifr.ifr_name) - 1))
			continue;

		dprintf("%s %s\n", wan_ifname, wan_proto);

		/* Set i/f hardware address before bringing it up */
		if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
			continue;
		strncpy(ifr.ifr_name, wan_ifname, sizeof(ifr.ifr_name));
		ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

		/* Configure i/f only once, specially for wl i/f shared by multiple connections */
		if (ioctl(s, SIOCGIFFLAGS, &ifr)) {
			close(s);
			continue;
		}

		if (!(ifr.ifr_flags & IFF_UP)) {
			/* Sync connection nvram address and i/f hardware address */
			memset(ifr.ifr_hwaddr.sa_data, 0, ETHER_ADDR_LEN);
			if (!nvram_invmatch(strcat_r(prefix, "hwaddr", tmp), "") ||
			    !ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)),
			    (unsigned char *)ifr.ifr_hwaddr.sa_data) ||
			    !memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN)) {
				if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
					close(s);
					continue;
				}
				nvram_set(strcat_r(prefix, "hwaddr", tmp),
					ether_etoa((unsigned char *)ifr.ifr_hwaddr.sa_data,
					eabuf));
			} else {
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				if (ioctl(s, SIOCSIFHWADDR, &ifr)) {
					close(s);
					continue;
				}
			}

			/* Bring up i/f */
			ifconfig(wan_ifname, IFUP, NULL, NULL);

			/* do wireless specific config */
			if (nvram_match("ure_disable", "1")) {
				eval("wlconf", wan_ifname, "up");
				eval("wlconf", wan_ifname, "start");
			}
		}

		close(s);

		/* Set initial QoS mode again now that WAN port is ready. */
		set_et_qos_mode();

		/* Configure PPPoE connection. The PPPoE client will run
		* ip-up/ip-down scripts upon link's connect/disconnect.
		*/
		if (strcmp(wan_proto, "pppoe") == 0) {
#ifdef BCA_HNDROUTER
			char *pppoe_argv[] = {
				"pppd",
				"-i", nvram_safe_get(strcat_r(prefix, "ifname", tmp)),
				"-c", nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), "") ?
				"ppp0" : nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)),
				"-u", nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)),
				"-p", nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)),
				NULL, NULL,	/* pppoe_idletime */
				NULL
			}, **arg;
			int timeout = 5;

			/* Add optional arguments */
			for (arg = pppoe_argv; *arg; arg++);
			if (nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1")) {
				*arg++ = "-o";
				*arg++ = nvram_safe_get(strcat_r(prefix, "pppoe_idletime", tmp));
			}
#else /* BCA_HNDROUTER */
			char *pppoe_argv[] = {
				"pppoecd",
				nvram_safe_get(strcat_r(prefix, "ifname", tmp)),
				"-u", nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)),
				"-p", nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)),
				"-r", nvram_safe_get(strcat_r(prefix, "pppoe_mru", tmp)),
				"-t", nvram_safe_get(strcat_r(prefix, "pppoe_mtu", tmp)),
				"-i", nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1") ?
				nvram_safe_get(strcat_r(prefix, "pppoe_idletime", tmp)) : "0",
				NULL, NULL,	/* pppoe_service */
				NULL, NULL,	/* pppoe_ac */
				NULL,		/* pppoe_keepalive */
				NULL, NULL,	/* ppp unit requested */
				NULL
			}, **arg;
			int timeout = 5;
			char pppunit[] = "XXXXXXXXXXXX";

			/* Add optional arguments */
			for (arg = pppoe_argv; *arg; arg++);
			if (nvram_invmatch(strcat_r(prefix, "pppoe_service", tmp), "")) {
				*arg++ = "-s";
				*arg++ = nvram_safe_get(strcat_r(prefix, "pppoe_service", tmp));
			}
			if (nvram_invmatch(strcat_r(prefix, "pppoe_ac", tmp), "")) {
				*arg++ = "-a";
				*arg++ = nvram_safe_get(strcat_r(prefix, "pppoe_ac", tmp));
			}
			if (nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1") ||
			nvram_match(strcat_r(prefix, "pppoe_keepalive", tmp), "1"))
				*arg++ = "-k";
			snprintf(pppunit, sizeof(pppunit), "%d", unit);
			*arg++ = "-U";
			*arg++ = pppunit;
#endif /* !BCA_HNDROUTER */

			/* launch pppoe client daemon */
			_eval(pppoe_argv, NULL, 0, &pid);

			/* ppp interface name is referenced from this point on */
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));

			/* Pretend that the WAN interface is up */
			if (nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1")) {
				/* Wait for pppx to be created */
				while (ifconfig(wan_ifname, IFUP, NULL, NULL) && timeout--)
					sleep(1);

				/* Retrieve IP info */
				if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
					continue;
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

				/* Set temporary IP address */
				if (ioctl(s, SIOCGIFADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "ipaddr", tmp),
				inet_ntoa(sin_addr(&ifr.ifr_addr)));
				nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.255.255");

				/* Set temporary P-t-P address */
				if (ioctl(s, SIOCGIFDSTADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "gateway", tmp),
				inet_ntoa(sin_addr(&ifr.ifr_dstaddr)));

				close(s);

				/* Preset routes so that traffic can be sent to proper pppx
				* even before the link is brought up.
				*/
				preset_wan_routes(wan_ifname);
			}
		}
		/* Configure DHCP connection. The DHCP client will run
		* 'udhcpc bound'/'udhcpc deconfig' upon finishing IP address
		* renew and release.
		*/
		else if (strcmp(wan_proto, "dhcp") == 0) {
			char *wan_hostname = nvram_safe_get(strcat_r(prefix, "hostname", tmp));
			char *dhcp_argv[] = { "udhcpc",
			"-i", wan_ifname,
			"-p", (sprintf(tmp, "/var/run/udhcpc%d.pid", unit), tmp),
			"-s", "/tmp/udhcpc",
			wan_hostname && *wan_hostname ? "-H" : NULL,
			wan_hostname && *wan_hostname ? wan_hostname : NULL,
			NULL
			};
			/* Start dhcp client */
			_eval(dhcp_argv, NULL, 0, &pid);
		}
		/* Configure static IP connection. */
		else if (strcmp(wan_proto, "static") == 0) {
			/* Assign static IP address to i/f */
			ifconfig(wan_ifname, IFUP,
				nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
				nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
			/* We are done configuration */
			wan_up(wan_ifname);
		}

		/* Start connection dependent firewall */
		start_firewall2(wan_ifname);

		dprintf("%s %s\n",
			nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
			nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
	}

	/* Report stats */
	if (nvram_invmatch("stats_server", "")) {
		char *stats_argv[] = { "stats", nvram_get("stats_server"), NULL };
		_eval(stats_argv, NULL, 5, NULL);
	}
}

void
stop_wan(void)
{
	char name[80], *next, signal[] = "XXXX";

#ifdef BCMQOS
		del_iQosRules();
#endif /* BCMQOS */
	eval("killall", "stats");
	eval("killall", "ntpclient");

	/* Shutdown and kill all possible tasks */
	eval("killall", "ip-up");
	eval("killall", "ip-down");
	snprintf(signal, sizeof(signal), "-%d", SIGHUP);
	eval("killall", signal, "pppoecd");
	eval("killall", "pppoecd");
	snprintf(signal, sizeof(signal), "-%d", SIGUSR2);
	eval("killall", signal, "udhcpc");
	eval("killall", "udhcpc");

	/* Bring down WAN interfaces */
	foreach(name, nvram_safe_get("wan_ifnames"), next)
		ifconfig(name, 0, "0.0.0.0", NULL);

	/* Remove dynamically created links */
	unlink("/tmp/udhcpc");

	unlink("/tmp/ppp/ip-up");
	unlink("/tmp/ppp/ip-down");
	rmdir("/tmp/ppp");

	dprintf("done\n");
}

static int
add_ns(char *wan_ifname)
{
	FILE *fp;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char word[100], *next;
	char line[100];

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	/* Open resolv.conf to read */
	if (!(fp = fopen("/tmp/resolv.conf", "r+"))) {
		perror("/tmp/resolv.conf");
		return errno;
	}
	/* Append only those not in the original list */
	foreach(word, nvram_safe_get(strcat_r(prefix, "dns", tmp)), next) {
		fseek(fp, 0, SEEK_SET);
		while (fgets(line, sizeof(line), fp)) {
			char *token = strtok(line, " \t\n");

			if (!token || strcmp(token, "nameserver") != 0)
				continue;
			if (!(token = strtok(NULL, " \t\n")))
				continue;

			if (!strcmp(token, word))
				break;
		}
		if (feof(fp))
			fprintf(fp, "nameserver %s\n", word);
	}
	fclose(fp);

	/* notify dnsmasq */
	snprintf(tmp, sizeof(tmp), "-%d", SIGHUP);
	eval("killall", tmp, "dnsmasq");

	return 0;
}

static int
del_ns(char *wan_ifname)
{
	FILE *fp, *fp2;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char word[100], *next;
	char line[100];

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	/* Open resolv.conf to read */
	if (!(fp = fopen("/tmp/resolv.conf", "r"))) {
		perror("fopen /tmp/resolv.conf");
		return errno;
	}
	/* Open resolv.tmp to save updated name server list */
	if (!(fp2 = fopen("/tmp/resolv.tmp", "w"))) {
		perror("fopen /tmp/resolv.tmp");
		fclose(fp);
		return errno;
	}
	/* Copy updated name servers */
	while (fgets(line, sizeof(line), fp)) {
		char *token = strtok(line, " \t\n");

		if (!token || strcmp(token, "nameserver") != 0)
			continue;
		if (!(token = strtok(NULL, " \t\n")))
			continue;

		foreach(word, nvram_safe_get(strcat_r(prefix, "dns", tmp)), next)
			if (!strcmp(word, token))
				break;
		if (!next)
			fprintf(fp2, "nameserver %s\n", token);
	}
	fclose(fp);
	fclose(fp2);
	/* Use updated file as resolv.conf */
	unlink("/tmp/resolv.conf");
	if (rename("/tmp/resolv.tmp", "/tmp/resolv.conf") != 0) {
		perror("rename fails /tmp/resolv.conf");
		return errno;
	}
	/* notify dnsmasq */
	snprintf(tmp, sizeof(tmp), "-%d", SIGHUP);
	eval("killall", tmp, "dnsmasq");

	return 0;
}

/*
*/
#ifdef __CONFIG_IPV6__
/* Start the 6to4 Tunneling interface.
*	Return > 0: number of interfaces processed by this routine.
*		==0: skipped since no action is required.
*		< 0: Error number
*/
static int
start_6to4(char *wan_ifname)
{
	int i, ret = 0;
	int siMode, siCount;
	unsigned short uw6to4ID;
	in_addr_t uiWANIP;
	char *pcLANIF, *pcWANIP, tmp[64], prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return 0;

	pcWANIP = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));

	/* Remove the extra white space at the end of wanX_ipaddr */
	{
		char *pcEnd = memccpy(tmp, pcWANIP, ' ', sizeof(tmp));

		if (pcEnd) {
			*(pcEnd - 1) = '\0'; /* Overwrite the white space */
		}
	}

	uiWANIP = inet_network(pcWANIP);

	/* Check if the wan IP is private(RFC1918). 6to4 needs a global IP */
	if ((uiWANIP == 0) || (uiWANIP == -1) ||
		(uiWANIP & 0xffff0000) == 0xc0a80000 || /* 192.168.x.x */
		(uiWANIP & 0xfff00000) == 0xac100000 || /* 172.16.x.x */
		(uiWANIP & 0xff000000) == 0x0a000000) /* 10.x.x.x */
		return 0;

	/* Create 6to4 intrface and setup routing table */
	for (i = 0, siCount = 0; i < MAX_NO_BRIDGE; i++) {
		if (i == 0) {
			pcLANIF = nvram_safe_get("lan_ifname");
			siMode = atoi(nvram_safe_get("lan_ipv6_mode"));
			uw6to4ID = (unsigned short)atoi(nvram_safe_get("lan_ipv6_6to4id"));
		}
		else {
			snprintf(tmp, sizeof(tmp), "lan%x_ifname", i);
			pcLANIF = nvram_safe_get(tmp);
			snprintf(tmp, sizeof(tmp), "lan%x_ipv6_mode", i);
			siMode = atoi(nvram_safe_get(tmp));
			snprintf(tmp, sizeof(tmp), "lan%x_ipv6_6to4id", i);
			uw6to4ID = (unsigned short)atoi(nvram_safe_get(tmp));
		}

		if (siMode & IPV6_6TO4_ENABLED) {
			/* Add the 6to4 route. */
			snprintf(tmp, sizeof(tmp), "2002:%x:%x:%x::/64",
				(unsigned short)(uiWANIP>>16), (unsigned short)uiWANIP,	uw6to4ID);
			ret = eval("ip", "-6", "route", "add", tmp,
				"dev", pcLANIF, "metric", "1");
			siCount++;
		}
	}

	if (siCount == 0)
		return 0;

	/* Create 6to4 intrface and setup routing table */
	{
		char *pc6to4IF = "v6to4"; /* The 6to4 tunneling interface name */
		struct in_addr stWANIP;

		stWANIP.s_addr = htonl(uiWANIP);

		/* Create the tunneling interface */
		ret = eval("ip", "tunnel", "add", pc6to4IF, "mode", "sit",
			"ttl", "64", "remote", "any", "local", inet_ntoa(stWANIP));

		/* Bring the device up */
		ret = eval("ip", "link", "set", "dev", pc6to4IF, "up");

		/* Add 6to4 v4 anycast route to the global IPv6 network */
		ret = eval("ip", "-6", "route", "add", "2000::/3",
			"via", "::192.88.99.1", "dev", pc6to4IF, "metric", "1");
	}

#ifdef __CONFIG_RADVD__
	/* Restart radvd */
	{
		system("killall -SIGHUP radvd");
	}
#endif /* __CONFIG_RADVD__ */

#ifdef __CONFIG_NAT__
	/* Enable IPv6 protocol=41(0x29) on v4NAT */
	{
		char *pcWANIF;

		pcWANIF = nvram_match("wan_proto", "pppoe")?
			nvram_safe_get("wan_pppoe_ifname"): nvram_safe_get("wan_ifname");
		add_ipv6_filter(nvram_safe_get(pcWANIF));
	}
#endif /* __CONFIG_NAT__ */

	return siCount;
}
#endif /* __CONFIG_IPV6__ */
/*
*/

void
wan_up(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto;

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return;

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	dprintf("%s %s\n", wan_ifname, wan_proto);

	/* Set default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
		route_add(wan_ifname, 0, "0.0.0.0",
			nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
			"0.0.0.0");

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);

	/* Add dns servers to resolv.conf */
	add_ns(wan_ifname);

	/* Sync time */
	start_ntpc();

#ifdef BCMQOS
	add_iQosRules(wan_ifname);
	start_iQos();
#endif /* BCMQOS */
/*
*/
#ifdef __CONFIG_IPV6__
	start_6to4(wan_ifname);
#endif /* __CONFIG_IPV6__ */
/*
*/
#if defined(BCA_HNDROUTER) && defined(MCPD_PROXY)
	restart_mcpd_proxy();
#endif /* BCA_HNDROUTER && MCPD_PROXY */

	dprintf("done\n");
}

void
wan_down(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto;

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return;

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	printf("%s %s\n", wan_ifname, wan_proto);

	/* Remove default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
		route_del(wan_ifname, 0, "0.0.0.0",
			nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
			"0.0.0.0");

	/* Remove interface dependent static routes */
	del_wan_routes(wan_ifname);

	/* Update resolv.conf */
	del_ns(wan_ifname);

	dprintf("done\n");
}
#endif	/* __CONFIG_NAT__ */

/* Enable WET DHCP relay for ethernet clients */
static int
enable_dhcprelay(char *ifname)
{
	char name[80], *next;
#ifdef __CONFIG_DHDAP__
	int is_dhd;
#endif /* __CONFIG_DHDAP__ */
	uint32 ip;
	char brx_ifnames[20];
	int subunit = -1;
	char urembss[] = "wlXXXXXXXXXX_ure_mbss";
	bool ure_mbss = FALSE;
	dprintf("%s\n", ifname);

	/* WET interface is meaningful only in bridged environment */
	if (strncmp(ifname, "br", 2) == 0) {
		int bridx = atoi(ifname+2);
		snprintf(brx_ifnames, sizeof(brx_ifnames), "%s_ifnames", ifname);
		foreach(name, nvram_safe_get(brx_ifnames), next) {
			char mode[] = "wlXXXXXXXXXX_mode";
			int unit;

			/* make sure the interface is indeed of wl */
			if (wl_probe(name))
				continue;

			/* get the instance number of the wl i/f */
			wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
			if (bridx == 0)
				snprintf(mode, sizeof(mode), "wl%d_mode", unit);
			else /* Should be wlx.x_mode */
				snprintf(mode, sizeof(mode), "%s_mode", name);

			if (!strcmp(nvram_safe_get("ure_disable"), "0")) {
				snprintf(urembss, sizeof(urembss), "wl%d_ure_mbss", unit);
				if (!strcmp(nvram_safe_get(urembss), "1")) {
					ure_mbss = TRUE;
				}
			}

			if (!ure_mbss) {
				subunit = 0;
				snprintf(mode, sizeof(mode), "wl%d_mode", unit);
			}
			else {
				if (strncmp(name, "eth", 3) == 0) {
					subunit = 0;
					snprintf(mode, sizeof(mode), "wl%d_mode", unit);
				}
				else {
					snprintf(mode, sizeof(mode), "%s_mode", name);
				}
			}

			/* enable DHCP relay, there should be only one WET i/f */
			if (nvram_match(mode, "wet")) {
				char *lan_ipaddr = nvram_safe_get("lan_ipaddr");
				char tmp[100];
				wet_host_t wh;
				memset(&wh, 0, sizeof(wet_host_t));

				if (bridx) {
					snprintf(tmp, sizeof(tmp), "lan%x_ipaddr", bridx);
					lan_ipaddr = nvram_safe_get(tmp);
				}

				if (ure_mbss && subunit) {
					if (get_ifname_unit(name, NULL, &subunit) != 0)
						continue;
				}

				if (subunit < 0)
					continue;

				inet_aton(lan_ipaddr, (struct in_addr *)&ip);
				wh.bssidx = subunit;
				memcpy(&wh.buf, &ip, sizeof(ip));
#ifdef __CONFIG_DHDAP__
				is_dhd = !dhd_probe(name);
				if (is_dhd) {
					dhd_iovar_set(name, "wet_host_ipv4", &wh,
						sizeof(wet_host_t));
				}
				else
#endif /* __CONFIG_DHDAP__ */
				{
					wl_iovar_set(name, "wet_host_ipv4", &wh,
						sizeof(wet_host_t));
				}
				break;
			}
		}
	}
	return 0;
}

void
lan_up(char *lan_ifname)
{
	/* Install default route to gateway - AP only */
	if (nvram_match("router_disable", "1") && nvram_invmatch("lan_gateway", ""))
		route_add(lan_ifname, 0, "0.0.0.0", nvram_safe_get("lan_gateway"), "0.0.0.0");

	/* Install interface dependent static routes */
	add_lan_routes(lan_ifname);

	/* Sync time - AP only */
	if (nvram_match("router_disable", "1"))
		start_ntpc();

	/* Enable WET DHCP relay if requested */
	if (atoi(nvram_safe_get("dhcp_relay")) == 1)
		enable_dhcprelay(lan_ifname);

	dprintf("done\n");
}

void
lan_down(char *lan_ifname)
{
	/* Remove default route to gateway - AP only */
	if (nvram_match("router_disable", "1") && nvram_invmatch("lan_gateway", ""))
		route_del(lan_ifname, 0, "0.0.0.0", nvram_safe_get("lan_gateway"), "0.0.0.0");

	/* Remove interface dependent static routes */
	del_lan_routes(lan_ifname);

	dprintf("done\n");
}

#ifdef BCA_HNDROUTER
/* Wait for LAN port transition to FORWARDING state */
#define SYSFS_BRPORT_STATE	"/sys/class/net/%s/brport/state"

static int
wait_to_forward_state(char *ifname)
{
	FILE *fp;
	char brport_state[64] = {0};
	int timeout, state;

	timeout = 5;
	state = BR_STATE_DISABLED;

	while (timeout-- && (state != BR_STATE_FORWARDING)) {
		sprintf(brport_state, SYSFS_BRPORT_STATE, ifname);
		if ((fp = fopen(brport_state, "r")) != NULL) {
			fscanf(fp, "%d", &state);
			fclose(fp);
		}
		if (state == BR_STATE_FORWARDING)
			break;
		sleep(1);
	}

	if (!timeout)
		return 1;

	return 0;
}

void
wait_lan_port_to_forward_state(void)
{
	char name[80], *next;
	FILE *fp;
	char brport_state[64] = {0};
	int i, timeout, state;
	char lan_stp[16];
	char *lan_ifnames;
	char tmp[100];

	for (i = 0; i < MAX_NO_BRIDGE; i++) {
		if (!i) {
			snprintf(lan_stp, sizeof(lan_stp), "lan_stp");
			lan_ifnames = nvram_safe_get("lan_ifnames");
		} else {
			snprintf(lan_stp, sizeof(lan_stp), "lan%x_stp", i);
			snprintf(tmp, sizeof(tmp), "lan%x_ifnames", i);
			lan_ifnames = nvram_safe_get(tmp);
		}

		if (nvram_match(lan_stp, "0"))
			continue;

		timeout = 5;
		state = BR_STATE_DISABLED;

		while (timeout-- && (state != BR_STATE_FORWARDING)) {
			foreach(name, lan_ifnames, next) {
				sprintf(brport_state, SYSFS_BRPORT_STATE, name);
				if ((fp = fopen(brport_state, "r")) != NULL) {
					fscanf(fp, "%d", &state);
					fclose(fp);
				}
				if (state == BR_STATE_FORWARDING)
					break;
			}
			sleep(1);
		}
	}
}
#endif /* BCA_HNDROUTER */

int
hotplug_net(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char *interface, *action;
	bool psta_if, dyn_if, add_event, remove_event, monitor_if;
	char wdsap_ifname[IFNAMSIZ] = {0};

	dprintf("hotplug_net(): start\n");

	if (!(interface = getenv("INTERFACE")) ||
	    !(action = getenv("ACTION")))
		return EINVAL;

	dprintf("hotplug_net(): interface %s action %s\n", interface, action);

#ifdef LINUX26
	add_event = !strcmp(action, "add");
#else
	add_event = !strcmp(action, "register");
#endif // endif

#ifdef LINUX26
	remove_event = !strcmp(action, "remove");
#else
	remove_event = !strcmp(action, "unregister");
#endif // endif

	psta_if = wl_wlif_is_psta(interface);
	dyn_if = !strncmp(interface, "wds", 3) || psta_if;
	monitor_if = !strncmp(interface, "radiotap", 8);

	if (monitor_if)
	{
		if (add_event) {
			ifconfig(interface, IFUP, NULL, NULL);
			return 0;
		}
	}

	if (!dyn_if && !remove_event)
		return 0;

	if (add_event) {
		/* Bring up the interface and add to the bridge */
		ifconfig(interface, IFUP, NULL, NULL);

		/* For WDS interface, get associated AP ifname
		 * and attach WDS interface to corresponding bridge.
		 * If fails attach WDS interface to default Bridge (LAN)
		 */
		if (dyn_if && !wl_wlif_wds_ap_ifname(interface, wdsap_ifname)) {
			/* Get Bridge for which associated AP is attached */
			get_bridge_by_ifname(wdsap_ifname, &lan_ifname);
		}

#ifdef __CONFIG_EMF__
		if (nvram_match("emf_enable", "1")) {
			eval("emf", "add", "iface", lan_ifname, interface);
			emf_mfdb_update(lan_ifname, interface, TRUE);
			emf_uffp_update(lan_ifname, interface, TRUE);
			emf_rtport_update(lan_ifname, interface, TRUE);
		}
#endif /* __CONFIG_EMF__ */

		/* Indicate interface create event to eapd */
		if (psta_if) {
			dprintf("hotplug_net(): send dif event to %s\n", interface);
			wl_send_dif_event(interface, 0);
			return 0;
		}

		/* Bridge WDS interfaces */
		if (!strncmp(lan_ifname, "br", 2) &&
#ifdef BCA_HNDROUTER
			eval("brctl", "addif", lan_ifname, interface) &&
#else
			eval("brctl", "addif", lan_ifname, interface, "wait") &&
#endif /* BCA_HNDROUTER */
			TRUE) {
			dprintf("hotplug_net():Adding interface %s\n", interface);
			return 0;
		}

#ifdef BCA_HNDROUTER
		wait_to_forward_state(interface);
#endif /* BCA_HNDROUTER */

		/* Inform driver to send up new WDS link event */
		if (wl_iovar_setint(interface, "wds_enable", 1)) {
			dprintf("%s set wds_enable failed\n", interface);
			return 0;
		}

		return 0;
	}

	if (remove_event) {
		/* Indicate interface delete event to eapd */
		wl_send_dif_event(interface, 1);

		/* For WDS interface get associated AP ifname and
		 * bridge for which associated AP is attached
		 */
		if (dyn_if && !wl_wlif_wds_ap_ifname(interface, wdsap_ifname)) {
			get_bridge_by_ifname(wdsap_ifname, &lan_ifname);
		}

#ifdef __CONFIG_EMF__
		if (nvram_match("emf_enable", "1"))
			eval("emf", "del", "iface", lan_ifname, interface);
#endif /* __CONFIG_EMF__ */
	}

	return 0;
}

#ifdef __CONFIG_NAT__
int
wan_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	if ((unit = ppp_ifunit(wan_ifname)) >= 0)
		return unit;
	else {
		for (unit = 0; unit < MAX_NVPARSE; unit ++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
				return unit;
		}
	}
	return -1;
}

int
preset_wan_routes(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	/* Set default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
		route_add(wan_ifname, 0, "0.0.0.0", "0.0.0.0", "0.0.0.0");

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);
	return 0;
}

int
wan_primary_ifunit(void)
{
	int unit;

	for (unit = 0; unit < MAX_NVPARSE; unit ++) {
		char tmp[100], prefix[] = "wanXXXXXXXXXX_";
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
			return unit;
	}

	return 0;
}
#endif	/* __CONFIG_NAT__ */

#if defined(BCA_HNDROUTER) && defined(PORT_BONDING)
/*
 * Broadcom Bonding Function
 *
 * NVRAM example for configuring LAN/WAN Bonding
 *
 * # Enable/disable LAN Bonding
 * bond_lan=1
 * bond_lan_ifnames=eth1 eth2 
 * bond_lan_ifnames=eth3 eth4 (AX11000)
 * # Enable/disable WAN Bonding
 * bond_wan=1
 * bond_wan_ifnames=eth0 eth4
 * bond_wan_ifnames=eth0 eth1 (AX11000)

 * # Disable Bonding Failsafe Feature, default is enabled
 * bonding_failsafe_disable=1
 *
 */

#define SYS_BONDING_IF			"/sys/class/net/%s/bonding/slaves"
#define LAN_BONDING_IFNAME		"bond0"
#define WAN_BONDING_IFNAME		"bond1"
#define MAX_BONDIF				4

static void
start_lan_bonding(void)
{
	char *bond_lan_ifnames;
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char *wan_ifname = nvram_safe_get("wan_ifname");
	char name[80], *next;
	char ifname[MAX_BONDIF][IFNAMSIZ];
	char confbuf[64] = {0};
	char cmdbuf[64] = {0};
	int i, count;

	/* Setup LAN bonding: bond0 */
	if (nvram_match("bond_lan", "1")) {
		bond_lan_ifnames = nvram_safe_get("bond_lan_ifnames");
		/* Set default bond_lan_ifnames if it doesn't set */
		if (strcmp(bond_lan_ifnames, "") == 0) {
#if defined (AX11000)
			nvram_set("bond_lan_ifnames", "eth3 eth4");
#else
			nvram_set("bond_lan_ifnames", "eth1 eth2");
#endif
			bond_lan_ifnames = nvram_safe_get("bond_lan_ifnames");
		}

		count = 0;
		foreach(name, bond_lan_ifnames, next) {
			if ((strncmp(name, "eth", 3) != 0) && (strncmp(name, wan_ifname, 4) == 0)) {
				fprintf(stderr, "%s can't be the interface for bonding\n", name);
				return;
			}
			strncpy(ifname[count], name, IFNAMSIZ);
			count++;
			if (count > MAX_BONDIF) {
				fprintf(stderr, "Too much port for LAN bonding: %d\n", count);
				return;
			}
		}

		for (i = 0; i < count; i++) {
			/* Bring down LAN interface */
			ifconfig(ifname[i], 0, NULL, NULL);

			eval("brctl", "delif", lan_ifname, ifname[i]);

			snprintf(confbuf, sizeof(confbuf), SYS_BONDING_IF, LAN_BONDING_IFNAME);
			snprintf(cmdbuf, sizeof(cmdbuf), "echo +%s > %s", ifname[i], confbuf);
			system(cmdbuf);

			ifconfig(ifname[i], IFUP | IFF_ALLMULTI, NULL, NULL);
		}
		ifconfig(LAN_BONDING_IFNAME, IFUP | IFF_ALLMULTI, NULL, NULL);
		eval("brctl", "addif", lan_ifname, LAN_BONDING_IFNAME);
	}

	return;
}

static void
stop_lan_bonding(void)
{
	FILE *fp;
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char bond_lan_ifnames[80];
	char name[80], *next;
	char ifname[MAX_BONDIF][IFNAMSIZ];
	char confbuf[64] = {0};
	char cmdbuf[64] = {0};
	int i, count;

	/* Stop LAN bonding: bond0 */
	snprintf(confbuf, sizeof(confbuf), SYS_BONDING_IF, LAN_BONDING_IFNAME);
	if (fp = fopen(confbuf, "r")) {
		memset(bond_lan_ifnames, 0, sizeof(bond_lan_ifnames));
		fread(bond_lan_ifnames, 1, sizeof(bond_lan_ifnames) - 1, fp);
		fclose(fp);

		if (strnlen(bond_lan_ifnames, sizeof(bond_lan_ifnames)) != 0){
			count = 0;
			bond_lan_ifnames[strnlen(bond_lan_ifnames, sizeof(bond_lan_ifnames)) - 1] = '\0';
			foreach(name, bond_lan_ifnames, next) {
				strncpy(ifname[count], name, IFNAMSIZ);
				count++;
			}

			/* Bring down bond interface */
			ifconfig(LAN_BONDING_IFNAME, 0, NULL, NULL);
			eval("brctl", "delif", lan_ifname, LAN_BONDING_IFNAME);
			for (i = 0; i < count; i++) {
				/* Bring down LAN interface */
				ifconfig(ifname[i], 0, NULL, NULL);

				snprintf(confbuf, sizeof(confbuf), SYS_BONDING_IF, LAN_BONDING_IFNAME);
				snprintf(cmdbuf, sizeof(cmdbuf), "echo -%s > %s", ifname[i], confbuf);
				system(cmdbuf);

				eval("brctl", "addif", lan_ifname, ifname[i]);
				ifconfig(ifname[i], IFUP | IFF_ALLMULTI, NULL, NULL);
			}
		}
	}

	return;
}

void
start_bonding(void)
{
	char *bond_wan_ifnames;
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char *wan_ifname = nvram_safe_get("wan_ifname");
	char *wan_ifnames = nvram_safe_get("wan_ifnames");
	char name[80], *next;
	char ifname[MAX_BONDIF][IFNAMSIZ];
	char confbuf[64] = {0};
	char cmdbuf[64] = {0};
	int i, count;

	/* Setup LAN bonding: bond0 */
	start_lan_bonding();

	/* Setup WAN bonding: bond1 */
	if (nvram_match("bond_wan", "1")) {
		bond_wan_ifnames = nvram_safe_get("bond_wan_ifnames");
		/* Set default bond_wan_ifnames if it doesn't set */
		if (strcmp(bond_wan_ifnames, "") == 0) {
#if defined(AX11000)			
			nvram_set("bond_wan_ifnames", "eth0 eth1");
#else
			nvram_set("bond_wan_ifnames", "eth0 eth4");
#endif
			bond_wan_ifnames = nvram_safe_get("bond_wan_ifnames");
			
			printf("\n\tDEBUG_WANAGG: %s(%d) bond_wan_ifnames=%s\n", __FUNCTION__, __LINE__, bond_wan_ifnames);
		}

		count = 0;
		foreach(name, bond_wan_ifnames, next) {
			strncpy(ifname[count], name, IFNAMSIZ);
			count++;
		}

		if (count != 2) {
			fprintf(stderr, "WAN bonding support two ports only: %d\n", count);
			return;
		}

		for (i = 0; i < count; i++) {
			ifconfig(ifname[i], 0, NULL, NULL);

			if (strncmp(ifname[i], wan_ifname, 4) != 0) {
				eval("brctl", "delif", lan_ifname, ifname[i]);
			}

			snprintf(confbuf, sizeof(confbuf), SYS_BONDING_IF, WAN_BONDING_IFNAME);
			snprintf(cmdbuf, sizeof(cmdbuf), "echo +%s > %s", ifname[i], confbuf);
			system(cmdbuf);

			ifconfig(ifname[i], IFUP, NULL, NULL);
		}
		ifconfig(WAN_BONDING_IFNAME, IFUP, NULL, NULL);

		if (nvram_match("wan_ifnames_bk", ""))
			nvram_set("wan_ifnames_bk", nvram_safe_get("wan_ifnames"));

		nvram_set("wan_ifnames", WAN_BONDING_IFNAME);
		nvram_set("wan0_ifnames", WAN_BONDING_IFNAME);
		nvram_set("wan_ifname", WAN_BONDING_IFNAME);
		nvram_set("wan0_ifname", WAN_BONDING_IFNAME);
	}

	return;
}

void
stop_bonding(void)
{
	FILE *fp;
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char *wan_ifnames_bk = nvram_safe_get("wan_ifnames_bk");
	char bond_wan_ifnames[80];
	char name[80], *next;
	char ifname[MAX_BONDIF][IFNAMSIZ];
	char confbuf[64] = {0};
	char cmdbuf[64] = {0};
	int i, count;

	/* Stop LAN bonding: bond0 */
	stop_lan_bonding();

	/* Stop WAN bonding: bond1 */
	snprintf(confbuf, sizeof(confbuf), SYS_BONDING_IF, WAN_BONDING_IFNAME);
	if (fp = fopen(confbuf, "r")) {
		memset(bond_wan_ifnames, 0, sizeof(bond_wan_ifnames));
		fread(bond_wan_ifnames, 1, sizeof(bond_wan_ifnames) - 1, fp);
		fclose(fp);

		if (strnlen(bond_wan_ifnames, sizeof(bond_wan_ifnames)) != 0){
			count = 0;
			bond_wan_ifnames[strnlen(bond_wan_ifnames, sizeof(bond_wan_ifnames)) - 1] = '\0';
			foreach(name, bond_wan_ifnames, next) {
				strncpy(ifname[count], name, IFNAMSIZ);
				count++;
			}

			/* Bring down bond interface */
			ifconfig(WAN_BONDING_IFNAME, 0, NULL, NULL);
			for (i = 0; i < count; i++) {
				/* Bring down WAN interfaces */
				ifconfig(ifname[i], 0, NULL, NULL);

				snprintf(confbuf, sizeof(confbuf), SYS_BONDING_IF, WAN_BONDING_IFNAME);
				snprintf(cmdbuf, sizeof(cmdbuf), "echo -%s > %s", ifname[i], confbuf);
				system(cmdbuf);

				if (strncmp(ifname[i], wan_ifnames_bk, 4) != 0)
					eval("brctl", "addif", lan_ifname, ifname[i]);

				ifconfig(ifname[i], IFUP | IFF_ALLMULTI, NULL, NULL);
			}

			nvram_set("wan_ifnames", wan_ifnames_bk);
			nvram_set("wan0_ifnames", wan_ifnames_bk);
			nvram_set("wan_ifname", wan_ifnames_bk);
			nvram_set("wan0_ifname", wan_ifnames_bk);
		}
	}

	return;
}

static bool
isLink(char *ifname)
{
	bool linkup = FALSE;
	int state = -1;
	int socId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	int rv;
	struct ifreq if_req;

	if (socId < 0)
		return linkup;

	strncpy(if_req.ifr_name, ifname, sizeof(if_req.ifr_name));
	rv = ioctl(socId, SIOCGIFFLAGS, &if_req);
	close(socId);

	if (rv == -1)
		dprintf("Ioctl failed: interface %s Errno = %d\n", ifname, errno);

	if ((if_req.ifr_flags & IFF_UP) && (if_req.ifr_flags & IFF_RUNNING))
		linkup = TRUE;

	return linkup;
}

static bool
is_allif_linkup(char *ifnames)
{
	char name[80], *next;

	foreach(name, ifnames, next) {
		if (!isLink(name))
			return FALSE;
	} /* foreach().... */

	return TRUE;
}

static int
get_linkup_if_count(char *ifnames)
{
	char name[80], *next;
	int if_count = 0;

	foreach(name, ifnames, next) {
		if (isLink(name))
			if_count++;
	} /* foreach().... */

	return if_count;
}

static char *
trim_white_space(char *str)
{
	char *end;

	// Trim leading space
	while(isspace(*str)) str++;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;

	// Write new null terminator
	*(end + 1) = 0;

	return str;
}

/*
 * Broadcom Bonding Failsafe Function
 *
 * NOTE:
 *    1. All connection will be broken when WAN bonding state changed,
 *       because we use "rc restart" to add/remove WAN bonding.
 */

#define LAN_BONDING_PROC		"/proc/net/bonding/bond0"
#define WAN_BONDING_PROC		"/proc/net/bonding/bond1"
#define BONDING_TIMEOUT			3
#define MAX_PROC_LINE_CHARS		160

#define rc_restart()			kill(1, SIGHUP)
enum {
	AR_START,
	AR_CHECKING,
	AR_LINK_CHANGE_S1,
	AR_LINK_CHANGE_S2,
	AR_IDLE,
};
static int bond_lan_stat = AR_START, bond_wan_stat = AR_START;
static int lan_bonding_timer = 0, wan_bonding_timer = 0;

void
bonding_failsafe(void)
{
	char line[MAX_PROC_LINE_CHARS];
	char name[80], *next;
	char *bond_lan_ifnames = nvram_safe_get("bond_lan_ifnames");
	char *bond_wan_ifnames = nvram_safe_get("bond_wan_ifnames");
	bool is_agg;
	FILE *proc_fp;

	/* check LAN bonding */
	if (!strcmp(nvram_safe_get("bond_lan"), "1"))
	{
		switch(bond_lan_stat) {
		case AR_START:
			lan_bonding_timer = 0;
			if (!is_allif_linkup(bond_lan_ifnames))
				break;
			bond_lan_stat = AR_CHECKING;
			/* Fall through */
		case AR_CHECKING:
			lan_bonding_timer++;
			/* check LAN binding status, /proc/net/binding/bondx */
			proc_fp = fopen(LAN_BONDING_PROC, "r");
			if (proc_fp == NULL)
				goto lan_bond_exit;

			is_agg = FALSE;
			while(fgets(line, MAX_PROC_LINE_CHARS, proc_fp)) {
				char buf_var[MAX_PROC_LINE_CHARS];
				char buf_val[MAX_PROC_LINE_CHARS];
				if(sscanf(line, "%[^:]:%[^\n]", buf_var, buf_val) == 2) {
					char *tok_var = trim_white_space(buf_var);
					char *tok_val = trim_white_space(buf_val);

					if (strncmp(tok_var, "MII Status", sizeof("MII Status")) == 0) {
						if (strncmp(tok_val, "up", sizeof("up")) != 0)
							break;
					}
					if (strncmp(tok_var, "Number of ports", sizeof("Number of ports")) == 0) {
						if (strtoul(tok_val, NULL, 0) != get_linkup_if_count(bond_lan_ifnames))
							break;
					}
					if (strncmp(tok_var, "Partner Mac Address", sizeof("Partner Mac Address")) == 0) {
						if (strncmp(tok_val, "00:00:00:00:00:00", sizeof("00:00:00:00:00:00")) != 0)
							is_agg = TRUE;

						break;		/* stop to lookup other bonding proc */
					}
				}
			} /* end while() */
			close(proc_fp);

			/* decide to remove bonding or not */
			if (!is_agg) {
				if (lan_bonding_timer == BONDING_TIMEOUT) {
					/*remove lan bonding */
					stop_lan_bonding();

					/* LAN bonding check finished, set stats to AR_LINK_CHANGE_S1 */
					bond_lan_stat = AR_LINK_CHANGE_S1;
					lan_bonding_timer = 0;
				}
			} else {
				/* LAN bonding check finished, set stats to AR_IDLE */
				bond_lan_stat = AR_IDLE;
				lan_bonding_timer = 0;
			}
			break;
		case AR_LINK_CHANGE_S1:
			/* Assume all bonding interfaces doesn't unplug this time.
			 * Waiting for all bonding interfaces linkup after stop_lan_bonding() is called
			 */
			if (!is_allif_linkup(bond_lan_ifnames))
				break;

			bond_lan_stat = AR_LINK_CHANGE_S2;
			/* Fall through */
		case AR_LINK_CHANGE_S2:
			/* Waiting for one of LAN bonding interface unplug to trigger bonding re-start */
			if (is_allif_linkup(bond_lan_ifnames))
				break;

			start_lan_bonding();
			bond_lan_stat = AR_IDLE;
			break;
		case AR_IDLE:
			if (!is_allif_linkup(bond_lan_ifnames))
				bond_lan_stat = AR_START;
			break;
		}
	}
lan_bond_exit:

	/* check WAN bonding */
	if (!strcmp(nvram_safe_get("bond_wan"), "1"))
	{
		switch(bond_wan_stat) {
		case AR_START:
			wan_bonding_timer = 0;
			if (!is_allif_linkup(bond_wan_ifnames))
				break;
			bond_wan_stat = AR_CHECKING;
			/* Fall through */
		case AR_CHECKING:
			wan_bonding_timer++;
			/* check WAN binding status, /proc/net/binding/bondx */
			proc_fp = fopen(WAN_BONDING_PROC, "r");
			if (proc_fp == NULL)
				goto wan_bond_exit;

			is_agg = FALSE;
			while(fgets(line, MAX_PROC_LINE_CHARS, proc_fp)) {
				char buf_var[MAX_PROC_LINE_CHARS];
				char buf_val[MAX_PROC_LINE_CHARS];
				if(sscanf(line, "%[^:]:%[^\n]", buf_var, buf_val) == 2) {
					char *tok_var = trim_white_space(buf_var);
					char *tok_val = trim_white_space(buf_val);

					if (strncmp(tok_var, "MII Status", sizeof("MII Status")) == 0) {
						if (strncmp(tok_val, "up", sizeof("up")) != 0)
							break;
					}
					if (strncmp(tok_var, "Number of ports", sizeof("Number of ports")) == 0) {
						if (strtoul(tok_val, NULL, 0) != get_linkup_if_count(bond_wan_ifnames))
							break;
					}
					if (strncmp(tok_var, "Partner Mac Address", sizeof("Partner Mac Address")) == 0) {
						if (strncmp(tok_val, "00:00:00:00:00:00", sizeof("00:00:00:00:00:00")) != 0)
							is_agg = TRUE;

						break;		/* stop to lookup other bonding proc */
					}
				}
			} /* end while() */
			close(proc_fp);

			/* decide to remove bonding or not */
			if (!is_agg) {
				if (wan_bonding_timer == BONDING_TIMEOUT) {
					/* remove wan bonding */
					nvram_set("bond_wan", "0");
					nvram_set("bond_wan_prestat", "1");
					rc_restart();

					/* WAN bonding check finished, set stats to AR_IDLE */
					bond_wan_stat = AR_IDLE;
					wan_bonding_timer = 0;
				}
			} else {
				/* WAN bonding check finished, set stats to AR_IDLE */
				bond_wan_stat = AR_IDLE;
				wan_bonding_timer = 0;
			}
			break;
		case AR_IDLE:
			if (!is_allif_linkup(bond_wan_ifnames))
				bond_wan_stat = AR_START;
			break;
		}
	} else if (!strcmp(nvram_safe_get("bond_wan_prestat"), "1")) {

		switch(bond_wan_stat) {
		case AR_START:
			/* Assume all bonding interfaces doesn't unplug this time.
			 * Waiting for all bonding interfaces linkup after WAN bonding is disabled
			 */
			wan_bonding_timer = 0;
			if (!is_allif_linkup(bond_wan_ifnames))
				break;
			bond_wan_stat = AR_CHECKING;
			/* Fall through */
		case AR_CHECKING:
			/* Waiting for one of WAN bonding interface unplug to trigger enable WAN bonding */
			if (is_allif_linkup(bond_wan_ifnames))
				break;

			/* add wan bonding */
			nvram_set("bond_wan", "1");
			nvram_unset("bond_wan_prestat");
			rc_restart();
			bond_wan_stat = AR_START;
			break;
		case AR_IDLE:
			if (!is_allif_linkup(bond_wan_ifnames))
				bond_wan_stat = AR_START;
			break;
		}
	}
wan_bond_exit:

	return;
}
#endif /* BCA_HNDROUTER && PORT_BONDING */
/* foxconn added start, wklin, 10/17/2006 */
void stop_wlan(void)
{ 
    /* Foxconn modified start pling 11/26/2009 */
    /* Should store the nvram value in a local variable, instead
     *  of keeping just the pointer, since other processes
     *  might modify NVRAM at any time.
     */

	char lan_ifname[32];
	char wlif[32];
    strcpy(lan_ifname, nvram_safe_get("lan_ifname"));
    strcpy(wlif, nvram_safe_get("wl0_ifname"));
    /* Foxconn modified end pling 11/26/2009 */    
	
	eval("wlconf", wlif, "down");
	ifconfig(wlif, 0, NULL, NULL);
	eval("brctl", "delif", lan_ifname, wlif);

    /* Bring down 2nd WLAN i/f */
    /* Foxconn modified start pling 12/02/2009 */
    //wlif = nvram_safe_get("wl1_ifname");
    strcpy(wlif, nvram_safe_get("wl1_ifname"));
    /* Foxconn modified end pling 12/02/2009 */
	eval("wlconf", wlif, "down");
	ifconfig(wlif, 0, NULL, NULL);
	eval("brctl", "delif", lan_ifname, wlif);

#if defined(INCLULDE_2ND_5G_RADIO)
    strcpy(wlif, nvram_safe_get("wl2_ifname"));
    /* Foxconn modified end pling 12/02/2009 */
	eval("wlconf", wlif, "down");
	ifconfig(wlif, 0, NULL, NULL);
	eval("brctl", "delif", lan_ifname, wlif);
#endif
    /* Foxconn added start pling 06/06/2007 */
//#ifdef BUILD_TWC
/* Foxconn add start by aspen Bai, 11/13/2008 */
#ifdef MULTIPLE_SSID
/* Foxconn add end by aspen Bai, 11/13/2008 */
    if (1)  /* Remove all BSSIDs from LAN */
    {
        int bssid_num;
        for (bssid_num=1; bssid_num<=3; bssid_num++)
        {
            char if_name[16];
            sprintf(if_name, "wl0.%d", bssid_num);
            ifconfig(if_name, 0, NULL, NULL);
    	    eval("brctl", "delif", lan_ifname, if_name);
        }
        for (bssid_num=1; bssid_num<=3; bssid_num++)
        {
            char if_name_5g[16];
            sprintf(if_name_5g, "wl1.%d", bssid_num);
            ifconfig(if_name_5g, 0, NULL, NULL);
    	    eval("brctl", "delif", lan_ifname, if_name_5g);
        }
#if defined(INCLULDE_2ND_5G_RADIO)
        for (bssid_num=1; bssid_num<=3; bssid_num++)
        {
            char if_name_5g[16];
            sprintf(if_name_5g, "wl2.%d", bssid_num);
            ifconfig(if_name_5g, 0, NULL, NULL);
    	    eval("brctl", "delif", lan_ifname, if_name_5g);
        }
#endif        
    }	
#endif
    /* Foxconn added end pling 06/06/2007 */

	return;
}

#ifdef VLAN_SUPPORT
void add_if_to_vlan_group(char *guest_if)
{
    int br_num;
    char lanxx_ifnames[64];
    char *lan_ifnames;
    
    for(br_num=1; br_num < MAX_NO_BRIDGE; br_num++)
    {
        sprintf(lanxx_ifnames,"lan%d_ifnames",br_num);
        lan_ifnames=nvram_safe_get(lanxx_ifnames);
        if(strlen(lan_ifnames))
            if(strstr(lan_ifnames,guest_if))
            {
                char bridge[32];
                sprintf(bridge,"br%d",br_num);
                eval("brctl", "delif", acosNvramConfig_get("lan_ifname"), guest_if);
                eval("brctl", "addif", bridge, guest_if);
            }
            	
    }
    
    if(br_num==MAX_NO_BRIDGE)
    {
        eval("brctl", "delif", acosNvramConfig_get("lan_ifname"), guest_if);
        eval("brctl", "addif", "br0", guest_if);
    }    
}
#endif

void start_wlan(void)
{
    /* Foxconn modified start pling 11/26/2009 */
    /* Should store the nvram value in a local variable, instead
     *  of keeping just the pointer, since other processes
     *  might modify NVRAM at any time.
     */
	char lan_ifname[32],wan_ifname[32];
	char wlif[32];
    strcpy(lan_ifname, nvram_safe_get("lan_ifname"));
    strcpy(wan_ifname, nvram_safe_get("wan_ifname"));
    strcpy(wlif, nvram_safe_get("wl0_ifname"));
    /* Foxconn modified end pling 11/26/2009 */
    char wl1_ifname[32];
#if defined(INCLULDE_2ND_5G_RADIO)
    char wl2_ifname[32];
#endif

    strcpy(wl1_ifname, nvram_safe_get("wl1_ifname"));

#if defined(INCLULDE_2ND_5G_RADIO)
    strcpy(wl2_ifname, nvram_safe_get("wl2_ifname"));
#endif

    /* Foxconn added start, Wins, 05/07/11, @RU_IPTV */
#ifdef CONFIG_RUSSIA_IPTV
    char iptv_intf[32];
#if defined(R8000) || defined(BCA_HNDROUTER)
    unsigned int iptv_intf_val = 0x00;
#else
    unsigned char iptv_intf_val = 0x00;
#endif
    int ru_iptv_en = 0;
    int wlan1_en = 0;
    int wlan2_en = 0;
#if defined(INCLULDE_2ND_5G_RADIO)
    int wlan3_en = 0;
#endif
    char mac[6];
    char *mac_ptr;
    char guest_mac[6];

#ifndef BCA_HNDROUTER /* no need to set mac address from HND Router. It will be configured by wlconf */
    mac_ptr=nvram_get("0:macaddr");
    sscanf(mac_ptr,"%02x:%02x:%02x:%02x:%02x:%02x",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
    mac[0] |= 0x2;
    sprintf(guest_mac,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvram_set("wl0.1_hwaddr",guest_mac);
    
    mac_ptr=nvram_get("1:macaddr");
    sscanf(mac_ptr,"%02x:%02x:%02x:%02x:%02x:%02x",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
    mac[0] |= 0x2;
    sprintf(guest_mac,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvram_set("wl1.1_hwaddr",guest_mac);
    mac_ptr=nvram_get("2:macaddr");
    sscanf(mac_ptr,"%02x:%02x:%02x:%02x:%02x:%02x",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
    mac[0] |= 0x2;
    sprintf(guest_mac,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvram_set("wl2.1_hwaddr",guest_mac);
#endif /* !BCA_HNDROUTER */    

    if (nvram_match(NVRAM_IPTV_ENABLED, "1") || nvram_match("enable_vlan", "enable"))
    {
        strcpy(iptv_intf, nvram_get(NVRAM_IPTV_INTF));
#if defined(R8000) || defined(BCA_HNDROUTER)
        sscanf(iptv_intf, "0x%04X", &iptv_intf_val);
#else
        sscanf(iptv_intf, "0x%02X", &iptv_intf_val);
#endif
        if (iptv_intf_val & IPTV_WLAN1)
            wlan1_en = 1;
        /* Foxconn modified start pling 04/20/2012 */
        /* WLAN1 and WLAN2 can both bridge to WAN */
        //else if (iptv_intf_val & IPTV_WLAN2)
        if (iptv_intf_val & IPTV_WLAN2)
        /* Foxconn modified end pling 04/20/2012 */
            wlan2_en = 1;

#if defined(INCLULDE_2ND_5G_RADIO)  
        if (iptv_intf_val & IPTV_WLAN3)
            wlan3_en = 1;
#endif

        ru_iptv_en = 1;
    }
#endif /* CONFIG_RUSSIA_IPTV */
    /* Foxconn added end, Wins, 05/07/11, @RU_IPTV */
    eval("wlconf", wlif, "up");
    //eval("wlconf", wlif, "start");    /* Bob removed 08/08/2014, no need to start here, start here will cause unexpected behavior of acsd. Start in start_wl after acsd */
    ifconfig(wlif, IFUP, NULL, NULL);

    /*foxconn Han edited, 01/24/2019 bring down guest wifi at beginning*/
    if((!strcmp(wlif,"wl0.1")) && nvram_match("wla_sec_profile_enable_2","0"))
    {
        cprintf("%s(%d) bring down wl0.1 (wl -i wl0.1 bss down)\n",__func__,__LINE__);
        system("wl -i wl0.1 bss down");
    }

    /* Foxconn modified start, Wins, 05/07/11, @RU_IPTV */
#ifdef CONFIG_RUSSIA_IPTV
    if(!nvram_match("enable_vlan", "enable"))
    {
        if (wlan1_en)
        {
            eval("brctl", "delif", lan_ifname, wlif);   /* pling added 04/03/2012 */
            eval("brctl", "addif", "br1", wlif);
        }
        else
        {
            eval("brctl", "delif", "br1", wlif);        /* pling added 04/03/2012 */
            //if(!nvram_match("gmac3_enable", "1"))
            //{
          	    eval("brctl", "addif", lan_ifname, wlif);
            //}
        }
    }
#ifdef VLAN_SUPPORT
    else
    	add_if_to_vlan_group(wlif);
#endif
    	
#else /* CONFIG_RUSSIA_IPTV */
	eval("brctl", "addif", lan_ifname, wlif);
#endif /* CONFIG_RUSSIA_IPTV */
    /* Foxconn modified end, Wins, 05/07/11, @RU_IPTV */


    /*foxconn Han edited, 06/20/2014*/
    #ifdef CONFIG_2ND_5G_BRIDGE_MODE
    if(nvram_match("bridge_interface_eth2_down","1"))
    { 
        cprintf("\n%s %s %d bridge_interface_eth2_down == 1, keep eth2 down \n",__func__,__FILE__,__LINE__);
    }
    else
    #endif /*CONFIG_2ND_5G_BRIDGE_MODE*/
    {
    eval("wlconf", wl1_ifname, "up");
    //eval("wlconf", wl1_ifname, "start");  /* Bob removed 08/08/2014, no need to start here, start here will cause unexpected behavior of acsd. Start in start_wl after acsd */
    }
    ifconfig(wl1_ifname, IFUP, NULL, NULL);

    /*foxconn Han edited, 01/24/2019 bring down guest wifi at beginning*/
    if((!strcmp(wl1_ifname,"wl1.1")) && nvram_match("wlg_sec_profile_enable_2","0"))
    {
        cprintf("%s(%d) bring down wl1.1 (wl -i wl1.1 bss down)\n",__func__,__LINE__);
        system("wl -i wl1.1 bss down");
    }

    /* Foxconn modified start, Wins, 05/07/11, @RU_IPTV */
#ifdef CONFIG_RUSSIA_IPTV
    if(!nvram_match("enable_vlan", "enable"))
    {
        if (wlan2_en)
        {
            eval("brctl", "delif", lan_ifname, wl1_ifname);/* pling added 04/03/2012 */
            eval("brctl", "addif", "br1", wl1_ifname);
        }
        else
        {
            eval("brctl", "delif", "br1", wl1_ifname);  /* pling added 04/03/2012 */
            //if(!nvram_match("gmac3_enable", "1"))
            //{
                eval("brctl", "addif", lan_ifname, wl1_ifname);
            //}
        }
    }
#ifdef VLAN_SUPPORT
    else
    	add_if_to_vlan_group(wl1_ifname);
#endif
#else /* CONFIG_RUSSIA_IPTV */
	eval("brctl", "addif", lan_ifname, wl1_ifname);
#endif /* CONFIG_RUSSIA_IPTV */
    /* Foxconn modified end, Wins, 05/07/11, @RU_IPTV */


#if defined(INCLULDE_2ND_5G_RADIO)
    /*foxconn Han edited, 06/20/2014*/
    #ifdef CONFIG_2ND_5G_BRIDGE_MODE
    if(nvram_match("bridge_interface_eth3_down","1"))
    { 
        cprintf("\n%s %s %d bridge_interface_eth3_down == 1, keep eth3 down \n",__func__,__FILE__,__LINE__);
    }
    else
    #endif /*CONFIG_2ND_5G_BRIDGE_MODE*/
    {
    eval("wlconf", wl2_ifname, "up");
    //eval("wlconf", wl2_ifname, "start");      /* Bob removed 08/08/2014, no need to start here, start here will cause unexpected behavior of acsd. Start in start_wl after acsd */
    }
    ifconfig(wl2_ifname, IFUP, NULL, NULL);

    /*foxconn Han edited, 01/24/2019 bring down guest wifi at beginning*/
    if((!strcmp(wl2_ifname,"wl2.1")) && nvram_match("wlh_sec_profile_enable_2","0"))
    {
        cprintf("%s(%d) bring down wl2.1 (wl -i wl2.1 bss down)\n",__func__,__LINE__);
        system("wl -i wl2.1 bss down");
    }

    /* Foxconn modified start, Wins, 05/07/11, @RU_IPTV */
#ifdef CONFIG_RUSSIA_IPTV
    if(!nvram_match("enable_vlan", "enable"))
    {
        if (wlan3_en)
        {
            eval("brctl", "delif", lan_ifname, wl2_ifname);/* pling added 04/03/2012 */
            eval("brctl", "addif", "br1", wl2_ifname);
        }
        else
        {
            eval("brctl", "delif", "br1", wl2_ifname);  /* pling added 04/03/2012 */
            //if(!nvram_match("gmac3_enable", "1"))
            //{
                eval("brctl", "addif", lan_ifname, wl2_ifname);
            //}
        }
    }
#ifdef VLAN_SUPPORT
    else
    	add_if_to_vlan_group(wl2_ifname);
#endif
#else /* CONFIG_RUSSIA_IPTV */
	eval("brctl", "addif", lan_ifname, wl2_ifname);
#endif /* CONFIG_RUSSIA_IPTV */
    /* Foxconn modified end, Wins, 05/07/11, @RU_IPTV */
#endif

    /* Foxocnn added start pling 03/30/2010 */
    /* For WiFi test case 4.2.41 */
    //if(nvram_match("wifi_test", "1"))
    /*Foxconn add start by Hank 08/14/2012*/
	/*change obss_coex by user selection*/
	if(nvram_match("wl0_obss_coex","0"))
        eval("wl", "-i", wlif, "obss_coex", "0");
#ifdef BCA_HNDROUTER
    /* disable obss_coex if wifi mode is set to HT20 */
    else if(nvram_match("wl0_bw_cap", "1"))
        eval("wl", "-i", wlif, "obss_coex", "0");
#endif        
	else
		eval("wl", "-i", wlif, "obss_coex", "1");
	/*Foxconn add start by Hank 08/14/2012*/
    /* Foxocnn added end pling 03/30/2010 */

    /* Foxconn added start pling 06/06/2007 */
//#ifdef BUILD_TWC
/* Foxconn add start by aspen Bai, 11/13/2008 */
#ifdef MULTIPLE_SSID
/* Foxconn add end by aspen Bai, 11/13/2008 */
    if (1)      /* Add the additional BSSIDs to LAN */
    {
        int bssid_num;
        for (bssid_num=1; bssid_num<=3; bssid_num++)
        {
            //char param_name[16];
            char param_name[20];/*foxconn modified water, @multi-ssid not workable..*/
            char if_name[16];
            sprintf(param_name, "wl0.%d_bss_enabled", bssid_num);
            sprintf(if_name, "wl0.%d", bssid_num);
            if (nvram_match(param_name, "1"))
            {
                wl_vif_hwaddr_set(if_name);
                ifconfig(if_name, IFUP, NULL, NULL);
#ifdef VLAN_SUPPORT
                if(nvram_match("enable_vlan", "enable"))
                    add_if_to_vlan_group(if_name);
                else if(nvram_match(NVRAM_IPTV_ENABLED, "1") && (bssid_num==1))
                {
                    if (iptv_intf_val & IPTV_WLAN_GUEST1)
      	                eval("brctl", "addif", wan_ifname, if_name);
  	                else
  	                    eval("brctl", "addif", lan_ifname, if_name);
                        
                }
                else
  	                eval("brctl", "addif", lan_ifname, if_name);
#else
  	                eval("brctl", "addif", lan_ifname, if_name);
#endif  	                
            }
        }
        for (bssid_num=1; bssid_num<=3; bssid_num++)
        {
            char param_name_5g[32]; // Foxconn modified pling 10/06/2010, 16->32
            char if_name_5g[16];
            sprintf(param_name_5g, "wl1.%d_bss_enabled", bssid_num);
            sprintf(if_name_5g, "wl1.%d", bssid_num);
            if (nvram_match(param_name_5g, "1"))
            {
                wl_vif_hwaddr_set(if_name_5g);
                ifconfig(if_name_5g, IFUP, NULL, NULL);
#ifdef VLAN_SUPPORT
                if(nvram_match("enable_vlan", "enable"))
                    add_if_to_vlan_group(if_name_5g);
                else if(nvram_match(NVRAM_IPTV_ENABLED, "1") && (bssid_num==1))
                {
                    if (iptv_intf_val & IPTV_WLAN_GUEST2)
      	                eval("brctl", "addif", wan_ifname, if_name_5g);
  	                else
  	                    eval("brctl", "addif", lan_ifname, if_name_5g);
                        
                }
                else
   	                eval("brctl", "addif", lan_ifname, if_name_5g);
#else
                eval("brctl", "addif", lan_ifname, if_name_5g);
#endif   	                
            }
        }
#if defined(INCLULDE_2ND_5G_RADIO)
        for (bssid_num=1; bssid_num<=3; bssid_num++)
        {
            char param_name_5g[32]; // Foxconn modified pling 10/06/2010, 16->32
            char if_name_5g[16];
            sprintf(param_name_5g, "wl2.%d_bss_enabled", bssid_num);
            sprintf(if_name_5g, "wl2.%d", bssid_num);
            if (nvram_match(param_name_5g, "1"))
            {
                wl_vif_hwaddr_set(if_name_5g);
                ifconfig(if_name_5g, IFUP, NULL, NULL);
#ifdef VLAN_SUPPORT
                if(nvram_match("enable_vlan", "enable"))
                    add_if_to_vlan_group(if_name_5g);
                else if(nvram_match(NVRAM_IPTV_ENABLED, "1") && (bssid_num==1))
                {
                    if (iptv_intf_val & IPTV_WLAN_GUEST3)
      	                eval("brctl", "addif", wan_ifname, if_name_5g);
  	                else
  	                    eval("brctl", "addif", lan_ifname, if_name_5g);
                        
                }
                else
                    eval("brctl", "addif", lan_ifname, if_name_5g);
#else
                eval("brctl", "addif", lan_ifname, if_name_5g);
#endif                    
            }
        }
#endif        
    }
#endif
    /* Foxconn added end pling 06/06/2007 */

    /* Foxconn, add by MJ., for wifi cert. */
    //if(!nvram_match("wifi_test", "1"))
        eval("wl", "-i", wl1_ifname, "obss_coex", "0"); /* foxconn added, zacker, 01/05/2011 */
#if defined(INCLULDE_2ND_5G_RADIO)
        eval("wl", "-i", wl2_ifname, "obss_coex", "0"); /* foxconn added, zacker, 01/05/2011 */
#endif

    /* foxconn added start, zacker, 11/01/2010 */
    {
        int s;
        struct ifreq ifr;
        unsigned char hwaddr[ETHER_ADDR_LEN] = "";

        ether_atoe(nvram_safe_get("lan_hwaddr"), hwaddr);
        if (memcmp(hwaddr, "\0\0\0\0\0\0", ETHER_ADDR_LEN) &&
            (s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
            strncpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
            ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
            memcpy(ifr.ifr_hwaddr.sa_data, hwaddr, ETHER_ADDR_LEN);
            ioctl(s, SIOCSIFHWADDR, &ifr);
            close(s);
        }
    }
    /* foxconn added end, zacker, 11/01/2010 */

	return;
}
/* foxconn added end, wklin, 10/17/2006 */