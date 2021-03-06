#include <CL/cl.h>
#include "ros/ros.h"
#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>

namespace kinect_icp
{

typedef pcl::PointXYZRGB Point;
typedef pcl::PointCloud<pcl::PointXYZRGB> PCloud;

class Vrip
{
public:

  Vrip();
  ~Vrip();

  void fuseCloud(const PCloud::ConstPtr& new_point_cloud);
  void testScan();

  void marchingCubes();

  class Vertex
  {
  public:
    float x, y, z;
  };

private:
  void Cleanup();

  cl_command_queue command_queue_;
  cl_kernel fuse_kernel_;
  cl_kernel preMarching_;
  cl_kernel mainMarching_;

  cl_kernel scanLargeArrays_;
  cl_kernel blockAddition_;
  cl_kernel prefixSum_;

  cl_device_id device_id_;
  cl_mem volume_mem_obj_;
  cl_mem march_mem_obj_;
  cl_mem image_mem_obj_;
  cl_mem proj_mem_obj_;
  cl_context context_;

  void bScan(cl_uint len,
    cl_mem *inputBuffer,
    cl_mem *outputBuffer,
    cl_mem *blockSumBuffer);

  void pScan(cl_uint len,
    cl_mem *inputBuffer,
    cl_mem * outputBuffer);

  void bAddition(cl_uint len,
    cl_mem *inputBuffer,
    cl_mem * outputBuffer);

  int preFixSum(cl_mem inputBuffer, int input_length);

  void loadKernel(const char* filename, int num_kernels, cl_kernel* kernels[],
  const char* kernel_names[]);

  int imageSize_;
  int volumeSize_;
  
  int fuseCount_;
};

}
