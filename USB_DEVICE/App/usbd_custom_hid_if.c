/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_custom_hid_if.c
  * @version        : v2.0_Cube
  * @brief          : USB Device Composite HID interface callbacks.
  *                   Interface 0: Keyboard (boot-compatible, no Report ID)
  *                   Interface 1: Mouse    (no Report ID)
  *                   Interface 2: Gamepad  (no Report ID)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "usbd_custom_hid_if.h"

/* USER CODE BEGIN INCLUDE */
#include "main.h"
/* USER CODE END INCLUDE */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* ---- Interface 0: Keyboard report descriptor (63 bytes) ---- */
__ALIGN_BEGIN static uint8_t HID_KbdReportDesc[USBD_CUSTOM_HID_KBD_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
  0x09, 0x06,  // USAGE (Keyboard)
  0xa1, 0x01,  // COLLECTION (Application)
  0x05, 0x07,  //   USAGE_PAGE (Keyboard)
  0x19, 0xe0,  //   USAGE_MINIMUM (Keyboard LeftControl)
  0x29, 0xe7,  //   USAGE_MAXIMUM (Keyboard Right GUI)
  0x15, 0x00,  //   LOGICAL_MINIMUM (0)
  0x25, 0x01,  //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,  //   REPORT_SIZE (1)
  0x95, 0x08,  //   REPORT_COUNT (8)
  0x81, 0x02,  //   INPUT (Data,Var,Abs)          <- modifier byte
  0x95, 0x01,  //   REPORT_COUNT (1)
  0x75, 0x08,  //   REPORT_SIZE (8)
  0x81, 0x03,  //   INPUT (Cnst,Var,Abs)           <- reserved byte
  0x95, 0x05,  //   REPORT_COUNT (5)
  0x75, 0x01,  //   REPORT_SIZE (1)
  0x05, 0x08,  //   USAGE_PAGE (LEDs)
  0x19, 0x01,  //   USAGE_MINIMUM (Num Lock)
  0x29, 0x05,  //   USAGE_MAXIMUM (Kana)
  0x91, 0x02,  //   OUTPUT (Data,Var,Abs)          <- LED 5 bits
  0x95, 0x01,  //   REPORT_COUNT (1)
  0x75, 0x03,  //   REPORT_SIZE (3)
  0x91, 0x03,  //   OUTPUT (Cnst,Var,Abs)          <- LED 3 pad bits
  0x95, 0x06,  //   REPORT_COUNT (6)
  0x75, 0x08,  //   REPORT_SIZE (8)
  0x15, 0x00,  //   LOGICAL_MINIMUM (0)
  0x25, 0x65,  //   LOGICAL_MAXIMUM (101)
  0x05, 0x07,  //   USAGE_PAGE (Keyboard)
  0x19, 0x00,  //   USAGE_MINIMUM (Reserved)
  0x29, 0x65,  //   USAGE_MAXIMUM (Keyboard Application)
  0x81, 0x00,  //   INPUT (Data,Ary,Abs)           <- 6 key slots
  0xc0         // END_COLLECTION
};

