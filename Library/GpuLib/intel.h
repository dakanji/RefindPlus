#ifndef __LIBSAIO_GMA_H
#define __LIBSAIO_GMA_H

#include "device_inject.h"

BOOLEAN setup_gma_devprop(LOADER_ENTRY *Entry, pci_dt_t *gma_dev);

struct gma_gpu_t {
	UINT32 device;
	CONST CHAR8 *name;
};

/*
Chameleon
uint32_t ram = (((getVBEVideoRam() + 512) / 1024) + 512) / 1024;
uint32_t ig_platform_id;
switch (ram)
{
  case 96:
    ig_platform_id = 0x01660000; // 96mb
    break;
  case 64:
    ig_platform_id = 0x01660009; // 64mb
    break;
  case 32:
    ig_platform_id = 0x01620005; // 32mb
    break;
  default:
*/

CONST CHAR16 *get_gma_model (
  IN UINT16 DeviceID
  );

  // BELOW from gma.cpp

  CONST CHAR16  *CFLFBPath  = L"/System/Library/Extensions/AppleIntelCFLGraphicsFramebuffer.kext";

extern CHAR8*    gDeviceProperties;
CONST UINT8 ClassFix[] =  { 0x00, 0x00, 0x03, 0x00 };


UINT8 common_vals[3][4] = {
  { 0x00, 0x00, 0x00, 0x00 },   //0 reg_FALSE
  { 0x01, 0x00, 0x00, 0x00 },   //1 reg_TRUE
  { 0x6B, 0x10, 0x00, 0x00 },   //2 "subsystem-vendor-id"
};


UINT8 calistoga_gma_vals[30][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,HasLid"
  { 0x01, 0x00, 0x00, 0x00 },   //1 "AAPL,HasPanel"
  { 0x04, 0x00, 0x00, 0x00 },   //2 "AAPL,NumDisplays"
  { 0x02, 0x00, 0x00, 0x00 },   //3 "AAPL,NumFramebuffers"
  { 0x3f, 0x00, 0x00, 0x08 },   //4 "AAPL01,BacklightIntensity"
  { 0x01, 0x00, 0x00, 0x00 },   //5 "AAPL01,BootDisplay"
  { 0x00, 0x00, 0x00, 0x00 },   //6 "AAPL01,CurrentDisplay"
  { 0x01, 0x00, 0x00, 0x00 },   //7 "AAPL01,DataJustify"
  { 0x00, 0x00, 0x00, 0x00 },   //8 "AAPL01,Depth"
  { 0x00, 0x00, 0x00, 0x00 },   //9 "AAPL01,Dither"
  { 0x20, 0x03, 0x00, 0x00 },   //10 "AAPL01,Height"
  { 0x00, 0x00, 0x00, 0x00 },   //11 "AAPL01,Interlace"
  { 0x00, 0x00, 0x00, 0x00 },   //12 "AAPL01,Inverter"
  { 0x00, 0x00, 0x00, 0x00 },   //13 "AAPL01,InverterCurrent"
  { 0xc8, 0x52, 0x00, 0x00 },   //14 "AAPL01,InverterFrequency"
  { 0x00, 0x10, 0x00, 0x00 },   //15 "AAPL01,IODisplayMode"
  { 0x00, 0x00, 0x00, 0x00 },   //16 "AAPL01,LinkFormat"
  { 0x00, 0x00, 0x00, 0x00 },   //17 "AAPL01,LinkType"
  { 0x01, 0x00, 0x00, 0x00 },   //18 "AAPL01,Pipe"
  { 0x00, 0x00, 0x00, 0x00 },   //19 "AAPL01,PixelFormat"
  { 0x3b, 0x00, 0x00, 0x00 },   //20 "AAPL01,Refresh"
  { 0x00, 0x00, 0x00, 0x00 },   //21 "AAPL01,Stretch"
  { 0x00, 0x00, 0x00, 0x00 },   //22 "AAPL01,T1"
  { 0x01, 0x00, 0x00, 0x00 },   //23 "AAPL01,T2"
  { 0xc8, 0x00, 0x00, 0x00 },   //24 "AAPL01,T3"
  { 0xc8, 0x01, 0x00, 0x00 },   //25 "AAPL01,T4"
  { 0x01, 0x00, 0x00, 0x00 },   //26 "AAPL01,T5"
  { 0x00, 0x00, 0x00, 0x00 },   //27 "AAPL01,T6"
  { 0x90, 0x01, 0x00, 0x00 },   //28 "AAPL01,T7"
  { 0x00, 0x05, 0x00, 0x00 },   //29 "AAPL01,Width"
};


UINT8 crestline_gma_vals[34][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,HasLid"
  { 0x01, 0x00, 0x00, 0x00 },   //1 "AAPL,HasPanel"
  { 0x04, 0x00, 0x00, 0x00 },   //2 "AAPL,NumDisplays"
  { 0x02, 0x00, 0x00, 0x00 },   //3 "AAPL,NumFramebuffers"
  { 0x01, 0x00, 0x00, 0x00 },   //4 "AAPL,SelfRefreshSupported"
  { 0x01, 0x00, 0x00, 0x00 },   //5 "AAPL,aux-power-connected"
  { 0x01, 0x00, 0x00, 0x08 },   //6 "AAPL,backlight-control"
  { 0x00, 0x00, 0x00, 0x08 },   //7 "AAPL00,blackscreen-preferences"
  { 0x01, 0x00, 0x00, 0x00 },   //8 "AAPL01,BootDisplay"
  { 0x38, 0x00, 0x00, 0x08 },   //9 "AAPL01,BacklightIntensity"
  { 0x00, 0x00, 0x00, 0x00 },   //10 "AAPL01,blackscreen-preferences"
  { 0x00, 0x00, 0x00, 0x00 },   //11 "AAPL01,CurrentDisplay"
  { 0x01, 0x00, 0x00, 0x00 },   //12 "AAPL01,DataJustify"
  { 0x20, 0x00, 0x00, 0x00 },   //13 "AAPL01,Depth"
  { 0x00, 0x00, 0x00, 0x00 },   //14 "AAPL01,Dither"
  { 0x20, 0x03, 0x00, 0x00 },   //15 "AAPL01,Height"
  { 0x00, 0x00, 0x00, 0x00 },   //16 "AAPL01,Interlace"
  { 0x00, 0x00, 0x00, 0x00 },   //17 "AAPL01,Inverter"
  { 0x08, 0x52, 0x00, 0x00 },   //18 "AAPL01,InverterCurrent"
  { 0xaa, 0x01, 0x00, 0x00 },   //19 "AAPL01,InverterFrequency"
  { 0x00, 0x00, 0x00, 0x00 },   //20 "AAPL01,LinkFormat"
  { 0x00, 0x00, 0x00, 0x00 },   //21 "AAPL01,LinkType"
  { 0x01, 0x00, 0x00, 0x00 },   //22 "AAPL01,Pipe"
  { 0x00, 0x00, 0x00, 0x00 },   //23 "AAPL01,PixelFormat"
  { 0x3d, 0x00, 0x00, 0x00 },   //24 "AAPL01,Refresh"
  { 0x00, 0x00, 0x00, 0x00 },   //25 "AAPL01,Stretch"
  { 0x00, 0x00, 0x00, 0x00 },   //26 "AAPL01,T1"
  { 0x01, 0x00, 0x00, 0x00 },   //27 "AAPL01,T2"
  { 0xc8, 0x00, 0x00, 0x00 },   //28 "AAPL01,T3"
  { 0xc8, 0x01, 0x00, 0x00 },   //29 "AAPL01,T4"
  { 0x01, 0x00, 0x00, 0x00 },   //30 "AAPL01,T5"
  { 0x00, 0x00, 0x00, 0x00 },   //31 "AAPL01,T6"
  { 0x90, 0x01, 0x00, 0x00 },   //32 "AAPL01,T7"
  { 0x00, 0x05, 0x00, 0x00 },   //33 "AAPL01,Width"
};


UINT8 ironlake_hd_vals[10][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,aux-power-connected"
  { 0x01, 0x00, 0x00, 0x00 },   //1 "AAPL,backlight-control"
  { 0x00, 0x00, 0x00, 0x00 },   //2 "AAPL00,T1"
  { 0x01, 0x00, 0x00, 0x00 },   //3 "AAPL00,T2"
  { 0xc8, 0x00, 0x00, 0x00 },   //4 "AAPL00,T3"
  { 0xc8, 0x01, 0x00, 0x00 },   //5 "AAPL00,T4"
  { 0x01, 0x00, 0x00, 0x00 },   //6 "AAPL00,T5"
  { 0x00, 0x00, 0x00, 0x00 },   //7 "AAPL00,T6"
  { 0x90, 0x01, 0x00, 0x00 },   //8 "AAPL00,T7"
  { 0x00, 0x00, 0x00, 0x12 },   //9 "VRAM,totalsize"
};


