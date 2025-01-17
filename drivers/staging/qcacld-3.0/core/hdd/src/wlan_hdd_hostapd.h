/*
 * Copyright (c) 2013-2018 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(WLAN_HDD_HOSTAPD_H)
#define WLAN_HDD_HOSTAPD_H

/**
 * DOC: wlan_hdd_hostapd.h
 *
 * WLAN Host Device driver hostapd header file
 */

/* Include files */

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <qdf_list.h>
#include <qdf_types.h>
#include <wlan_hdd_main.h>

/* Preprocessor definitions and constants */

/* max length of command string in hostapd ioctl */
#define HOSTAPD_IOCTL_COMMAND_STRLEN_MAX   8192

struct hdd_adapter *hdd_wlan_create_ap_dev(struct hdd_context *hdd_ctx,
				      tSirMacAddr macAddr,
				      unsigned char name_assign_type,
				      uint8_t *name);

QDF_STATUS hdd_unregister_hostapd(struct hdd_adapter *adapter, bool rtnl_held);

eCsrAuthType
hdd_translate_rsn_to_csr_auth_type(uint8_t auth_suite[4]);

int hdd_softap_set_channel_change(struct net_device *dev,
					int target_channel,
					enum phy_ch_width target_bw,
					bool forced);

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
void hdd_sap_restart_with_channel_switch(struct hdd_adapter *adapter,
				uint32_t target_channel,
				uint32_t target_bw,
				bool forced);
/**
 * hdd_sap_restart_chan_switch_cb() - Function to restart SAP with
 * a different channel
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 * @channel: channel to switch
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * This function restarts SAP with a different channel
 *
 * Return: None
 *
 */
void hdd_sap_restart_chan_switch_cb(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id, uint32_t channel,
				    uint32_t channel_bw,
				    bool forced);
/**
 * wlan_hdd_get_channel_for_sap_restart() - Function to get
 * suitable channel and restart SAP
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 * @channel: channel to be returned
 * @sec_ch: secondary channel to be returned
 *
 * This function gets the channel parameters to restart SAP
 *
 * Return: None
 *
 */
QDF_STATUS wlan_hdd_get_channel_for_sap_restart(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, uint8_t *channel,
				uint8_t *sec_ch);
#endif

eCsrEncryptionType
hdd_translate_rsn_to_csr_encryption_type(uint8_t cipher_suite[4]);

eCsrEncryptionType
hdd_translate_rsn_to_csr_encryption_type(uint8_t cipher_suite[4]);

eCsrAuthType
hdd_translate_wpa_to_csr_auth_type(uint8_t auth_suite[4]);

eCsrEncryptionType
hdd_translate_wpa_to_csr_encryption_type(uint8_t cipher_suite[4]);

QDF_STATUS hdd_softap_sta_deauth(struct hdd_adapter *adapter,
		struct csr_del_sta_params *pDelStaParams);
void hdd_softap_sta_disassoc(struct hdd_adapter *adapter,
			     struct csr_del_sta_params *pDelStaParams);

QDF_STATUS hdd_hostapd_sap_event_cb(tpSap_Event pSapEvent,
				    void *context);
/**
 * hdd_init_ap_mode() - to init the AP adaptor
 * @adapter: SAP/GO adapter
 * @rtnl_held: flag to indicate if RTNL lock needs to be acquired
 *
 * This API can be called to open the SAP session as well as
 * to create and store the vdev object. It also initializes necessary
 * SAP adapter related params.
 */
QDF_STATUS hdd_init_ap_mode(struct hdd_adapter *adapter, bool reinit);
/**
 * hdd_deinit_ap_mode() - to deinit the AP adaptor
 * @hdd_ctx: pointer to hdd_ctx
 * @adapter: SAP/GO adapter
 * @rtnl_held: flag to indicate if RTNL lock needs to be acquired
 *
 * This API can be called to close the SAP session as well as
 * release the vdev object completely. It also deinitializes necessary
 * SAP adapter related params.
 */
void hdd_deinit_ap_mode(struct hdd_context *hdd_ctx,
			struct hdd_adapter *adapter, bool rtnl_held);
void hdd_set_ap_ops(struct net_device *dev);
/**
 * hdd_sap_create_ctx() - Wrapper API to create SAP context
 * @adapter: pointer to adapter
 *
 * This wrapper API can be called to create the sap context. It will
 * eventually calls SAP API to create the sap context
 *
 * Return: true or false based on overall success or failure
 */
bool hdd_sap_create_ctx(struct hdd_adapter *adapter);
/**
 * hdd_sap_destroy_ctx() - Wrapper API to destroy SAP context
 * @adapter: pointer to adapter
 *
 * This wrapper API can be called to destroy the sap context. It will
 * eventually calls SAP API to destroy the sap context
 *
 * Return: true or false based on overall success or failure
 */
bool hdd_sap_destroy_ctx(struct hdd_adapter *adapter);
int hdd_hostapd_stop(struct net_device *dev);
int hdd_sap_context_init(struct hdd_context *hdd_ctx);
void hdd_sap_context_destroy(struct hdd_context *hdd_ctx);
#ifdef QCA_HT_2040_COEX
QDF_STATUS hdd_set_sap_ht2040_mode(struct hdd_adapter *adapter,
				   uint8_t channel_type);
#endif

int wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy,
			      struct net_device *dev);

int wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
			       struct net_device *dev,
			       struct cfg80211_ap_settings *params);

int wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
				    struct net_device *dev,
				    struct cfg80211_beacon_data *params);

/**
 * hdd_is_peer_associated - is peer connected to softap
 * @adapter: pointer to softap adapter
 * @mac_addr: address to check in peer list
 *
 * This function has to be invoked only when bss is started and is used
 * to check whether station with specified addr is peer or not
 *
 * Return: true if peer mac, else false
 */
bool hdd_is_peer_associated(struct hdd_adapter *adapter,
			    struct qdf_mac_addr *mac_addr);

int hdd_destroy_acs_timer(struct hdd_adapter *adapter);

QDF_STATUS wlan_hdd_config_acs(struct hdd_context *hdd_ctx,
			       struct hdd_adapter *adapter);

void hdd_sap_indicate_disconnect_for_sta(struct hdd_adapter *adapter);
#endif /* end #if !defined(WLAN_HDD_HOSTAPD_H) */
