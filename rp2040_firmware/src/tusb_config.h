#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE)
#define CFG_TUD_ENDPOINT0_SIZE      64

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUD_DEVICE_BUFSIZE      512

//------------- CLASS -------------//
#define CFG_TUD_HID                 1
#define CFG_TUD_ECM_RNDIS           1

// HID buffer size
#define CFG_TUD_HID_EP_BUFSIZE      64

// Net buffer size
#define CFG_TUD_NET_BUFSIZE         (TUD_NET_MTU + 20)

#endif /* TUSB_CONFIG_H */