UINT8 sandy_bridge_snb_vals[7][4] = {
  { 0x00, 0x00, 0x01, 0x00 },   //0 *MacBookPro8,1/MacBookPro8,2/MacBookPro8,3 - Intel HD Graphics 3000 - SNB0: 0x10000, Mobile: 1, PipeCount: 2, PortCount: 4, Connector: LVDS1/DP3, BL: 0x0710
  { 0x00, 0x00, 0x02, 0x00 },   //1 Intel HD Graphics 3000 - SNB1: 0x20000, Mobile: 1, PipeCount: 2, PortCount: 1, Connector: LVDS1, BL: 0x0710
  { 0x10, 0x00, 0x03, 0x00 },   //2 *Macmini5,1/Macmini5,3 - Intel HD Graphics 3000 - SNB2: 0x30010, Mobile: 0, PipeCount: 2, PortCount: 3, Connector: DP2/HDMI1, BL:
  { 0x20, 0x00, 0x03, 0x00 },   //3 *Macmini5,1/Macmini5,3 - Intel HD Graphics 3000 - SNB2: 0x30020, Mobile: 0, PipeCount: 2, PortCount: 3, Connector: DP2/HDMI1, BL:
  { 0x30, 0x00, 0x03, 0x00 },   //4 *Macmini5,2 - Intel HD Graphics 3000 - SNB3: 0x30030, Mobile: 0, PipeCount: 0, PortCount: 0, Connector:, BL:
  { 0x00, 0x00, 0x04, 0x00 },   //5 *MacBookAir4,1/MacBookAir4,2 - Intel HD Graphics 3000 - SNB4: 0x40000, Mobile: 1, PipeCount: 2, PortCount: 3, Connector: LVDS1/DP2, BL: 0x0710
  { 0x00, 0x00, 0x05, 0x00 },   //6 *iMac12,1/iMac12,2 - Intel HD Graphics 3000 - SNB5: 0x50000, Mobile: 0, PipeCount: 0, PortCount: 0, Connector:, BL:
};

UINT8 sandy_bridge_hd_vals[13][4] = {
  { 0x00, 0x00, 0x00, 0x00 },   //0 "AAPL00,DataJustify"
  { 0x00, 0x00, 0x00, 0x00 },   //1 "AAPL00,Dither"
  { 0x00, 0x00, 0x00, 0x00 },   //2 "AAPL00,LinkFormat"
  { 0x00, 0x00, 0x00, 0x00 },   //3 "AAPL00,LinkType"
  { 0x00, 0x00, 0x00, 0x00 },   //4 "AAPL00,PixelFormat"
  { 0x00, 0x00, 0x00, 0x00 },   //5 "AAPL00,T1"
  { 0x14, 0x00, 0x00, 0x00 },   //6 "AAPL00,T2"
  { 0xfa, 0x00, 0x00, 0x00 },   //7 "AAPL00,T3"
  { 0x2c, 0x01, 0x00, 0x00 },   //8 "AAPL00,T4"
  { 0x00, 0x00, 0x00, 0x00 },   //9 "AAPL00,T5"
  { 0x14, 0x00, 0x00, 0x00 },   //10 "AAPL00,T6"
  { 0xf4, 0x01, 0x00, 0x00 },   //11 "AAPL00,T7"
  { 0x04, 0x00, 0x00, 0x00 },   //12 "graphic-options"
};


UINT8 ivy_bridge_ig_vals[12][4] = {
  { 0x05, 0x00, 0x62, 0x01 },   //0 Intel HD Graphics 4000 - Mobile: 0, PipeCount: 2, PortCount: 3, STOLEN: 32MB, FBMEM: 16MB, VRAM: 1536MB, Connector: DP3, BL: 0x0710
  { 0x06, 0x00, 0x62, 0x01 },   //1 *iMac13,1 - Intel HD Graphics 4000 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 256MB, Connector:, BL: 0x0710
  { 0x07, 0x00, 0x62, 0x01 },   //2 *iMac13,2 - Intel HD Graphics 4000 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 256MB, Connector:, BL: 0x0710
  { 0x00, 0x00, 0x66, 0x01 },   //3 Intel HD Graphics 4000 - Mobile: 0, PipeCount: 3, PortCount: 4, STOLEN: 96MB, FBMEM: 24MB, VRAM: 1024MB, Connector: LVDS1/DP3, BL: 0x0710
  { 0x01, 0x00, 0x66, 0x01 },   //4 *MacBookPro10,2 - Intel HD Graphics 4000 - Mobile: 1, PipeCount: 3, PortCount: 4, STOLEN: 96MB, FBMEM: 24MB, VRAM: 1536MB, Connector: LVDS1/HDMI1/DP2, BL: 0x0710
  { 0x02, 0x00, 0x66, 0x01 },   //5 *MacBookPro10,1 - Intel HD Graphics 4000 - Mobile: 1, PipeCount: 3, PortCount: 1, STOLEN: 64MB, FBMEM: 24MB, VRAM: 1536MB, Connector: LVDS1, BL: 0x0710
  { 0x03, 0x00, 0x66, 0x01 },   //6 *MacBookPro9,2 - Intel HD Graphics 4000 - Mobile: 1, PipeCount: 2, PortCount: 4, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1536MB, Connector: LVDS1/DP3, BL: 0x0710
  { 0x04, 0x00, 0x66, 0x01 },   //7 *MacBookPro9,1 - Intel HD Graphics 4000 - Mobile: 1, PipeCount: 3, PortCount: 1, STOLEN: 32MB, FBMEM: 16MB, VRAM: 1536MB, Connector: LVDS1, BL: 0x0710
  { 0x08, 0x00, 0x66, 0x01 },   //8 *MacBookAir5,1 - Intel HD Graphics 4000 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x0710
  { 0x09, 0x00, 0x66, 0x01 },   //9 *MacBookAir5,2 - Intel HD Graphics 4000 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x0710
  { 0x0a, 0x00, 0x66, 0x01 },   //10 *Macmini6,1 - Intel HD Graphics 4000 - Mobile: 0, PipeCount: 2, PortCount: 3, STOLEN: 32MB, FBMEM: 16MB, VRAM: 1536MB, Connector: DP2/HDMI1, BL: 0x0710
  { 0x0b, 0x00, 0x66, 0x01 },   //11 *Macmini6,2 - Intel HD Graphics 4000 - Mobile: 0, PipeCount: 2, PortCount: 3, STOLEN: 32MB, FBMEM: 16MB, VRAM: 1536MB, Connector: DP2/HDMI1, BL: 0x0710
};

UINT8 ivy_bridge_hd_vals[1][4] = {
  { 0x0c, 0x00, 0x00, 0x00 },   //0 "graphics-options"
};


