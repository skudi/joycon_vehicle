#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#include <switch.h>

// Define the desired framebuffer resolution (here we set it to 720p).
#define FB_WIDTH  1280
#define FB_HEIGHT 720
#define MAXVOL 0.8f;

void userAppInit(void)
{
	Result rc;

	rc = irsInitialize();
	if (R_FAILED(rc))
		fatalSimple(rc);
}

void userAppExit(void)
{
	irsExit();
}

__attribute__((format(printf, 1, 2)))
static int error_screen(const char* fmt, ...)
{
	consoleInit(NULL);
	va_list va;
	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
	printf("\x1b[16;16HPress PLUS to exit.");
	while (appletMainLoop())
	{
		hidScanInput();
		if (hidKeysDown(CONTROLLER_P1_AUTO) & KEY_PLUS)
			break;
		consoleUpdate(NULL);
	}
	consoleExit(NULL);
	return EXIT_FAILURE;
}

#define RUMBLECOUNT 2

int main(int argc, char **argv)
{
	u32 VibrationDeviceHandles[RUMBLECOUNT];
	Result rc = 0, rc2 = 0;
	
	u32 irhandle=0;
	IrsImageTransferProcessorConfig config;
	IrsImageTransferProcessorState state;
	size_t ir_buffer_size = 0x12c00;
	u8 *ir_buffer = NULL;

	const u32 ir_width = 240;
	const u32 ir_height = 320;
	u32 scrpos, irpos=0;
	
	HidVibrationValue VibrationValue[RUMBLECOUNT];
	float newVolume[RUMBLECOUNT];

	//Two VibrationDeviceHandles are returned: first one for left-joycon, second one for right-joycon.
	rc = hidInitializeVibrationDevices(VibrationDeviceHandles, 2, CONTROLLER_PLAYER_1, LAYOUT_DEFAULT);

	for (int rumbleNum=0; rumbleNum<RUMBLECOUNT; rumbleNum++) {
		//initialize vibration settings for booth rumbles
		VibrationValue[rumbleNum].freq_low  = 10.0f;
		VibrationValue[rumbleNum].amp_low   = 0.0f;
		VibrationValue[rumbleNum].freq_high = 166.0f;
		VibrationValue[rumbleNum].amp_high  = 0.0f;
	}

	//Initialization of the IR camera
	ir_buffer = (u8*)malloc(ir_buffer_size);
	if (!ir_buffer)
	{
		rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
		return error_screen("Failed to allocate memory for ir_buffer.\n");
	}
	memset(ir_buffer, 0, ir_buffer_size);

	rc = irsActivateIrsensor(1);
	if (R_FAILED(rc))
		return error_screen("irsActivateIrsensor() returned 0x%x\n", rc);
	
	rc = irsGetIrCameraHandle(&irhandle, CONTROLLER_PLAYER_1);
	if (R_FAILED(rc))
		return error_screen("irsGetIrCameraHandle() returned 0x%x\n", rc);

	irsGetDefaultImageTransferProcessorConfig(&config);
	rc = irsRunImageTransferProcessor(irhandle, &config, 0x100000);
	if (R_FAILED(rc))
		return error_screen("irsRunImageTransferProcessor() returned 0x%x\n", rc);

	Framebuffer fb;
	framebufferCreate(&fb, nwindowGetDefault(), FB_WIDTH, FB_HEIGHT, PIXEL_FORMAT_RGBA_8888, 2);
	framebufferMakeLinear(&fb);
	
	//More stuff that would be necessary to display a background image. But again, it doesn't work yet.
	//framebuf = (u32*) gfxGetFramebuffer((u32*)&width, (u32*)&height);
	u32 x, y;
				
	// Main loop
	while(appletMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();
		touchPosition touch;

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		//kHeld returns which buttons are currently held, KeysUp which buttons aren't pressed. But we only need kDown to return
		//to the main menu if the user presses KEY_PLUS
		u32 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
		//u32 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
		//u32 kUp = hidKeysUp(CONTROLLER_P1_AUTO);
		u32 touch_count = hidTouchCount();

		if (kDown & KEY_PLUS) break; // break in order to return to hbmenu
		
			
		u32 stride;
		u32* framebuf = (u32*)framebufferBegin(&fb, &stride);

		//Draw the IR image if it's ready
		rc = irsGetImageTransferProcessorState(irhandle, ir_buffer, ir_buffer_size, &state);
		if (R_SUCCEEDED(rc)) {
			//IR image width/height with the default config.
			//The image is grayscale (1 byte per pixel / 8bits, with 1 color-component).
			for (y=0; y<ir_height; y++)//Access the buffer linearly.
			{
				for (x=0; x<ir_width; x++)
				{
					scrpos = y * stride/sizeof(u32) + (ir_width/2) + FB_WIDTH/2 - x;		//draw IR screen from right to left at the center
					irpos = x * ir_height + y;//The IR image/camera is sideways with the joycon held flat. Position of the current
					//pixel at the image buffer of the camera
					framebuf[scrpos] = RGBA8_MAXALPHA(/*ir_buffer[irpos]*/0, ir_buffer[irpos], /*ir_buffer[irpos]*/0);	//The R and B values are 0
				}
			}
		}

		for (int rumbleNum=0; rumbleNum<RUMBLECOUNT; rumbleNum++) {
			newVolume[rumbleNum] = 0;
		}
			
		//Calling hidSendVibrationValue is really only needed when sending a new VibrationValue.
		//Turn the rumble motors on when the correct side of the screen gets pressed
		//Remember that we have multitouch. We have to cycle through every touch
		for (int i = 0; i < touch_count; i++)
		{
			hidTouchRead(&touch, i);
			//draw dot at touched point //DEBUG
			framebuf[touch.py*stride/sizeof(u32)+touch.px]=RGBA8_MAXALPHA(0xff,0,0); //DEBUG
			int rumbleNum = 0; //right rumble
			if(touch.px < 640) {
				rumbleNum = 1; //left rumble
			}
			newVolume[rumbleNum] = (1.0f - (float)touch.py / (float)FB_HEIGHT) * MAXVOL;
		}

		//send new vibration settings if they differ from current
		for (int rumbleNum=0; rumbleNum<RUMBLECOUNT; rumbleNum++) {
			if (VibrationValue[rumbleNum].amp_high != newVolume[rumbleNum]) {
				framebuf[(int)(VibrationValue[rumbleNum].amp_high*FB_HEIGHT*stride/sizeof(u32))+rumbleNum*(FB_WIDTH-1)]=RGBA8_MAXALPHA(0xff,0,0); //DEBUG
				framebuf[(int)(newVolume[rumbleNum]*FB_HEIGHT*stride/sizeof(u32))+rumbleNum*(FB_WIDTH-1)]=RGBA8_MAXALPHA(0,0xff,0); //DEBUG
				VibrationValue[rumbleNum].amp_high = newVolume[rumbleNum];
				VibrationValue[rumbleNum].amp_low = newVolume[rumbleNum]>0?0.2f:0.0f;
				rc = hidSendVibrationValue(&VibrationDeviceHandles[rumbleNum], &VibrationValue[rumbleNum]);
				if (R_FAILED(rc2)) error_screen("hidSendVibrationValue() returned: 0x%x\n", rc);
			}
		}
			
		framebufferEnd(&fb);
	}

	//mute vibrations
	for (int rumbleNum=0; rumbleNum<RUMBLECOUNT; rumbleNum++) {
		VibrationValue[rumbleNum].amp_high = 0.0f;
		VibrationValue[rumbleNum].amp_low = 0.0f;
		rc = hidSendVibrationValue(&VibrationDeviceHandles[rumbleNum], &VibrationValue[rumbleNum]);
		if (R_FAILED(rc2)) error_screen("hidSendVibrationValue() returned: 0x%x\n", rc);
	}

	framebufferClose(&fb);
	irsStopImageProcessor(irhandle);
	free(ir_buffer);
	return 0;
}

