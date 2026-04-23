/**
  ******************************************************************************
  * @file    usbd_customhid.c
  * @author  MCD Application Team
  * @brief   Composite HID class driver: Keyboard (IF0) + Mouse (IF1) + Gamepad (IF2).
  *          Each interface owns an independent IN endpoint so reports never block
  *          each other. Keyboard additionally has an OUT endpoint for LED state.
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

#include "usbd_customhid.h"
#include "usbd_ctlreq.h"

static uint8_t USBD_CUSTOM_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CUSTOM_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CUSTOM_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t *USBD_CUSTOM_HID_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_CUSTOM_HID_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_CUSTOM_HID_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_CUSTOM_HID_GetDeviceQualifierDesc(uint16_t *length);
static uint8_t USBD_CUSTOM_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CUSTOM_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CUSTOM_HID_EP0_RxReady(USBD_HandleTypeDef *pdev);

USBD_ClassTypeDef USBD_CUSTOM_HID = {
    USBD_CUSTOM_HID_Init,
    USBD_CUSTOM_HID_DeInit,
    USBD_CUSTOM_HID_Setup,
    NULL,
    USBD_CUSTOM_HID_EP0_RxReady,
    USBD_CUSTOM_HID_DataIn,
    USBD_CUSTOM_HID_DataOut,
    NULL,
    NULL,
    NULL,
    USBD_CUSTOM_HID_GetHSCfgDesc,
    USBD_CUSTOM_HID_GetFSCfgDesc,
    USBD_CUSTOM_HID_GetOtherSpeedCfgDesc,
    USBD_CUSTOM_HID_GetDeviceQualifierDesc,
};

/*
 * Configuration descriptor: 91 bytes
 *
 * Offsets of per-interface HID descriptors (used by GET_DESCRIPTOR):
 *   Interface 0 (Keyboard) HID desc: byte 18
 *   Interface 1 (Mouse)    HID desc: byte 50
 *   Interface 2 (Gamepad)  HID desc: byte 75
 */
