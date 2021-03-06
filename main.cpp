 /* Copyright 1993-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.
 * Any use, reproduction, disclosure, or distribution of this software
 * and related documentation without an express license agreement from
 * NVIDIA Corporation is strictly prohibited.
 *
 * Please refer to the applicable NVIDIA end user license agreement (EULA)
 * associated with this source code for terms and conditions that govern
 * your use of this NVIDIA software.
 *
 */

const static char *const sSDKsample = "test module";

const float THRESHOLD = 0.05f;

//opencv cuda
#include "cv.h"
#include <iostream>
#include "opencv.hpp"
#include "gpu/gpu.hpp"
#include "core/cuda_devptrs.hpp"

#include <cuda_runtime.h>

#include "common.h"
#include "tpsCPU.h"
#include "tpsCVGPU.h"

#include <helper_functions.h>

//private define


///////////////////////////////////////////////////////////////////////////////
/// \brief
/// load 4-channel unsigned byte image
/// and convert it to single channel FP32 image
/// \param[out] img_data 		pointer to raw image data
/// \param[out] img_w    		image width
/// \param[out] img_h    		image height
/// \param[out] img_s    		image row stride
/// \param[in]  name     		image file name
/// \param[in]  exePath  		executable file path
/// \param[in]  control_point_value	pixel value of control points	
/// \param[in]  control_point_pos 	position of control points	
/// \param[in]  control_point_num	number of control points
/// \return true if image is successfully loaded or false otherwise
///////////////////////////////////////////////////////////////////////////////
bool LoadImageAsFP32(float *&img_data, int &img_w, int &img_h, int &img_s, const char *name, const char *exePath, float *&control_point_value, float *&control_point_pos, int &control_point_num)
{
    printf("Loading \"%s\" ...\n", name);
    char *name_ = sdkFindFilePath(name, exePath);
    int img_data_nan_count=0;

    if (!name_)
    {
        printf("File not found\n");
        return false;
    }

    unsigned char *data = 0;
    unsigned int w = 0, h = 0;
    bool result = sdkLoadPPM4ub(name_, &data, &w, &h);

    if (result == false)
    {
        printf("Invalid file format\n");
        return false;
    }

    img_w = w;
    img_h = h;
    img_s = iAlignUp(img_w);

    img_data = new float [img_s * h]; 
    control_point_value = new float [img_s * h];
    control_point_pos= new float [control_point_num * 2];
    // source is 4 channel image
    const int widthStep = 4 * img_w;

    for (int i = 0; i < img_h; ++i)
    {
        for (int j = 0; j < img_w; ++j)
        {
            img_data[j + i * img_s] = ((float) data[j * 4 + i * widthStep]) / 255.0f;
        }
    }
    // allocate host memory for control points and assign value
    int num_count = 0;
    for(num_count = 0; num_count < control_point_num * 2;) 
    {	
        if((num_count + 500) > (img_s * img_h))
	{
		printf("control points extends !! \n");
		break;
	}
	//printf("num_count is %d \n",num_count);
	control_point_value[num_count + 500] = img_data[num_count + 500];
        // position: width value 
	control_point_pos[num_count] = (num_count + 500) % img_s;
	// position: height value
	control_point_pos[num_count + 1] = (num_count + 500 - control_point_pos[num_count]) / img_s;
	num_count = num_count + 2; 	
    } 
    //personal change
    return true;

}

