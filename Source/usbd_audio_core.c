/**
  ******************************************************************************
  * @file    usbd_audio_core.c
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    19-March-2012
  * @brief   This file provides the high layer firmware functions to manage the 
  *          following functionalities of the USB Audio Class:
  *           - Initialization and Configuration of high and low layer
  *           - Enumeration as Audio Streaming Device
  *           - Audio Streaming data transfer
  *           - AudioControl requests management
  *           - Error management
  *           
  *  @verbatim
  *      
  *          ===================================================================      
  *                                Audio Class Driver Description
  *          =================================================================== 
  *           This driver manages the Audio Class 1.0 following the "USB Device Class Definition for
  *           Audio Devices V1.0 Mar 18, 98".
  *           This driver implements the following aspects of the specification:
  *             - Device descriptor management
  *             - Configuration descriptor management
  *             - Standard AC Interface Descriptor management
  *             - 1 Audio Streaming Interface (with single channel, PCM, Stereo mode)
  *             - 1 Audio Streaming Endpoint
  *             - 1 Audio Terminal Input (1 channel)
  *             - Audio Class-Specific AC Interfaces
  *             - Audio Class-Specific AS Interfaces
  *             - AudioControl Requests: only SET_CUR and GET_CUR requests are supported (for Mute)
  *             - Audio Feature Unit (limited to Mute control)
  *             - Audio Synchronization type: Asynchronous
  *             - Single fixed audio sampling rate (configurable in usbd_conf.h file)
  *          
  *           @note
  *            The Audio Class 1.0 is based on USB Specification 1.0 and thus supports only
  *            Low and Full speed modes and does not allow High Speed transfers.
  *            Please refer to "USB Device Class Definition for Audio Devices V1.0 Mar 18, 98"
  *            for more details.
  * 
  *           These aspects may be enriched or modified for a specific user application.
  *          
  *            This driver doesn't implement the following aspects of the specification 
  *            (but it is possible to manage these features with some modifications on this driver):
  *             - AudioControl Endpoint management
  *             - AudioControl requsests other than SET_CUR and GET_CUR
  *             - Abstraction layer for AudioControl requests (only Mute functionality is managed)
  *             - Audio Synchronization type: Adaptive
  *             - Audio Compression modules and interfaces
  *             - MIDI interfaces and modules
  *             - Mixer/Selector/Processing/Extension Units (Feature unit is limited to Mute control)
  *             - Any other application-specific modules
  *             - Multiple and Variable audio sampling rates
  *             - Out Streaming Endpoint/Interface (microphone)
  *      
  *  @endverbatim
  *                                  
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/

#include "usbd_audio_core.h"
#include "usbd_audio_out_if.h"
#include "midicontrol.h"

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */


/** @defgroup usbd_audio 
  * @brief usbd core module
  * @{
  */ 

/** @defgroup usbd_audio_Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup usbd_audio_Private_Defines
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup usbd_audio_Private_Macros
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup usbd_audio_Private_FunctionPrototypes
  * @{
  */

/*********************************************
   AUDIO Device library callbacks
 *********************************************/
static uint8_t  usbd_audio_Init       (void  *pdev, uint8_t cfgidx);
static uint8_t  usbd_audio_DeInit     (void  *pdev, uint8_t cfgidx);
static uint8_t  usbd_audio_Setup      (void  *pdev, USB_SETUP_REQ *req);
static uint8_t  usbd_audio_EP0_RxReady(void *pdev);
static uint8_t  usbd_audio_DataIn     (void *pdev, uint8_t epnum);
static uint8_t  usbd_audio_DataOut    (void *pdev, uint8_t epnum);
static uint8_t  usbd_audio_SOF        (void *pdev);
//static uint8_t  usbd_audio_OUT_Incplt (void  *pdev);
static void Handle_USBAsynchXfer (void *pdev);

/*********************************************
   AUDIO Requests management functions
 *********************************************/
//static void AUDIO_Req_GetCurrent(void *pdev, USB_SETUP_REQ *req);
//static void AUDIO_Req_SetCurrent(void *pdev, USB_SETUP_REQ *req);
static uint8_t  *USBD_audio_GetCfgDesc (uint8_t speed, uint16_t *length);
/**
  * @}
  */ 