__ALIGN_BEGIN static uint8_t USBD_CUSTOM_HID_CfgFSDesc[USB_CUSTOM_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
    /* Configuration Descriptor */
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(USB_CUSTOM_HID_CONFIG_DESC_SIZ), HIBYTE(USB_CUSTOM_HID_CONFIG_DESC_SIZ),
    0x03,           /* bNumInterfaces */
    0x01,           /* bConfigurationValue */
    0x00,           /* iConfiguration */
    0xC0,           /* bmAttributes: self powered */
    0x32,           /* MaxPower 100 mA */

    /* ---- Interface 0: Keyboard ---- */
    0x09, USB_DESC_TYPE_INTERFACE,
    0x00,           /* bInterfaceNumber */
    0x00,           /* bAlternateSetting */
    0x02,           /* bNumEndpoints: IN + OUT */
    0x03,           /* bInterfaceClass: HID */
    0x01,           /* bInterfaceSubClass: Boot */
    0x01,           /* bInterfaceProtocol: Keyboard */
    0x00,           /* iInterface */

    /* HID Descriptor — Keyboard (offset 18) */
    0x09, CUSTOM_HID_DESCRIPTOR_TYPE,
    0x11, 0x01,     /* bcdHID 1.11 */
    0x00,           /* bCountryCode */
    0x01,           /* bNumDescriptors */
    0x22,           /* bDescriptorType: Report */
    LOBYTE(USBD_CUSTOM_HID_KBD_REPORT_DESC_SIZE),
    HIBYTE(USBD_CUSTOM_HID_KBD_REPORT_DESC_SIZE),

    /* EP1 IN: Keyboard (offset 27) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    CUSTOM_HID_EPIN_ADDR,   /* 0x81 */
    0x03,                   /* Interrupt */
    LOBYTE(CUSTOM_HID_KBD_EPIN_SIZE), HIBYTE(CUSTOM_HID_KBD_EPIN_SIZE),
    0x01,                   /* bInterval: 1 ms */

    /* EP1 OUT: Keyboard LEDs (offset 34) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    CUSTOM_HID_EPOUT_ADDR,  /* 0x01 */
    0x03,                   /* Interrupt */
    LOBYTE(CUSTOM_HID_KBD_EPOUT_SIZE), HIBYTE(CUSTOM_HID_KBD_EPOUT_SIZE),
    0x01,                   /* bInterval: 1 ms */

    /* ---- Interface 1: Mouse ---- */
    0x09, USB_DESC_TYPE_INTERFACE,
    0x01,           /* bInterfaceNumber */
    0x00,
    0x01,           /* bNumEndpoints: 1 IN */
    0x03,           /* HID */
    0x00,           /* No boot subclass */
    0x02,           /* bInterfaceProtocol: Mouse */
    0x00,

    /* HID Descriptor — Mouse (offset 50) */
    0x09, CUSTOM_HID_DESCRIPTOR_TYPE,
    0x11, 0x01,
    0x00,
    0x01,
    0x22,
    LOBYTE(USBD_CUSTOM_HID_MOUSE_REPORT_DESC_SIZE),
    HIBYTE(USBD_CUSTOM_HID_MOUSE_REPORT_DESC_SIZE),

    /* EP2 IN: Mouse (offset 59) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    CUSTOM_HID_EP2IN_ADDR,  /* 0x82 */
    0x03,
    LOBYTE(CUSTOM_HID_MOUSE_EPIN_SIZE), HIBYTE(CUSTOM_HID_MOUSE_EPIN_SIZE),
    0x01,                   /* bInterval: 1 ms */

    /* ---- Interface 2: Gamepad ---- */
    0x09, USB_DESC_TYPE_INTERFACE,
    0x02,           /* bInterfaceNumber */
    0x00,
    0x01,           /* bNumEndpoints: 1 IN */
    0x03,           /* HID */
    0x00,
    0x00,
    0x00,

    /* HID Descriptor — Gamepad (offset 75) */
    0x09, CUSTOM_HID_DESCRIPTOR_TYPE,
    0x11, 0x01,
    0x00,
    0x01,
    0x22,
    LOBYTE(USBD_CUSTOM_HID_GP_REPORT_DESC_SIZE),
    HIBYTE(USBD_CUSTOM_HID_GP_REPORT_DESC_SIZE),

    /* EP3 IN: Gamepad (offset 84) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    CUSTOM_HID_EP3IN_ADDR,  /* 0x83 */
    0x03,
    LOBYTE(CUSTOM_HID_GP_EPIN_SIZE), HIBYTE(CUSTOM_HID_GP_EPIN_SIZE),
    0x04,                   /* bInterval: 4 ms (gamepad axes, 250 Hz sufficient) */
};

/* HS and OtherSpeed share the same layout — copy at compile time via same array */
__ALIGN_BEGIN static uint8_t USBD_CUSTOM_HID_CfgHSDesc[USB_CUSTOM_HID_CONFIG_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN static uint8_t USBD_CUSTOM_HID_OtherSpeedCfgDesc[USB_CUSTOM_HID_CONFIG_DESC_SIZ] __ALIGN_END;

/* Device qualifier (required for HS-capable devices) */
__ALIGN_BEGIN static uint8_t USBD_CUSTOM_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    0x40,
    0x01,
    0x00,
};

/* Byte offsets of the HID descriptor within CfgFSDesc for each interface */
static const uint8_t hid_desc_offset[USBD_COMPOSITE_HID_NUM_ITF] = { 18U, 50U, 75U };

/* IN endpoint addresses indexed by interface */
static const uint8_t epin_addr[USBD_COMPOSITE_HID_NUM_ITF] = {
    CUSTOM_HID_EPIN_ADDR,
    CUSTOM_HID_EP2IN_ADDR,
    CUSTOM_HID_EP3IN_ADDR,
};

/* -------------------------------------------------------------------------- */