UINT8 haswell_ig_vals[24][4] = {
  { 0x00, 0x00, 0x06, 0x04 },   //0 Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x04, 0x00, 0x12, 0x04 },   //1 Intel HD Graphics 4600 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 32MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL:
  { 0x0b, 0x00, 0x12, 0x04 },   //2 *iMac15,1 - Intel HD Graphics 4600 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 32MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL:
  { 0x00, 0x00, 0x16, 0x04 },   //3 Intel HD Graphics 4600 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x00, 0x00, 0x26, 0x04 },   //4 Intel HD Graphics 5000 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x00, 0x00, 0x16, 0x0a },   //5 Intel HD Graphics 4400 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x0AD9
  { 0x0c, 0x00, 0x16, 0x0a },   //6 Intel HD Graphics 4400 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 34MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 1388
  { 0x00, 0x00, 0x26, 0x0a },   //7 Intel HD Graphics 5000 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x0AD9
  { 0x05, 0x00, 0x26, 0x0a },   //8 Intel HD Graphics 5000 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 32MB, FBMEM: 19MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x0AD9
  { 0x06, 0x00, 0x26, 0x0a },   //9 *MacBookAir6,1/MacBookAir6,2/Macmini7,1 - Intel HD Graphics 5000 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 32MB, FBMEM: 19MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x0AD9
  { 0x0a, 0x00, 0x26, 0x0a },   //10 Intel HD Graphics 5000 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 32MB, FBMEM: 19MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x1499
  { 0x0d, 0x00, 0x26, 0x0a },   //11 Intel HD Graphics 5000 - Mobile: 0, PipeCount: 3, PortCount: 2, STOLEN: 96MB, FBMEM: 34MB, VRAM: 1536MB, Connector: DP2, BL: 0x1499
  { 0x08, 0x00, 0x2e, 0x0a },   //12 *MacBookPro11,1 - Intel Iris Graphics 5100 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 34MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x0a, 0x00, 0x2e, 0x0a },   //13 Intel Iris Graphics 5100 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 32MB, FBMEM: 19MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x1499
  { 0x0d, 0x00, 0x2e, 0x0a },   //14 Intel Iris Graphics 5100 - Mobile: 0, PipeCount: 3, PortCount: 2, STOLEN: 96MB, FBMEM: 34MB, VRAM: 1536MB, Connector: DP2, BL: 0x1499
  { 0x00, 0x00, 0x06, 0x0c },   //15 Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x00, 0x00, 0x16, 0x0c },   //16 Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x00, 0x00, 0x26, 0x0c },   //17 Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x03, 0x00, 0x22, 0x0d },   //18 *iMac14,1/iMac14,4 - Intel Iris Pro Graphics 5200 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 32MB, FBMEM: 19MB, VRAM: 1536MB, Connector: DP3, BL: 0x1499
  { 0x00, 0x00, 0x26, 0x0d },   //19 Intel Iris Pro Graphics 5200 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 64MB, FBMEM: 16MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x07, 0x00, 0x26, 0x0d },   //20 *MacBookPro11,2/MacBookPro11,3 - Intel Iris Pro Graphics 5200 - Mobile: 1, PipeCount: 3, PortCount: 4, STOLEN: 64MB, FBMEM: 34MB, VRAM: 1536MB, Connector: LVDS1/DP2/HDMI1, BL: 0x07A1
  { 0x09, 0x00, 0x26, 0x0d },   //21 Intel Iris Pro Graphics 5200 - Mobile: 1, PipeCount: 3, PortCount: 1, STOLEN: 64MB, FBMEM: 34MB, VRAM: 1536MB, Connector: LVDS1, BL: 0x07A1
  { 0x0e, 0x00, 0x26, 0x0d },   //22 Intel Iris Pro Graphics 5200 - Mobile: 1, PipeCount: 3, PortCount: 4, STOLEN: 96MB, FBMEM: 34MB, VRAM: 1536MB, Connector: LVDS1/DP2/HDMI1, BL: 0x07A1
  { 0x0f, 0x00, 0x26, 0x0d },   //23 Intel Iris Pro Graphics 5200 - Mobile: 1, PipeCount: 3, PortCount: 1, STOLEN: 96MB, FBMEM: 34MB, VRAM: 1536MB, Connector: LVDS1, BL: 0x07A1
};

UINT8 haswell_hd_vals[1][4] = {
  { 0x0c, 0x00, 0x00, 0x00 },   //0 "graphics-options"
};


UINT8 broadwell_ig_vals[22][4] = {
  { 0x00, 0x00, 0x06, 0x16 },   //0 Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 16MB, FBMEM: 15MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x02, 0x00, 0x06, 0x16 },   //1 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x0e, 0x16 },   //2 Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 16MB, FBMEM: 15MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x01, 0x00, 0x0e, 0x16 },   //3 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x03, 0x00, 0x12, 0x16 },   //4 Intel HD Graphics 5600 - Mobile: 1, PipeCount: 3, PortCount: 4, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2/HDMI1, BL: 0x07A1
  { 0x00, 0x00, 0x16, 0x16 },   //5 Intel HD Graphics 5500 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 16MB, FBMEM: 15MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x02, 0x00, 0x16, 0x16 },   //6 Intel HD Graphics 5500 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x1e, 0x16 },   //7 Intel HD Graphics 5300 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 16MB, FBMEM: 15MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x01, 0x00, 0x1e, 0x16 },   //8 *MacBook8,1 - Intel HD Graphics 5300 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x22, 0x16 },   //9 Intel Iris Pro Graphics 6200 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 16MB, FBMEM: 15MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x02, 0x00, 0x22, 0x16 },   //10 Intel Iris Pro Graphics 6200 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x07, 0x00, 0x22, 0x16 },   //11 *iMac16,2 - Intel Iris Pro Graphics 6200 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 38MB, VRAM: 1536MB, Connector: DP3, BL: 0x1499
  { 0x00, 0x00, 0x26, 0x16 },   //12 Intel HD Graphics 6000 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 16MB, FBMEM: 15MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x02, 0x00, 0x26, 0x16 },   //13 Intel HD Graphics 6000 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x04, 0x00, 0x26, 0x16 },   //14 Intel HD Graphics 6000 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x1499
  { 0x05, 0x00, 0x26, 0x16 },   //15 Intel HD Graphics 6000 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x0AD9
  { 0x06, 0x00, 0x26, 0x16 },   //16 *iMac16,1/MacBookAir7,1/MacBookAir7,2 - Intel HD Graphics 6000 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x0AD9
  { 0x08, 0x00, 0x26, 0x16 },   //17 Intel HD Graphics 6000 - Mobile: 0, PipeCount: 2, PortCount: 2, STOLEN: 34MB, FBMEM: 34MB, VRAM: 1536MB, Connector: DP2, BL: 0x1499
  { 0x00, 0x00, 0x2b, 0x16 },   //18 Intel Iris Graphics 6100 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 16MB, FBMEM: 15MB, VRAM: 1024MB, Connector: LVDS1/DDVI1/HDMI1, BL: 0x1499
  { 0x02, 0x00, 0x2b, 0x16 },   //19 *MacBookPro12,1 - Intel Iris Graphics 6100 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x04, 0x00, 0x2b, 0x16 },   //20 Intel Iris Graphics 6100 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x1499
  { 0x08, 0x00, 0x2b, 0x16 },   //21 Intel Iris Graphics 6100 - Mobile: 0, PipeCount: 2, PortCount: 2, STOLEN: 34MB, FBMEM: 34MB, VRAM: 1536MB, Connector: DP2, BL: 0x1499
};

UINT8 broadwell_hd_vals[2][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,ig-tcon-scaler"
  { 0x0c, 0x00, 0x00, 0x00 },   //1 "graphics-options"
};


UINT8 skylake_ig_vals[19][4] = {
  { 0x01, 0x00, 0x02, 0x19 },   //0 Intel HD Graphics 510 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL:
  { 0x00, 0x00, 0x12, 0x19 },   //1 Intel HD Graphics 530 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: DUMMY1/DP2, BL: 0x056C
  { 0x01, 0x00, 0x12, 0x19 },   //2 *iMac17,1 - Intel HD Graphics 530 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL:
  { 0x00, 0x00, 0x16, 0x19 },   //3 Intel HD Graphics 520 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x02, 0x00, 0x16, 0x19 },   //4 Intel HD Graphics 520 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x01, 0x00, 0x17, 0x19 },   //5 Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL:
  { 0x00, 0x00, 0x1b, 0x19 },   //6 *MacBookPro13,3 - Intel HD Graphics 530 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x06, 0x00, 0x1b, 0x19 },   //7 Intel HD Graphics 530 - Mobile: 1, PipeCount: 1, PortCount: 1, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1, BL: 0x056C
  { 0x00, 0x00, 0x1e, 0x19 },   //8 Intel HD Graphics 515 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x03, 0x00, 0x1e, 0x19 },   //9 *MacBook9,1 - Intel HD Graphics 515 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 40MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x26, 0x19 },   //10 Intel Iris Graphics 540 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x02, 0x00, 0x26, 0x19 },   //11 *MacBookPro13,1 - Intel Iris Graphics 540 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x04, 0x00, 0x26, 0x19 },   //12 Intel Iris Graphics 540 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x07, 0x00, 0x26, 0x19 },   //13 Intel Iris Graphics 540 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x27, 0x19 },   //14 Intel Iris Graphics 550 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x04, 0x00, 0x27, 0x19 },   //15 *MacBookPro13,2 - Intel Iris Graphics 550 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x01, 0x00, 0x32, 0x19 },   //16 Intel Iris Pro Graphics 580 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL:
  { 0x00, 0x00, 0x3b, 0x19 },   //17 Intel Iris Pro Graphics 580 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x05, 0x00, 0x3b, 0x19 },   //18 Intel Iris Pro Graphics 580 - Mobile: 1, PipeCount: 3, PortCount: 4, STOLEN: 34MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP3, BL: 0x056C
};