/** @defgroup usbd_audio_Private_Variables
  * @{
  */ 
#if 0
/* Main Buffer for Audio Data Out transfers and its relative pointers */
uint8_t  IsocOutBuff [TOTAL_OUT_BUF_SIZE * 2];
uint8_t* IsocOutWrPtr = IsocOutBuff;
uint8_t* IsocOutRdPtr = IsocOutBuff;
#endif

__ALIGN_BEGIN uint8_t USB_Rx_Buffer   [MIDI_MAX_PACKET_SIZE] __ALIGN_END ;


/* Main Buffer for Audio Control Rrequests transfers and its relative variables */
uint8_t  AudioCtl[64];
uint8_t  AudioCtlCmd = 0;
uint32_t AudioCtlLen = 0;
uint8_t  AudioCtlUnit = 0;

static uint32_t PlayFlag = 0;

static __IO uint32_t  usbd_audio_AltSet = 0;
static uint8_t usbd_audio_CfgDesc[AUDIO_CONFIG_DESC_SIZE];

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
  #if defined ( __ICCARM__ ) /*!< IAR Compiler */
    #pragma data_alignment=4
  #endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */
__ALIGN_BEGIN uint8_t USB_Rx_Buffer   [MIDI_MAX_PACKET_SIZE] __ALIGN_END ;

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
  #if defined ( __ICCARM__ ) /*!< IAR Compiler */
    #pragma data_alignment=4
  #endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */
__ALIGN_BEGIN uint8_t APP_Rx_Buffer   [APP_RX_DATA_SIZE] __ALIGN_END ;

volatile uint32_t APP_Rx_ptr_in  = 0;
volatile uint32_t APP_Rx_ptr_out = 0;
volatile uint32_t APP_Rx_length  = 0;

uint8_t  USB_Tx_State = 0;

/* AUDIO interface class callbacks structure */
USBD_Class_cb_TypeDef  AUDIO_cb = 
{
  usbd_audio_Init,
  usbd_audio_DeInit,
  usbd_audio_Setup,
  NULL, /* EP0_TxSent */
  usbd_audio_EP0_RxReady,
  usbd_audio_DataIn,
  usbd_audio_DataOut,
  usbd_audio_SOF,
  NULL,
  NULL, /* usbd_audio_OUT_Incplt */
  USBD_audio_GetCfgDesc,
#ifdef USB_OTG_HS_CORE  
  USBD_audio_GetCfgDesc, /* use same config as per FS */
#endif    
};