static uint8_t USBD_CUSTOM_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    USBD_CUSTOM_HID_HandleTypeDef *hhid;

    /* Open all IN endpoints */
    USBD_LL_OpenEP(pdev, CUSTOM_HID_EPIN_ADDR,  USBD_EP_TYPE_INTR, CUSTOM_HID_KBD_EPIN_SIZE);
    pdev->ep_in[CUSTOM_HID_EPIN_ADDR  & 0x0FU].is_used = 1U;

    USBD_LL_OpenEP(pdev, CUSTOM_HID_EP2IN_ADDR, USBD_EP_TYPE_INTR, CUSTOM_HID_MOUSE_EPIN_SIZE);
    pdev->ep_in[CUSTOM_HID_EP2IN_ADDR & 0x0FU].is_used = 1U;

    USBD_LL_OpenEP(pdev, CUSTOM_HID_EP3IN_ADDR, USBD_EP_TYPE_INTR, CUSTOM_HID_GP_EPIN_SIZE);
    pdev->ep_in[CUSTOM_HID_EP3IN_ADDR & 0x0FU].is_used = 1U;

    /* Open keyboard LED OUT endpoint */
    USBD_LL_OpenEP(pdev, CUSTOM_HID_EPOUT_ADDR, USBD_EP_TYPE_INTR, CUSTOM_HID_KBD_EPOUT_SIZE);
    pdev->ep_out[CUSTOM_HID_EPOUT_ADDR & 0x0FU].is_used = 1U;

    pdev->pClassData = USBD_malloc(sizeof(USBD_CUSTOM_HID_HandleTypeDef));
    if (pdev->pClassData == NULL) return 1U;

    hhid = (USBD_CUSTOM_HID_HandleTypeDef *)pdev->pClassData;
    /* Zero everything — all states start as CUSTOM_HID_IDLE (0) */
    for (uint8_t i = 0; i < USBD_COMPOSITE_HID_NUM_ITF; i++) {
        hhid->state[i]    = CUSTOM_HID_IDLE;
        hhid->Protocol[i] = 0U;
        hhid->IdleState[i]= 0U;
        hhid->AltSetting[i]= 0U;
    }
    hhid->IsReportAvailable  = 0U;
    hhid->OutReportInterface = 0U;

    ((USBD_CUSTOM_HID_ItfTypeDef *)pdev->pUserData)->Init();

    /* Arm the keyboard LED OUT endpoint */
    USBD_LL_PrepareReceive(pdev, CUSTOM_HID_EPOUT_ADDR,
                           hhid->Report_buf, USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);

    /* Copy FS descriptor to HS / OtherSpeed arrays (same timing) */
    for (uint16_t i = 0; i < USB_CUSTOM_HID_CONFIG_DESC_SIZ; i++) {
        USBD_CUSTOM_HID_CfgHSDesc[i]         = USBD_CUSTOM_HID_CfgFSDesc[i];
        USBD_CUSTOM_HID_OtherSpeedCfgDesc[i] = USBD_CUSTOM_HID_CfgFSDesc[i];
    }

    return USBD_OK;
}

static uint8_t USBD_CUSTOM_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    USBD_LL_CloseEP(pdev, CUSTOM_HID_EPIN_ADDR);
    pdev->ep_in[CUSTOM_HID_EPIN_ADDR  & 0x0FU].is_used = 0U;

    USBD_LL_CloseEP(pdev, CUSTOM_HID_EP2IN_ADDR);
    pdev->ep_in[CUSTOM_HID_EP2IN_ADDR & 0x0FU].is_used = 0U;

    USBD_LL_CloseEP(pdev, CUSTOM_HID_EP3IN_ADDR);
    pdev->ep_in[CUSTOM_HID_EP3IN_ADDR & 0x0FU].is_used = 0U;

    USBD_LL_CloseEP(pdev, CUSTOM_HID_EPOUT_ADDR);
    pdev->ep_out[CUSTOM_HID_EPOUT_ADDR & 0x0FU].is_used = 0U;

    if (pdev->pClassData != NULL) {
        ((USBD_CUSTOM_HID_ItfTypeDef *)pdev->pUserData)->DeInit();
        USBD_free(pdev->pClassData);
        pdev->pClassData = NULL;
    }
    return USBD_OK;
}

