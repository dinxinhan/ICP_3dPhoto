#include "kinect_icp/vrip.h"
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fstream>

using namespace kinect_icp;

#define MAX_SOURCE_SIZE (0x100000)

#define SEP printf("-----------------------------------------------------------\n")

int device_stats(cl_device_id device_id)
{

  int err;
  size_t returned_size;

  // Report the device vendor and device name
  //
  cl_char vendor_name[1024] = {0};
  cl_char device_name[1024] = {0};
  cl_char device_profile[1024] = {0};
  cl_char device_extensions[1024] = {0};
  cl_device_local_mem_type local_mem_type;

  cl_ulong global_mem_size, global_mem_cache_size;
  cl_ulong max_mem_alloc_size;

  cl_uint clock_frequency, vector_width, max_compute_units;

  size_t max_work_item_dims, max_work_group_size, max_work_item_sizes[3];

  cl_uint vector_types[] = {CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE};
  const char *vector_type_names[] = {"char", "short", "int", "long", "float", "double"};

  err = clGetDeviceInfo(device_id, CL_DEVICE_VENDOR, sizeof (vendor_name), vendor_name, &returned_size);
  err |= clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof (device_name), device_name, &returned_size);
  err |= clGetDeviceInfo(device_id, CL_DEVICE_PROFILE, sizeof (device_profile), device_profile, &returned_size);
  err |= clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, sizeof (device_extensions), device_extensions, &returned_size);
  err |= clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_TYPE, sizeof (local_mem_type), &local_mem_type, &returned_size);

  err |= clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof (global_mem_size), &global_mem_size, &returned_size);
  err |= clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, sizeof (global_mem_cache_size), &global_mem_cache_size, &returned_size);
  err |= clGetDeviceInfo(device_id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof (max_mem_alloc_size), &max_mem_alloc_size, &returned_size);

  err |= clGetDeviceInfo(device_id, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof (clock_frequency), &clock_frequency, &returned_size);

  err |= clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof (max_work_group_size), &max_work_group_size, &returned_size);

  err |= clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof (max_work_item_dims), &max_work_item_dims, &returned_size);

  err |= clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof (max_work_item_sizes), max_work_item_sizes, &returned_size);

  err |= clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof (max_compute_units), &max_compute_units, &returned_size);

  printf("Vendor: %s\n", vendor_name);
  printf("Device Name: %s\n", device_name);
  printf("Profile: %s\n", device_profile);
  printf("Supported Extensions: %s\n\n", device_extensions);

  printf("Local Mem Type (Local=1, Global=2): %i\n", (int) local_mem_type);
  printf("Global Mem Size (MB): %i\n", (int) global_mem_size / (1024 * 1024));
  printf("Global Mem Cache Size (Bytes): %i\n", (int) global_mem_cache_size);
  printf("Max Mem Alloc Size (MB): %ld\n", (long int) max_mem_alloc_size / (1024 * 1024));

  printf("Clock Frequency (MHz): %i\n\n", clock_frequency);

  for (int i = 0; i < 6; i++)
  {
    err |= clGetDeviceInfo(device_id, vector_types[i], sizeof (clock_frequency), &vector_width, &returned_size);
    printf("Vector type width for: %s = %i\n", vector_type_names[i], vector_width);
  }

  printf("\nMax Work Group Size: %lu\n", (unsigned long) max_work_group_size);
  //printf("Max Work Item Dims: %lu\n",max_work_item_dims);
  //for(size_t i=0;i<max_work_item_dims;i++)
  //	printf("Max Work Items in Dim %lu: %lu\n",(long unsigned)(i+1),(long unsigned)max_work_item_sizes[i]);

  printf("Max Compute Units: %i\n", max_compute_units);
  printf("\n");

  return CL_SUCCESS;
}

const int Volume_Size = 16;
const float d_max = 0.05;
const float d_min = -d_max;

#define CHECK(value) if (value != 0) {std::cerr << "An Error occurred at " << __FILE__ << ":" << __LINE__ << std::endl; exit(1);}