UINT8 skylake_hd_vals[12][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,Gfx324"            - MacBookPro
  { 0x01, 0x00, 0x00, 0x00 },   //1 "AAPL,GfxYTile"
  { 0xfa, 0x00, 0x00, 0x00 },   //2 "AAPL00,PanelCycleDelay"
  { 0x11, 0x00, 0x00, 0x08 },   //3 "AAPL00,PanelPowerDown"  - MacBook
  { 0x11, 0x00, 0x00, 0x00 },   //4 "AAPL00,PanelPowerOff"   - MacBook
  { 0xe2, 0x00, 0x00, 0x08 },   //5 "AAPL00,PanelPowerOn"    - MacBook
  { 0x48, 0x00, 0x00, 0x00 },   //6 "AAPL00,PanelPowerUp"    - MacBook
  { 0x3c, 0x00, 0x00, 0x08 },   //7 "AAPL00,PanelPowerDown"  - MacBookPro
  { 0x11, 0x00, 0x00, 0x00 },   //8 "AAPL00,PanelPowerOff"   - MacBookPro
  { 0x19, 0x01, 0x00, 0x08 },   //9 "AAPL00,PanelPowerOn"    - MacBookPro
  { 0x30, 0x00, 0x00, 0x00 },   //10 "AAPL00,PanelPowerUp"   - MacBookPro
  { 0x0c, 0x00, 0x00, 0x00 },   //11 "graphic-options"
};


UINT8 kabylake_ig_vals[19][4] = {
  { 0x00, 0x00, 0x12, 0x59 },   //0 Intel HD Graphics 630 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: DP3, BL: 0x056C
  { 0x03, 0x00, 0x12, 0x59 },   //1 *iMac18,2/iMac18,3 - Intel HD Graphics 630 - Mobile: 1, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL: 0x056C
  { 0x00, 0x00, 0x16, 0x59 },   //2 Intel HD Graphics 620 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x09, 0x00, 0x16, 0x59 },   //3 Intel HD Graphics 620 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x02, 0x00, 0x18, 0x59 },   //4 Mobile: 1, PipeCount: 0, PortCount: 0, STOLEN: 0MB, FBMEM: 0MB, VRAM: 1536MB, Connector:, BL: 0x056C
  { 0x00, 0x00, 0x1b, 0x59 },   //5 *MacBookPro14,3 - Intel HD Graphics 630 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x06, 0x00, 0x1b, 0x59 },   //6 Intel HD Graphics 630 - Mobile: 1, PipeCount: 1, PortCount: 1, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1, BL: 0x056C
  { 0x05, 0x00, 0x1c, 0x59 },   //7 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x1e, 0x59 },   //8 Intel HD Graphics 615 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x01, 0x00, 0x1e, 0x59 },   //9 *MacBook10,1 - Intel HD Graphics 615 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x23, 0x59 },   //10 Intel HD Graphics 635 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x26, 0x59 },   //11 Intel Iris Plus Graphics 640 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x02, 0x00, 0x26, 0x59 },   //12 *MacBookPro14,1/iMac18,1 - Intel Iris Plus Graphics 640 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x07, 0x00, 0x26, 0x59 },   //13 Intel Iris Plus Graphics 640 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 21MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0x27, 0x59 },   //14 Intel Iris Plus Graphics 650 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x04, 0x00, 0x27, 0x59 },   //15 *MacBookPro14,2 - Intel Iris Plus Graphics 650 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x09, 0x00, 0x27, 0x59 },   //16 Intel Iris Plus Graphics 650 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x00, 0x00, 0xC0, 0x87 },   //17 Intel UHD Graphics 617 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 34MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
  { 0x05, 0x00, 0xC0, 0x87 },   //18 *MacBookAir8,1 - Intel UHD Graphics 617 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0x056C
};

UINT8 kabylake_hd_vals[12][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,Gfx324"            - MacBookPro
  { 0x01, 0x00, 0x00, 0x00 },   //1 "AAPL,GfxYTile"
  { 0xfa, 0x00, 0x00, 0x00 },   //2 "AAPL00,PanelCycleDelay"
  { 0x11, 0x00, 0x00, 0x08 },   //3 "AAPL00,PanelPowerDown"  - MacBook
  { 0x11, 0x00, 0x00, 0x00 },   //4 "AAPL00,PanelPowerOff"   - MacBook
  { 0xe2, 0x00, 0x00, 0x08 },   //5 "AAPL00,PanelPowerOn"    - MacBook
  { 0x48, 0x00, 0x00, 0x00 },   //6 "AAPL00,PanelPowerUp"    - MacBook
  { 0x3c, 0x00, 0x00, 0x08 },   //7 "AAPL00,PanelPowerDown"  - MacBookPro
  { 0x11, 0x00, 0x00, 0x00 },   //8 "AAPL00,PanelPowerOff"   - MacBookPro
  { 0x19, 0x01, 0x00, 0x08 },   //9 "AAPL00,PanelPowerOn"    - MacBookPro
  { 0x30, 0x00, 0x00, 0x00 },   //10 "AAPL00,PanelPowerUp"   - MacBookPro
  { 0x0c, 0x00, 0x00, 0x00 },   //11 "graphic-options"
};


UINT8 coffeelake_ig_vals[15][4] = {
  { 0x00, 0x00, 0x00, 0x3E },   // 0 Intel UHD Graphics 630 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x03, 0x00, 0x91, 0x3E },   // 1 Intel UHD Graphics 630 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN:  0MB, FBMEM: 0MB, VRAM: 1536MB, Connector: DUMMY3, BL: 0xFFFF
  { 0x00, 0x00, 0x92, 0x3E },   // 2 Intel UHD Graphics 630 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x03, 0x00, 0x92, 0x3E },   // 3 Intel UHD Graphics 630 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN:  0MB, FBMEM: 0MB, VRAM: 1536MB, Connector: DUMMY3, BL: 0xFFFF
  { 0x09, 0x00, 0x92, 0x3E },   // 4 Intel UHD Graphics 630 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DUMMY2, BL: 0xFFFF
  { 0x03, 0x00, 0x98, 0x3E },   // 5 *iMac19,1 - Intel UHD Graphics 630 - Mobile: 0, PipeCount: 0, PortCount: 0, STOLEN:  0MB, FBMEM: 0MB, VRAM: 1536MB, Connector: DUMMY3, BL: 0xFFFF
  { 0x00, 0x00, 0x9B, 0x3E },   // 6 Intel UHD Graphics 630 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x06, 0x00, 0x9B, 0x3E },   // 7 *MacBookPro15,1 - Intel UHD Graphics 630 - Mobile: 1, PipeCount: 1, PortCount: 1, STOLEN: 38MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DUMMY2, BL: 0xFFFF
  { 0x07, 0x00, 0x9B, 0x3E },   // 8 *Macmini8,1/iMac19,2 - Intel UHD Graphics 630 - Mobile: 0, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: DP3, BL: 0xFFFF
  { 0x09, 0x00, 0x9B, 0x3E },   // 9 Intel UHD Graphics 630 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x00, 0x00, 0xA5, 0x3E },   //10 Intel Iris Plus Graphics 655 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x04, 0x00, 0xA5, 0x3E },   //11 *MacBookPro15,2 - Intel Iris Plus Graphics 655 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x05, 0x00, 0xA5, 0x3E },   //12 Intel UHD Graphics 630 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x09, 0x00, 0xA5, 0x3E },   //13 Intel Iris Plus Graphics 655 - Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x05, 0x00, 0xA6, 0x3E },   //14 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
};

UINT8 coffeelake_hd_vals[8][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,Gfx324"            - MacBookPro
  { 0x01, 0x00, 0x00, 0x00 },   //1 "AAPL,GfxYTile"
  { 0xfa, 0x00, 0x00, 0x00 },   //2 "AAPL00,PanelCycleDelay"
  { 0x3c, 0x00, 0x00, 0x08 },   //3 "AAPL00,PanelPowerDown"  - MacBookPro
  { 0x11, 0x00, 0x00, 0x00 },   //4 "AAPL00,PanelPowerOff"   - MacBookPro
  { 0x19, 0x01, 0x00, 0x08 },   //5 "AAPL00,PanelPowerOn"    - MacBookPro
  { 0x30, 0x00, 0x00, 0x00 },   //6 "AAPL00,PanelPowerUp"    - MacBookPro
  { 0x0c, 0x00, 0x00, 0x00 },   //7 "graphic-options"
};