static uint8_t USBD_CUSTOM_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    USBD_CUSTOM_HID_HandleTypeDef *hhid = (USBD_CUSTOM_HID_HandleTypeDef *)pdev->pClassData;
    USBD_CUSTOM_HID_ItfTypeDef    *fops = (USBD_CUSTOM_HID_ItfTypeDef *)pdev->pUserData;
    uint16_t len    = 0U;
    uint8_t *pbuf   = NULL;
    uint16_t status = 0U;
    uint8_t  ret    = USBD_OK;
    uint8_t  itf    = (uint8_t)(req->wIndex & 0xFFU);

    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
        case USB_REQ_TYPE_CLASS:
            if (itf >= USBD_COMPOSITE_HID_NUM_ITF) {
                USBD_CtlError(pdev, req);
                return USBD_FAIL;
            }
            switch (req->bRequest)
            {
                case CUSTOM_HID_REQ_SET_PROTOCOL:
                    hhid->Protocol[itf] = (uint8_t)(req->wValue);
                    break;
                case CUSTOM_HID_REQ_GET_PROTOCOL:
                    USBD_CtlSendData(pdev, (uint8_t *)&hhid->Protocol[itf], 1U);
                    break;
                case CUSTOM_HID_REQ_SET_IDLE:
                    hhid->IdleState[itf] = (uint8_t)(req->wValue >> 8);
                    break;
                case CUSTOM_HID_REQ_GET_IDLE:
                    USBD_CtlSendData(pdev, (uint8_t *)&hhid->IdleState[itf], 1U);
                    break;
                case CUSTOM_HID_REQ_SET_REPORT:
                    hhid->IsReportAvailable  = 1U;
                    hhid->OutReportInterface = itf;
                    USBD_CtlPrepareRx(pdev, hhid->Report_buf, req->wLength);
                    break;
                default:
                    USBD_CtlError(pdev, req);
                    ret = USBD_FAIL;
                    break;
            }
            break;

        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest)
            {
                case USB_REQ_GET_STATUS:
                    if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                        USBD_CtlSendData(pdev, (uint8_t *)&status, 2U);
                    } else {
                        USBD_CtlError(pdev, req);
                        ret = USBD_FAIL;
                    }
                    break;

                case USB_REQ_GET_DESCRIPTOR:
                    if ((req->wValue >> 8) == CUSTOM_HID_REPORT_DESC) {
                        if (itf < USBD_COMPOSITE_HID_NUM_ITF) {
                            len  = MIN(fops->ReportDescSize[itf], req->wLength);
                            pbuf = fops->pReportDesc[itf];
                        }
                    } else if ((req->wValue >> 8) == CUSTOM_HID_DESCRIPTOR_TYPE) {
                        if (itf < USBD_COMPOSITE_HID_NUM_ITF) {
                            pbuf = &USBD_CUSTOM_HID_CfgFSDesc[hid_desc_offset[itf]];
                            len  = MIN(USB_CUSTOM_HID_DESC_SIZ, req->wLength);
                        }
                    }
                    USBD_CtlSendData(pdev, pbuf, len);
                    break;

                case USB_REQ_GET_INTERFACE:
                    if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                        if (itf < USBD_COMPOSITE_HID_NUM_ITF) {
                            USBD_CtlSendData(pdev, (uint8_t *)&hhid->AltSetting[itf], 1U);
                        } else {
                            USBD_CtlError(pdev, req);
                            ret = USBD_FAIL;
                        }
                    } else {
                        USBD_CtlError(pdev, req);
                        ret = USBD_FAIL;
                    }
                    break;

                case USB_REQ_SET_INTERFACE:
                    if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                        if (itf < USBD_COMPOSITE_HID_NUM_ITF) {
                            hhid->AltSetting[itf] = (uint8_t)(req->wValue);
                        } else {
                            USBD_CtlError(pdev, req);
                            ret = USBD_FAIL;
                        }
                    } else {
                        USBD_CtlError(pdev, req);
                        ret = USBD_FAIL;
                    }
                    break;

                default:
                    USBD_CtlError(pdev, req);
                    ret = USBD_FAIL;
                    break;
            }
            break;

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
    }
    return ret;
}

