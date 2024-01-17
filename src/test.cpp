#include <iostream>
#include <X11/Xlib.h> 
#include <assert.h>  
#include "math.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h> 
#include "MvCameraControl.h"

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <pthread.h>

bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("The Pointer of pstMVDevInfo is NULL!\n");
        return false;
    }
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

        // ch:打印当前相机ip和用户自定义名字 | en:print current ip and user defined name
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
        printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
        printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
        printf("UserDefinedName: %s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    }
    else
    {
        printf("Not support.\n");
    }

    return true;
}

int main()
{
    int nRet = MV_OK;

    void* handle = NULL;
	unsigned char * pData = NULL;        
    unsigned char *pDataForRGB = NULL;
    unsigned char *pDataForSaveImage = NULL;
    do 
    {
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

        // 枚举设备
        // enum device
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            printf("MV_CC_EnumDevices fail! nRet [%x]\n", nRet);
            break;
        }
        if (stDeviceList.nDeviceNum > 0)
        {
            for (int i = 0; i < stDeviceList.nDeviceNum; i++)
            {
                printf("[device %d]:\n", i);
                MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
                if (NULL == pDeviceInfo)
                {
                    break;
                } 
                PrintDeviceInfo(pDeviceInfo);            
            }  
        } 
        else
        {
            printf("Find No Devices!\n");
            break;
        }
        unsigned int nIndex = 0;
        //scanf("%d", &nIndex);

        if (nIndex >= stDeviceList.nDeviceNum)
        {
            printf("Intput error!\n");
            break;
        }

        // 选择设备并创建句柄
        // select device and create handle
        nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        if (MV_OK != nRet)
        {
            printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
            break;
        }

        // 打开设备
        // open device
        nRet = MV_CC_OpenDevice(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_OpenDevice fail! nRet [%x]\n", nRet);
            break;
        }
		
        // ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
        if (stDeviceList.pDeviceInfo[nIndex]->nTLayerType == MV_GIGE_DEVICE)
        {
            int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
            if (nPacketSize > 0)
            {
                nRet = MV_CC_SetIntValue(handle,"GevSCPSPacketSize",nPacketSize);
                if(nRet != MV_OK)
                {
                    printf("Warning: Set Packet Size fail nRet [0x%x]!\n", nRet);
                }
            }
            else
            {
                printf("Warning: Get Packet Size fail nRet [0x%x]!\n", nPacketSize);
            }
        }
		
        nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 0);
        if (MV_OK != nRet)
        {
            printf("MV_CC_SetTriggerMode fail! nRet [%x]\n", nRet);
            break;
        }

        // ch:获取数据包大小 | en:Get payload size
        MVCC_INTVALUE stParam;
        memset(&stParam, 0, sizeof(MVCC_INTVALUE));
        nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParam);
        if (MV_OK != nRet)
        {
            printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
            break;
        }

        // 开始取流
        // start grab image
        nRet = MV_CC_StartGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_StartGrabbing fail! nRet [%x]\n", nRet);
            break;
        }
        
        MV_FRAME_OUT_INFO_EX stImageInfo = {0};
        memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
        pData = (unsigned char *)malloc(sizeof(unsigned char) * stParam.nCurValue);
        if (NULL == pData)
        {
            break;
        }

        cv::Mat frame,image;
        unsigned int nDataSize = stParam.nCurValue;
        nRet = MV_CC_GetOneFrameTimeout(handle, pData, nDataSize, &stImageInfo, 1000);
        if (MV_OK != nRet)
        {
            break;
        }
        pDataForRGB = (unsigned char*)malloc(stImageInfo.nWidth * stImageInfo.nHeight * 4 + 2048);//切勿放在循环里面，会内存泄露
        if (NULL == pDataForRGB)
            {
                break;
        }
        while(1){
    
            nRet = MV_CC_GetOneFrameTimeout(handle, pData, nDataSize, &stImageInfo, 1000);
            if (nRet == MV_OK)
            {
                //printf("Now you GetOneFrame, Width[%d], Height[%d], nFrameNum[%d]\n\n", 
                //stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum);
                
                
                // 像素格式转换
                // convert pixel format 
                MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};
                // 从上到下依次是：图像宽，图像高，输入数据缓存，输入数据大小，源像素格式，
                // 目标像素格式，输出数据缓存，提供的输出缓冲区大小
                // Top to bottom are：image width, image height, input data buffer, input data size, source pixel format, 
                // destination pixel format, output data buffer, provided output buffer size
                stConvertParam.nWidth = stImageInfo.nWidth;
                stConvertParam.nHeight = stImageInfo.nHeight;
                stConvertParam.pSrcData = pData;
                stConvertParam.nSrcDataLen = stImageInfo.nFrameLen;
                stConvertParam.enSrcPixelType = stImageInfo.enPixelType;
                stConvertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
                stConvertParam.pDstBuffer = pDataForRGB;
                stConvertParam.nDstBufferSize = stImageInfo.nWidth * stImageInfo.nHeight *  4 + 2048;
                nRet = MV_CC_ConvertPixelType(handle, &stConvertParam);
                if (MV_OK != nRet)
                {
                    printf("MV_CC_ConvertPixelType fail! nRet [%x]\n", nRet);
                    break;
                }
                image = cv::Mat(cv::Size(stImageInfo.nWidth, stImageInfo.nHeight), CV_8UC3, pDataForRGB); 
                cv::cvtColor(image, frame, cv::COLOR_RGB2BGR); 
                cv::imshow("frame", frame);
                // 按下ESC键退出循环
                int keyCode = cv::waitKey(30);
                if (keyCode == 27) {
                    break;
                }
                cv::destroyWindow;
                cv::destroyAllWindows;

            }
        else
        {
            printf("No data[%x]\n", nRet);
        }
        }

        // 停止取流
        // end grab image
        nRet = MV_CC_StopGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_StopGrabbing fail! nRet [%x]\n", nRet);
            break;
        }

        // 销毁句柄
        // destroy handle
        nRet = MV_CC_DestroyHandle(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_DestroyHandle fail! nRet [%x]\n", nRet);
            break;
        }
    } while (0);

    if (nRet != MV_OK)
    {
        if (handle != NULL)
        {
            MV_CC_DestroyHandle(handle);
            handle = NULL;
        }
    }
    if (pData)
    {
        free(pData);	
        pData = NULL;
    }
    if (pDataForRGB)
    {
        free(pDataForRGB);
        pDataForRGB = NULL;
    }
    if (pDataForSaveImage)
    {
        free(pDataForSaveImage);
        pDataForSaveImage = NULL;
    }

    printf("exit.\n");
    return 0;
}