UINT8 cannonlake_ig_vals[14][4] = {
  { 0x00, 0x00, 0x01, 0x0A },   //0 Mobile: 1, PipeCount: 1, PortCount: 1, STOLEN: 34MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1, BL: 0xFFFF
  { 0x00, 0x00, 0x40, 0x5A },   //1 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x09, 0x00, 0x40, 0x5A },   //2 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x00, 0x00, 0x41, 0x5A },   //3 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x09, 0x00, 0x41, 0x5A },   //4 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x00, 0x00, 0x49, 0x5A },   //5 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x09, 0x00, 0x49, 0x5A },   //6 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x00, 0x00, 0x50, 0x5A },   //7 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x09, 0x00, 0x50, 0x5A },   //8 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x00, 0x00, 0x51, 0x5A },   //9 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x09, 0x00, 0x51, 0x5A },   //10 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
  { 0x00, 0x00, 0x52, 0x5A },   //11 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP1/HDMI1, BL: 0xFFFF
  { 0x00, 0x00, 0x59, 0x5A },   //12 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP1/HDMI1, BL: 0xFFFF
  { 0x09, 0x00, 0x59, 0x5A },   //13 Mobile: 1, PipeCount: 3, PortCount: 3, STOLEN: 57MB, FBMEM: 0MB, VRAM: 1536MB, Connector: LVDS1/DP2, BL: 0xFFFF
};

UINT8 cannonlake_hd_vals[8][4] = {
  { 0x01, 0x00, 0x00, 0x00 },   //0 "AAPL,Gfx324"            - MacBookPro
  { 0x01, 0x00, 0x00, 0x00 },   //1 "AAPL,GfxYTile"
  { 0xfa, 0x00, 0x00, 0x00 },   //2 "AAPL00,PanelCycleDelay"
  { 0x3c, 0x00, 0x00, 0x08 },   //3 "AAPL00,PanelPowerDown"  - MacBookPro
  { 0x11, 0x00, 0x00, 0x00 },   //4 "AAPL00,PanelPowerOff"   - MacBookPro
  { 0x19, 0x01, 0x00, 0x08 },   //5 "AAPL00,PanelPowerOn"    - MacBookPro
  { 0x30, 0x00, 0x00, 0x00 },   //6 "AAPL00,PanelPowerUp"    - MacBookPro
  { 0x0c, 0x00, 0x00, 0x00 },   //7 "graphic-options"
};


// The following values came from MacBookPro6,1
UINT8 mbp_HD_os_info[20]  = {
  0x30, 0x49, 0x01, 0x11, 0x01, 0x10, 0x08, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
};

// The following values came from MacBookAir4,1
UINT8 mba_HD3000_tbl_info[18] = {
  0x30, 0x44, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00,
  0x00, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01
};

// The following values came from MacBookAir4,1
UINT8 mba_HD3000_os_info[20] = {
  0x30, 0x49, 0x01, 0x12, 0x12, 0x12, 0x08, 0x00, 0x00, 0x01,
  0xf0, 0x1f, 0x01, 0x00, 0x00, 0x00, 0x10, 0x07, 0x00, 0x00
};

// The following values came from MacBookPro8,1
UINT8 mbp_HD3000_tbl_info[18] = {
  0x30, 0x44, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};

// The following values came from MacBookPro8,1
UINT8 mbp_HD3000_os_info[20] = {
  0x30, 0x49, 0x01, 0x11, 0x11, 0x11, 0x08, 0x00, 0x00, 0x01,
  0xf0, 0x1f, 0x01, 0x00, 0x00, 0x00, 0x10, 0x07, 0x00, 0x00
};

// The following values came from Macmini5,1
UINT8 mn_HD3000_tbl_info[18] = {
  0x30, 0x44, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00,
  0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00
};

// The following values came from Macmini5,1
UINT8 mn_HD3000_os_info[20] = {
  0x30, 0x49, 0x01, 0x50, 0x50, 0x50, 0x08, 0x00, 0x00, 0x01,
  0xf0, 0x1f, 0x01, 0x00, 0x00, 0x00, 0x10, 0x07, 0x00, 0x00
};


static struct gma_gpu_t KnownGPUS[] = {
  { 0xFFFF, "Intel Unsupported"              }, // common name for unsuported devices
#if WILL_WORK
  //============== PowerVR ===================
  //--------Canmore/Sodaville/Groveland-------
  { 0x2E5B, "Intel 500"                      }, //

  //----------------Poulsbo-------------------
  { 0x8108, "Intel 500"                      }, // Menlow
  { 0x8109, "Intel 500"                      }, // Menlow

  //----------------Lincroft------------------
  { 0x4102, "Intel 600"                      }, // Moorestown

  //----------------Cedarview-----------------
  { 0x0BE0, "Intel GMA 3600"                 }, // Cedar Trail
  { 0x0BE1, "Intel GMA 3600"                 }, // Cedar Trail
  { 0x0BE2, "Intel GMA 3650"                 }, // Cedar Trail
  { 0x0BE3, "Intel GMA 3650"                 }, // Cedar Trail

  //----------------Cloverview----------------
  { 0x08C7, "Intel GMA"                      }, // Clover Trail
  { 0x08C8, "Intel GMA"                      }, // Clover Trail
  { 0x08C9, "Intel GMA"                      }, // Clover Trail
  { 0x08CA, "Intel GMA"                      }, // Clover Trail
  { 0x08CB, "Intel GMA"                      }, // Clover Trail
  { 0x08CC, "Intel GMA"                      }, // Clover Trail
  { 0x08CD, "Intel GMA"                      }, // Clover Trail
  { 0x08CE, "Intel GMA"                      }, // Clover Trail
  { 0x08CF, "Intel GMA"                      }, // Clover Trail


  //============== 1st generation ============
  //----------------Auburn--------------------
  { 0x7800, "Intel 740"                      }, // Desktop - Intel 740 GMCH Express Chipset Family

  //----------------Portola-------------------
  { 0x1240, "Intel 752"                      }, // Desktop - Intel 752 GMCH Express Chipset Family

  //----------------Whitney-------------------
  { 0x7121, "Intel 3D graphics 810"          }, // Desktop - Intel 810 GMCH Express Chipset Family
  { 0x7123, "Intel 3D graphics 810"          }, // Desktop - Intel 810-DC100 GMCH Express Chipset Family
  { 0x7125, "Intel 3D graphics 810"          }, // Desktop - Intel 810E GMCH Express Chipset Family

  //----------------Solano--------------------
  { 0x1132, "Intel 3D graphics 815"          }, // Desktop - Intel 815 GMCH Express Chipset Family


  //============== 2nd generation ============
  //----------------Almador-------------------
  { 0x3577, "Intel Extreme Graphics 830"     }, // Mobile - Intel 830M GMCH Express Chipset Family
  { 0x357B, "Intel Extreme Graphics 835"     }, // Desktop - Intel 835G GMCH Express Chipset Family

  //----------------Brookdale-----------------
  { 0x2562, "Intel Extreme Graphics 845"     }, // Desktop - Intel 845G GMCH Express Chipset Family

  //----------------Montara-------------------
  { 0x358E, "Intel Extreme Graphics 2 854"   }, // Mobile - Intel 852GM/855GM GMCH Express Chipset Family
  { 0x3582, "Intel Extreme Graphics 2 855"   }, // Mobile - Intel 852GM/855GM GMCH Express Chipset Family

  //----------------Springdale----------------
  { 0x2572, "Intel Extreme Graphics 2 865"   }, // Desktop - Intel 865G Express Chipset Family


  //============== 3rd generation ============
  //----------------Grantsdale----------------
  { 0x2582, "Intel GMA 900"                  }, // Desktop - Intel 915G Express Chipset Family
  { 0x258A, "Intel GMA 900"                  }, // Desktop - Intel 915GM Express Chipset Family
  { 0x2782, "Intel GMA 900"                  }, // Desktop - Intel 915GV Express Chipset Family

  //----------------Alviso--------------------
  { 0x2592, "Intel GMA 900"                  }, // Mobile - Intel 82915GM/GMS, 910GML Express Chipset Family
  { 0x2792, "Intel GMA 900"                  }, // Mobile - Intel 82915GM/GMS, 910GML Express Chipset Family
#endif
  //----------------Lakeport------------------
  { 0x2772, "Intel GMA 950"                  }, // Desktop - Intel 82945G Express Chipset Family
  { 0x2776, "Intel GMA 950"                  }, // Desktop - Intel 82945G Express Chipset Family