uint8_t USBD_CUSTOM_HID_SendReport(USBD_HandleTypeDef *pdev,
                                   uint8_t itf_idx,
                                   uint8_t *report,
                                   uint16_t len)
{
    USBD_CUSTOM_HID_HandleTypeDef *hhid = (USBD_CUSTOM_HID_HandleTypeDef *)pdev->pClassData;

    if (pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (itf_idx >= USBD_COMPOSITE_HID_NUM_ITF)    return USBD_FAIL;
    if (hhid->state[itf_idx] != CUSTOM_HID_IDLE)  return USBD_BUSY;

    hhid->state[itf_idx] = CUSTOM_HID_BUSY;
    USBD_LL_Transmit(pdev, epin_addr[itf_idx], report, len);
    return USBD_OK;
}

static uint8_t USBD_CUSTOM_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    /* epnum = endpoint number without direction bit (1, 2, or 3) */
    USBD_CUSTOM_HID_HandleTypeDef *hhid = (USBD_CUSTOM_HID_HandleTypeDef *)pdev->pClassData;
    if (epnum >= 1U && epnum <= USBD_COMPOSITE_HID_NUM_ITF) {
        hhid->state[epnum - 1U] = CUSTOM_HID_IDLE;
    }
    return USBD_OK;
}

static uint8_t USBD_CUSTOM_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    /* Only EP1 OUT is used (keyboard LED state, interface 0) */
    (void)epnum;
    USBD_CUSTOM_HID_HandleTypeDef *hhid = (USBD_CUSTOM_HID_HandleTypeDef *)pdev->pClassData;

    ((USBD_CUSTOM_HID_ItfTypeDef *)pdev->pUserData)->OutEvent(0U, hhid->Report_buf[0]);

    USBD_LL_PrepareReceive(pdev, CUSTOM_HID_EPOUT_ADDR,
                           hhid->Report_buf, USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);
    return USBD_OK;
}

static uint8_t USBD_CUSTOM_HID_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    USBD_CUSTOM_HID_HandleTypeDef *hhid = (USBD_CUSTOM_HID_HandleTypeDef *)pdev->pClassData;
    if (hhid->IsReportAvailable == 1U) {
        ((USBD_CUSTOM_HID_ItfTypeDef *)pdev->pUserData)->OutEvent(
            hhid->OutReportInterface, hhid->Report_buf[0]);
        hhid->IsReportAvailable = 0U;
    }
    return USBD_OK;
}

static uint8_t *USBD_CUSTOM_HID_GetFSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_CUSTOM_HID_CfgFSDesc);
    return USBD_CUSTOM_HID_CfgFSDesc;
}

static uint8_t *USBD_CUSTOM_HID_GetHSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_CUSTOM_HID_CfgHSDesc);
    return USBD_CUSTOM_HID_CfgHSDesc;
}

static uint8_t *USBD_CUSTOM_HID_GetOtherSpeedCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_CUSTOM_HID_OtherSpeedCfgDesc);
    return USBD_CUSTOM_HID_OtherSpeedCfgDesc;
}

static uint8_t *USBD_CUSTOM_HID_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = sizeof(USBD_CUSTOM_HID_DeviceQualifierDesc);
    return USBD_CUSTOM_HID_DeviceQualifierDesc;
}

uint8_t USBD_CUSTOM_HID_RegisterInterface(USBD_HandleTypeDef *pdev,
                                          USBD_CUSTOM_HID_ItfTypeDef *fops)
{
    if (fops == NULL) return USBD_FAIL;
    pdev->pUserData = fops;
    return USBD_OK;
}