Vrip::Vrip()
: imageSize_(Volume_Size*Volume_Size*4)
, volumeSize_(Volume_Size*Volume_Size*Volume_Size)
{
  // Get platform and device information
  cl_platform_id platform_id = NULL;
  cl_uint ret_num_devices;
  cl_uint ret_num_platforms;
  cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
  ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 1,
    &device_id_, &ret_num_devices);

  device_stats(device_id_);

  // Create an OpenCL context
  context_ = clCreateContext(NULL, 1, &device_id_, NULL, NULL, &ret);
  CHECK(ret);

  // Create a command queue
  command_queue_ = clCreateCommandQueue(context_, device_id_, 0, &ret);
  CHECK(ret);

  // Create memory buffers on the device for each vector
  volume_mem_obj_ = clCreateBuffer(context_, CL_MEM_READ_WRITE,
    volumeSize_ * 2 * sizeof (float), NULL, &ret);

  CHECK(ret);

  march_mem_obj_ = clCreateBuffer(context_, CL_MEM_READ_WRITE,
    volumeSize_ * sizeof (int), NULL, &ret);

  CHECK(ret);

  image_mem_obj_ = clCreateBuffer(context_, CL_MEM_READ_ONLY,
    imageSize_ * sizeof (float), NULL, &ret);

  CHECK(ret);

  float* volume = (float*) malloc(volumeSize_ * 2 * sizeof (float));
  int i = 0;
  while (i < volumeSize_ * 2)
  {
    volume[i++] = d_max;
    volume[i++] = 0;
  }

  // Copy the lists A and B to their respective memory buffers
  CHECK(clEnqueueWriteBuffer(command_queue_, volume_mem_obj_, CL_TRUE, 0,
    volumeSize_ * 2 * sizeof (float), volume, 0, NULL, NULL));

  free(volume);

  cl_kernel* kernels[3] = {&fuse_kernel_, &preMarching_, &mainMarching_};
  const char* names[3] = {"fuse", "precube", "cube"};

  loadKernel("fuse.cl", 3, kernels, names);
}

void Vrip::loadKernel(const char* filename, int num_kernels, cl_kernel* kernels[],
  const char* kernel_names[])
{
  // Load the kernel source code into the array source_str
  FILE *fp;
  char *source_str = (char*) malloc(MAX_SOURCE_SIZE);
  size_t source_size;

  fp = fopen(filename, "r");
  CHECK(!fp);

  source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
  fclose(fp);

  // Create a program from the kernel source
  cl_int ret;
  cl_program program;
  program = clCreateProgramWithSource(context_, 1,
    (const char **) &source_str, &source_size, &ret);
  CHECK(ret);

  // Build the program
  CHECK(clBuildProgram(program, 1, &device_id_, NULL, NULL, NULL));

  char buffer[2048];
  size_t len;
  clGetProgramBuildInfo(program, device_id_, CL_PROGRAM_BUILD_LOG, sizeof (buffer), buffer, &len);

  std::cout << buffer << std::endl;

  for (int i=0; i<num_kernels; i++)
  {
    // Create the OpenCL kernel
    *kernels[i] = clCreateKernel(program, kernel_names[i], &ret);
    CHECK(ret);
  }

  // Release program
  CHECK(clReleaseProgram(program));

  free(source_str);
}

Vrip::~Vrip()
{
  // Clean up
  cl_int ret = clFlush(command_queue_);
  ret = clFinish(command_queue_);
  ret = clReleaseKernel(fuse_kernel_);
  ret = clReleaseKernel(mainMarching_);
  ret = clReleaseMemObject(volume_mem_obj_);
  ret = clReleaseMemObject(march_mem_obj_);
  ret = clReleaseMemObject(image_mem_obj_);
  ret = clReleaseCommandQueue(command_queue_);
  ret = clReleaseContext(context_);
}

void Vrip::fuseCloud(const PCloud::ConstPtr& new_point_cloud)
{
  float* image = (float*) malloc(imageSize_ * sizeof (float));
  int index = 0;
  for (int i = 0; i < Volume_Size; ++i)
  {
    for (int j = 0; j < Volume_Size; ++j)
    {
      float ii = (float) i / (float) Volume_Size;
      float jj = (float) j / (float) Volume_Size;
      image[index++] = ii;
      image[index++] = jj;
      image[index++] = (ii - 0.5f)*(ii - 0.5f) + (jj - 0.5f)*(jj - 0.5f) + 0.25;

      //RBG actually not used but will be in the data
      image[index++] = 0;
    }
  }
  
  cl_int ret = clEnqueueWriteBuffer(command_queue_, image_mem_obj_, CL_TRUE, 0,
    imageSize_ * sizeof (float), image, 0, NULL, NULL);
    
  clFinish(command_queue_);

  free(image);

  // Set the arguments of the kernel
  ret = clSetKernelArg(fuse_kernel_, 0, sizeof (cl_mem), (void *) &volume_mem_obj_);
  ret = clSetKernelArg(fuse_kernel_, 1, sizeof (cl_mem), (void *) &image_mem_obj_);
  ret = clSetKernelArg(fuse_kernel_, 2, sizeof (int), (void *) &Volume_Size);
  ret = clSetKernelArg(fuse_kernel_, 3, sizeof (float), (void *) &d_min);
  ret = clSetKernelArg(fuse_kernel_, 4, sizeof (float), (void *) &d_max);

  // Execute the OpenCL kernel on the list
  size_t localWorkSize[] = {16, 16};
  size_t globalWorkSize[] = {Volume_Size, Volume_Size};
  ret = clEnqueueNDRangeKernel(command_queue_, fuse_kernel_, 2, NULL,
    globalWorkSize, localWorkSize, 0, NULL, NULL);

  /*float* volume = (float*) malloc(volumeSize_ * 2 *sizeof (float));
  ret = clEnqueueReadBuffer(command_queue_, volume_mem_obj_, CL_TRUE, 0,
    volumeSize_ * 2 * sizeof (float), volume, 0, NULL, NULL);

  int i = 0;
  for(int x = 0; x < Volume_Size; ++x)
  {
    for(int y = 0; y < Volume_Size; ++y)
    {
      for(int z = 0; z < Volume_Size; ++z)
      {
        std::cout << volume[i++] << " ";
        std::cout << volume[i++] << " - ";
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }*/
  
  marchingCubes();
}