  //----------------Calistoga-----------------
  { 0x27A2, "Intel GMA 950"                  }, // Mobile - Intel 945GM Express Chipset Family - MacBook1,1/MacBook2,1
  { 0x27A6, "Intel GMA 950"                  }, // Mobile - Intel 945GM Express Chipset Family
  { 0x27AE, "Intel GMA 950"                  }, // Mobile - Intel 945GM Express Chipset Family
#if WILL_WORK
  //----------------Bearlake------------------
  { 0x29B2, "Intel GMA 3100"                 }, // Desktop - Intel Q35 Express Chipset Family
  { 0x29B3, "Intel GMA 3100"                 }, // Desktop - Intel Q35 Express Chipset Family
  { 0x29C2, "Intel GMA 3100"                 }, // Desktop - Intel G33/G31 Express Chipset Family
  { 0x29C3, "Intel GMA 3100"                 }, // Desktop - Intel G33/G31 Express Chipset Family
  { 0x29D2, "Intel GMA 3100"                 }, // Desktop - Intel Q33 Express Chipset Family
  { 0x29D3, "Intel GMA 3100"                 }, // Desktop - Intel Q33 Express Chipset Family

  //----------------Pineview------------------
  { 0xA001, "Intel GMA 3150"                 }, // Nettop - Intel NetTop Atom D410
  { 0xA002, "Intel GMA 3150"                 }, // Nettop - Intel NetTop Atom D510
  { 0xA011, "Intel GMA 3150"                 }, // Netbook - Intel NetBook Atom N4x0
  { 0xA012, "Intel GMA 3150"                 }, // Netbook - Intel NetBook Atom N4x0


  //============== 4th generation ============
  //----------------Lakeport------------------
  { 0x2972, "Intel GMA 3000"                 }, // Desktop - Intel 946GZ Express Chipset Family
  { 0x2973, "Intel GMA 3000"                 }, // Desktop - Intel 946GZ Express Chipset Family

  //----------------Broadwater----------------
  { 0x2992, "Intel GMA 3000"                 }, // Desktop - Intel Q965/Q963 Express Chipset Family
  { 0x2993, "Intel GMA 3000"                 }, // Desktop - Intel Q965/Q963 Express Chipset Family
  { 0x29A2, "Intel GMA X3000"                }, // Desktop - Intel G965 Express Chipset Family
  { 0x29A3, "Intel GMA X3000"                }, // Desktop - Intel G965 Express Chipset Family
#endif
  //----------------Crestline-----------------
  { 0x2A02, "Intel GMA X3100"                }, // Mobile - Intel 965 Express Chipset Family - MacBook3,1/MacBook4,1/MacbookAir1,1
  { 0x2A03, "Intel GMA X3100"                }, // Mobile - Intel 965 Express Chipset Family
  { 0x2A12, "Intel GMA X3100"                }, // Mobile - Intel 965 Express Chipset Family
  { 0x2A13, "Intel GMA X3100"                }, // Mobile - Intel 965 Express Chipset Family
#if WILL_WORK
  //----------------Bearlake------------------
  { 0x2982, "Intel GMA X3500"                }, // Desktop - Intel G35 Express Chipset Family
  { 0x2983, "Intel GMA X3500"                }, // Desktop - Intel G35 Express Chipset Family

  //----------------Eaglelake-----------------
  { 0x2E02, "Intel GMA 4500"                 }, // Desktop - Intel 4 Series Express Chipset Family
  { 0x2E03, "Intel GMA 4500"                 }, // Desktop - Intel 4 Series Express Chipset Family
  { 0x2E12, "Intel GMA 4500"                 }, // Desktop - Intel G45/G43 Express Chipset Family
  { 0x2E13, "Intel GMA 4500"                 }, // Desktop - Intel G45/G43 Express Chipset Family
  { 0x2E42, "Intel GMA 4500"                 }, // Desktop - Intel B43 Express Chipset Family
  { 0x2E43, "Intel GMA 4500"                 }, // Desktop - Intel B43 Express Chipset Family
  { 0x2E92, "Intel GMA 4500"                 }, // Desktop - Intel B43 Express Chipset Family
  { 0x2E93, "Intel GMA 4500"                 }, // Desktop - Intel B43 Express Chipset Family
  { 0x2E32, "Intel GMA X4500"                }, // Desktop - Intel G45/G43 Express Chipset Family
  { 0x2E33, "Intel GMA X4500"                }, // Desktop - Intel G45/G43 Express Chipset Family
  { 0x2E22, "Intel GMA X4500"                }, // Mobile - Intel G45/G43 Express Chipset Family
  { 0x2E23, "Intel GMA X4500HD"              }, // Mobile - Intel G45/G43 Express Chipset Family

  //----------------Cantiga-------------------
  { 0x2A42, "Intel GMA X4500MHD"             }, // Mobile - Intel 4 Series Express Chipset Family
  { 0x2A43, "Intel GMA X4500MHD"             }, // Mobile - Intel 4 Series Express Chipset Family

#endif
  //============== 5th generation ============
  //----------------Ironlake------------------
  { 0x0042, "Intel HD Graphics"              }, // Desktop - Clarkdale
  { 0x0046, "Intel HD Graphics"              }, // Mobile - Arrandale - MacBookPro6,x


  //============== 6th generation ============
  //----------------Sandy Bridge--------------
  //GT1
  { 0x0102, "Intel HD Graphics 2000"         }, // Desktop - iMac12,x
  { 0x0106, "Intel HD Graphics 2000"         }, // Mobile
  { 0x010A, "Intel HD Graphics P3000"        }, // Server
  //GT2
  { 0x0112, "Intel HD Graphics 3000"         }, // Desktop
  { 0x0116, "Intel HD Graphics 3000"         }, // Mobile - MacBookAir4,x/MacBookPro8,2/MacBookPro8,3
  { 0x0122, "Intel HD Graphics 3000"         }, // Desktop
  { 0x0126, "Intel HD Graphics 3000"         }, // Mobile - MacBookPro8,1/Macmini5,x


  //============== 7th generation ============
  //----------------Ivy Bridge----------------
  //GT1
  { 0x0152, "Intel HD Graphics 2500"         }, // Desktop - iMac13,x
  { 0x0156, "Intel HD Graphics 2500"         }, // Mobile
  { 0x015A, "Intel HD Graphics 2500"         }, // Server
  { 0x015E, "Intel Ivy Bridge GT1"           }, // Reserved
  //GT2
  { 0x0162, "Intel HD Graphics 4000"         }, // Desktop
  { 0x0166, "Intel HD Graphics 4000"         }, // Mobile - MacBookPro9,x/MacBookPro10,x/MacBookAir5,x/Macmini6,x
  { 0x016A, "Intel HD Graphics P4000"        }, // Server