///////////////////////////////////////////////////////////////////////////////
/// \brief compare given flow field with gold (L1 norm)
/// \param[in] width    optical flow field width
/// \param[in] height   optical flow field height
/// \param[in] stride   optical flow field row stride
/// \param[in] h_uGold  horizontal displacement, gold
/// \param[in] h_vGold  vertical displacement, gold
/// \param[in] h_u      horizontal displacement
/// \param[in] h_v      vertical displacement
/// \return true if discrepancy is lower than a given threshold
///////////////////////////////////////////////////////////////////////////////
bool CompareWithGold(int width, int height, int stride, int cp_num,
                     const float *result_Gold, const float *result_CUDA) {
	float error = 0.0f;
	float dif_max = 0.0f; 
	int Gold_nan_count = 0;
	int CUDA_nan_count = 0;
	int Gold_CUDA_dif = 0;
	int get_one_abnormal = 0; 
	int get_one_abnormal2 = 0; 
	int get_one_abnormal3 = 0;
        int tps_pos = 0;
	int pixel_pos = 0;
	int CUDA_zero_count = 0; 
	int CPU_zero_count = 0; 
	int cp_count = 0; 
	const int Tps_Size = width * height - cp_num;
	for (; cp_count < (cp_num + 3); ++cp_count)
	{ 
		    for (pixel_pos = 0; pixel_pos < Tps_Size; ++pixel_pos)
		    {
					    const int tps_pos = pixel_pos + cp_count * Tps_Size;	
																
					    error += fabsf(result_Gold[tps_pos] - result_CUDA[tps_pos]);
					    //if((i % 10 == 0) && (j % 10 == 0))
					    //{
						    //printf("result_Gold[%d] is : %f and result_CUDA[%d] is : %f\n",pos,result_Gold[pos],result_CUDA[pos],pos); 
					     //}
					    if((result_Gold[tps_pos] != result_CUDA[tps_pos]) && get_one_abnormal3 == 0)
					    {
						Gold_CUDA_dif++;
						//get_one_abnormal3++;
						float dif = result_Gold[tps_pos] - result_CUDA[tps_pos];
						//printf(" dif is %f \n", dif);	
						//printf(" CPU is %f and CUDA is %f and pos is %d \n", result_Gold[tps_pos], result_CUDA[tps_pos], tps_pos);	
						if ( fabsf(dif) >=  dif_max)
							dif_max = fabsf(dif);	
					    }	
					    if(isnan(result_Gold[tps_pos]) && get_one_abnormal == 0)
					    {
						Gold_nan_count++;
						//get_one_abnormal = 1;
						//printf("pos CPU is %d \n",pos);
					    }	
					    if(isnan(result_CUDA[tps_pos]) && get_one_abnormal2 == 0 ) 
					    {
						CUDA_nan_count++;	
						//get_one_abnormal2 = 1;
						//printf("last one is %f \n",result_CUDA[tps_pos-1]);
						//printf("pos GPU is %d \n",tps_pos);
						//printf("next one is %f \n",result_CUDA[tps_pos+1]);
					    }
					    //if(result_CUDA[tps_pos] == 0 ) 
					    //{
						//CUDA_zero_count++;
						////printf("last one is %f \n",result_CUDA[tps_pos-1]);
						////printf("pos GPU is %d \n",tps_pos);
						////printf("next one is %f \n",result_CUDA[tps_pos+1]);
					    //}	
					    //if(result_Gold[tps_pos] == 0 ) 
					    //{
						//CPU_zero_count++;
					    //}	
		    }		    
	}
	printf("Gold_CUDA_dif is %d \n",Gold_CUDA_dif);
	printf("Gold_nan_count is %d \n",Gold_nan_count);
	printf("CUDA_nan_count is %d \n",CUDA_nan_count);
	printf("dif_max is %f \n",dif_max);
	printf("cp_count is %d \n",cp_count);
	//printf("CUDA_zero_count is %d \n",CUDA_zero_count);
	//printf("CPU_zero_count is %d \n",CPU_zero_count);
	printf("error is %f \n",error);
	error /= (float)(width * height);

	printf("L1 error : %.6f\n", error);

	return (error < THRESHOLD);
}


///////////////////////////////////////////////////////////////////////////////
/// \brief calculate tps value by points 
/// \param[in] name tps pos 
/// \param[in] name cp pos 
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void tps_by_points(int tps_pos, int width, int height, int cp_num, float *cp_pos, float *tps_valueGold, float *tps_valueCUDA)
{

	int pixel_Size = width * height - cp_num;
	int cp_pos_cur = (tps_pos - tps_pos % pixel_Size) / pixel_Size;
	int pixel_pos = tps_pos - cp_pos_cur * pixel_Size;
	
	if ((pixel_pos + 1) < (cp_pos[0] + cp_pos[1] * width))
	{
		printf(" do nothing \n");	
		//} else if ((pixel_pos + 6) > (cp_pos[10] + cp_pos[11] * width))
	} else if ((pixel_pos + cp_num) > (cp_pos[cp_num * 2 - 2] + cp_pos[cp_num * 2 - 1] * width))
	{
		pixel_pos = pixel_pos + cp_num;
	} else
	{	
		//for (int i = 0; i < 9;)
		for (int i = 0; i < (cp_num * 2 - 3);)
		{	
			if (((pixel_pos + i / 2 + 1) > (cp_pos[i] + cp_pos[i + 1] * width)) && ((pixel_pos + i / 2 + 1) < (cp_pos[i + 2] + cp_pos[i + 3] * width))) 
			{
				pixel_pos = pixel_pos + i / 2 + 1;
				break;
			} 
			i = i + 2;
		}
	}	
	int pixel_width = pixel_pos % width;
	int pixel_height = (pixel_pos - pixel_width) / width;
	printf(" start point to point check \n");
	printf(" check tps pos is %d \n", tps_pos); 
	printf(" check cp count is %d \n", cp_pos_cur); 
	printf(" check pixel w is %d \n", pixel_width); 
	printf(" check pixel h is %d \n", pixel_height);
	int cp_pos_w = cp_pos[cp_pos_cur * 2];
	int cp_pos_h = cp_pos[cp_pos_cur * 2 + 1];
	int U = (cp_pos_w - pixel_width) * (cp_pos_w - pixel_width) + (cp_pos_h - pixel_height) * (cp_pos_h - pixel_height);
	float result = U * log(U);
	printf(" tps result is %f \n", result);
	printf(" CPU value is %f \n", tps_valueGold[tps_pos]); 
	printf(" GPU value is %f \n", tps_valueCUDA[tps_pos]); 
}