/* ---- Interface 1: Mouse report descriptor (52 bytes) ---- */
__ALIGN_BEGIN static uint8_t HID_MouseReportDesc[USBD_CUSTOM_HID_MOUSE_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
  0x09, 0x02,  // USAGE (Mouse)
  0xa1, 0x01,  // COLLECTION (Application)
  0x09, 0x01,  //   USAGE (Pointer)
  0xa1, 0x00,  //   COLLECTION (Physical)
  0x05, 0x09,  //     USAGE_PAGE (Button)
  0x19, 0x01,  //     USAGE_MINIMUM (Button 1)
  0x29, 0x03,  //     USAGE_MAXIMUM (Button 3)
  0x15, 0x00,  //     LOGICAL_MINIMUM (0)
  0x25, 0x01,  //     LOGICAL_MAXIMUM (1)
  0x95, 0x03,  //     REPORT_COUNT (3)
  0x75, 0x01,  //     REPORT_SIZE (1)
  0x81, 0x02,  //     INPUT (Data,Var,Abs)         <- 3 button bits
  0x95, 0x01,  //     REPORT_COUNT (1)
  0x75, 0x05,  //     REPORT_SIZE (5)
  0x81, 0x03,  //     INPUT (Cnst,Var,Abs)          <- 5 pad bits
  0x05, 0x01,  //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,  //     USAGE (X)
  0x09, 0x31,  //     USAGE (Y)
  0x09, 0x38,  //     USAGE (Wheel)
  0x15, 0x81,  //     LOGICAL_MINIMUM (-127)
  0x25, 0x7f,  //     LOGICAL_MAXIMUM (127)
  0x75, 0x08,  //     REPORT_SIZE (8)
  0x95, 0x03,  //     REPORT_COUNT (3)
  0x81, 0x06,  //     INPUT (Data,Var,Rel)          <- X, Y, Wheel
  0xc0,        //   END_COLLECTION
  0xc0         // END_COLLECTION
};

/* ---- Interface 2: Gamepad report descriptor (48 bytes) ---- */
__ALIGN_BEGIN static uint8_t HID_GpReportDesc[USBD_CUSTOM_HID_GP_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
  0x09, 0x05,        // USAGE (Game Pad)
  0xa1, 0x01,        // COLLECTION (Application)
  0x05, 0x09,        //   USAGE_PAGE (Button)
  0x19, 0x01,        //   USAGE_MINIMUM (Button 1)
  0x29, 0x10,        //   USAGE_MAXIMUM (Button 16)
  0x15, 0x00,        //   LOGICAL_MINIMUM (0)
  0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,        //   REPORT_SIZE (1)
  0x95, 0x10,        //   REPORT_COUNT (16)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)      <- 16 buttons
  0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
  0x09, 0x30,        //   USAGE (X)
  0x09, 0x31,        //   USAGE (Y)
  0x09, 0x32,        //   USAGE (Z)
  0x09, 0x33,        //   USAGE (Rx)
  0x09, 0x34,        //   USAGE (Ry)
  0x09, 0x35,        //   USAGE (Rz)
  0x15, 0x00,        //   LOGICAL_MINIMUM (0)
  0x26, 0xff, 0x00,  //   LOGICAL_MAXIMUM (255)
  0x75, 0x08,        //   REPORT_SIZE (8)
  0x95, 0x06,        //   REPORT_COUNT (6)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)      <- 6 axes
  0xc0               // END_COLLECTION
};

/* ---- Interface callbacks ---- */

static int8_t CUSTOM_HID_Init_FS(void)
{
  return USBD_OK;
}

static int8_t CUSTOM_HID_DeInit_FS(void)
{
  return USBD_OK;
}

/*
 * itf_idx: 0 = keyboard LED report (data = LED bitmask)
 *          data bit0 = NumLock, bit1 = CapsLock, bit2 = ScrollLock
 */
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t itf_idx, uint8_t data)
{
  if (itf_idx == 0U) {
    HAL_GPIO_WritePin(NumLock_GPIO_Port,    NumLock_Pin,    (data & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(CapsLock_GPIO_Port,   CapsLock_Pin,   (data & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ScrollLock_GPIO_Port, ScrollLock_Pin, (data & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
  return USBD_OK;
}

USBD_CUSTOM_HID_ItfTypeDef USBD_CustomHID_fops_FS = {
  { HID_KbdReportDesc,  HID_MouseReportDesc,  HID_GpReportDesc  },
  { USBD_CUSTOM_HID_KBD_REPORT_DESC_SIZE,
    USBD_CUSTOM_HID_MOUSE_REPORT_DESC_SIZE,
    USBD_CUSTOM_HID_GP_REPORT_DESC_SIZE },
  CUSTOM_HID_Init_FS,
  CUSTOM_HID_DeInit_FS,
  CUSTOM_HID_OutEvent_FS,
};