/* USB AUDIO device Configuration Descriptor */
static uint8_t usbd_audio_CfgDesc[AUDIO_CONFIG_DESC_SIZE] =
{
  /* Configuration 1 */
  0x09,                                 /* bLength */
  USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
  LOBYTE(AUDIO_CONFIG_DESC_SIZE),       /* wTotalLength  101 bytes*/
  HIBYTE(AUDIO_CONFIG_DESC_SIZE),      
  0x02,                                 /* bNumInterfaces */
  0x01,                                 /* bConfigurationValue */
  0x00,                                 /* iConfiguration */
  0xC0,                                 /* bmAttributes  BUS Powred*/
  0x32,                                 /* bMaxPower = 100 mA*/
  /* 09 byte*/
  
  /* USB Speaker Standard interface descriptor */
  AUDIO_INTERFACE_DESC_SIZE,            /* bLength */
  USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
  0x00,                                 /* bInterfaceNumber */
  0x00,                                 /* bAlternateSetting */
  0x00,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOCONTROL,          /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */
  /* 09 byte*/
  
  /* USB Speaker Class-specific AC Interface Descriptor */
  AUDIO_INTERFACE_DESC_SIZE,            /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_HEADER,                 /* bDescriptorSubtype */
  0x00,          /* 1.00 */             /* bcdADC */
  0x01,
  AUDIO_INTERFACE_DESC_SIZE,            /* was 0x27 */
  0x00,									/* high byte of length */
  0x01,                                 /* bInCollection */
  0x01,                                 /* baInterfaceNr */
  /* 09 byte*/
  
  /* USB MIDI Standard AS Interface Descriptor */
  /* Interface 1, Alternate Setting 0                                             */
  AUDIO_INTERFACE_DESC_SIZE,  /* bLength */
  USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
  0x01,                                 /* bInterfaceNumber */
  0x00,                                 /* bAlternateSetting */
  0x02,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_MIDISTREAMING,         /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */
  /* 09 byte*/
  
  /* USB MIDI class-specific Streaming Interface Descriptor */
  MIDI_STREAMING_INTERFACE_DESC_SIZE,  /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_STREAMING_GENERAL,              /* bDescriptorSubtype */
  0x00,          /* 1.00 */             /* bcdADC */
  0x01,
  0x41,                                 /* length of this header and following = 65 */
  0x00,                                 /* high byte of length */
  /* 07 byte*/
  
  /* Embedded input jack */
  MIDI_INPUT_JACK_DESC_SIZE,
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,
  AUDIO_CONTROL_INPUT_TERMINAL,
  MIDI_JACKTYPE_EMBEDDED,
  0x01,									/* Jack ID */
  0x00,
  /* 06 byte */
  
  /* External input jack */
  MIDI_INPUT_JACK_DESC_SIZE,
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,
  AUDIO_CONTROL_INPUT_TERMINAL,
  MIDI_JACKTYPE_EXTERNAL,
  0x02,									/* Jack ID */
  0x00,
  /* 06 byte */

  /* Embedded output jack */
  MIDI_OUTPUT_JACK_DESC_SIZE,
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,
  AUDIO_CONTROL_OUTPUT_TERMINAL,
  MIDI_JACKTYPE_EMBEDDED,
  0x03,									/* jack ID */
  0x01,									/* number of pins */
  0x02,								/* source jack ID */
  0x01,								/* source jack pin ID */
  0x00,
  /* 09 byte */

  /* External output jack */
  MIDI_OUTPUT_JACK_DESC_SIZE,
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,
  AUDIO_CONTROL_OUTPUT_TERMINAL,
  MIDI_JACKTYPE_EXTERNAL,
  0x04,									/* jack ID */
  0x01,									/* number of pins */
  0x01,								/* source jack ID */
  0x01,								/* source jack pin ID */
  0x00,
  /* 09 byte */

  /* MIDI OUT endpoint - Standard Descriptor */
  AUDIO_STANDARD_ENDPOINT_DESC_SIZE,    /* bLength */
  USB_ENDPOINT_DESCRIPTOR_TYPE,         /* bDescriptorType */
  AUDIO_OUT_EP,                         /* bEndpointAddress 1 out endpoint*/
  USB_ENDPOINT_TYPE_BULK,               /* bmAttributes */
  MIDI_STREAM_EPSIZE,    				/* wMaxPacketSize in Bytes (Freq(Samples)*2(Stereo)*2(HalfWord)) */
  0x00,									/* high byte of length */
  0x01,                                 /* bInterval */
  0x00,                                 /* bRefresh */
  0x00,                                 /* bSynchAddress */
  /* 09 byte*/

  /* MIDI OUT endpoint - Class-Specific Descriptor */
  MIDI_ENDPOINT_DESC_SIZE,              /* bLength */
  AUDIO_ENDPOINT_DESCRIPTOR_TYPE,		/* type */
  AUDIO_ENDPOINT_GENERAL,
  0x01,									/* total embedded jacks */
  0x01,									/* associated jack ID */
  /* 05 bytes */

  /* MIDI IN endpoint - Standard Descriptor */
  AUDIO_STANDARD_ENDPOINT_DESC_SIZE,    /* bLength */
  USB_ENDPOINT_DESCRIPTOR_TYPE,         /* bDescriptorType */
  AUDIO_IN_EP,                          /* bEndpointAddress 2 in endpoint*/
  USB_ENDPOINT_TYPE_BULK,               /* bmAttributes */
  MIDI_STREAM_EPSIZE,    				/* wMaxPacketSize in Bytes (Freq(Samples)*2(Stereo)*2(HalfWord)) */
  0x00,									/* high byte of length */
  0x01,                                 /* bInterval */
  0x00,                                 /* bRefresh */
  0x00,                                 /* bSynchAddress */
  /* 09 byte*/

  /* MIDI IN endpoint - Class-Specific Descriptor */
  MIDI_ENDPOINT_DESC_SIZE,              /* bLength */
  AUDIO_ENDPOINT_DESCRIPTOR_TYPE,		/* type */
  AUDIO_ENDPOINT_GENERAL,
  0x01,									/* total embedded jacks */
  0x02,									/* associated jack ID */
  /* 05 bytes */
};


