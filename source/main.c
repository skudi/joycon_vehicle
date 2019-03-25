#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include <switch.h>

#include "image_bin.h"//I may try to add a custom background image in the future. It doesn't work at the current state!

//The code is based on the HID examples of libnx: https://github.com/switchbrew/switch-examples

int main(int argc, char **argv)
{
    u32 VibrationDeviceHandles[2];
    Result rc = 0, rc2 = 0;
	
	u32 irhandle=0;
    IrsImageTransferProcessorConfig config;
    IrsImageTransferProcessorState state;
    size_t ir_buffer_size = 0x12c00;
    u8 *ir_buffer = NULL;

    u32 width, height;
    u32 ir_width, ir_height;
    u32 pos, pos2=0;
    u32* framebuf;
	
	
    //u8*  imageptr = (u8*)image_bin;

    HidVibrationValue VibrationValue;
    HidVibrationValue VibrationValue_stop;

    gfxInitDefault();
    consoleInit(NULL);
	
	//All the printfs will be overwritten once the IR image is displayed
    printf("Who needs Labo anyways? V3.\nWritten by Tcm0 (fuk-team.tk)\n");

    //Two VibrationDeviceHandles are returned: first one for left-joycon, second one for right-joycon.
    rc = hidInitializeVibrationDevices(VibrationDeviceHandles, 2, CONTROLLER_PLAYER_1, LAYOUT_DEFAULT);
    printf("hidInitializeVibrationDevice() returned: 0x%x\n", rc);

    if (R_SUCCEEDED(rc)) printf("Touch the screen to let the joy cons vibrate.\n");

    VibrationValue.amp_low   = 0.2f;
    VibrationValue.freq_low  = 10.0f;
    VibrationValue.amp_high  = 2.0f;
    VibrationValue.freq_high = 166.0f;	//previously 170

    memset(&VibrationValue_stop, 0, sizeof(HidVibrationValue));
    // Switch like stop behavior with muted band channels and frequencies set to default.
    VibrationValue_stop.freq_low  = 160.0f;
        VibrationValue_stop.freq_high = 320.0f;
		
	char left_on = 0;
	char right_on = 0;
	char prev_left_on = 0;
	char prev_right_on = 0;
	
	
	//Initialization of the IR camera
	ir_buffer = (u8*)malloc(ir_buffer_size);
	if (ir_buffer==NULL)
    {
        rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        printf("Failed to allocate memory for ir_buffer.\n");
    }
    else
    {
        memset(ir_buffer, 0, ir_buffer_size);
    }
	
    if (R_SUCCEEDED(rc))
    {
        rc = irsInitialize();
        printf("irsInitialize() returned 0x%x\n", rc);
    }

    if (R_SUCCEEDED(rc))
    {
        rc = irsActivateIrsensor(1);
        printf("irsActivateIrsensor() returned 0x%x\n", rc);
    }

    if (R_SUCCEEDED(rc))
    {
        rc = irsGetIrCameraHandle(&irhandle, CONTROLLER_PLAYER_1);
        printf("irsGetIrCameraHandle() returned 0x%x\n", rc);
    }

    if (R_SUCCEEDED(rc))
    {
        irsGetDefaultImageTransferProcessorConfig(&config);
        rc = irsRunImageTransferProcessor(irhandle, &config, 0x100000);
        printf("irsRunImageTransferProcessor() returned 0x%x\n", rc);
    }

    if (R_SUCCEEDED(rc))
    {
        //Switch to using regular framebuffer.
        consoleClear();
        gfxSetMode(GfxMode_LinearDouble);
    }
	
	//More stuff that would be necessary to display a background image. But again, it doesn't work yet.
	//framebuf = (u32*) gfxGetFramebuffer((u32*)&width, (u32*)&height);
    u32 x, y;
				
	//for (y=0; y<height; y++)//Access the buffer linearly.
    //{
    //     for (x=0; x<width; x++)
	//	{
    //        pos = y * width + x;
    //        framebuf[pos] = RGBA8_MAXALPHA(imageptr[pos*3+0]/*+(cnt*4)*/, imageptr[pos*3+1], imageptr[pos*3+2]);
	//	}
	//}
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
		
        if (R_SUCCEEDED(rc))
        {
			//Draw the IR image
			framebuf = (u32*) gfxGetFramebuffer((u32*)&width, (u32*)&height);
			
			 rc2 = irsGetImageTransferProcessorState(irhandle, ir_buffer, ir_buffer_size, &state);

            if (R_SUCCEEDED(rc2))
            {
				//make the complete framebuffer black
                memset(framebuf, 0, gfxGetFramebufferSize());

                //IR image width/height with the default config.
                //The image is grayscale (1 byte per pixel / 8bits, with 1 color-component).
                ir_width = 240;
                ir_height = 320;

                for (y=0; y<ir_height; y++)//Access the buffer linearly.
                {
                    for (x=0; x<ir_width; x++)
                    {
                        pos = y * width + x-(ir_width/2) + 640;		//Position of the current pixel in the frame buffer for the screen
                        pos2 = x * ir_height + y;//The IR image/camera is sideways with the joycon held flat. Position of the current
						//pixel at the image buffer of the camera
                        framebuf[pos] = RGBA8_MAXALPHA(/*ir_buffer[pos2]*/0, ir_buffer[pos2], /*ir_buffer[pos2]*/0);	//The R and B values are 0
                    }
                }
            }
			
            //Calling hidSendVibrationValue is really only needed when sending a new VibrationValue.
			//Turn the rumble motors on when the correct side of the screen gets pressed
			//Remember that we have multitouch. We have to cycle through every touch
			for (int i = 0; i < touch_count; i++)
			{
				hidTouchRead(&touch, i);
				if(touch.px <= 640 && touch_count > 0) {
					left_on = 1;
				}
				if(touch.px >= 640 && touch_count > 0) {
					right_on = 1;
				}
			}
			
			if (left_on == 1 && prev_left_on == 0) {
				//right joycon (the vehicle goes left if the right joy con vibrates)
				rc2 = hidSendVibrationValue(&VibrationDeviceHandles[1], &VibrationValue);
				if (R_FAILED(rc2)) printf("hidSendVibrationValue() returned: 0x%x\n", rc2);
			}
			
			if (right_on == 0 && prev_right_on == 1) {
				//left joycon
				rc2 = hidSendVibrationValue(&VibrationDeviceHandles[0], &VibrationValue);
				if (R_FAILED(rc2)) printf("hidSendVibrationValue() returned: 0x%x\n", rc2);
			}
            //iprintf("TC: %d, Tl: %d, TR: %d\n", touch_count, left_on, right_on);
            
        }
			//All debugging printfs in the loop are commented out because it would interfere with the IR image
			//Turn rumble motors back off when the screen region isn't pressed any more.
            if (left_on == 0) rc2 = hidSendVibrationValue(&VibrationDeviceHandles[1], &VibrationValue_stop);
            //if (R_FAILED(rc2)) printf("hidSendVibrationValue() for stop returned: 0x%x\n", rc2);

            if (right_on == 0) rc2 = hidSendVibrationValue(&VibrationDeviceHandles[0], &VibrationValue_stop);
            //if (R_FAILED(rc2)) printf("hidSendVibrationValue() for stop returned: 0x%x\n", rc2);
		
		prev_left_on = left_on;
		prev_right_on = right_on;
		left_on = right_on = 0;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gfxWaitForVsync();
    }

    irsStopImageProcessor(irhandle);
    irsExit();

    free(ir_buffer);
	
	
    gfxExit();
    return 0;
}

