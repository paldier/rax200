#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h> /* for open */
#include <string.h>
#include <sys/klog.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/utsname.h> /* for uname */
#include <net/if_arp.h>
#include <dirent.h>

#include <epivers.h>
#include <router_version.h>
#include <mtd.h>
#include <shutils.h>
#include <rc.h>
#include <netconf.h>
#include <nvparse.h>
#include <bcmdevs.h>
#include <bcmparams.h>
#include <bcmnvram.h>
#include <wlutils.h>
#include <ezc.h>
#include <pmon.h>
#include <bcmconfig.h>
#include <confmtd_utils.h>
#include <linux/version.h>
#include "ambitCfg.h"
#ifdef BCA_HNDROUTER
#include "ambitCfg.h"
#endif

#if defined(__CONFIG_WAPI__) || defined(__CONFIG_WAPI_IAS__)
#include <wapi_path.h>
#endif // endif
#if defined(__CONFIG_CIFS__)
#include <cifs_path.h>
#endif // endif

#ifdef __BRCM_GENERIC_IQOS__
#include "bcmIqosDef.h"
#endif
#include <etutils.h>

#ifdef BCA_HNDROUTER
#include <cms_image.h>
#include <bcm_imgif.h>
#include <bcm/bcmswapitypes.h>
#include <bcmnet.h>
#endif /* BCA_HNDROUTER */

/* foxconn added start, zacker, 09/17/2009, @wps_led */
#include <fcntl.h>
#include <wps_led.h>
/* foxconn added end, zacker, 09/17/2009, @wps_led */

/*fxcn added by dennis start,05/03/2012, fixed guest network can't reconnect issue*/
#define MAX_BSSID_NUM       4
#define MIN_BSSID_NUM       2
/*fxcn added by dennis end,05/03/2012, fixed guest network can't reconnect issue*/

#ifdef __CONFIG_NAT__
static void auto_bridge(void);
#endif	/* __CONFIG_NAT__ */

#include <sys/sysinfo.h> /* foxconn wklin added */
#ifdef __CONFIG_EMF__
extern void load_emf(void);
#endif /* __CONFIG_EMF__ */

#ifdef __CONFIG_DHDAP__
#define MAX_FW_PATH	512
#endif /* __CONFIG_DHDAP__ */

static void restore_defaults(void);
static void sysinit(void);
static void rc_signal(int sig);
/* Foxconn added start, Wins, 05/16/2011, @RU_IPTV */
#if defined(CONFIG_RUSSIA_IPTV)
static int is_russia_specific_support (void);
static int is_china_specific_support (void); /* Foxconn add, Edward zhang, 09/05/2012, @add IPTV support for PR SKU*/
#endif /* CONFIG_RUSSIA_IPTV */
/* Foxconn added end, Wins, 05/16/2011, @RU_IPTV */
/*Foxconn add start, edward zhang, 2013/07/03*/
#ifdef VLAN_SUPPORT
static int getVlanname(char vlanname[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE]);
int getVlanRule(vlan_rule vlan[C_MAX_VLAN_RULE]);/* Foxconn modified by Max Ding, 11/22/2016 remove static, need use it in interface.c */
static int getTokens(char *str, char *delimiter, char token[][C_MAX_TOKEN_SIZE], int maxNumToken);
#endif
/*Foxconn add end, edward zhang, 2013/07/03*/
#ifdef BCA_HNDROUTER
static int nvram_erase();
static int bca_sys_upgrade(const char *path);
static int foxconn_sys_upgrade(const char *path, int size, int offset);
#endif /* BCA_HNDROUTER */

extern struct nvram_tuple router_defaults[];

#define RESTORE_DEFAULTS() \
	(!nvram_match("restore_defaults", "0") || nvram_invmatch("os_name", "linux"))

	
#define RESTORE_DEFAULTS() (!nvram_match("restore_defaults", "0") || nvram_invmatch("os_name", "linux"))