/**
  * @}
  */ 

/** @defgroup usbd_audio_Private_Functions
  * @{
  */ 

/**
* @brief  usbd_audio_Init
*         Initilaizes the AUDIO interface.
* @param  pdev: device instance
* @param  cfgidx: Configuration index
* @retval status
*/
static uint8_t  usbd_audio_Init (void  *pdev, 
                                 uint8_t cfgidx)
{
	  /* Open EP IN */
	  DCD_EP_Open(pdev,
	              AUDIO_IN_EP,
	              MIDI_MAX_PACKET_SIZE,
	              USB_OTG_EP_BULK);

	  /* Open EP OUT */
	  DCD_EP_Open(pdev,
	              AUDIO_OUT_EP,
	              MIDI_MAX_PACKET_SIZE,
	              USB_OTG_EP_BULK);

#if 0
  /* Open EP OUT */
  DCD_EP_Open(pdev,
              AUDIO_OUT_EP,
              AUDIO_OUT_PACKET,
              USB_OTG_EP_ISOC);

  /* Initialize the Audio output Hardware layer */
  if (AUDIO_OUT_fops.Init(USBD_AUDIO_FREQ, DEFAULT_VOLUME, 0) != USBD_OK)
  {
    return USBD_FAIL;
  }
    
#endif

  /* Prepare Out endpoint to receive MIDI data */
  DCD_EP_PrepareRx(pdev,
                   AUDIO_OUT_EP,
                   (uint8_t*)USB_Rx_Buffer,
                   MIDI_MAX_PACKET_SIZE);
  
  /* Could do hardware init here, but for now there's nothing to do */

  return USBD_OK;
}

/**
* @brief  usbd_audio_DeInit
*         DeInitializes the AUDIO layer.
* @param  pdev: device instance
* @param  cfgidx: Configuration index
* @retval status
*/
static uint8_t  usbd_audio_DeInit (void  *pdev, 
                                   uint8_t cfgidx)
{ 
	/* Close USB endpoints */
	DCD_EP_Close (pdev, AUDIO_OUT_EP);
	DCD_EP_Close (pdev, AUDIO_IN_EP);

	/* Could do any hardware de-init here but for now, there's nothing to do */

	return USBD_OK;
}

/**
  * @brief  usbd_audio_Setup
  *         Handles the Audio control request parsing.
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t  usbd_audio_Setup (void  *pdev, 
                                  USB_SETUP_REQ *req)
{
  uint16_t len=USB_AUDIO_DESC_SIZ;
  uint8_t  *pbuf=usbd_audio_CfgDesc + 18;
  
  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    /* AUDIO Class Requests -------------------------------*/
  case USB_REQ_TYPE_CLASS :    
    switch (req->bRequest)
    {
    case AUDIO_REQ_GET_CUR:
      //AUDIO_Req_GetCurrent(pdev, req); // Left over from USB-audio. Delete me if not needed
      break;
      
    case AUDIO_REQ_SET_CUR:
      //AUDIO_Req_SetCurrent(pdev, req); // Left over from USB-audio. Delete me if not needed
      break;

    default:
      USBD_CtlError (pdev, req);
      return USBD_FAIL;
    }
    break;
    
    /* Standard Requests -------------------------------*/
  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR: 
      if( (req->wValue >> 8) == AUDIO_DESCRIPTOR_TYPE)
      {
#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
        pbuf = usbd_audio_Desc;   
#else
        pbuf = usbd_audio_CfgDesc + 18;
#endif 
        len = MIN(USB_AUDIO_DESC_SIZ , req->wLength);
      }
      
      USBD_CtlSendData (pdev, 
                        pbuf,
                        len);
      break;
      
    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        (uint8_t *)&usbd_audio_AltSet,
                        1);
      break;
      
    case USB_REQ_SET_INTERFACE :
      if ((uint8_t)(req->wValue) < AUDIO_TOTAL_IF_NUM)
      {
        usbd_audio_AltSet = (uint8_t)(req->wValue);
      }
      else
      {
        /* Call the error management function (command will be nacked */
        USBD_CtlError (pdev, req);
      }
      break;
    }
  }
  return USBD_OK;
}