  //----------------Haswell-------------------
  //GT1
  { 0x0402, "Intel Haswell GT1"              }, // Desktop
  { 0x0406, "Intel Haswell GT1"              }, // Mobile
  { 0x040A, "Intel Haswell GT1"              }, // Server
  { 0x040B, "Intel Haswell GT1"              }, //
  { 0x040E, "Intel Haswell GT1"              }, //
  //GT2
  { 0x0412, "Intel HD Graphics 4600"         }, // Desktop - iMac15,1
  { 0x0416, "Intel HD Graphics 4600"         }, // Mobile
  { 0x041A, "Intel HD Graphics P4600"        }, // Server
  { 0x041B, "Intel Haswell GT2"              }, //
  { 0x041E, "Intel HD Graphics 4400"         }, //
  //GT3
  { 0x0422, "Intel HD Graphics 5000"         }, // Desktop
  { 0x0426, "Intel HD Graphics 5000"         }, // Mobile
  { 0x042A, "Intel HD Graphics 5000"         }, // Server
  { 0x042B, "Intel Haswell GT3"              }, //
  { 0x042E, "Intel Haswell GT3"              }, //
  //GT1
  { 0x0A02, "Intel Haswell GT1"              }, // Desktop ULT
  { 0x0A06, "Intel HD Graphics"              }, // Mobile ULT
  { 0x0A0A, "Intel Haswell GT1"              }, // Server ULT
  { 0x0A0B, "Intel Haswell GT1"              }, // ULT
  { 0x0A0E, "Intel Haswell GT1"              }, // ULT
  //GT2
  { 0x0A12, "Intel Haswell GT2"              }, // Desktop ULT
  { 0x0A16, "Intel HD Graphics 4400"         }, // Mobile ULT
  { 0x0A1A, "Intel Haswell GT2"              }, // Server ULT
  { 0x0A1B, "Intel Haswell GT2"              }, // ULT
  { 0x0A1E, "Intel HD Graphics 4200"         }, // ULT
  //GT3
  { 0x0A22, "Intel Iris Graphics 5100"       }, // Desktop ULT
  { 0x0A26, "Intel HD Graphics 5000"         }, // Mobile ULT - MacBookAir6,x/Macmini7,1
  { 0x0A2A, "Intel Iris Graphics 5100"       }, // Server ULT
  { 0x0A2B, "Intel Iris Graphics 5100"       }, // ULT
  { 0x0A2E, "Intel Iris Graphics 5100"       }, // ULT - MacBookPro11,1
  //GT1
  { 0x0C02, "Intel Haswell GT1"              }, // Desktop SDV
  { 0x0C06, "Intel Haswell GT1"              }, // Mobile SDV
  { 0x0C0A, "Intel Haswell GT1"              }, // Server SDV
  { 0x0C0B, "Intel Haswell GT1"              }, // SDV
  { 0x0C0E, "Intel Haswell GT1"              }, // SDV
  //GT2
  { 0x0C12, "Intel Haswell GT2"              }, // Desktop SDV
  { 0x0C16, "Intel Haswell GT2"              }, // Mobile SDV
  { 0x0C1A, "Intel Haswell GT2"              }, // Server SDV
  { 0x0C1B, "Intel Haswell GT2"              }, // SDV
  { 0x0C1E, "Intel Haswell GT2"              }, // SDV
  //GT3
  { 0x0C22, "Intel Haswell GT3"              }, // Desktop SDV
  { 0x0C26, "Intel Haswell GT3"              }, // Mobile SDV
  { 0x0C2A, "Intel Haswell GT3"              }, // Server SDV
  { 0x0C2B, "Intel Haswell GT3"              }, // SDV
  { 0x0C2E, "Intel Haswell GT3"              }, // SDV
  //GT1
  { 0x0D02, "Intel Haswell GT1"              }, // Desktop CRW
  { 0x0D06, "Intel Haswell GT1"              }, // Mobile CRW
  { 0x0D0A, "Intel Haswell GT1"              }, // Server CRW
  { 0x0D0B, "Intel Haswell GT1"              }, // CRW
  { 0x0D0E, "Intel Haswell GT1"              }, // CRW
  //GT2
  { 0x0D12, "Intel HD Graphics 4600"         }, // Desktop CRW
  { 0x0D16, "Intel HD Graphics 4600"         }, // Mobile CRW
  { 0x0D1A, "Intel Haswell GT2"              }, // Server CRW
  { 0x0D1B, "Intel Haswell GT2"              }, // CRW
  { 0x0D1E, "Intel Haswell GT2"              }, // CRW
  //GT3
  { 0x0D22, "Intel Iris Pro Graphics 5200"   }, // Desktop CRW - iMac14,1/iMac14,4
  { 0x0D26, "Intel Iris Pro Graphics 5200"   }, // Mobile CRW - MacBookPro11,2/MacBookPro11,3
  { 0x0D2A, "Intel Iris Pro Graphics 5200"   }, // Server CRW
  { 0x0D2B, "Intel Iris Pro Graphics 5200"   }, // CRW
  { 0x0D2E, "Intel Iris Pro Graphics 5200"   }, // CRW

  //----------------ValleyView----------------
  { 0x0F30, "Intel HD Graphics"              }, // Bay Trail
  { 0x0F31, "Intel HD Graphics"              }, // Bay Trail
  { 0x0F32, "Intel HD Graphics"              }, // Bay Trail
  { 0x0F33, "Intel HD Graphics"              }, // Bay Trail
  { 0x0155, "Intel HD Graphics"              }, // Bay Trail
  { 0x0157, "Intel HD Graphics"              }, // Bay Trail


  //============== 8th generation ============
  //----------------Broadwell-----------------
  //GT1
  { 0x1602, "Intel Broadwell GT1"            }, // Desktop
  { 0x1606, "Intel Broadwell GT1"            }, // Mobile
  { 0x160A, "Intel Broadwell GT1"            }, //
  { 0x160B, "Intel Broadwell GT1"            }, //
  { 0x160D, "Intel Broadwell GT1"            }, //
  { 0x160E, "Intel Broadwell GT1"            }, //
  //GT2
  { 0x1612, "Intel HD Graphics 5600"         }, // Mobile
  { 0x1616, "Intel HD Graphics 5500"         }, // Mobile
  { 0x161A, "Intel Broadwell GT2"            }, //
  { 0x161B, "Intel Broadwell GT2"            }, //
  { 0x161D, "Intel Broadwell GT2"            }, //
  { 0x161E, "Intel HD Graphics 5300"         }, // Ultramobile - MacBook8,1
  //GT3
  { 0x1626, "Intel HD Graphics 6000"         }, // Mobile - iMac16,1/MacBookAir7,x
  { 0x162B, "Intel Iris Graphics 6100"       }, // Mobile - MacBookPro12,1
  { 0x162D, "Intel Iris Pro Graphics P6300"  }, // Workstation, Mobile Workstation
  //GT3e
  { 0x1622, "Intel Iris Pro Graphics 6200"   }, // Desktop, Mobile - iMac16,2
  { 0x162A, "Intel Iris Pro Graphics P6300"  }, // Workstation
  //RSVD
  { 0x162E, "Intel Broadwell RSVD"           }, // Reserved
  { 0x1632, "Intel Broadwell RSVD"           }, // Reserved
  { 0x1636, "Intel Broadwell RSVD"           }, // Reserved
  { 0x163A, "Intel Broadwell RSVD"           }, // Reserved
  { 0x163B, "Intel Broadwell RSVD"           }, // Reserved
  { 0x163D, "Intel Broadwell RSVD"           }, // Reserved
  { 0x163E, "Intel Broadwell RSVD"           }, // Reserved

  //------------Cherryview/Braswell-----------
  { 0x22B0, "Intel HD Graphics 400"          }, // Cherry Trail - Atom x5 series - Z83X0/Z8550
  { 0x22B1, "Intel HD Graphics 405"          }, // Cherry Trail - Atom x7 series - Z8750
  { 0x22B2, "Intel HD Graphics 400"          }, // Braswell - Cerelon QC/DC series - X3X60
  { 0x22B3, "Intel HD Graphics 405"          }, // Braswell - Pentium QC series - X3710


  //============== 9th generation ============
  //----------------Skylake-------------------
  //GT1
  { 0x1902, "Intel HD Graphics 510"          }, // Desktop
  { 0x1906, "Intel HD Graphics 510"          }, // Mobile
  { 0x190A, "Intel Skylake GT1"              }, //
  { 0x190B, "Intel HD Graphics 510"          }, //
  { 0x190E, "Intel Skylake GT1"              }, //
  //GT2
  { 0x1912, "Intel HD Graphics 530"          }, // Desktop - iMac17,1
  { 0x1916, "Intel HD Graphics 520"          }, // Mobile
  { 0x191A, "Intel Skylake GT2"              }, //
  { 0x191B, "Intel HD Graphics 530"          }, // Mobile - MacBookPro13,3
  { 0x191D, "Intel HD Graphics P530"         }, // Workstation, Mobile Workstation
  { 0x191E, "Intel HD Graphics 515"          }, // Mobile - MacBook9,1
  { 0x1921, "Intel HD Graphics 520"          }, //
  //GT2f
  { 0x1913, "Intel Skylake GT2f"             }, //
  { 0x1915, "Intel Skylake GT2f"             }, //
  { 0x1917, "Intel Skylake GT2f"             }, //
  //GT3
  { 0x1923, "Intel HD Graphics 535"          }, //
  //GT3e
  { 0x1926, "Intel Iris Graphics 540"        }, // Mobile - MacBookPro13,1
  { 0x1927, "Intel Iris Graphics 550"        }, // Mobile - MacBookPro13,2
  { 0x192B, "Intel Iris Graphics 555"        }, //
  { 0x192D, "Intel Iris Graphics P555"       }, // Workstation
  //GT4
  { 0x192A, "Intel Skylake GT4"              }, //
  //GT4e
  { 0x1932, "Intel Iris Pro Graphics 580"    }, // Desktop
  { 0x193A, "Intel Iris Pro Graphics P580"   }, // Server
  { 0x193B, "Intel Iris Pro Graphics 580"    }, // Mobile
  { 0x193D, "Intel Iris Pro Graphics P580"   }, // Workstation, Mobile Workstation