/* for WCN support, Foxconn added start by EricHuang, 12/13/2006 */
void convert_wlan_params_for_wps(void)
{
    int config_flag = 0; /* Foxconn add, Tony W.Y. Wang, 01/06/2010 */

    /* Foxconn added start pling 03/05/2010 */
    /* Added for dual band WPS req. for WNDR3400 */
#define MAX_SSID_LEN    32
    char wl0_ssid[64], wl1_ssid[64];
#if defined(INCLULDE_2ND_5G_RADIO)
    char wl2_ssid[64];
#endif
    /* first check how we arrived here?
     * 1. or by "add WPS client" in unconfigured state.
     * 2. by external register configure us, 
     */
    strcpy(wl0_ssid, nvram_safe_get("wl0_ssid"));
    strcpy(wl1_ssid, nvram_safe_get("wl1_ssid"));
#if defined(INCLULDE_2ND_5G_RADIO)
    strcpy(wl2_ssid, nvram_safe_get("wl2_ssid"));
#endif

printf("%s(%d) wps_start=%s, wps_pbc_conn_success=%s\n",__func__,__LINE__,nvram_get("wps_start")?:"",nvram_get("wps_pbc_conn_success")?:"");
printf("%s(%d) wps_currentRFband=%s\n",__func__,__LINE__,nvram_get("wps_currentRFband")?:"");
printf("%s(%d) wl0_ssid=%s, wl1_ssid=%s\n",__func__,__LINE__,wl0_ssid,wl1_ssid);

    /* Foxconn modified, Tony W.Y. Wang, 03/24/2010 @WPS random ssid setting */
    if (!nvram_match("wps_start", "none") || nvram_match("wps_pbc_conn_success", "1"))
    {
        /* case 1 above, either via pbc, or gui */
        /* In this case, the WPS set both SSID to be
         *  either "NTRG-2.4G_xxx" or "NTRG-5G_xxx".
         * We need to set proper SSID for each radio.
         */
#define RANDOM_SSID_2G  "NTGR-2.4G_"
#define RANDOM_SSID_5G  "NTGR-5G_"

#if defined(INCLULDE_2ND_5G_RADIO)
#define RANDOM_SSID_5G_1  "NTGR-5G-1_"
#define RANDOM_SSID_5G_2  "NTGR-5G-2_"
#endif

        /* Foxconn modified start pling 05/23/2012 */
        /* Fix a issue where 2.4G radio is disabled, 
         * router uses incorrect random ssid */
        /* if (strncmp(wl0_ssid, RANDOM_SSID_2G, strlen(RANDOM_SSID_2G)) == 0) */
        if (strncmp(wl0_ssid, RANDOM_SSID_2G, strlen(RANDOM_SSID_2G)) == 0 && !nvram_match("wl0_radio","0") && nvram_match("wps_currentRFband", "1"))
        {
            printf("Random ssid 2.4G\n");
            /* Set correct ssid for 5G */
#if defined(INCLULDE_2ND_5G_RADIO)
            sprintf(wl1_ssid, "%s%s", RANDOM_SSID_5G_1, 
                    &wl0_ssid[strlen(RANDOM_SSID_2G)]);
#else
            sprintf(wl1_ssid, "%s%s", RANDOM_SSID_5G, 
                    &wl0_ssid[strlen(RANDOM_SSID_2G)]);
#endif
            nvram_set("wl1_ssid", wl1_ssid);
#if defined(INCLULDE_2ND_5G_RADIO)
            sprintf(wl2_ssid, "%s%s", RANDOM_SSID_5G_2, 
                    &wl0_ssid[strlen(RANDOM_SSID_2G)]);
            nvram_set("wl2_ssid", wl2_ssid);
            if (nvram_match("wl_5g_bandsteering", "1"))
                nvram_set("wl2_ssid", wl1_ssid);
#endif

            nvram_set("wl1_wpa_psk", nvram_safe_get("wl0_wpa_psk"));
            nvram_set("wl1_akm", nvram_safe_get("wl0_akm"));
            nvram_set("wl1_crypto", nvram_safe_get("wl0_crypto"));
            #if defined(INCLULDE_2ND_5G_RADIO)
            nvram_set("wl2_wpa_psk", nvram_safe_get("wl0_wpa_psk"));
            nvram_set("wl2_akm", nvram_safe_get("wl0_akm"));
            nvram_set("wl2_crypto", nvram_safe_get("wl0_crypto"));
            #endif
        }
        else
            if (strncmp(wl1_ssid, RANDOM_SSID_5G, strlen(RANDOM_SSID_5G)) == 0 && !nvram_match("wl1_radio","0") 
                    && nvram_match("wps_currentRFband", "2") 
                    #if defined(INCLULDE_2ND_5G_RADIO)
                    && nvram_match("wl2_radio", "0") 
                    #endif
                    )
            {
                printf("Random ssid 5G\n");
                /* Set correct ssid for 2.4G */
                sprintf(wl0_ssid, "%s%s", RANDOM_SSID_2G, 
                        &wl1_ssid[strlen(RANDOM_SSID_5G)]);
                nvram_set("wl0_ssid", wl0_ssid);
#if defined(INCLULDE_2ND_5G_RADIO)
                sprintf(wl2_ssid, "%s%s", RANDOM_SSID_5G_2, 
                        &wl1_ssid[strlen(RANDOM_SSID_5G)]);
                nvram_set("wl2_ssid", wl2_ssid);
                if (nvram_match("wl_5g_bandsteering", "1"))
                    nvram_set("wl2_ssid", wl1_ssid);
#endif

                nvram_set("wl0_wpa_psk", nvram_safe_get("wl1_wpa_psk"));
                nvram_set("wl0_akm", nvram_safe_get("wl1_akm"));
                nvram_set("wl0_crypto", nvram_safe_get("wl1_crypto"));
                #if defined(INCLULDE_2ND_5G_RADIO)
                nvram_set("wl2_wpa_psk", nvram_safe_get("wl1_wpa_psk"));
                nvram_set("wl2_akm", nvram_safe_get("wl1_akm"));
                nvram_set("wl2_crypto", nvram_safe_get("wl1_crypto"));
                #endif
            }
#if defined(INCLULDE_2ND_5G_RADIO)
            else
                if (strncmp(wl2_ssid, RANDOM_SSID_5G, strlen(RANDOM_SSID_5G)) == 0 && !nvram_match("wl2_radio","0")
                        && nvram_match("wps_currentRFband", "2"))
                {
                    printf("Random ssid 5G_2\n");
                    /* Set correct ssid for 2.4G */
                    sprintf(wl0_ssid, "%s%s", RANDOM_SSID_2G, 
                            &wl2_ssid[strlen(RANDOM_SSID_5G)]);
                    nvram_set("wl0_ssid", wl0_ssid);
                    sprintf(wl1_ssid, "%s%s", RANDOM_SSID_5G_1, 
                            &wl2_ssid[strlen(RANDOM_SSID_5G)]);
                    nvram_set("wl1_ssid", wl1_ssid);

                    if (nvram_match("wl_5g_bandsteering", "1"))
                        nvram_set("wl1_ssid", wl2_ssid);

                    nvram_set("wl0_wpa_psk", nvram_safe_get("wl2_wpa_psk"));
                    nvram_set("wl0_akm", nvram_safe_get("wl2_akm"));
                    nvram_set("wl0_crypto", nvram_safe_get("wl2_crypto"));
                    nvram_set("wl1_wpa_psk", nvram_safe_get("wl2_wpa_psk"));
                    nvram_set("wl1_akm", nvram_safe_get("wl2_akm"));
                    nvram_set("wl1_crypto", nvram_safe_get("wl2_crypto"));

                }
#endif
        nvram_unset("wps_pbc_conn_success");
    }
    else
    {
        /* case 2 */
        /* now check whether external register is from:
         * 1. UPnP,
         * 2. 2.4GHz radio
         * 3. 5GHz radio
         * 4. 5GHz radio 2
         */
        if (nvram_match("wps_is_upnp", "1"))
        {
            /* Case 1: UPnP: wired registrar */
            /* SSID for both interface should be same already.
             * So nothing to do.
             */
            printf("Wired External registrar!\n");
        }
        else
            if (nvram_match("wps_currentRFband", "1"))
            {
                /* Case 2: 2.4GHz radio */
                /* Need to add "-5G" to the SSID of the 5GHz band */
                char ssid_suffix[] = "-5G";
#if defined(INCLULDE_2ND_5G_RADIO) 
                char ssid_suffix_2[] = "-5G-2";
#endif
                if (MAX_SSID_LEN - strlen(wl0_ssid) >= strlen(ssid_suffix))
                {
                    printf("2.4G Wireless External registrar 1!\n");
                    /* SSID is not long, so append suffix to wl1_ssid */
                    sprintf(wl1_ssid, "%s%s", wl0_ssid, ssid_suffix);
                }
                else
                {
                    printf("2.4G Wireless External registrar 2!\n");
                    /* SSID is too long, so replace last few chars of ssid
                     * with suffix
                     */
                    strcpy(wl1_ssid, wl0_ssid);
                    strcpy(&wl1_ssid[MAX_SSID_LEN - strlen(ssid_suffix)], ssid_suffix);
                }
                #if defined(INCLULDE_2ND_5G_RADIO)
                if (MAX_SSID_LEN - strlen(wl0_ssid) >= strlen(ssid_suffix_2))
                {
                    printf("2.4G Wireless External registrar 1!\n");
                    /* SSID is not long, so append suffix to wl1_ssid */
                    sprintf(wl2_ssid, "%s%s", wl0_ssid, ssid_suffix_2);
                }
                else
                {
                    printf("2.4G Wireless External registrar 2!\n");
                    /* SSID is too long, so replace last few chars of ssid
                     * with suffix
                     */
                    strcpy(wl2_ssid, wl0_ssid);
                    strcpy(&wl2_ssid[MAX_SSID_LEN - strlen(ssid_suffix_2)], ssid_suffix_2);
                }
                #endif
                if (strlen(wl1_ssid) > MAX_SSID_LEN)
                    printf("Error wl1_ssid too long (%d)!\n", strlen(wl1_ssid));

#if defined(INCLULDE_2ND_5G_RADIO)
                if (strlen(wl2_ssid) > MAX_SSID_LEN)
                    printf("Error wl2_ssid too long (%d)!\n", strlen(wl2_ssid));
#endif
                nvram_set("wl1_ssid", wl1_ssid);
#if defined(INCLULDE_2ND_5G_RADIO)
                if (nvram_match("wl_5g_bandsteering", "1"))
                    nvram_set("wl2_ssid", wl1_ssid);
                else
                    nvram_set("wl2_ssid", wl2_ssid);
#endif

                nvram_set("wl1_wpa_psk", nvram_safe_get("wl0_wpa_psk"));
                nvram_set("wl1_akm", nvram_safe_get("wl0_akm"));
                nvram_set("wl1_crypto", nvram_safe_get("wl0_crypto"));
#if defined(INCLULDE_2ND_5G_RADIO)  /* Foxconn Bob added start 09/29/2014, must sync wifi security as well since WPS of 5G radio 1 is disabled. */
                nvram_set("wl2_wpa_psk", nvram_safe_get("wl0_wpa_psk"));
                nvram_set("wl2_akm", nvram_safe_get("wl0_akm"));
                nvram_set("wl2_crypto", nvram_safe_get("wl0_crypto"));
#endif
            }
#if defined(INCLULDE_2ND_5G_RADIO) 
            else
                if (nvram_match("wps_currentRFband", "2"))
                {
                    /* Case 2: 5GHz radio */
                    /* Need to add "-2.4G" to the SSID of the 2.4GHz band */

                    if (nvram_match("wl2_radio", "1"))
                    {
                        /*wps is done with 5G radio 2 */
                        char ssid_suffix[] = "-2.4G";
                        char ssid_suffix_2[] = "-5G-1";

                        if (MAX_SSID_LEN - strlen(wl2_ssid) >= strlen(ssid_suffix))
                        {
                            printf("5G Wireless External registrar 1!\n");
                            /* SSID is not long, so append suffix to wl1_ssid */
                            sprintf(wl0_ssid, "%s%s", wl2_ssid, ssid_suffix);
                        }
                        else
                        {
                            printf("5G Wireless External registrar 2!\n");
                            /* Replace last few chars ssid with suffix */
                            /* SSID is too long, so replace last few chars of ssid
                             * with suffix
                             */
                            strcpy(wl0_ssid, wl2_ssid);
                            strcpy(&wl0_ssid[MAX_SSID_LEN - strlen(ssid_suffix)], ssid_suffix);
                        }
#if (defined R8000) || defined(BCA_HNDROUTER)
                        if (MAX_SSID_LEN - strlen(wl2_ssid) >= strlen(ssid_suffix_2))
                        {
                            printf("5G Wireless External registrar 1!\n");
                            /* SSID is not long, so append suffix to wl1_ssid */
                            sprintf(wl1_ssid, "%s%s", wl2_ssid, ssid_suffix_2);
                        }
                        else
                        {
                            printf("5G Wireless External registrar 2!\n");
                            /* Replace last few chars ssid with suffix */
                            /* SSID is too long, so replace last few chars of ssid
                             * with suffix
                             */
                            strcpy(wl1_ssid, wl2_ssid);
                            strcpy(&wl1_ssid[MAX_SSID_LEN - strlen(ssid_suffix_2)], ssid_suffix_2);
                        }
#endif
                        nvram_set("wl0_ssid", wl0_ssid);
#if (defined R8000) || defined(BCA_HNDROUTER)
                        if (nvram_match("wl_5g_bandsteering", "1"))
                            nvram_set("wl1_ssid", wl2_ssid);
                        else
                            nvram_set("wl1_ssid", wl1_ssid);
#endif            
#if defined(R8000) || defined(BCA_HNDROUTER)  /* Foxconn Bob added start 09/29/2014, must sync wifi security as well since WPS of 5G radio 1 is disabled. */
                        nvram_set("wl1_wpa_psk", nvram_safe_get("wl2_wpa_psk"));
                        nvram_set("wl1_akm", nvram_safe_get("wl2_akm"));
                        nvram_set("wl1_crypto", nvram_safe_get("wl2_crypto"));
                        nvram_set("wl0_wpa_psk", nvram_safe_get("wl2_wpa_psk"));
                        nvram_set("wl0_akm", nvram_safe_get("wl2_akm"));
                        nvram_set("wl0_crypto", nvram_safe_get("wl2_crypto"));
#endif
                    }
                    else if (nvram_match("wl2_radio", "0"))
                    {
                        /*wps is done with 5G radio 1 */
                        char ssid_suffix[] = "-2.4G";
                        char ssid_suffix_2[] = "-5G-2";

                        if (MAX_SSID_LEN - strlen(wl1_ssid) >= strlen(ssid_suffix))
                        {
                            printf("5G Wireless External registrar 1!\n");
                            /* SSID is not long, so append suffix to wl1_ssid */
                            sprintf(wl0_ssid, "%s%s", wl1_ssid, ssid_suffix);
                        }
                        else
                        {
                            printf("5G Wireless External registrar 2!\n");
                            /* Replace last few chars ssid with suffix */
                            /* SSID is too long, so replace last few chars of ssid
                             * with suffix
                             */
                            strcpy(wl0_ssid, wl1_ssid);
                            strcpy(&wl0_ssid[MAX_SSID_LEN - strlen(ssid_suffix)], ssid_suffix);
                        }
#if (defined R8000) || defined(BCA_HNDROUTER)
                        if (MAX_SSID_LEN - strlen(wl1_ssid) >= strlen(ssid_suffix_2))
                        {
                            printf("5G Wireless External registrar 1!\n");
                            /* SSID is not long, so append suffix to wl1_ssid */
                            sprintf(wl2_ssid, "%s%s", wl1_ssid, ssid_suffix_2);
                        }
                        else
                        {
                            printf("5G Wireless External registrar 2!\n");
                            /* Replace last few chars ssid with suffix */
                            /* SSID is too long, so replace last few chars of ssid
                             * with suffix
                             */
                            strcpy(wl2_ssid, wl1_ssid);
                            strcpy(&wl2_ssid[MAX_SSID_LEN - strlen(ssid_suffix_2)], ssid_suffix_2);
                        }
#endif
                        nvram_set("wl0_ssid", wl0_ssid);
                        if (nvram_match("wl_5g_bandsteering", "1"))
                            nvram_set("wl2_ssid", wl1_ssid);
                        else
                            nvram_set("wl2_ssid", wl2_ssid);

#if defined(R8000) || defined(BCA_HNDROUTER)  /* Foxconn Bob added start 09/29/2014, must sync wifi security as well since WPS of 5G radio 1 is disabled. */
                        nvram_set("wl2_wpa_psk", nvram_safe_get("wl1_wpa_psk"));
                        nvram_set("wl2_akm", nvram_safe_get("wl1_akm"));
                        nvram_set("wl2_crypto", nvram_safe_get("wl1_crypto"));
                        nvram_set("wl0_wpa_psk", nvram_safe_get("wl1_wpa_psk"));
                        nvram_set("wl0_akm", nvram_safe_get("wl1_akm"));
                        nvram_set("wl0_crypto", nvram_safe_get("wl1_crypto"));
#endif
                    }
                }
#endif /*  defined(INCLULDE_2ND_5G_RADIO) */
                else
                    printf("Error! unknown external register!\n");
    }
    /* Foxconn added end pling 03/05/2010 */

    nvram_set("wla_ssid", nvram_safe_get("wl0_ssid"));
    nvram_set("wla_temp_ssid", nvram_safe_get("wl0_ssid"));

    if ( strncmp(nvram_safe_get("wl0_akm"), "psk psk2", 7) == 0 )
    {
        nvram_set("wla_secu_type", "WPA-AUTO-PSK");
        nvram_set("wla_temp_secu_type", "WPA-AUTO-PSK");
        nvram_set("wla_passphrase", nvram_safe_get("wl0_wpa_psk"));

        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */

        if (!nvram_match("wps_randomssid", "") ||
                !nvram_match("wps_randomkey", ""))
        {
            nvram_set("wla_secu_type", "WPA-AUTO-PSK");
            nvram_set("wla_temp_secu_type", "WPA-AUTO-PSK");

            nvram_set("wl0_akm", "psk psk2 ");
            nvram_set("wl0_crypto", "tkip+aes");

            nvram_set("wps_mixedmode", "2");
            //nvram_set("wps_randomssid", "");
            //nvram_set("wps_randomkey", "");
            config_flag = 1;
            /* Since we changed to mixed mode, 
             * so we need to disable WDS if it is already enabled
             */
            if (nvram_match("wla_wds_enable", "1"))
            {
                nvram_set("wla_wds_enable",  "0");
                nvram_set("wl0_wds", "");
                nvram_set("wl0_mode", "ap");
            }
        }
        else
        {
            /* Foxconn added start pling 02/25/2007 */
            /* Disable WDS if it is already enabled */
            if (nvram_match("wla_wds_enable", "1"))
            {
                nvram_set("wla_wds_enable",  "0");
                nvram_set("wl0_wds", "");
                nvram_set("wl0_mode", "ap");
            }
            /* Foxconn added end pling 02/25/2007 */
        }
    }
    else if ( strncmp(nvram_safe_get("wl0_akm"), "psk2", 4) == 0 )
    {
        nvram_set("wla_secu_type", "WPA2-PSK");
        nvram_set("wla_temp_secu_type", "WPA2-PSK");
        nvram_set("wla_passphrase", nvram_safe_get("wl0_wpa_psk"));


        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */
        /* Foxconn added start pling 06/15/2010 */
        if (nvram_match("wl0_crypto", "tkip"))
        {
            /* DTM fix: 
             * Registrar may set to WPA2-PSK TKIP mode.
             * In this case, don't try to modify the
             * security type.
             */
            nvram_unset("wps_mixedmode");
        }
        else
            /* Foxconn added end pling 06/15/2010 */
            if (!nvram_match("wps_randomssid", "") ||
                    !nvram_match("wps_randomkey", ""))
            {
                nvram_set("wla_secu_type", "WPA2-PSK");
                nvram_set("wla_temp_secu_type", "WPA2-PSK");

                nvram_set("wl0_akm", "psk2");
                nvram_set("wl0_crypto", "aes");

                nvram_set("wps_mixedmode", "2");
                //nvram_set("wps_randomssid", "");
                //nvram_set("wps_randomkey", "");
                config_flag = 1;
                /* Since we changed to mixed mode, 
                 * so we need to disable WDS if it is already enabled
                 */
                if (nvram_match("wla_wds_enable", "1"))
                {
                    nvram_set("wla_wds_enable",  "0");
                    nvram_set("wl0_wds", "");
                    nvram_set("wl0_mode", "ap");
                }
            }
            else
            {
                /* Foxconn added start pling 02/25/2007 */
                /* Disable WDS if it is already enabled */
                if (nvram_match("wla_wds_enable", "1"))
                {
                    nvram_set("wla_wds_enable",  "0");
                    nvram_set("wl0_wds", "");
                    nvram_set("wl0_mode", "ap");
                }
                /* Foxconn added end pling 02/25/2007 */
            }
    }
    else if ( strncmp(nvram_safe_get("wl0_akm"), "psk", 3) == 0 )
    {
        nvram_set("wla_secu_type", "WPA-PSK");
        nvram_set("wla_temp_secu_type", "WPA-PSK");
        nvram_set("wla_passphrase", nvram_safe_get("wl0_wpa_psk"));

        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */
        /* Foxconn added start pling 06/15/2010 */
        if (nvram_match("wl0_crypto", "aes"))
        {
            /* DTM fix: 
             * Registrar may set to WPA-PSK AES mode.
             * In this case, don't try to modify the
             * security type.
             */
            nvram_unset("wps_mixedmode");
        }
        else
            /* Foxconn added end pling 06/15/2010 */
            if (!nvram_match("wps_randomssid", "") ||
                    !nvram_match("wps_randomkey", ""))
            {
                /* Foxconn add start, Tony W.Y. Wang, 11/30/2009 */
                /* WiFi TKIP changes for WNDR3400*/
                /*
                   When external registrar configures our router as WPA-PSK [TKIP], security, 
                   we auto change the wireless mode to Up to 54Mbps. This should only apply to
                   router when router is in "WPS Unconfigured" state.
                   */
                nvram_set("wla_mode",  "g and b");

                /* Disable 11n support, copied from bcm_wlan_util.c */
                acosNvramConfig_set("wl_nmode", "0");
                acosNvramConfig_set("wl0_nmode", "0");

                acosNvramConfig_set("wl_gmode", "1");
                acosNvramConfig_set("wl0_gmode", "1");

                /* Set bandwidth to 20MHz */
#if ( !(defined BCM4718) && !(defined BCM4716) && !(defined R6300v2) && !defined(R6250) && !defined(R6200v2) && !defined(R7000) && !defined(R8000)) && !defined(BCA_HNDROUTER)
                acosNvramConfig_set("wl_nbw", "20");
                acosNvramConfig_set("wl0_nbw", "20");
#endif

                acosNvramConfig_set("wl_nbw_cap", "0");
                acosNvramConfig_set("wl0_nbw_cap", "0");

                /* Disable extension channel */
                acosNvramConfig_set("wl_nctrlsb", "none");
                acosNvramConfig_set("wl0_nctrlsb", "none");

                /* Now set the security */
                nvram_set("wla_secu_type", "WPA-PSK");
                nvram_set("wla_temp_secu_type", "WPA-PSK");

                nvram_set("wl0_akm", "psk ");
                nvram_set("wl0_crypto", "tkip");

                /*
                   nvram_set("wla_secu_type", "WPA-AUTO-PSK");
                   nvram_set("wla_temp_secu_type", "WPA-AUTO-PSK");

                   nvram_set("wl0_akm", "psk psk2 ");
                   nvram_set("wl0_crypto", "tkip+aes");
                   */
                /* Foxconn add end, Tony W.Y. Wang, 11/30/2009 */
                nvram_set("wps_mixedmode", "1");
                //nvram_set("wps_randomssid", "");
                //nvram_set("wps_randomkey", "");
                config_flag = 1;
                /* Since we changed to mixed mode, 
                 * so we need to disable WDS if it is already enabled
                 */
                if (nvram_match("wla_wds_enable", "1"))
                {
                    nvram_set("wla_wds_enable",  "0");
                    nvram_set("wl0_wds", "");
                    nvram_set("wl0_mode", "ap");
                }
            }
    }
    else if ( strncmp(nvram_safe_get("wl0_wep"), "enabled", 7) == 0 )
    {
        int key_len=0;
        if ( strncmp(nvram_safe_get("wl0_auth"), "1", 1) == 0 ) /*shared mode*/
        {
            nvram_set("wla_auth_type", "sharedkey");
            nvram_set("wla_temp_auth_type", "sharedkey");
        }
        else
        {
            nvram_set("wla_auth_type", "opensystem");
            nvram_set("wla_temp_auth_type", "opensystem");
        }

        nvram_set("wla_secu_type", "WEP");
        nvram_set("wla_temp_secu_type", "WEP");
        nvram_set("wla_defaKey", "0");
        nvram_set("wla_temp_defaKey", "0");
        /* Foxconn add start by aspen Bai, 02/24/2009 */
        /*
           nvram_set("wla_key1", nvram_safe_get("wl_key1"));
           nvram_set("wla_temp_key1", nvram_safe_get("wl_key1"));

           printf("wla_wep_length: %d\n", strlen(nvram_safe_get("wl_key1")));

           key_len = atoi(nvram_safe_get("wl_key1"));
           */
        nvram_set("wla_key1", nvram_safe_get("wl0_key1"));
        nvram_set("wla_temp_key1", nvram_safe_get("wl0_key1"));

        printf("wla_wep_length: %d\n", strlen(nvram_safe_get("wl0_key1")));

        key_len = strlen(nvram_safe_get("wl0_key1"));
        /* Foxconn add end by aspen Bai, 02/24/2009 */
        if (key_len==5 || key_len==10)
        {
            nvram_set("wla_wep_length", "1");
        }
        else
        {
            nvram_set("wla_wep_length", "2");
        }
        /* Foxconn add start by aspen Bai, 02/24/2009 */
        if (key_len==5 || key_len==13)
        {
            char HexKeyArray[32];
            char key[32], tmp[32];
            int i;

            strcpy(key, nvram_safe_get("wl0_key1"));
            memset(HexKeyArray, 0, sizeof(HexKeyArray));
            for (i=0; i<key_len; i++)
            {
                sprintf(tmp, "%02X", (unsigned char)key[i]);
                strcat(HexKeyArray, tmp);
            }
            printf("ASCII WEP key (%s) convert -> HEX WEP key (%s)\n", key, HexKeyArray);

            nvram_set("wla_key1", HexKeyArray);
            nvram_set("wla_temp_key1", HexKeyArray);
        }
        /* Foxconn add end by aspen Bai, 02/24/2009 */
    }
    else
    {
        nvram_set("wla_secu_type", "None");
        nvram_set("wla_temp_secu_type", "None");
        nvram_set("wla_passphrase", "");
    }
    /* Foxconn add start, Tony W.Y. Wang, 11/23/2009 */
    nvram_set("wlg_ssid", nvram_safe_get("wl1_ssid"));
    nvram_set("wlg_temp_ssid", nvram_safe_get("wl1_ssid"));

    if ( strncmp(nvram_safe_get("wl1_akm"), "psk psk2", 7) == 0 )
    {
        nvram_set("wlg_secu_type", "WPA-AUTO-PSK");
        nvram_set("wlg_temp_secu_type", "WPA-AUTO-PSK");
        nvram_set("wlg_passphrase", nvram_safe_get("wl1_wpa_psk"));

        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */

        if (!nvram_match("wps_randomssid", "") ||
                !nvram_match("wps_randomkey", ""))
        {
            nvram_set("wlg_secu_type", "WPA-AUTO-PSK");
            nvram_set("wlg_temp_secu_type", "WPA-AUTO-PSK");

            nvram_set("wl1_akm", "psk psk2 ");
            nvram_set("wl1_crypto", "tkip+aes");

            nvram_set("wps_mixedmode", "2");
            //nvram_set("wps_randomssid", "");
            //nvram_set("wps_randomkey", "");
            config_flag = 1;
            /* Since we changed to mixed mode, 
             * so we need to disable WDS if it is already enabled
             */
            if (nvram_match("wlg_wds_enable", "1"))
            {
                nvram_set("wlg_wds_enable",  "0");
                nvram_set("wl1_wds", "");
                nvram_set("wl1_mode", "ap");
            }
        }
        else
        {
            /* Foxconn added start pling 02/25/2007 */
            /* Disable WDS if it is already enabled */
            if (nvram_match("wlg_wds_enable", "1"))
            {
                nvram_set("wlg_wds_enable",  "0");
                nvram_set("wl1_wds", "");
                nvram_set("wl1_mode", "ap");
            }
            /* Foxconn added end pling 02/25/2007 */
        }
    }
    else if ( strncmp(nvram_safe_get("wl1_akm"), "psk2", 4) == 0 )
    {
        nvram_set("wlg_secu_type", "WPA2-PSK");
        nvram_set("wlg_temp_secu_type", "WPA2-PSK");
        nvram_set("wlg_passphrase", nvram_safe_get("wl1_wpa_psk"));


        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */

        /* Foxconn added start pling 06/15/2010 */
        if (nvram_match("wl1_crypto", "tkip"))
        {
            /* DTM fix: 
             * Registrar may set to WPA2-PSK TKIP mode.
             * In this case, don't try to modify the
             * security type.
             */
            nvram_unset("wps_mixedmode");
        }
        else
            /* Foxconn added end pling 06/15/2010 */
            if (!nvram_match("wps_randomssid", "") ||
                    !nvram_match("wps_randomkey", ""))
            {
                nvram_set("wlg_secu_type", "WPA2-PSK");
                nvram_set("wlg_temp_secu_type", "WPA2-PSK");

                nvram_set("wl1_akm", "psk2");
                nvram_set("wl1_crypto", "aes");

                nvram_set("wps_mixedmode", "2");
                //nvram_set("wps_randomssid", "");
                //nvram_set("wps_randomkey", "");
                config_flag = 1;
                /* Since we changed to mixed mode, 
                 * so we need to disable WDS if it is already enabled
                 */
                if (nvram_match("wlg_wds_enable", "1"))
                {
                    nvram_set("wlg_wds_enable",  "0");
                    nvram_set("wl1_wds", "");
                    nvram_set("wl1_mode", "ap");
                }
            }
            else
            {
                /* Foxconn added start pling 02/25/2007 */
                /* Disable WDS if it is already enabled */
                if (nvram_match("wlg_wds_enable", "1"))
                {
                    nvram_set("wlg_wds_enable",  "0");
                    nvram_set("wl1_wds", "");
                    nvram_set("wl1_mode", "ap");
                }
                /* Foxconn added end pling 02/25/2007 */
            }
    }
    else if ( strncmp(nvram_safe_get("wl1_akm"), "psk", 3) == 0 )
    {
        nvram_set("wlg_secu_type", "WPA-PSK");
        nvram_set("wlg_temp_secu_type", "WPA-PSK");
        nvram_set("wlg_passphrase", nvram_safe_get("wl1_wpa_psk"));

        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */

        /* Foxconn added start pling 06/15/2010 */
        if (nvram_match("wl1_crypto", "aes"))
        {
            /* DTM fix: 
             * Registrar may set to WPA-PSK AES mode.
             * In this case, don't try to modify the
             * security type.
             */
            nvram_unset("wps_mixedmode");
        }
        else
            /* Foxconn added end pling 06/15/2010 */
            if (!nvram_match("wps_randomssid", "") ||
                    !nvram_match("wps_randomkey", ""))
            {
                /* Foxconn add start, Tony W.Y. Wang, 11/30/2009 */
                /* WiFi TKIP changes for WNDR3400*/
                /*
                   When external registrar configures our router as WPA-PSK [TKIP], security, 
                   we auto change the wireless mode to Up to 54Mbps. This should only apply to
                   router when router is in "WPS Unconfigured" state.
                   */
                nvram_set("wlg_mode",  "g and b");

                /* Disable 11n support, copied from bcm_wlan_util.c */
                acosNvramConfig_set("wl1_nmode", "0");

                acosNvramConfig_set("wl1_gmode", "1");

                /* Set bandwidth to 20MHz */
#if ( !(defined BCM4718) && !(defined BCM4716) && !(defined R6300v2) && !defined(R6250) && !defined(R6200v2) && !defined(R7000) && !defined(R8000)) && !defined(BCA_HNDROUTER)
                acosNvramConfig_set("wl1_nbw", "20");
#endif

                acosNvramConfig_set("wl1_nbw_cap", "0");

                /* Disable extension channel */
                acosNvramConfig_set("wl1_nctrlsb", "none");

                /* Now set the security */
                nvram_set("wlg_secu_type", "WPA-PSK");
                nvram_set("wlg_temp_secu_type", "WPA-PSK");

                nvram_set("wl1_akm", "psk ");
                nvram_set("wl1_crypto", "tkip");
                /*
                   nvram_set("wlg_secu_type", "WPA-AUTO-PSK");
                   nvram_set("wlg_temp_secu_type", "WPA-AUTO-PSK");

                   nvram_set("wl1_akm", "psk psk2 ");
                   nvram_set("wl1_crypto", "tkip+aes");
                   */
                /* Foxconn add end, Tony W.Y. Wang, 11/30/2009 */
                nvram_set("wps_mixedmode", "1");
                //nvram_set("wps_randomssid", "");
                //nvram_set("wps_randomkey", "");
                config_flag = 1;
                /* Since we changed to mixed mode, 
                 * so we need to disable WDS if it is already enabled
                 */
                if (nvram_match("wlg_wds_enable", "1"))
                {
                    nvram_set("wlg_wds_enable",  "0");
                    nvram_set("wl1_wds", "");
                    nvram_set("wl1_mode", "ap");
                }
            }
    }
    else if ( strncmp(nvram_safe_get("wl1_wep"), "enabled", 7) == 0 )
    {
        int key_len=0;
        if ( strncmp(nvram_safe_get("wl1_auth"), "1", 1) == 0 ) /*shared mode*/
        {
            nvram_set("wlg_auth_type", "sharedkey");
            nvram_set("wlg_temp_auth_type", "sharedkey");
        }
        else
        {
            nvram_set("wlg_auth_type", "opensystem");
            nvram_set("wlg_temp_auth_type", "opensystem");
        }

        nvram_set("wlg_secu_type", "WEP");
        nvram_set("wlg_temp_secu_type", "WEP");
        nvram_set("wlg_defaKey", "0");
        nvram_set("wlg_temp_defaKey", "0");
        /* Foxconn add start by aspen Bai, 02/24/2009 */
        /*
           nvram_set("wla_key1", nvram_safe_get("wl_key1"));
           nvram_set("wla_temp_key1", nvram_safe_get("wl_key1"));

           printf("wla_wep_length: %d\n", strlen(nvram_safe_get("wl_key1")));

           key_len = atoi(nvram_safe_get("wl_key1"));
           */
        nvram_set("wlg_key1", nvram_safe_get("wl1_key1"));
        nvram_set("wlg_temp_key1", nvram_safe_get("wl1_key1"));

        printf("wlg_wep_length: %d\n", strlen(nvram_safe_get("wl1_key1")));

        key_len = strlen(nvram_safe_get("wl1_key1"));
        /* Foxconn add end by aspen Bai, 02/24/2009 */
        if (key_len==5 || key_len==10)
        {
            nvram_set("wlg_wep_length", "1");
        }
        else
        {
            nvram_set("wlg_wep_length", "2");
        }
        /* Foxconn add start by aspen Bai, 02/24/2009 */
        if (key_len==5 || key_len==13)
        {
            char HexKeyArray[32];
            char key[32], tmp[32];
            int i;

            strcpy(key, nvram_safe_get("wl1_key1"));
            memset(HexKeyArray, 0, sizeof(HexKeyArray));
            for (i=0; i<key_len; i++)
            {
                sprintf(tmp, "%02X", (unsigned char)key[i]);
                strcat(HexKeyArray, tmp);
            }
            printf("ASCII WEP key (%s) convert -> HEX WEP key (%s)\n", key, HexKeyArray);

            nvram_set("wlg_key1", HexKeyArray);
            nvram_set("wlg_temp_key1", HexKeyArray);
        }
        /* Foxconn add end by aspen Bai, 02/24/2009 */
    }
    else
    {
        nvram_set("wlg_secu_type", "None");
        nvram_set("wlg_temp_secu_type", "None");
        nvram_set("wlg_passphrase", "");
    }

#if defined(INCLULDE_2ND_5G_RADIO)
    nvram_set("wlh_ssid", nvram_safe_get("wl2_ssid"));
    nvram_set("wlh_temp_ssid", nvram_safe_get("wl2_ssid"));

    if ( strncmp(nvram_safe_get("wl2_akm"), "psk psk2", 7) == 0 )
    {
        nvram_set("wlh_secu_type", "WPA-AUTO-PSK");
        nvram_set("wlh_temp_secu_type", "WPA-AUTO-PSK");
        nvram_set("wlh_passphrase", nvram_safe_get("wl2_wpa_psk"));

        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */

        if (!nvram_match("wps_randomssid", "") ||
                !nvram_match("wps_randomkey", ""))
        {
            nvram_set("wlh_secu_type", "WPA-AUTO-PSK");
            nvram_set("wlh_temp_secu_type", "WPA-AUTO-PSK");

            nvram_set("wl2_akm", "psk psk2 ");
            nvram_set("wl2_crypto", "tkip+aes");

            nvram_set("wps_mixedmode", "2");
            //nvram_set("wps_randomssid", "");
            //nvram_set("wps_randomkey", "");
            config_flag = 1;
            /* Since we changed to mixed mode, 
             * so we need to disable WDS if it is already enabled
             */
            if (nvram_match("wlh_wds_enable", "1"))
            {
                nvram_set("wlh_wds_enable",  "0");
                nvram_set("wl2_wds", "");
                nvram_set("wl2_mode", "ap");
            }
        }
        else
        {
            /* Foxconn added start pling 02/25/2007 */
            /* Disable WDS if it is already enabled */
            if (nvram_match("wlh_wds_enable", "1"))
            {
                nvram_set("wlh_wds_enable",  "0");
                nvram_set("wl2_wds", "");
                nvram_set("wl2_mode", "ap");
            }
            /* Foxconn added end pling 02/25/2007 */
        }
    }
    else if ( strncmp(nvram_safe_get("wl2_akm"), "psk2", 4) == 0 )
    {
        nvram_set("wlh_secu_type", "WPA2-PSK");
        nvram_set("wlh_temp_secu_type", "WPA2-PSK");
        nvram_set("wlh_passphrase", nvram_safe_get("wl2_wpa_psk"));


        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */

        /* Foxconn added start pling 06/15/2010 */
        if (nvram_match("wl2_crypto", "tkip"))
        {
            /* DTM fix: 
             * Registrar may set to WPA2-PSK TKIP mode.
             * In this case, don't try to modify the
             * security type.
             */
            nvram_unset("wps_mixedmode");
        }
        else
            /* Foxconn added end pling 06/15/2010 */
            if (!nvram_match("wps_randomssid", "") ||
                    !nvram_match("wps_randomkey", ""))
            {
                nvram_set("wlh_secu_type", "WPA2-PSK");
                nvram_set("wlh_temp_secu_type", "WPA2-PSK");

                nvram_set("wl2_akm", "psk2");
                nvram_set("wl2_crypto", "aes");

                nvram_set("wps_mixedmode", "2");
                //nvram_set("wps_randomssid", "");
                //nvram_set("wps_randomkey", "");
                config_flag = 1;
                /* Since we changed to mixed mode, 
                 * so we need to disable WDS if it is already enabled
                 */
                if (nvram_match("wlh_wds_enable", "1"))
                {
                    nvram_set("wlh_wds_enable",  "0");
                    nvram_set("wl2_wds", "");
                    nvram_set("wl2_mode", "ap");
                }
            }
            else
            {
                /* Foxconn added start pling 02/25/2007 */
                /* Disable WDS if it is already enabled */
                if (nvram_match("wlh_wds_enable", "1"))
                {
                    nvram_set("wlh_wds_enable",  "0");
                    nvram_set("wl2_wds", "");
                    nvram_set("wl2_mode", "ap");
                }
                /* Foxconn added end pling 02/25/2007 */
            }
    }
    else if ( strncmp(nvram_safe_get("wl2_akm"), "psk", 3) == 0 )
    {
        nvram_set("wlh_secu_type", "WPA-PSK");
        nvram_set("wlh_temp_secu_type", "WPA-PSK");
        nvram_set("wlh_passphrase", nvram_safe_get("wl2_wpa_psk"));

        /* If router changes from 'unconfigured' to 'configured' state by
         * adding a WPS client, the wsc_randomssid and wsc_randomkey will
         * be set. In this case, router should use mixedmode security.
         */

        /* Foxconn added start pling 06/15/2010 */
        if (nvram_match("wl2_crypto", "aes"))
        {
            /* DTM fix: 
             * Registrar may set to WPA-PSK AES mode.
             * In this case, don't try to modify the
             * security type.
             */
            nvram_unset("wps_mixedmode");
        }
        else
            /* Foxconn added end pling 06/15/2010 */
            if (!nvram_match("wps_randomssid", "") ||
                    !nvram_match("wps_randomkey", ""))
            {
                /* Foxconn add start, Tony W.Y. Wang, 11/30/2009 */
                /* WiFi TKIP changes for WNDR3400*/
                /*
                   When external registrar configures our router as WPA-PSK [TKIP], security, 
                   we auto change the wireless mode to Up to 54Mbps. This should only apply to
                   router when router is in "WPS Unconfigured" state.
                   */
                nvram_set("wlh_mode",  "g and b");

                /* Disable 11n support, copied from bcm_wlan_util.c */
                acosNvramConfig_set("wl2_nmode", "0");

                acosNvramConfig_set("wl2_gmode", "1");

                /* Set bandwidth to 20MHz */
                acosNvramConfig_set("wl2_nbw", "20");

                acosNvramConfig_set("wl2_nbw_cap", "0");

                /* Disable extension channel */
                acosNvramConfig_set("wl2_nctrlsb", "none");

                /* Now set the security */
                nvram_set("wlh_secu_type", "WPA-PSK");
                nvram_set("wlh_temp_secu_type", "WPA-PSK");

                nvram_set("wlh_akm", "psk ");
                nvram_set("wlh_crypto", "tkip");

                nvram_set("wps_mixedmode", "1");
                //nvram_set("wps_randomssid", "");
                //nvram_set("wps_randomkey", "");
                config_flag = 1;
                /* Since we changed to mixed mode, 
                 * so we need to disable WDS if it is already enabled
                 */
                if (nvram_match("wlg_wds_enable", "1"))
                {
                    nvram_set("wlg_wds_enable",  "0");
                    nvram_set("wl2_wds", "");
                    nvram_set("wl2_mode", "ap");
                }
            }
    }
    else if ( strncmp(nvram_safe_get("wl2_wep"), "enabled", 7) == 0 )
    {
        int key_len=0;
        if ( strncmp(nvram_safe_get("wl2_auth"), "1", 1) == 0 ) /*shared mode*/
        {
            nvram_set("wlh_auth_type", "sharedkey");
            nvram_set("wlh_temp_auth_type", "sharedkey");
        }
        else
        {
            nvram_set("wlh_auth_type", "opensystem");
            nvram_set("wlh_temp_auth_type", "opensystem");
        }

        nvram_set("wlh_secu_type", "WEP");
        nvram_set("wlh_temp_secu_type", "WEP");
        nvram_set("wlh_defaKey", "0");
        nvram_set("wlh_temp_defaKey", "0");
        /* Foxconn add start by aspen Bai, 02/24/2009 */
        /*
           nvram_set("wla_key1", nvram_safe_get("wl_key1"));
           nvram_set("wla_temp_key1", nvram_safe_get("wl_key1"));

           printf("wla_wep_length: %d\n", strlen(nvram_safe_get("wl_key1")));

           key_len = atoi(nvram_safe_get("wl_key1"));
           */
        nvram_set("wlh_key1", nvram_safe_get("wl2_key1"));
        nvram_set("wlh_temp_key1", nvram_safe_get("wl2_key1"));

        printf("wlh_wep_length: %d\n", strlen(nvram_safe_get("wl2_key1")));

        key_len = strlen(nvram_safe_get("wl2_key1"));
        /* Foxconn add end by aspen Bai, 02/24/2009 */
        if (key_len==5 || key_len==10)
        {
            nvram_set("wlh_wep_length", "1");
        }
        else
        {
            nvram_set("wlh_wep_length", "2");
        }
        /* Foxconn add start by aspen Bai, 02/24/2009 */
        if (key_len==5 || key_len==13)
        {
            char HexKeyArray[32];
            char key[32], tmp[32];
            int i;

            strcpy(key, nvram_safe_get("wl2_key1"));
            memset(HexKeyArray, 0, sizeof(HexKeyArray));
            for (i=0; i<key_len; i++)
            {
                sprintf(tmp, "%02X", (unsigned char)key[i]);
                strcat(HexKeyArray, tmp);
            }
            printf("ASCII WEP key (%s) convert -> HEX WEP key (%s)\n", key, HexKeyArray);

            nvram_set("wlh_key1", HexKeyArray);
            nvram_set("wlh_temp_key1", HexKeyArray);
        }
        /* Foxconn add end by aspen Bai, 02/24/2009 */
    }
    else
    {
        nvram_set("wlh_secu_type", "None");
        nvram_set("wlh_temp_secu_type", "None");
        nvram_set("wlh_passphrase", "");
    }
#endif

    if (config_flag == 1)
    {
        //nvram_set("wps_randomssid", "");
        //nvram_set("wps_randomkey", "");
        nvram_set("wl0_wps_config_state", "1");
        nvram_set("wl1_wps_config_state", "1");
#if defined(INCLULDE_2ND_5G_RADIO)
        nvram_set("wl2_wps_config_state", "1");
#endif        
    }
    /* Foxconn add end, Tony W.Y. Wang, 11/23/2009 */
    nvram_set("allow_registrar_config", "0");  /* Foxconn added pling, 05/16/2007 */

    /* Foxconn added start pling 02/25/2008 */
    /* 'wl_unit' is changed to "0.-1" after Vista configure router (using Borg DTM1.3 patch).
     * This will make WPS fail to work on the correct interface.
     * Set it back to "0" if it is not.
     */
    if (!nvram_match("wl_unit", "0"))
        nvram_set("wl_unit", "0");
    /* Foxconn added end pling 02/25/2008 */
}
/* Foxconn added end by EricHuang, 12/13/2006 */