/**
  * @brief  usbd_audio_EP0_RxReady
  *         Handles audio control requests data.
  * @param  pdev: device device instance
  * @retval status
  */
static uint8_t  usbd_audio_EP0_RxReady (void  *pdev)
{ 
  /* Check if an AudioControl request has been issued */
  if (AudioCtlCmd == AUDIO_REQ_SET_CUR)
  {/* In this driver, to simplify code, only SET_CUR request is managed */
    /* Check for which addressed unit the AudioControl request has been issued */
    if (AudioCtlUnit == AUDIO_OUT_STREAMING_CTRL)
    {/* In this driver, to simplify code, only one unit is manage */
#if 0
      /* Call the audio interface mute function */
      AUDIO_OUT_fops.MuteCtl(AudioCtl[0]);
#endif
      
      /* Reset the AudioCtlCmd variable to prevent re-entering this function */
      AudioCtlCmd = 0;
      AudioCtlLen = 0;
    }
  } 
  
  return USBD_OK;
}

/**
  * @brief  usbd_audio_DataIn
  *         Handles the audio IN data stage.
  * @param  pdev: instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  usbd_audio_DataIn (void *pdev, uint8_t epnum)
{
	uint16_t USB_Tx_ptr;
	uint16_t USB_Tx_length;

	if (USB_Tx_State == 1)
	{
		if (APP_Rx_length == 0)
		{
		  USB_Tx_State = 0;
		}
		else
		{
		  if (APP_Rx_length > MIDI_MAX_PACKET_SIZE){
			USB_Tx_ptr = APP_Rx_ptr_out;
			USB_Tx_length = MIDI_MAX_PACKET_SIZE;

			APP_Rx_ptr_out += MIDI_MAX_PACKET_SIZE;
			APP_Rx_length -= MIDI_MAX_PACKET_SIZE;
		  }
		  else
		  {
			USB_Tx_ptr = APP_Rx_ptr_out;
			USB_Tx_length = APP_Rx_length;

			APP_Rx_ptr_out += APP_Rx_length;
			APP_Rx_length = 0;
		  }

		  /* Prepare the available data buffer to be sent on IN endpoint */
		  DCD_EP_Tx (pdev,
					 AUDIO_IN_EP,
					 (uint8_t*)&APP_Rx_Buffer[USB_Tx_ptr],
					 USB_Tx_length);
		}
	}

	return USBD_OK;
}

/**
  * @brief  usbd_audio_DataOut
  *         Handles the Audio Out data stage.
  * @param  pdev: instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  usbd_audio_DataOut (void *pdev, uint8_t epnum)
{     
  if (epnum == AUDIO_OUT_EP)
  {    
	  uint16_t USB_Rx_Cnt;

	  /* Get the received data buffer and update the counter */
	  USB_Rx_Cnt = ((USB_OTG_CORE_HANDLE*)pdev)->dev.out_ep[epnum].xfer_count;

	  /* USB data will be immediately processed, this allow next USB traffic being
	     NAKed till the end of the application Xfer */
	  midi_receive_usb_buffer(USB_Rx_Buffer, USB_Rx_Cnt);

	  /* Prepare Out endpoint to receive next packet */
	  DCD_EP_PrepareRx(pdev,
	                   AUDIO_OUT_EP,
	                   (uint8_t*)(USB_Rx_Buffer),
	                   MIDI_MAX_PACKET_SIZE);
  }
  
  return USBD_OK;
}

