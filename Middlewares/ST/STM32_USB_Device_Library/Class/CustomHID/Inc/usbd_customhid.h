/**
  ******************************************************************************
  * @file    usbd_customhid.h
  * @author  MCD Application Team
  * @brief   header file for the usbd_customhid.c file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                      www.st.com/SLA0044
  *
  ******************************************************************************
  */

#ifndef __USB_CUSTOMHID_H
#define __USB_CUSTOMHID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_ioreq.h"

/* Number of independent HID interfaces (Keyboard + Mouse + Gamepad) */
#define USBD_COMPOSITE_HID_NUM_ITF          3U

/* Endpoint addresses */
#define CUSTOM_HID_EPIN_ADDR                0x81U   /* Interface 0: Keyboard IN  */
#define CUSTOM_HID_EPOUT_ADDR               0x01U   /* Interface 0: Keyboard OUT (LEDs) */
#define CUSTOM_HID_EP2IN_ADDR               0x82U   /* Interface 1: Mouse IN     */
#define CUSTOM_HID_EP3IN_ADDR               0x83U   /* Interface 2: Gamepad IN   */

/* Max packet sizes per interface (no Report ID, pure data) */
#define CUSTOM_HID_KBD_EPIN_SIZE            8U      /* modifier+reserved+6 keycodes */
#define CUSTOM_HID_KBD_EPOUT_SIZE           1U      /* LED bitmask */
#define CUSTOM_HID_MOUSE_EPIN_SIZE          4U      /* buttons+x+y+wheel */
#define CUSTOM_HID_GP_EPIN_SIZE             8U      /* 2 btn bytes + 6 axis bytes */

/* Legacy alias kept so usbd_conf.c sizeof() still compiles */
#define CUSTOM_HID_EPIN_SIZE                CUSTOM_HID_KBD_EPIN_SIZE
#define CUSTOM_HID_EPOUT_SIZE               CUSTOM_HID_KBD_EPOUT_SIZE

/* Configuration descriptor total size:
   9 (cfg) + [9+9+7+7] (kbd) + [9+9+7] (mouse) + [9+9+7] (gamepad) = 91 */
#define USB_CUSTOM_HID_CONFIG_DESC_SIZ      91U
#define USB_CUSTOM_HID_DESC_SIZ             9U

/* Per-interface HID report descriptor sizes */
#define USBD_CUSTOM_HID_KBD_REPORT_DESC_SIZE    63U
#define USBD_CUSTOM_HID_MOUSE_REPORT_DESC_SIZE  52U
#define USBD_CUSTOM_HID_GP_REPORT_DESC_SIZE     48U

#ifndef CUSTOM_HID_HS_BINTERVAL
#define CUSTOM_HID_HS_BINTERVAL             0x05U
#endif

#ifndef CUSTOM_HID_FS_BINTERVAL
#define CUSTOM_HID_FS_BINTERVAL             0x05U
#endif

#ifndef USBD_CUSTOMHID_OUTREPORT_BUF_SIZE
#define USBD_CUSTOMHID_OUTREPORT_BUF_SIZE   0x02U
#endif

#define CUSTOM_HID_DESCRIPTOR_TYPE          0x21U
#define CUSTOM_HID_REPORT_DESC              0x22U

#define CUSTOM_HID_REQ_SET_PROTOCOL         0x0BU
#define CUSTOM_HID_REQ_GET_PROTOCOL         0x03U
#define CUSTOM_HID_REQ_SET_IDLE             0x0AU
#define CUSTOM_HID_REQ_GET_IDLE             0x02U
#define CUSTOM_HID_REQ_SET_REPORT           0x09U
#define CUSTOM_HID_REQ_GET_REPORT           0x01U

typedef enum {
    CUSTOM_HID_IDLE = 0U,
    CUSTOM_HID_BUSY,
} CUSTOM_HID_StateTypeDef;

/* Per-interface report descriptor pointers + sizes, plus shared callbacks */
typedef struct _USBD_CUSTOM_HID_Itf {
    uint8_t  *pReportDesc[USBD_COMPOSITE_HID_NUM_ITF];
    uint16_t  ReportDescSize[USBD_COMPOSITE_HID_NUM_ITF];
    int8_t  (*Init)(void);
    int8_t  (*DeInit)(void);
    int8_t  (*OutEvent)(uint8_t itf_idx, uint8_t data);
} USBD_CUSTOM_HID_ItfTypeDef;

/* One state slot per IN endpoint */
typedef struct {
    uint8_t               Report_buf[USBD_CUSTOMHID_OUTREPORT_BUF_SIZE];
    uint32_t              Protocol[USBD_COMPOSITE_HID_NUM_ITF];
    uint32_t              IdleState[USBD_COMPOSITE_HID_NUM_ITF];
    uint32_t              AltSetting[USBD_COMPOSITE_HID_NUM_ITF];
    uint32_t              IsReportAvailable;
    uint8_t               OutReportInterface;
    CUSTOM_HID_StateTypeDef state[USBD_COMPOSITE_HID_NUM_ITF];
} USBD_CUSTOM_HID_HandleTypeDef;

extern USBD_ClassTypeDef USBD_CUSTOM_HID;
#define USBD_CUSTOM_HID_CLASS    &USBD_CUSTOM_HID

/* itf_idx: 0=Keyboard, 1=Mouse, 2=Gamepad */
uint8_t USBD_CUSTOM_HID_SendReport(USBD_HandleTypeDef *pdev,
                                   uint8_t itf_idx,
                                   uint8_t *report,
                                   uint16_t len);

uint8_t USBD_CUSTOM_HID_RegisterInterface(USBD_HandleTypeDef *pdev,
                                          USBD_CUSTOM_HID_ItfTypeDef *fops);

#ifdef __cplusplus
}
#endif

#endif /* __USB_CUSTOMHID_H */