/* foxconn added start wklin, 11/02/2006 */
void save_wlan_time(void)
{
    struct sysinfo info;
    char command[128];
    sysinfo(&info);
    sprintf(command, "echo %lu > /tmp/wlan_time", info.uptime);
    system(command);
    return;
}
/* foxconn added end, wklin, 11/02/2006 */

#ifdef MFP
int disable_mfp()
{
    /*foxconn Han edited for GUI pmf enable/disable support once enable_pmf==1 then we should not overwrite mfp value*/
    if(nvram_match("enable_pmf","1"))
        return 0;

    /* Foxconn Bob added start 07/24/2015, force disable PMF to fix IOT issue with Nexus 5 */
    nvram_set("wl0_mfp", "0");
    nvram_set("wl1_mfp", "0");
    #if defined(INCLULDE_2ND_5G_RADIO) 
    nvram_set("wl2_mfp", "0");
    #endif
    /* Foxconn Bob added end 07/24/2015, force disable PMF to fix IOT issue with Nexus 5 */
}
#endif /*MFP*/

/*foxconn Han edited start, 02/23/2016*/
#ifdef PORT_TRUNKING_SUPPORT
extern int check_lacp_vlan_conflict(unsigned int intf, int gui);
/* move to ap/acos/share/lan_util.c
int check_lacp_vlan_conflict(unsigned int intf)
{
    unsigned int flag;
    printf("%s(%d) intf=0x%X\n",__func__,__LINE__,intf);

    flag = intf & (IPTV_LAN1|IPTV_LAN2);

    if(flag != 0 && flag != (IPTV_LAN1|IPTV_LAN2))
    {
        nvram_set("lacp_vlan_conflict","1");
        return 1;
    }
    
    return 0;
}*/
#endif /*PORT_TRUNKING_SUPPORT*/
/*foxconn Han edited end, 02/23/2016*/