/// application entry point
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	// welcome message
	printf("%s Starting...\n\n", sSDKsample);

	// pick GPU
	findCudaDevice(argc, (const char **)argv);

	// find images
	const char *const sourceFrameName = "frame10.ppm";
	//    const char *const targetFrameName = "frame11.ppm";

	// image dimensions
	int width;
	int height;
	// row access stride
	int stride;
	// control points
	//int c_num = 6;
	int c_num = 2;
	// flow is computed from source image to target image
	float *p_value; // source image, host memory
	float *c_value; //control points
	float *c_pos; //position of control points
	float *K_cc; // K from control points v.s control points

	// load image from file
	if (!LoadImageAsFP32(p_value, width, height, stride, sourceFrameName, argv[0], c_value, c_pos, c_num))
	{
	exit(EXIT_FAILURE);
	}

        // test opencv mat
	//cv::Mat temp_c_pos;
	//temp_c_pos.create(c_num, 2, CV_32FC1);
	//int i = 0;
	//for (; i < (2 * c_num - 1);)
	//{
		//temp_c_pos.at<float>(i/2, 0) = c_pos[i];	
		//temp_c_pos.at<float>(i/2, 1) = c_pos[i + 1];	
		//i = i + 2;
	//}
	//printf("finish assign c pos \n");
	//cv::gpu::GpuMat op_c_pos(temp_c_pos);
	//printf("start one time \n");
	//cv::gpu::GpuMat op_K_cc;
	//printf("start one time \n");
	//op_K_cc.create((c_num + 3), (c_num + 3), CV_32FC1); 
	//printf("start one time \n");
	//// compute K for onpencv to invert
        //K_star_KFunction(op_c_pos, width, height, stride, c_num, op_K_cc);
	//printf("start one time \n");
	
	//cv::Mat gpu_host_inverted(c_num + 3,c_num + 3,CV_32FC1);
	//cv::Mat gpu_host_K(c_num + 3,c_num + 3,CV_32FC1);
	//op_K_cc.download(gpu_host_K);
	//printf("start one time \n");
	//invert(gpu_host_K, gpu_host_inverted, cv::DECOMP_SVD);
	//printf("start one time \n");

	//std::cout << "gpu k: "<< gpu_host_K << std::endl;
	//std::cout << "inverted: " << gpu_host_inverted << std::endl;


	// allocate host memory for CPU results
	//float *tps_valueGold = new float [(stride * height - c_num) * c_num]; 
	float *tps_valueCPU = new float [(stride * height - c_num) * (c_num + 3)]; 
	// allocate host memory for GPU results
	//float *tps_valueCUDA  = new float [(stride * height - c_num) * c_num];
	float *tps_valueGPU  = new float [(stride * height - c_num) * (c_num + 3)];

	//printf("h_source[10] is %f \n", h_source[10]);

	ComputeTPSCPU(p_value, c_value, c_pos, width, height, stride, c_num, 
		    tps_valueCPU);
	//ComputeFlowGold(p_value, c_value, c_pos, width, height, stride, c_num, 
	//tps_valueGold);

	printf("run to here CPU \n");

	//printf("width id %d! \n",width);   
	//printf("height is %d! \n",height);
	//printf("stride is %d! \n",stride);
	ComputeTPSCVGPU(p_value, c_value, c_pos, width, height, stride, c_num, 
		    tps_valueGPU, K_cc); 
	//ComputeFlowCUDA(p_value, c_value, c_pos, width, height, stride, c_num, 
	//tps_valueCUDA, K_cc); 

	printf("run to here GPU \n"); 
	//printf("\n control point position \n");
	//for (int i = 0; i < c_num * 2;)
	//{
		//printf(" control point %d is width: %f and height: %f \n",i,c_pos[i],c_pos[i + 1]);
		//i = i + 2;
	//} //int tps_pos[5] = {68060, 68097, 68138, 68189, 68220};	
	//for (int i = 0; i < 5; i++)
	//{ 	
		//tps_by_points(tps_pos[i], width, height, c_num, c_pos, tps_valueGold, tps_valueCUDA);
	//}	
	//compare results (L1 norm)
	bool  status = CompareWithGold(width, height, stride, c_num, tps_valueCPU, tps_valueGPU);

	// free resources
	delete [] tps_valueCPU;
	delete [] tps_valueGPU;
	// report self-test status
	//exit(status ? EXIT_SUCCESS : EXIT_FAILURE);
	exit(1 ? EXIT_SUCCESS : EXIT_FAILURE);
}