/**
  * @brief  usbd_audio_SOF
  *         Handles the SOF event (data buffer update and synchronization).
  * @param  pdev: instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  usbd_audio_SOF (void *pdev)
{     
	static uint32_t FrameCount = 0;

	if (++FrameCount == MIDI_IN_FRAME_INTERVAL)
	{
		/* Reset the frame counter */
		FrameCount = 0;

		/* Check the data to be sent through IN pipe */
		Handle_USBAsynchXfer(pdev);
	}

	return USBD_OK;
}


/**
  * @brief  Handle_USBAsynchXfer
  *         Send data to USB
  * @param  pdev: instance
  * @retval None
  */
static void Handle_USBAsynchXfer (void *pdev)
{
  uint16_t USB_Tx_ptr;
  uint16_t USB_Tx_length;

  if(USB_Tx_State != 1)
  {
    if (APP_Rx_ptr_out == APP_RX_DATA_SIZE)
    {
      APP_Rx_ptr_out = 0;
    }

    if(APP_Rx_ptr_out == APP_Rx_ptr_in)
    {
      USB_Tx_State = 0;
      return;
    }

    if(APP_Rx_ptr_out > APP_Rx_ptr_in) /* rollback */
    {
      APP_Rx_length = APP_RX_DATA_SIZE - APP_Rx_ptr_out;
    }
    else
    {
      APP_Rx_length = APP_Rx_ptr_in - APP_Rx_ptr_out;
    }
#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
     APP_Rx_length &= ~0x03;
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */

    if (APP_Rx_length > MIDI_MAX_PACKET_SIZE)
    {
      USB_Tx_ptr = APP_Rx_ptr_out;
      USB_Tx_length = MIDI_MAX_PACKET_SIZE;

      APP_Rx_ptr_out += MIDI_MAX_PACKET_SIZE;
      APP_Rx_length -= MIDI_MAX_PACKET_SIZE;
    }
    else
    {
      USB_Tx_ptr = APP_Rx_ptr_out;
      USB_Tx_length = APP_Rx_length;

      APP_Rx_ptr_out += APP_Rx_length;
      APP_Rx_length = 0;
    }
    USB_Tx_State = 1;

    DCD_EP_Tx (pdev,
               AUDIO_IN_EP,
               (uint8_t*)&APP_Rx_Buffer[USB_Tx_ptr],
               USB_Tx_length);
  }

}

#if 0
/**
  * @brief  usbd_audio_OUT_Incplt
  *         Handles the iso out incomplete event.
  * @param  pdev: instance
  * @retval status
  */
static uint8_t  usbd_audio_OUT_Incplt (void  *pdev)
{
  return USBD_OK;
}

/******************************************************************************
     AUDIO Class requests management
******************************************************************************/
/**
  * @brief  AUDIO_Req_GetCurrent
  *         Handles the GET_CUR Audio control request.
  * @param  pdev: instance
  * @param  req: setup class request
  * @retval status
  */
static void AUDIO_Req_GetCurrent(void *pdev, USB_SETUP_REQ *req)
{  
  /* Send the current mute state */
  USBD_CtlSendData (pdev, 
                    AudioCtl,
                    req->wLength);
}

/**
  * @brief  AUDIO_Req_SetCurrent
  *         Handles the SET_CUR Audio control request.
  * @param  pdev: instance
  * @param  req: setup class request
  * @retval status
  */
static void AUDIO_Req_SetCurrent(void *pdev, USB_SETUP_REQ *req)
{ 
  if (req->wLength)
  {
    /* Prepare the reception of the buffer over EP0 */
    USBD_CtlPrepareRx (pdev, 
                       AudioCtl,
                       req->wLength);
    
    /* Set the global variables indicating current request and its length 
    to the function usbd_audio_EP0_RxReady() which will process the request */
    AudioCtlCmd = AUDIO_REQ_SET_CUR;     /* Set the request value */
    AudioCtlLen = req->wLength;          /* Set the request data length */
    AudioCtlUnit = HIBYTE(req->wIndex);  /* Set the request target unit */
  }
}
#endif

/**
  * @brief  USBD_audio_GetCfgDesc 
  *         Returns configuration descriptor.
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_audio_GetCfgDesc (uint8_t speed, uint16_t *length)
{
  *length = sizeof (usbd_audio_CfgDesc);
  return usbd_audio_CfgDesc;
}
/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/