/* foxconn added start, zacker, 01/13/2012, @iptv_igmp */
#ifdef CONFIG_RUSSIA_IPTV
int config_iptv_params(void)
{
#ifdef VLAN_SUPPORT
    unsigned int enabled_vlan_ports = 0x00;
    unsigned int iptv_bridge_intf = 0x00;
#endif
    char vlan1_ports[16] = "";
    char vlan_iptv_ports[16] = "";
    /*added by dennis start,05/04/2012,for guest network reconnect issue*/
    char br0_ifnames[64]="";
    char if_name[16]="";
    char wl_param[16]="";
    char command[128]="";
    int i = 0;
    /*added by dennis end,05/04/2012,for guest network reconnect issue*/

/*Foxconn add start, edward zhang, 2013/07/03*/
#ifdef VLAN_SUPPORT
    char br_ifname[16] = "";
    char br_ifnames[64] = "";
    char clean_vlan[16] = "";
    char clean_vlan_hw[16] = "";


	/*clean up the nvram ,to let the new config work*/
	
    if (nvram_match ("enable_vlan", "enable"))
    {
        for(i=1; i<7; i++)
        {
            sprintf(br_ifname,"lan%d_ifname",i);
            sprintf(br_ifnames,"lan%d_ifnames",i);
            nvram_set(br_ifnames, "");
            nvram_set(br_ifname, "");
        }
        for(i=1; i < 4094; i++)
        {
            sprintf(clean_vlan,"vlan%dports",i);
            sprintf(clean_vlan_hw,"vlan%dhwname",i);
            if( i == 1 || i == 2)
            {
              nvram_set(clean_vlan,"");
              nvram_set(clean_vlan_hw,"");
            }
            else
            {
                nvram_unset(clean_vlan);
                nvram_unset(clean_vlan_hw);
            }
            #ifdef CONFIG_2ND_SWITCH
            /*foxconn Han edited 05/27/2015, for external switch nvram cleanup*/
            sprintf(clean_vlan,"evlan%dports",i);
            if( i == 1 || i == 2)
                nvram_set(clean_vlan,"");
            else
                nvram_unset(clean_vlan);
            #endif /*CONFIG_2ND_SWITCH*/
        }
    }
    else
    {
        /* Foxconn add start, Max Ding, 11/22/2016 */
        /* fix issues when enable_vlan change from enable to disable */
        for(i=1; i<7; i++)
        {
            sprintf(br_ifname,"lan%d_ifname",i);
            sprintf(br_ifnames,"lan%d_ifnames",i);
            nvram_set(br_ifnames, "");
            nvram_set(br_ifname, "");
        }
        /* Foxconn add end, Max Ding, 11/22/2016 */

        for(i=3; i < 4094; i++)
        {
            sprintf(clean_vlan,"vlan%dports",i);
            sprintf(clean_vlan_hw,"vlan%dhwname",i);
            nvram_unset(clean_vlan);
            nvram_unset(clean_vlan_hw);
            #ifdef CONFIG_2ND_SWITCH
            /*foxconn Han edited 05/27/2015, for external switch nvram cleanup*/
            sprintf(clean_vlan,"evlan%dports",i);
            nvram_unset(clean_vlan);
            #endif /*CONFIG_2ND_SWITCH*/
        }
    }

    /*foxconn Han edited start, 02/23/2016*/
#ifdef  PORT_TRUNKING_SUPPORT
    nvram_set("lacp_vlan_conflict","0");
#endif  /*PORT_TRUNKING_SUPPORT*/
    /*foxconn Han edited end, 02/23/2016*/

    if (!nvram_match("enable_vlan", "enable") && !nvram_match(NVRAM_IPTV_ENABLED, "1") )
        return 0;
#endif
/*Foxconn add end, edward zhang, 2013/07/03*/

    if (nvram_match(NVRAM_IPTV_ENABLED, "1"))
    {
        char iptv_intf[32];

        strcpy(iptv_intf, nvram_safe_get(NVRAM_IPTV_INTF));
        sscanf(iptv_intf, "0x%04X", &iptv_bridge_intf);
    }

    /*foxconn Han edited start, 02/23/2016*/
#ifdef PORT_TRUNKING_SUPPORT
    check_lacp_vlan_conflict(iptv_bridge_intf ,0);
    check_lacp_vlan_conflict(~iptv_bridge_intf ,0);
#endif /*PORT_TRUNKING_SUPPORT*/
    /*foxconn Han edited end, 02/23/2016*/

    /* Foxconn modified start pling 04/03/2012 */
    /* Swap LAN1 ~ LAN4 due to reverse labeling */

    if (iptv_bridge_intf & IPTV_LAN1)
        strcat(vlan_iptv_ports, "1 ");
    else
        strcat(vlan1_ports, "1 ");

    if (iptv_bridge_intf & IPTV_LAN2)   /* Foxconn modified pling 02/09/2012, fix a typo */
        strcat(vlan_iptv_ports, "2 ");
    else
        strcat(vlan1_ports, "2 ");

    if (iptv_bridge_intf & IPTV_LAN3)
        strcat(vlan_iptv_ports, "3 ");
    else
        strcat(vlan1_ports, "3 ");

        if (iptv_bridge_intf & IPTV_LAN4)
            strcat(vlan_iptv_ports, "4 ");
        else
            strcat(vlan1_ports, "4 ");
        #if defined(AX6000)
        if (iptv_bridge_intf & IPTV_LAN5)
            strcat(vlan_iptv_ports, "5 ");
        else
            strcat(vlan1_ports, "5 ");
        #elif defined(AX11000)
        if (iptv_bridge_intf & IPTV_LAN5)//2.5Gor1G
            strcat(vlan_iptv_ports, "5 ");
        else
            strcat(vlan1_ports, "5 ");
        #endif



    #ifdef __CONFIG_GMAC3__
    if(nvram_match("gmac3_enable", "1"))
        strcat(vlan1_ports, "5 7 8*");
    else
        strcat(vlan1_ports, "5*");
    #else
    strcat(vlan1_ports, "5*");
    #endif    
    /*Foxconn add start, edward zhang, 2013/07/03*/
    #ifdef VLAN_SUPPORT
    char lan_interface[16]="";
    char lan_hwname[16]="";
	#ifdef CONFIG_2ND_SWITCH
	char ext_lan_interface[16]="";
	#endif /*CONFIG_2ND_SWITCH*/
    if (nvram_match ("enable_vlan", "enable"))
    {
        sprintf(lan_interface,"vlan%sports",nvram_safe_get("vlan_lan_id"));
        nvram_set(lan_interface,vlan1_ports);
        sprintf(lan_hwname,"vlan%shwname",nvram_safe_get("vlan_lan_id"));
        nvram_set(lan_hwname,"et0");
        /*foxconn Han edited 05/27/2015, for external switch*/
		#ifdef CONFIG_2ND_SWITCH
		if(isTriBand())
		{
			sprintf(ext_lan_interface,"evlan%sports",nvram_safe_get("vlan_lan_id"));
            //printf("%s %d ext_lan_interface=%s, evlan1_ports=%s\n",__func__,__LINE__,ext_lan_interface,evlan1_ports);
			nvram_set(ext_lan_interface, evlan1_ports);
		}
		#endif /*CONFIG_2ND_SWITCH*/
    }
    else
    #endif
	{
		/*Foxconn add end, edward zhang, 2013/07/03*/
		nvram_set("vlan1ports", vlan1_ports);
		#ifdef CONFIG_2ND_SWITCH
		if(isTriBand())
			nvram_set("evlan1ports", evlan1_ports);
        //printf("%s %d evlan1_ports=%s\n",__func__,__LINE__,evlan1_ports);
		#endif /*CONFIG_2ND_SWITCH*/
	}

    /* build vlan3 for IGMP snooping on IPTV ports */
    /*Foxconn add start, edward zhang, 2013/07/03*/
#ifdef VLAN_SUPPORT
    if (nvram_match ("enable_vlan", "enable"))
        ;/*do nothing*/
    else
#endif
    /*Foxconn add end, edward zhang, 2013/07/03*/
    {
        if (strlen(vlan_iptv_ports))
        {
            strcat(vlan_iptv_ports, "5");
            nvram_set("vlan3ports", vlan_iptv_ports);
            nvram_set("vlan3hwname", nvram_safe_get("vlan2hwname"));

        }
        else
        {
            nvram_unset("vlan3ports");
            nvram_unset("vlan3hwname");
        }
		#ifdef CONFIG_2ND_SWITCH
        /*foxconn Han edited 05/27/2015, for external switch*/
		if(strlen(evlan_iptv_ports))
		{
			if(isTriBand())
			{
				printf("%s %d evlan3_ports=%s\n",__func__,__LINE__,evlan_iptv_ports);
				nvram_set("evlan3ports",evlan_iptv_ports);
			}
		}
		else
		{
			nvram_unset("evlan3ports");
		}
		#endif /*CONFIG_2ND_SWITCH*/		
    }

/*Foxconn add start, edward zhang, 2013/07/03*/
#ifdef VLAN_SUPPORT
    
    if (nvram_match ("enable_vlan", "enable"))
    {
        char vlan_ifname[16] = "";
        char vlan_ifname_ports[16] = "";
        char vlan_ports[16]  = "";
        char vlan_prio[16] = "";
        char vlan_hwname[16] = "";
        char wan_vlan_ifname[16] = "";
        char lan_vlan_hwname[16] = "";
        vlan_rule vlan[C_MAX_VLAN_RULE];
        int numVlanRule = getVlanRule(vlan);
		unsigned int vlan_bridge_intf = 0x00;
        char lan_vlan_ports[16] = "";
        char lan_ports[16] = "";
        int lan_vlan_port = 4;
        int lan_vlan_br = 1;
        char lan_vlan_ifname[16] = "";
        char lan_vlan_ifnames[128] = "";
        char lan_ifnames[128] = "";
        char lan_ifname[16] = "";
        int internet_vlan_id = 0;
		#ifdef CONFIG_2ND_SWITCH
		char evlan_ifname_ports[16] = "";
		char evlan_ports[16]="";
		char elan_ports[16]="";
		#endif /*CONFIG_2ND_SWITCH*/
		
		
        /* always set emf_enable to 0 when vlan is enable*/
        nvram_set("emf_enable", "0");

        cprintf("rule_num:%d \n",numVlanRule);
#ifdef BCA_HNDROUTER
        strcpy(lan_ifnames, ""); /* Foxconn added by Max Ding, 11/22/2016 */
#else
        sprintf(lan_ifnames,"%s ",nvram_safe_get("lan_interface"));
#endif
        for(i=0;i<numVlanRule;i++)
        {
            memset(lan_vlan_ifnames,0,sizeof(lan_vlan_ifnames));
            memset(vlan_ports,0,sizeof(vlan_ports));
            if(!strcmp(vlan[i].enable_rule,"0"))
                continue;
            sprintf(vlan_ifname,"vlan%s ",vlan[i].vlan_id);
            sprintf(wan_vlan_ifname,"vlan%s",vlan[i].vlan_id);
            sprintf(vlan_ifname_ports,"vlan%sports",vlan[i].vlan_id);
            sprintf(vlan_hwname,"vlan%shwname",vlan[i].vlan_id);
            nvram_set(vlan_hwname,"et0");
            sprintf(vlan_prio,"vlan%s_prio",vlan[i].vlan_id);
            nvram_set(vlan_prio,vlan[i].vlan_prio);
			#ifdef CONFIG_2ND_SWITCH
			sprintf(evlan_ifname_ports,"evlan%sports",vlan[i].vlan_id);
			#endif /*CONFIG_2ND_SWITCH*/
            
            if(!strcmp(vlan[i].vlan_name, "Internet"))
            {
         	    nvram_set(vlan_ifname_ports,"0t 5");
                nvram_set("internet_prio",vlan[i].vlan_prio);
                nvram_set("internet_vlan",vlan[i].vlan_id);
                nvram_set("wan_ifnames", vlan_ifname);
                nvram_set("wan_ifname", wan_vlan_ifname);
                internet_vlan_id=atoi(vlan[i].vlan_id);
                continue;
            }
            
            if(internet_vlan_id==atoi(vlan[i].vlan_id))
            {
                nvram_set("wan_ifnames", "br1");
                nvram_set("wan_ifname", "br1");
            }
            
            sscanf(vlan[i].vlan_ports, "0x%04X", &vlan_bridge_intf);

            strcat(lan_vlan_ifnames, vlan_ifname);
            enabled_vlan_ports |= vlan_bridge_intf ;

            //printf("%s %d %d vlan_bridge_intf=0x%X enabled_vlan_ports=0x%X\n",__func__,__LINE__,i,vlan_bridge_intf,enabled_vlan_ports);

            /*foxconn Han edited start, 02/23/2016*/
#ifdef      PORT_TRUNKING_SUPPORT
            check_lacp_vlan_conflict(vlan_bridge_intf ,0);
#endif      /*PORT_TRUNKING_SUPPORT*/
            /*foxconn Han edited end, 02/23/2016*/


            if (vlan_bridge_intf & IPTV_LAN1) {
#ifdef BCA_HNDROUTER
                    strcat(lan_vlan_ifnames, LAN1_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
                strcat(vlan_ports, "1 ");
            }

            if (vlan_bridge_intf & IPTV_LAN2) {
#ifdef BCA_HNDROUTER
                    strcat(lan_vlan_ifnames, LAN2_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
                strcat(vlan_ports, "2 ");
            }

            if (vlan_bridge_intf & IPTV_LAN3) {
#ifdef BCA_HNDROUTER
                strcat(lan_vlan_ifnames, LAN3_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
                strcat(vlan_ports, "3 ");
            }

			#ifdef CONFIG_2ND_SWITCH
			if(isTriBand())
			{
				if (vlan_bridge_intf & (IPTV_LAN4 | IPTV_LAN5 | IPTV_LAN6)) /*0x304*/
				{
					strcat(vlan_ports, "4t ");
					strcat(evlan_ports, "1t ");
				}
				else
				{
					;
				}
				
				if (vlan_bridge_intf & IPTV_LAN4)
				{
					strcat(evlan_ports, "2 ");
				}
				if (vlan_bridge_intf & IPTV_LAN5)
				{
					strcat(evlan_ports, "3 ");
				}
				if (vlan_bridge_intf & IPTV_LAN6)
				{
					strcat(evlan_ports, "4 ");
				}
				if(strlen(evlan_ports)>0)
				{
					//strcat(evlan_ports, "5 ");
				}
				printf("%s %d evlan_ifname_ports=%s evlan_ports=%s\n",__func__,__LINE__,evlan_ifname_ports,evlan_ports);
				nvram_set(evlan_ifname_ports,evlan_ports);
			}
			else
			#endif /*CONFIG_2ND_SWITCH*/
            {
                if (vlan_bridge_intf & IPTV_LAN4) {
#ifdef BCA_HNDROUTER
                    strcat(lan_vlan_ifnames, LAN4_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
                    strcat(vlan_ports, "4 ");
                }
#if defined(AX6000) || defined(AX11000)
                if (vlan_bridge_intf & IPTV_LAN5) {
#ifdef BCA_HNDROUTER
                    strcat(lan_vlan_ifnames, LAN5_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
                    strcat(vlan_ports, "5 ");
                }
#endif
            }
            strcat(vlan_ports, "0t 5");

            nvram_set(vlan_ifname_ports,vlan_ports);    /*Foxconn add, edward zhang ,set the bridge ports*/

            if (vlan_bridge_intf & IPTV_WLAN1) {
                //strcat(lan_vlan_ifnames, "eth1 ");
                strcat(lan_vlan_ifnames, WLAN_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
            }

            if (vlan_bridge_intf & IPTV_WLAN2) {
                //strcat(lan_vlan_ifnames, "eth2 ");
                strcat(lan_vlan_ifnames, WLAN_N_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
            }
#if defined(DUAL_TRI_BAND_HW_SUPPORT)
            if(isTriBand())
            {
                if (vlan_bridge_intf & IPTV_WLAN3) {
                    //strcat(lan_vlan_ifnames, "eth3 ");
                    strcat(lan_vlan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
                }
            }
#elif defined(INCLULDE_2ND_5G_RADIO)
            if (vlan_bridge_intf & IPTV_WLAN3) {
                //strcat(lan_vlan_ifnames, "eth3 ");
                strcat(lan_vlan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
            }
#endif

            if (vlan_bridge_intf & IPTV_WLAN_GUEST1)
                //strcat(lan_vlan_ifnames, "wl0.1 ");
                strcat(lan_vlan_ifnames, WLAN_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */

            if (vlan_bridge_intf & IPTV_WLAN_GUEST2)
                //strcat(lan_vlan_ifnames, "wl1.1 ");
                strcat(lan_vlan_ifnames, WLAN_5G_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
            
#if defined(DUAL_TRI_BAND_HW_SUPPORT)
            if(isTriBand())
            {
                if (vlan_bridge_intf & IPTV_WLAN_GUEST3)
                    //strcat(lan_vlan_ifnames, "wl2.1 ");
                    strcat(lan_vlan_ifnames, WLAN_5G_2_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
            }
#elif defined(INCLULDE_2ND_5G_RADIO)
            if (vlan_bridge_intf & IPTV_WLAN_GUEST3)
                //strcat(lan_vlan_ifnames, "wl2.1 ");
                strcat(lan_vlan_ifnames, WLAN_5G_2_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
#endif

            
            sprintf(br_ifname,"lan%d_ifname",lan_vlan_br);
            sprintf(br_ifnames,"lan%d_ifnames",lan_vlan_br);
            sprintf(lan_vlan_ifname,"br%d",lan_vlan_br);
            nvram_set(br_ifname,lan_vlan_ifname);
            nvram_set(br_ifnames,lan_vlan_ifnames);
            lan_vlan_br++;
        }
        
        /*foxconn Han edited start, 02/23/2016*/
#ifdef  PORT_TRUNKING_SUPPORT
        check_lacp_vlan_conflict(~enabled_vlan_ports ,0);
#endif  /*PORT_TRUNKING_SUPPORT*/
        /*foxconn Han edited end, 02/23/2016*/


        if (!(enabled_vlan_ports & IPTV_LAN1)) {
#ifdef BCA_HNDROUTER
            strcat(lan_ifnames, LAN1_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
            strcat(lan_ports, "1 ");
        }

        if (!(enabled_vlan_ports & IPTV_LAN2)) {
#ifdef BCA_HNDROUTER
            strcat(lan_ifnames, LAN2_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
            strcat(lan_ports, "2 ");
        }

        if (!(enabled_vlan_ports & IPTV_LAN3)) {
#ifdef BCA_HNDROUTER
            strcat(lan_ifnames, LAN3_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
            strcat(lan_ports, "3 ");
        }

		#ifdef CONFIG_2ND_SWITCH
		if(isTriBand())
		{
			if(enabled_vlan_ports & (IPTV_LAN4 | IPTV_LAN5 | IPTV_LAN6))
			{
				strcat(lan_ports, "4t ");
				strcat(elan_ports, "0 1t ");
				//strcat(lan_ports, "4 ");
				//strcat(elan_ports, "0 1 ");
				//strcat(elan_ports, "0 ");
				if (!(enabled_vlan_ports & IPTV_LAN4))
					strcat(elan_ports, "2 ");
				if (!(enabled_vlan_ports & IPTV_LAN5))
					strcat(elan_ports, "3 ");
				if (!(enabled_vlan_ports & IPTV_LAN6))
					strcat(elan_ports, "4 ");
			}
			else
			{
				strcat(lan_ports, "4 ");
				strcat(elan_ports, "0 1 2 3 4 ");
			}
			
			strcat(elan_ports, "5u ");
			printf("%s %d ext_lan_interface=%s,elan_ports=%s\n",__func__,__LINE__,ext_lan_interface,elan_ports);
			nvram_set(ext_lan_interface,elan_ports);
		}
		else
		#endif /*CONFIG_2ND_SWITCH*/ 
        {    
			if (!(enabled_vlan_ports & IPTV_LAN4)) {
#ifdef BCA_HNDROUTER
                strcat(lan_ifnames, LAN4_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
				strcat(lan_ports, "4 ");
            }
            
#if defined(AX6000) || defined(AX11000)
            if (!(enabled_vlan_ports & IPTV_LAN5)) {
#ifdef BCA_HNDROUTER
                strcat(lan_ifnames, LAN5_IF_NAME_NUM" "); /* Foxconn added by Max Ding, 11/22/2016 */
#endif
                strcat(vlan_ports, "5 ");
            }
#endif
        }
		
		strcat(lan_ports, "5* ");
		nvram_set(lan_interface,lan_ports);
		
        
        if (!(enabled_vlan_ports & IPTV_WLAN1)) {
            //strcat(lan_ifnames, "eth1 ");
            strcat(lan_ifnames, WLAN_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        }

        if (!(enabled_vlan_ports & IPTV_WLAN2)) {
            //strcat(lan_ifnames, "eth2 ");
            strcat(lan_ifnames, WLAN_N_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        }
        
#if defined(DUAL_TRI_BAND_HW_SUPPORT)
		if(isTriBand())
		{
			if (!(enabled_vlan_ports & IPTV_WLAN3)) {
				//strcat(lan_ifnames, "eth3 ");
                strcat(lan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
            }
		}
#elif defined(INCLULDE_2ND_5G_RADIO)
        if (!(enabled_vlan_ports & IPTV_WLAN3)) {
            //strcat(lan_ifnames, "eth3 ");
            strcat(lan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        }
#endif
        
        strcpy(br0_ifnames,lan_ifnames);
		#ifdef __CONFIG_IGMP_SNOOPING__
        /* always enable snooping for VLAN IPTV */
        //nvram_set("emf_enable", "1");
		#endif
		#ifdef VLAN_SUPPORT
		{
            nvram_set("vlan2hwname", "et0");
            nvram_set("vlan1hwname", "et0");
		}
		#endif
    }
	else
#endif /*VLAN_SUPPORT*/
/*Foxconn add end, edward zhang, 2013/07/03*/
	#ifdef CONFIG_2ND_SWITCH
	if (((!isTriBand()) && (iptv_bridge_intf & IPTV_MASK)) 
		|| (isTriBand() && (iptv_bridge_intf & IPTV_EXT_MASK)))
	#else
    if (iptv_bridge_intf & IPTV_MASK)
	#endif /*CONFIG_2ND_SWITCH*/
    {
        /* Foxconn add start, Max Ding, 11/22/2016 */
#ifdef BCA_HNDROUTER
        char lan_ifnames[128] = "";
        char wan_ifnames[128] = WAN_IF_NAME_NUM" ";
#else
        /* Foxconn add end, Max Ding, 11/22/2016 */
        char lan_ifnames[128] = "vlan1 ";
        char wan_ifnames[128] = "vlan2 ";
#endif
    
		#ifdef __CONFIG_IGMP_SNOOPING__
        /* always enable snooping for IPTV */
        nvram_set("emf_enable", "1");
		#endif

        /* always build vlan2 and br1 and enable vlan tag output for all vlan */
		#ifdef __CONFIG_GMAC3__
        if(nvram_match("gmac3_enable", "1"))
            nvram_set("vlan2ports", "0 8");
        else
        {
			/*Foxconn add , edward zhang, 2013/07/03*/
            nvram_set("vlan2ports", "0 5");
        }
		#else

        nvram_set("vlan2ports", "4 5");
		#endif

        /* Foxconn add start, Max Ding, 11/22/2016 */        
#ifdef BCA_HNDROUTER
        /* Foxconn modify start, Max Ding, 01/09/2017 */
        if (iptv_bridge_intf & IPTV_LAN1) {
                strcat(wan_ifnames, LAN1_IF_NAME_NUM" ");
        } else {
                strcat(lan_ifnames, LAN1_IF_NAME_NUM" ");
        }

        if (iptv_bridge_intf & IPTV_LAN2) {
                strcat(wan_ifnames, LAN2_IF_NAME_NUM" ");
        } else {
                strcat(lan_ifnames, LAN2_IF_NAME_NUM" ");
        }
        /* Foxconn modify end, Max Ding, 01/09/2017 */

        if (iptv_bridge_intf & IPTV_LAN3)
            strcat(wan_ifnames, LAN3_IF_NAME_NUM" ");
        else
            strcat(lan_ifnames, LAN3_IF_NAME_NUM" ");

        if (iptv_bridge_intf & IPTV_LAN4)
            strcat(wan_ifnames, LAN4_IF_NAME_NUM" ");
        else
            strcat(lan_ifnames, LAN4_IF_NAME_NUM" ");

#if defined(AX6000) || defined(AX11000)
        if (iptv_bridge_intf & IPTV_LAN5)
            strcat(wan_ifnames, LAN5_IF_NAME_NUM" ");
        else
            strcat(lan_ifnames, LAN5_IF_NAME_NUM" ");
#endif
#else /*!BCA_HNDROUTER*/
        /* Foxconn add end, Max Ding, 11/22/2016 */
        /* build vlan3 for IGMP snooping on IPTV ports */
        if (strlen(vlan_iptv_ports))
            strcat(wan_ifnames, "vlan3 ");
#endif /*BCA_HNDROUTER*/

        if (iptv_bridge_intf & IPTV_WLAN1)
            //strcat(wan_ifnames, "eth1 ");
            strcat(wan_ifnames, WLAN_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        else
            //strcat(lan_ifnames, "eth1 ");
            strcat(lan_ifnames, WLAN_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */

        if (iptv_bridge_intf & IPTV_WLAN2)
            //strcat(wan_ifnames, "eth2 ");
            strcat(wan_ifnames, WLAN_N_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        else
            //strcat(lan_ifnames, "eth2 ");
            strcat(lan_ifnames, WLAN_N_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        
        
#if defined(DUAL_TRI_BAND_HW_SUPPORT)
		if(isTriBand())
		{
			if (iptv_bridge_intf & IPTV_WLAN3)
				//strcat(wan_ifnames, "eth3 ");
                strcat(wan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
			else
				//strcat(lan_ifnames, "eth3 ");
                strcat(lan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
		}
#elif defined(INCLULDE_2ND_5G_RADIO)
        if (iptv_bridge_intf & IPTV_WLAN3)
            //strcat(wan_ifnames, "eth3 ");
            strcat(wan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        else
            //strcat(lan_ifnames, "eth3 ");
            strcat(lan_ifnames, WLAN_5G_2_IF_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
#endif

        if (iptv_bridge_intf & IPTV_WLAN_GUEST1)
            //strcat(wan_ifnames, "wl0.1 ");
            strcat(wan_ifnames, WLAN_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        else
            //strcat(lan_ifnames, "wl0.1 ");
            strcat(lan_ifnames, WLAN_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */

        if (iptv_bridge_intf & IPTV_WLAN_GUEST2)
            //strcat(wan_ifnames, "wl1.1 ");
            strcat(wan_ifnames, WLAN_5G_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        else
            //strcat(lan_ifnames, "wl1.1 ");
            strcat(lan_ifnames, WLAN_5G_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */

#if defined(DUAL_TRI_BAND_HW_SUPPORT)  
		if(isTriBand())
		{
			if (iptv_bridge_intf & IPTV_WLAN_GUEST3)
				//strcat(wan_ifnames, "wl2.1 ");
                strcat(wan_ifnames, WLAN_5G_2_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
			else
				//strcat(lan_ifnames, "wl2.1 ");
                strcat(lan_ifnames, WLAN_5G_2_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
		}
#elif defined(INCLULDE_2ND_5G_RADIO)
        if (iptv_bridge_intf & IPTV_WLAN_GUEST3)
            //strcat(wan_ifnames, "wl2.1 ");
            strcat(wan_ifnames, WLAN_5G_2_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
        else
            //strcat(lan_ifnames, "wl2.1 ");
            strcat(lan_ifnames, WLAN_5G_2_BSS1_NAME_NUM" "); /* Foxconn modified by Max Ding, 11/22/2016 */
#endif

        //nvram_set("lan_ifnames", lan_ifnames);
#ifdef __CONFIG_GMAC3__
        strcpy(br0_ifnames,lan_ifnames);
#else
        strcpy(br0_ifnames,lan_ifnames);
#endif        
        nvram_set("wan_ifnames", wan_ifnames);
        nvram_set("lan1_ifnames", wan_ifnames);

        nvram_set("wan_ifname", "br1");
        nvram_set("lan1_ifname", "br1");
    }
    else
    {
        
        //nvram_set("lan_ifnames", "vlan1 eth1 eth2 wl0.1");
        /*modified by dennis start, 05/03/2012,fixed guest network cannot reconnect issue*/
#ifdef __CONFIG_GMAC3__
        if(nvram_match("gmac3_enable", "1"))
            strcpy(br0_ifnames,"vlan1");       
        else
        {
/*Foxconn add start, edward zhang, 2013/07/03*/
#ifdef VLAN_SUPPORT
            nvram_set("vlan2hwname", "et0");
            nvram_set("vlan1hwname", "et0");
#endif
            if(!nvram_match("enable_vlan", "enable"))
#ifdef BCA_HNDROUTER
            {
                {
                /* Foxconn add end, Max Ding, 01/09/2017 */
                #if defined(INCLULDE_2ND_5G_RADIO)
                    sprintf(br0_ifnames,"%s %s %s %s %s %s %s", \
                        LAN1_IF_NAME_NUM, LAN2_IF_NAME_NUM, LAN3_IF_NAME_NUM, LAN4_IF_NAME_NUM, \
                        WLAN_IF_NAME_NUM, WLAN_N_IF_NAME_NUM, WLAN_5G_2_IF_NAME_NUM);  
                #else
                    sprintf(br0_ifnames,"%s %s %s %s %s %s", \
                        LAN1_IF_NAME_NUM, LAN2_IF_NAME_NUM, LAN3_IF_NAME_NUM, LAN4_IF_NAME_NUM, 
                        WLAN_IF_NAME_NUM, WLAN_N_IF_NAME_NUM);  
                #endif
                #if defined(AX6000) || defined(AX11000)
                    strcat(br0_ifnames, " ");
                    strcat(br0_ifnames, LAN5_IF_NAME_NUM);
                #endif
                }
            }
#else /* !BCA_HNDROUTER */
                strcpy(br0_ifnames,"vlan1 eth1 eth2 eth3");   
#endif /* !BCA_HDNROUTER */                
        }
#else
        strcpy(br0_ifnames,"vlan1 eth1 eth2");       
#endif
        /*modified by dennis end, 05/03/2012,fixed guest network cannot reconnect issue*/
        nvram_set("lan1_ifnames", "");
        nvram_set("lan1_ifname", "");

#ifdef __CONFIG_IGMP_SNOOPING__
        /* foxconn Bob modified start 07/18/2014, not to bridge eth0 and vlan1 in the same bridge, or may cause broadcast radiation */
        if (nvram_match("emf_enable", "1") || nvram_match("enable_ap_mode", "1") ) {
        /* foxconn Bob modified end 07/18/2014 */
#ifdef __CONFIG_GMAC3__
            if(nvram_match("gmac3_enable", "1"))
                nvram_set("vlan2ports", "0 8");
            else
                nvram_set("vlan2ports", "0 5");
#else
            nvram_set("vlan2ports", "0 5");
#endif
            /* foxconn Bob modified start 07/18/2014, not to bridge eth0 and vlan1 in the same bridge, or may cause broadcast radiation */
            nvram_set("wan_ifnames", "vlan2");
            nvram_set("wan_ifname", "vlan2");
            /* foxconn Bob modified end 07/18/2014 */
        }
        else
#endif
        {
#ifdef __CONFIG_GMAC3__

            if(nvram_match("gmac3_enable", "1"))
            {
                if (nvram_match("enable_ap_mode", "1")) {
                    nvram_set("vlan2ports", "0 8");
                    nvram_set("wan_ifnames", "vlan2 ");
                    nvram_set("wan_ifname", "vlan2");
                }
                else {
                    nvram_set("vlan2ports", "0 8u");
                    nvram_set("wan_ifnames", "eth0 ");
                    nvram_set("wan_ifname", "eth0");
                }
            }
            else
            {
                if (nvram_match("enable_ap_mode", "1")) {
                    nvram_set("vlan2ports", "0 5");
                    nvram_set("wan_ifnames", "vlan2 ");
                    nvram_set("wan_ifname", "vlan2");
                }
                else {
                    nvram_set("vlan2ports", "0 5u");
                    {
                        nvram_set("wan_ifnames", "eth0 ");
                        nvram_set("wan_ifname", "eth0");
                    }
                }
            }
#else
            nvram_set("vlan2ports", "0 5");
            {
                nvram_set("wan_ifnames", "eth0 ");
                nvram_set("wan_ifname", "eth0");
            }
#endif
/* foxconn revise end ken chen @ 08/23/2013, to fix IGMP report duplicated in AP mode*/
        }
    }

     /*added by dennis start, 05/03/2012,fixed guest network cannot reconnect issue*/
     for(i = MIN_BSSID_NUM; i <= MAX_BSSID_NUM; i++){
        sprintf(wl_param, "%s_%d", "wla_sec_profile_enable", i);     
        if(nvram_match(wl_param, "1")){
            sprintf(if_name, "wl0.%d", i-1);
            if(nvram_match("enable_vlan", "enable"))
            {
                if(!(enabled_vlan_ports & IPTV_WLAN_GUEST1))
                {
                    strcat(br0_ifnames, " ");
                    strcat(br0_ifnames, if_name);
                }
            }
            else if (nvram_match(NVRAM_IPTV_ENABLED, "1"))
            {
            	// Do nothing here
            }
            else
            {
                strcat(br0_ifnames, " ");
                strcat(br0_ifnames, if_name);
            }
            	
        }
     }

     for(i = MIN_BSSID_NUM; i <= MAX_BSSID_NUM; i++){
         sprintf(wl_param, "%s_%d", "wlg_sec_profile_enable", i);        
         if(nvram_match(wl_param, "1")){
             sprintf(if_name, "wl1.%d", i-1);
            if(nvram_match("enable_vlan", "enable"))
            {
                if(!(enabled_vlan_ports & IPTV_WLAN_GUEST2))
                {
                    strcat(br0_ifnames, " ");
                    strcat(br0_ifnames, if_name);
                }
            }
            else if (nvram_match(NVRAM_IPTV_ENABLED, "1"))
            {
            	// Do nothing here
            }
            else
            {
                strcat(br0_ifnames, " ");
                strcat(br0_ifnames, if_name);
            }
         }
     }

#if defined(INCLULDE_2ND_5G_RADIO)  
     for(i = MIN_BSSID_NUM; i <= MAX_BSSID_NUM; i++){
         sprintf(wl_param, "%s_%d", "wlh_sec_profile_enable", i);        
         if(nvram_match(wl_param, "1")){
             sprintf(if_name, "wl2.%d", i-1);
            if(nvram_match("enable_vlan", "enable"))
            {
                if(!(enabled_vlan_ports & IPTV_WLAN_GUEST3))
                {
                    strcat(br0_ifnames, " ");
                    strcat(br0_ifnames, if_name);
                }
            }
            else if (nvram_match(NVRAM_IPTV_ENABLED, "1"))
            {
            	// Do nothing here
            }
	          else
            {
                strcat(br0_ifnames, " ");
                strcat(br0_ifnames, if_name);
            }
         }
     }
#endif
#ifdef __CONFIG_GMAC3__
     if(nvram_match("iptv_enabled", "1"))
         nvram_set("lan_ifnames", br0_ifnames);
     else if(nvram_match("enable_vlan", "enable"))
         nvram_set("lan_ifnames", br0_ifnames);
     else
         nvram_set("lan_ifnames", "vlan1 eth1 eth2 eth3 wl0.1 wl1.1 wl2.1");
#else
     nvram_set("lan_ifnames", br0_ifnames);
#endif     
    /*added by dennis start, 05/03/2012,fixed guest network cannot reconnect issue*/
	/* Foxconn added start pling 08/17/2012 */
    /* Fix: When IPTV is enabled, WAN interface is "br1".
     * This can cause CTF/pktc to work abnormally.
     * So bypass CTF/pktc altogether */
    if (nvram_match(NVRAM_IPTV_ENABLED, "1"))
        eval("et", "robowr", "0xFFFF", "0xFB", "1");
    else
        eval("et", "robowr", "0xFFFF", "0xFB", "0");
    /* Foxconn added end pling 08/17/2012 */
    return 0;
}
#endif
#ifdef VLAN_SUPPORT

int active_vlan(void)
{
    char buf[128];
    unsigned char mac[ETHER_ADDR_LEN];
    char eth0_mac[32];

    /* foxconn Han edited, 05/28/2015 for external switch, 
     * don't change switch configuration by our own.*/
#if !defined(CONFIG_2ND_SWITCH)
    return 0;
#endif

    strcpy(eth0_mac, nvram_safe_get("et0macaddr"));
    ether_atoe(eth0_mac, mac);

    /* Set MAC address byte 0 */
    sprintf(buf, "et robowr 0x%04X 0x%02X 0x%02X%02X", VCFG_PAGE, VCFG_REG, MAC_BYTE0, mac[0]);
    system(buf);
    /* Set MAC address byte 1 */
    sprintf(buf, "et robowr 0x%04X 0x%02X 0x%02X%02X", VCFG_PAGE, VCFG_REG, MAC_BYTE1, mac[1]);
    system(buf);
    /* Set MAC address byte 2 */
    sprintf(buf, "et robowr 0x%04X 0x%02X 0x%02X%02X", VCFG_PAGE, VCFG_REG, MAC_BYTE2, mac[2]);
    system(buf);
    /* Set MAC address byte 3 */
    sprintf(buf, "et robowr 0x%04X 0x%02X 0x%02X%02X", VCFG_PAGE, VCFG_REG, MAC_BYTE3, mac[3]);
    system(buf);
    /* Set MAC address byte 4 */
    sprintf(buf, "et robowr 0x%04X 0x%02X 0x%02X%02X", VCFG_PAGE, VCFG_REG, MAC_BYTE4, mac[4]);
    system(buf);
    /* Set MAC address byte 5 */
    sprintf(buf, "et robowr 0x%04X 0x%02X 0x%02X%02X", VCFG_PAGE, VCFG_REG, MAC_BYTE5, mac[5]);
    system(buf);
    /* Issue command to activate new vlan configuration. */
    sprintf(buf, "et robowr 0x%04X 0x%02X 0x%02X00", VCFG_PAGE, VCFG_REG, SET_VLAN);
    system(buf);

    return 0;
}
#endif

#if (defined INCLUDE_QOS) || (defined __CONFIG_IGMP_SNOOPING__)
/* these settings are for BCM53115S switch */
int config_switch_reg(void)
{

    /* foxconn Han edited, 05/28/2015 for external switch, 
     * don't change switch configuration by our own.*/
    return 0;

#ifdef VLAN_SUPPORT
    if(nvram_match("enable_vlan", "enable"))
    {

        system("et robowr 0x00 0x08 0x1C");
        system("et robowr 0x00 0x0B 0x07");
        system("et robowr 0x02 0x00 0x80");
#ifdef BCM5301X           
        /*Enable BRCM header for port 5*/
        system("et robowr 0x02 0x03 0x02");  /* Foxconn Bob added for 4708 */
#endif        
        system("et robowr 0xFFFF 0xFA 1");    	
    }
#endif

    if (
#if (defined __CONFIG_IGMP_SNOOPING__)
        nvram_match("emf_enable", "1") ||
#endif
#if defined(CONFIG_RUSSIA_IPTV)
		nvram_match("iptv_enabled", "1") ||
#endif          
        (nvram_match("qos_enable", "1")  
        && !nvram_match("wla_repeater", "1")
#if (defined INCLUDE_DUAL_BAND)
        && !nvram_match("wlg_repeater", "1")
#endif
        && !nvram_match("qos_port", "")))
    {
        /* Enables the receipt of unicast, multicast and broadcast on IMP port */
        system("et robowr 0x00 0x08 0x1C");
        /* Enable Frame-managment mode */
        system("et robowr 0x00 0x0B 0x07");
        /* Enable management port */
        system("et robowr 0x02 0x00 0x80");
#ifdef BCM5301X           
        /*Enable BRCM header for port 5*/
        system("et robowr 0x02 0x03 0x02");  /* Foxconn Bob added for 4708 */
#endif        
        /* CRC bypass and auto generation */
//        system("et robowr 0x34 0x06 0x11");
#if (defined __CONFIG_IGMP_SNOOPING__)
        if (nvram_match("emf_enable", "1"))
        {
#if 0
            /* Set IMP port default tag id */
            system("et robowr 0x34 0x20 0x02");
            /* Enable IPMC bypass V fwdmap */
            system("et robowr 0x34 0x01 0x2E");
            /* Set Multiport address enable */
            system("et robowr 0x04 0x0E 0x0AAA");
#endif
        }
#endif
        /* Turn on the flags for kernel space (et/emf/igs) handling */
        system("et robowr 0xFFFF 0xFE 0x03");
    }
    else
    {
#if 0
        system("et robowr 0x00 0x08 0x00");
        system("et robowr 0x00 0x0B 0x06");
        system("et robowr 0x02 0x00 0x00");
#ifdef BCM5301X          
        /*Enable BRCM header for port 8*/
        system("et robowr 0x02 0x03 0x01");  /* Foxconn Bob added for 4708 */
#endif        
        system("et robowr 0x34 0x06 0x10");
#if (defined __CONFIG_IGMP_SNOOPING__)
        system("et robowr 0x34 0x20 0x02");
        system("et robowr 0x34 0x01 0x0E");
        system("et robowr 0x04 0x0E 0x0000");
#endif
        if (nvram_match("qos_enable", "1") )
            system("et robowr 0xFFFF 0xFE 0x01");
        else if (!nvram_match("qos_port", ""))
            system("et robowr 0xFFFF 0xFE 0x02");
        else
            system("et robowr 0xFFFF 0xFE 0x00");
#endif
    }

    return 0;
}
/* foxconn added end, zacker, 01/13/2012, @iptv_igmp */

/* foxconn modified start, zacker, 01/13/2012, @iptv_igmp */
void config_switch(void)
{
    /* BCM5325 & BCM53115 switch request to change these vars
     * to output ethernet port tag/id in packets.
     */
    struct nvram_tuple generic_gmac3[] = {
        { "wan_ifname", "eth0", 0 },
        { "wan_ifnames", "eth0 ", 0 },

        { "vlan1ports", "1 2 3 4 5 7 8*", 0 },
        { "vlan2ports", "0 8u", 0 },
        { 0, 0, 0 }
    };

    struct nvram_tuple generic[] = {
        { "wan_ifname", "eth0", 0 },
        { "wan_ifnames", "eth0 ", 0 },

        { "vlan1ports", "1 2 3 4 5*", 0 },
        { "vlan2ports", "0 5u", 0 },
        { 0, 0, 0 }
    };

    struct nvram_tuple vlan_gmac3[] = {
        { "wan_ifname", "vlan2", 0 },
        { "wan_ifnames", "vlan2 ", 0 },

        { "vlan1ports", "1 2 3 4 5 7 8*", 0 },
        { "vlan2ports", "0 8", 0 },
        { 0, 0, 0 }
    };

    struct nvram_tuple vlan[] = {
        { "wan_ifname", "vlan2", 0 },
        { "wan_ifnames", "vlan2 ", 0 },

        { "vlan1ports", "1 2 3 4 5*", 0 },
        { "vlan2ports", "0 5", 0 },
        { 0, 0, 0 }
    };

    struct nvram_tuple *u;
    int commit = 0;

    if(nvram_match("gmac3_enable", "1"))
        u = generic_gmac3;
    else
        u = generic;
#ifndef BCA_HNDROUTER    	
    /* foxconn Bob modified start 08/26/2013, not to bridge eth0 and vlan1 in the same bridge */
    if (nvram_match("emf_enable", "1") || nvram_match("enable_ap_mode", "1") ) {
        if(nvram_match("gmac3_enable", "1"))
            u = vlan_gmac3;
        else
            u = vlan;
    }
    /* foxconn Bob modified end 08/26/2013, not to bridge eth0 and vlan1 in the same bridge */
#endif
    /* don't need vlan in repeater mode */
    if (nvram_match("wla_repeater", "1")
#if (defined INCLUDE_DUAL_BAND)
        || nvram_match("wlg_repeater", "1")
#endif
        ) {
    if(nvram_match("gmac3_enable", "1"))
        u = generic_gmac3;
    else
        u = generic;
    }

    for ( ; u && u->name; u++) {
        if (strcmp(nvram_safe_get(u->name), u->value)) {
            commit = 1;
            nvram_set(u->name, u->value);
        }
    }

    /*foxconn Han edited, 05/11/2015
    * From CSP 915149
    * details:
    * =========================================
    * Cathy Yeh 08-May-2015 12:40:27 AM 
    *      
    * Hi Han,
    *
    * Please find the 2nd switch's patch, 150506_erobo_patch.tgz, in attachment.
    * 1. Set nvram "erobo=1" to attach the 2nd switch
    * 2. Support et command for read/write the 2nd switch's registers:
    *   et -i eth0 erobord <page> <reg> [length] (read the reg of external switch, the usage is same as robord)
    *   et -i eth0 erobowr <page> <reg> <val> [length] (write the reg of external switch, the usage is same as robowr)
    * 3. Set nvram "evlanXXXXports" to configure the 2nd switch's vlan table:
    *   Ex: nvram set evlan1ports"0 1 2 3 4 5u"
    *
    *       Regards,
    *       Cathy  
    * ==========================================*/
    #ifdef CONFIG_2ND_SWITCH
    if(isTriBand())
    {
        nvram_set("erobo","1");
        nvram_set("evlan1ports", "0 1 2 3 4 5u");
    }
    #endif /*CONFIG_2ND_SWITCH*/

    if (commit) {
        cprintf("Commit new ethernet config...\n");
        nvram_commit();
        commit = 0;
    }
}
#endif
/* foxconn modified end, zacker, 01/13/2012, @iptv_igmp */

/* foxconn modified start, zacker, 01/04/2011 */
static int should_stop_wps(void)
{
    /* WPS LED OFF */
    if ((nvram_match("wla_wlanstate","Disable") || acosNvramConfig_match("wifi_on_off", "0"))
#if (defined INCLUDE_DUAL_BAND)
        && (nvram_match("wlg_wlanstate","Disable") || acosNvramConfig_match("wifi_on_off", "0"))
#endif
       )
        return WPS_LED_STOP_RADIO_OFF;

    /* WPS LED quick blink for 5sec */
    if (nvram_match("wps_mode", "disabled")
        || nvram_match("wla_repeater", "1")
#if (defined INCLUDE_DUAL_BAND)
        || nvram_match("wlg_repeater", "1")
#endif
       )
        return WPS_LED_STOP_DISABLED;

    /* WPS LED original action */
    return WPS_LED_STOP_NO;
}

static int is_secure_wl(void)
{
    /* for ACR5500 , there is only on WiFi LED for WPS */
#if defined(R6300v2) || defined(R6250) || defined(R6200v2) || defined(R7000) || defined(R8000) || defined(BCA_HNDROUTER)

    if ((acosNvramConfig_match("wla_wlanstate","Disable") || acosNvramConfig_match("wifi_on_off", "0"))
        && (acosNvramConfig_match("wlg_wlanstate","Disable") || acosNvramConfig_match("wifi_on_off", "0")) )
        return 0;

    return 1;
#else    
    
    if (   (!acosNvramConfig_match("wla_secu_type", "None")
            && (acosNvramConfig_match("wla_wlanstate","Enable") && acosNvramConfig_match("wifi_on_off", "1")))
#if (defined INCLUDE_DUAL_BAND)
        || (!acosNvramConfig_match("wlg_secu_type", "None")
            && (acosNvramConfig_match("wlg_wlanstate","Enable") && acosNvramConfig_match("wifi_on_off", "1")))
#endif
        )
        return 1;

    return 0;
#endif /* defined(R6300v2) */    
}

/* Foxconn added start, Wins, 04/20/2011 @RU_IPTV */
#ifdef CONFIG_RUSSIA_IPTV
static int is_russia_specific_support (void)
{
    int result = 0;
    char sku_name[8];

    /* Router Spec v2.0:                                                        *
     *   Case 1: RU specific firmware.                                          *
     *   Case 2: single firmware & region code is RU.                           *
     *   Case 3: WW firmware & GUI language is Russian.                         *
     *   Case 4: single firmware & region code is WW & GUI language is Russian. *
     * Currently, new built firmware will be single firmware.                   */
    strcpy(sku_name, nvram_get("sku_name"));
    if (!strcmp(sku_name, "RU"))
    {
        /* Case 2: single firmware & region code is RU. */
        /* Region is RU (0x0005) */
        result = 1;
    }
    else if (!strcmp(sku_name, "WW"))
    {
        /* Region is WW (0x0002) */
        char gui_region[16];
        strcpy(gui_region, nvram_get("gui_region"));
        if (!strcmp(gui_region, "Russian"))
        {
            /* Case 4: single firmware & region code is WW & GUI language is Russian */
            /* GUI language is Russian */
            result = 1;
        }
    }

    return result;
}
/* Foxconn add start, Edward zhang, 09/05/2012, @add IPTV support for PR SKU*/
static int is_china_specific_support (void)
{
    int result = 0;
    char sku_name[8];

    /* Router Spec v2.0:                                                        *
     *   Case 1: WW specific firmware.                                          *
     *   Case 2: single firmware & region code is PR.                           *
     *   Case 3: WW firmware & GUI language is Chinise.                         *
     *   Case 4: single firmware & region code is WW & GUI language is Chinise. *
     * Currently, new built firmware will be single firmware.                   */
    strcpy(sku_name, nvram_get("sku_name"));
    if (!strcmp(sku_name, "PR"))
    {
        /* Case 2: single firmware & region code is PR. */
        /* Region is PR (0x0004) */
        result = 1;
    }
    else if (!strcmp(sku_name, "WW"))
    {
        /* Region is WW (0x0002) */
        char gui_region[16];
        strcpy(gui_region, nvram_get("gui_region"));
        if (!strcmp(gui_region, "Chinese"))
        {
            /* Case 4: single firmware & region code is WW & GUI language is Chinise */
            /* GUI language is Chinise */
            result = 1;
        }
    }

    return result;
}
/* Foxconn add end, Edward zhang, 09/05/2012, @add IPTV support for PR SKU*/
#endif /* CONFIG_RUSSIA_IPTV */
/* Foxconn added end, Wins, 04/20/2011 @RU_IPTV */

/*Foxconn add start, edward zhang, 2013/07/03*/
#ifdef VLAN_SUPPORT
static int getVlanname(char vlanname[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE])
{
    char *var;

    if ((var = acosNvramConfig_get("vlan_name")) != NULL)
    {
        int num, i;
        num = getTokens(var, " ", vlanname, C_MAX_VLAN_RULE);
        for (i = 0; i< num; i++)
            restore_devname(vlanname[i]);
        return num;
    }

    return 0;
}



int getVlanRule(vlan_rule vlan[C_MAX_VLAN_RULE]) /* Foxconn modified by Max Ding, 11/22/2016 remove static, need use it in interface.c */
{
    int numVlanRule = 0 , i;
    char *var;
    char VlanName[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE];
    char VlanId[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE];
    char VlanPrio[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE];
    char VlanPorts[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE];
    char VlanRuleEnable[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE];
#ifdef CONFIG_ORANGE_ISP
    char VlanISP[C_MAX_VLAN_RULE][C_MAX_TOKEN_SIZE];
#endif
    if ( (var = acosNvramConfig_get("vlan_id")) != NULL )
    {
        getTokens(var, " ", VlanId, C_MAX_VLAN_RULE);
    }
    
    if ( (var=acosNvramConfig_get("vlan_prio")) != NULL )
    {
        getTokens(var, " ", VlanPrio, C_MAX_VLAN_RULE);
    }
    
    if ( (var=acosNvramConfig_get("vlan_ports")) != NULL )
    {
        getTokens(var, " ", VlanPorts, C_MAX_VLAN_RULE);
    }
 
    if ( (var=acosNvramConfig_get("vlan_rule_enable")) != NULL )
    {
        getTokens(var, " ", VlanRuleEnable, C_MAX_VLAN_RULE);
    }
#ifdef CONFIG_ORANGE_ISP
    if ( (var = acosNvramConfig_get("vlan_isp")) != NULL )
    {
        getTokens(var, " ", VlanISP, C_MAX_VLAN_RULE);
    }
#endif
    
    numVlanRule = getVlanname(VlanName);
    
    for(i=0;i<numVlanRule;i++)
    {
        strcpy( vlan[i].vlan_name , VlanName[i]);
        strcpy( vlan[i].vlan_id , VlanId[i]);
        strcpy( vlan[i].vlan_prio , VlanPrio[i]);
        //strcpy( vlan[i].vlan_ports , VlanPorts[i]);
        sprintf( vlan[i].vlan_ports,"%s",VlanPorts[i]);
        strcpy( vlan[i].enable_rule , VlanRuleEnable[i]);
#ifdef CONFIG_ORANGE_ISP
        strcpy( vlan[i].vlan_isp , VlanISP[i]);
#endif
    }
    
    return numVlanRule;
}
#endif

static int send_wps_led_cmd(int cmd, int arg)
{
    int ret_val=0;
    int fd;

    fd = open(DEV_WPS_LED, O_RDWR);
    if (fd < 0) 
        return -1;

    if (is_secure_wl())
        arg = 1;
    else
        arg = 0;

    switch (should_stop_wps())
    {
        case WPS_LED_STOP_RADIO_OFF:
            cmd = WPS_LED_BLINK_OFF;
            break;
            
        case WPS_LED_STOP_DISABLED:
            if (cmd == WPS_LED_BLINK_NORMAL)
                cmd = WPS_LED_BLINK_QUICK;
            break;
            
        case WPS_LED_STOP_NO:
        default:
            break;
    }

    ret_val = ioctl(fd, cmd, arg);
    close(fd);

    return ret_val;
}
/* foxconn modified end, zacker, 01/04/2011 */

#if defined(LINUX_2_6_36) && defined(__CONFIG_TREND_IQOS__)
void iqos_restore_defaults(void)
{
	nvram_set("broadstream_iqos_default_conf", "1");
}
#endif /* LINUX_2_6_36 & __CONFIG_TREND_IQOS__ */

/* Foxconn add start, Edward zhang, 09/14/2012, @add ARP PROTECTION support for RU SKU*/
#ifdef ARP_PROTECTION
static int getTokens(char *str, char *delimiter, char token[][C_MAX_TOKEN_SIZE], int maxNumToken)
{
    char temp[16*1024];    
    char *field;
    int numToken=0, i, j;
    char *ppLast = NULL;

    /* Check for empty string */
    if (str == NULL || str[0] == '\0')
        return 0;
   
    /* Now get the tokens */
    strcpy(temp, str);
    
    for (i=0; i<maxNumToken; i++)
    {
        if (i == 0)
            field = strtok_r(temp, delimiter, &ppLast);
        else 
            field = strtok_r(NULL, delimiter, &ppLast);

        /* Foxconn modified start, Wins, 06/27/2010 */
        //if (field == NULL || field[0] == '\0')
        if (field == NULL || (field != NULL && field[0] == '\0'))
        /* Foxconn modified end, Wins, 06/27/2010 */
        {
            for (j=i; j<maxNumToken; j++)
                token[j][0] = '\0';
            break;
        }

        numToken++;
        strcpy(token[i], field);
    }

    return numToken;
}

static int getReservedAddr(char reservedMacAddr[][C_MAX_TOKEN_SIZE], char reservedIpAddr[][C_MAX_TOKEN_SIZE])
/* Foxconn modified end, zacker, 10/31/2008, @lan_setup_change */
{
    int numReservedMac=0, numReservedIp=0;
    char *var;
    
    /* Read MAC and IP address tokens */
    if ( (var = acosNvramConfig_get("dhcp_resrv_mac")) != NULL )
    {
        numReservedMac = getTokens(var, " ", reservedMacAddr, C_MAX_RESERVED_IP);
    }
    
    if ( (var=acosNvramConfig_get("dhcp_resrv_ip")) != NULL )
    {
        numReservedIp = getTokens(var, " ", reservedIpAddr, C_MAX_RESERVED_IP);
    }
    
    if (numReservedMac != numReservedIp)
    {
        printf("getReservedAddr: reserved mac and ip not match\n");
    }
    
    return (numReservedMac<numReservedIp ? numReservedMac:numReservedIp);
}

void config_arp_table(void)
{
    if(acosNvramConfig_match("arp_enable","enable"))
    {
        int i;
        char resrvMacAddr[C_MAX_RESERVED_IP][C_MAX_TOKEN_SIZE];
        char resrvIpAddr[C_MAX_RESERVED_IP][C_MAX_TOKEN_SIZE];
        int numResrvAddr = getReservedAddr(resrvMacAddr, resrvIpAddr);
        char arp_cmd[64];
        for (i=0; i<numResrvAddr; i++)
        {
            sprintf(arp_cmd,"arp -s %s %s",resrvIpAddr[i],resrvMacAddr[i]);
            printf("%s\n",arp_cmd);
            system(arp_cmd);
        }
    }
    
    return 0;
}
#endif
/* Foxconn add end, Edward zhang, 09/14/2012, @add ARP PROTECTION support for RU SKU*/

#ifdef SUPPORT_2DOT5G_WAN
int switch_2dot5G_role(int type)
{
    if(type) /*2dot5g as WAN*/
    {
        printf("\n=========RC use 2dot5G as WAN\n\n");
        system("ifconfig eth0 down");
        system("ifconfig eth5 down");
        system("ethctl eth0 phy-crossbar port 9");
        system("ethctl eth5 phy-crossbar port 10");
        system("ifconfig eth0 up");
        system("ifconfig eth5 up");
    }
    else
    {
        printf("\n=========RC use 2dot5G as LAN\n\n");
        system("ifconfig eth0 down");
        system("ifconfig eth5 down");
        system("ethctl eth0 phy-crossbar port 10");
        system("ethctl eth5 phy-crossbar port 9");
        system("ifconfig eth0 up");
        system("ifconfig eth5 up");
    }
   
}
#endif /*SUPPORT_2DOT5G_WAN*/