  //----------------Goldmont------------------
  { 0x0A84, "Intel HD Graphics"              }, // Broxton(cancelled)
  { 0x1A84, "Intel HD Graphics"              }, // Broxton(cancelled)
  { 0x1A85, "Intel HD Graphics"              }, // Broxton(cancelled)
  { 0x5A84, "Intel HD Graphics 505"          }, // Apollo Lake
  { 0x5A85, "Intel HD Graphics 500"          }, // Apollo Lake

  //----------------Kaby Lake-----------------
  //GT1
  { 0x5902, "Intel HD Graphics 610"          }, // Desktop
  { 0x5906, "Intel HD Graphics 610"          }, // Mobile
  { 0x5908, "Intel Kaby Lake GT1"            }, //
  { 0x590A, "Intel Kaby Lake GT1"            }, //
  { 0x590B, "Intel Kaby Lake GT1"            }, //
  { 0x590E, "Intel Kaby Lake GT1"            }, //
  //GT1.5
  { 0x5913, "Intel Kaby Lake GT1.5"          }, //
  { 0x5915, "Intel Kaby Lake GT1.5"          }, //
  //GT2
  { 0x5912, "Intel HD Graphics 630"          }, // Desktop - iMac18,2/iMac18,3
  { 0x5916, "Intel HD Graphics 620"          }, // Mobile
  { 0x591A, "Intel HD Graphics P630"         }, // Server
  { 0x591B, "Intel HD Graphics 630"          }, // Mobile - MacBookPro14,3
  { 0x591D, "Intel HD Graphics P630"         }, // Workstation, Mobile Workstation
  { 0x591E, "Intel HD Graphics 615"          }, // Mobile - MacBook10,1
  //GT2F
  { 0x5921, "Intel Kaby Lake GT2F"           }, //
  //GT3
  { 0x5923, "Intel HD Graphics 635"          }, //
  { 0x5926, "Intel Iris Plus Graphics 640"   }, // Mobile - MacBookPro14,1/iMac18,1
  { 0x5927, "Intel Iris Plus Graphics 650"   }, // Mobile - MacBookPro14,2
  //GT4
  { 0x593B, "Intel Kaby Lake GT4"            }, //

  //-------------Kaby Lake Refresh------------
  //GT1.5
  { 0x5917, "Intel UHD Graphics 620"         }, // Mobile

  //----------------Amber Lake----------------
  //GT2
  { 0x591C, "Intel UHD Graphics 615"         }, // Kaby Lake
  { 0x87C0, "Intel UHD Graphics 617"         }, // Kaby Lake - Mobile - MacBookAir8,1
  { 0x87CA, "Intel UHD Graphics 615"         }, // Comet Lake

  //----------------Coffee Lake---------------
  //GT1
  { 0x3E90, "Intel UHD Graphics 610"         }, // Desktop
  { 0x3E93, "Intel UHD Graphics 610"         }, // Desktop
  { 0x3E99, "Intel Coffee Lake GT1"          }, //
  //GT2
  { 0x3E91, "Intel UHD Graphics 630"         }, // Desktop
  { 0x3E92, "Intel UHD Graphics 630"         }, // Desktop
  { 0x3E94, "Intel Coffee Lake GT2"          }, //
  { 0x3E96, "Intel Coffee Lake GT2"          }, //
  { 0x3E98, "Intel UHD Graphics 630"         }, // Desktop
  { 0x3E9A, "Intel Coffee Lake GT2"          }, //
  { 0x3E9B, "Intel UHD Graphics 630"         }, // Mobile - MacBookPro15,1/Macmini8,1
  { 0x3EA9, "Intel Coffee Lake GT2"          }, //
  //GT3
  { 0x3EA5, "Intel Iris Plus Graphics 655"   }, // Mobile - MacBookPro15,2
  { 0x3EA6, "Intel Coffee Lake GT3"          }, //
  { 0x3EA7, "Intel Coffee Lake GT3"          }, //
  { 0x3EA8, "Intel Coffee Lake GT3"          }, //

  //----------------Whiskey Lake--------------
  //GT1
  { 0x3EA1, "Intel Whiskey Lake GT1"         }, //
  { 0x3EA4, "Intel Whiskey Lake GT1"         }, //
  //GT2
  { 0x3EA0, "Intel UHD Graphics 620"         }, // Mobile
  { 0x3EA3, "Intel Whiskey Lake GT2"         }, //
  //GT3
  { 0x3EA2, "Intel Whiskey Lake GT3"         }, //

  //----------------Comet Lake----------------
  //GT1
  { 0x9B21, "Intel Comet Lake GT1"           }, //
  { 0x9BA0, "Intel Comet Lake GT1"           }, //
  { 0x9BA2, "Intel Comet Lake GT1"           }, //
  { 0x9BA4, "Intel Comet Lake GT1"           }, //
  { 0x9BA5, "Intel Comet Lake GT1"           }, //
  { 0x9BA8, "Intel Comet Lake GT1"           }, //
  { 0x9BAA, "Intel Comet Lake GT1"           }, //
  { 0x9BAB, "Intel Comet Lake GT1"           }, //
  { 0x9BAC, "Intel Comet Lake GT1"           }, //
  //GT2
  { 0x9B41, "Intel UHD Graphics 620"         }, // Mobile
  { 0x9BC0, "Intel Comet Lake GT2"           }, //
  { 0x9BC2, "Intel Comet Lake GT2"           }, //
  { 0x9BC4, "Intel Comet Lake GT2"           }, //
  { 0x9BC5, "Intel Comet Lake GT2"           }, //
  { 0x9BC8, "Intel Comet Lake GT2"           }, //
  { 0x9BCA, "Intel UHD Graphics 620"         }, // Mobile
  { 0x9BCB, "Intel Comet Lake GT2"           }, //
  { 0x9BCC, "Intel Comet Lake GT2"           }, //

  //----------------Gemini Lake---------------
  { 0x3184, "Intel UHD Graphics 605"         }, //
  { 0x3185, "Intel UHD Graphics 600"         }, //


  //============== 10th generation ===========
  //----------------Cannonlake----------------
  //GTx
  { 0x5A40, "Intel Cannonlake GTx"           }, //
  //GT0.5
  { 0x5A49, "Intel Cannonlake GT0.5"         }, //
  { 0x5A4A, "Intel Cannonlake GT0.5"         }, //
  //GT1
  { 0x0A01, "Intel Cannonlake GT1"           }, // Desktop
  { 0x5A41, "Intel Cannonlake GT1"           }, //
  { 0x5A42, "Intel Cannonlake GT1"           }, //
  { 0x5A44, "Intel Cannonlake GT1"           }, //
  //GT1.5
  { 0x5A59, "Intel Cannonlake GT1.5"         }, //
  { 0x5A5A, "Intel Cannonlake GT1.5"         }, //
  { 0x5A5C, "Intel Cannonlake GT1.5"         }, //
  //GT2
  { 0x5A50, "Intel Cannonlake GT2"           }, //
  { 0x5A51, "Intel Cannonlake GT2"           }, //
  { 0x5A52, "Intel Cannonlake GT2"           }, // Mobile
  { 0x5A54, "Intel Cannonlake GT2"           }, // Mobile


  //============== 11th generation ===========
  //----------------Ice Lake------------------
  //GT0.5
  { 0x8A71, "Intel Ice Lake GT0.5"           }, //
  //GT1
  { 0x8A5B, "Intel Ice Lake GT1"             }, //
  { 0x8A5D, "Intel Ice Lake GT1"             }, //
  //GT1.5
  { 0x8A5A, "Intel Ice Lake GT1.5"           }, //
  { 0x8A5C, "Intel Ice Lake GT1.5"           }, //
  //GT2
  { 0x8A50, "Intel Ice Lake GT2"             }, //
  { 0x8A51, "Intel Ice Lake GT2"             }, //
  { 0x8A52, "Intel Ice Lake GT2"             }, //

  //----------------Lakefield-----------------
  { 0x9840, "Intel Lakefield"                }, //
  { 0x9850, "Intel Lakefield"                }, //

  //----------------Jasper Lake---------------
  { 0x4500, "Intel Jasper Lake"              }, //

};


#endif /* !__LIBSAIO_GMA_H */