void Vrip::marchingCubes()
{
  std::cout << "pre Marching cubes started" << std::endl;

  // Set the arguments of the kernel
  int ret = clSetKernelArg(preMarching_, 0, sizeof (cl_mem), (void *) &volume_mem_obj_);
  ret = clSetKernelArg(preMarching_, 1, sizeof (cl_mem), (void *) &march_mem_obj_);
  ret = clSetKernelArg(preMarching_, 2, sizeof (int), (void *) &Volume_Size);

  size_t localWorkSize3D[] = {16, 16, 1};
  size_t globalWorkSize3D[] = {Volume_Size, Volume_Size, Volume_Size};
  ret = clEnqueueNDRangeKernel(command_queue_, preMarching_, 3, NULL,
    globalWorkSize3D, localWorkSize3D, 0, NULL, NULL);

  std::cout << "pre Marching cubes finished" << std::endl;

  int memoryToAllocate = preFixSum(march_mem_obj_, march_mem_obj_, 1024);

  std::cout << "Marching cubes started" << std::endl;

  cl_mem out_mem_obj = clCreateBuffer(context_, CL_MEM_WRITE_ONLY,
    memoryToAllocate * sizeof (float), NULL, &ret);
    
  CHECK(ret);

  // Set the arguments of the kernel
  ret = clSetKernelArg(mainMarching_, 0, sizeof (cl_mem), (void *) &volume_mem_obj_);
  ret = clSetKernelArg(mainMarching_, 1, sizeof (cl_mem), (void *) &march_mem_obj_);
  ret = clSetKernelArg(mainMarching_, 2, sizeof (cl_mem), (void *) &out_mem_obj);
  ret = clSetKernelArg(mainMarching_, 3, sizeof (int), (void *) &Volume_Size);

  ret = clEnqueueNDRangeKernel(command_queue_, mainMarching_, 3, NULL,
    globalWorkSize3D, localWorkSize3D, 0, NULL, NULL);

  float* hostOut = (float*) malloc(memoryToAllocate * sizeof (float));

  ret = clEnqueueReadBuffer(command_queue_, out_mem_obj, CL_TRUE, 0,
    memoryToAllocate * sizeof(float), hostOut, 0, NULL, NULL);

  clFinish(command_queue_);

  std::cout << "Marching cubes finished" << std::endl;

  //Write Marching Cube Surface
  std::ofstream File("test.off");
  File << "OFF" << std::endl;

  memoryToAllocate /= 3;//3 indices per vertex
  File << memoryToAllocate << " " << memoryToAllocate / 3 << " " << 0 << std::endl;

  for (int i = 0; i < memoryToAllocate; ++i)
  {
    File << hostOut[3*i] << " " << hostOut[3*i+1] << " " << hostOut[3*i+2] << std::endl;
  }

  memoryToAllocate /= 3;//3 vertices per face
  for (int i = 0; i < memoryToAllocate; ++i)
  {
    File << 3 << " " << i * 3 << " " << i * 3 + 1 << " " << i * 3 + 2 << std::endl;
  }

  ret = clReleaseMemObject(out_mem_obj);

  free(hostOut);
}

int Vrip::preFixSum(cl_mem input, cl_mem output, int BlockSize)
{
  int* sumArray = (int*) malloc(volumeSize_ *sizeof (int));
  CHECK(clEnqueueReadBuffer(command_queue_, input, CL_TRUE, 0,
    volumeSize_ * sizeof (int), sumArray, 0, NULL, NULL));
  
  clFinish(command_queue_);  
  
  int sum = 0;
  for(int i = 0; i < volumeSize_; ++i)
  {
    int tmp = sumArray[i];
    sumArray[i] = sum;
    sum += tmp;
  }
  
  CHECK(clEnqueueWriteBuffer(command_queue_, output, CL_TRUE, 0,
    volumeSize_ * sizeof (int), sumArray, 0, NULL, NULL));
   
  free(sumArray);
  
  return sum;
}

