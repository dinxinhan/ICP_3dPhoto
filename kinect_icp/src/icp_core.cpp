#include "kinect_icp/icp_core.h"
#include <Eigen/Dense>

using namespace kinect_icp;
using namespace Eigen;
using namespace std;

typedef union
{

  struct /*anonymous*/
  {
    unsigned char Blue;
    unsigned char Green;
    unsigned char Red;
    unsigned char Alpha;
  };
  float float_value;
  long long_value;
} RGBValue;

IcpCore::IcpCore(ros::Publisher publisher)
  : singleMerge_(false)
  , accumulateResults_(true)
  , publisher_(publisher)
  , outCloud_(NULL)
  , cloud1_(NULL)
  , cloud2_(NULL)
  , algorithm_(NULL)
  , totalTime_(0)
  , numComputes_(0)
  , lastTransformation_(Eigen::Matrix4f::Identity())
{
  Clouds_.reserve(1000);


  RGBValue green;
  green.Red = 0;
  green.Green = 255;
  green.Blue = 0;

  green_ = green.float_value;

  RGBValue red;
  red.Red = 255;
  red.Green = 0;
  red.Blue = 0;

  red_ = red.float_value;

  RGBValue blue;
  blue.Red = 0;
  blue.Green = 0;
  blue.Blue = 255;

  blue_ = blue.float_value;
}

void IcpCore::visualizeNormals(const PCloud::ConstPtr& new_point_cloud)
{
  PCloud* cloud = new PCloud(*new_point_cloud);

  RGBValue red;
  //color.float_value = pt.rgb;
  red.Red = 255;
  red.Green = 0;
  red.Blue = 0;

  RGBValue green;
  //color.float_value = pt.rgb;
  green.Red = 0;
  green.Green = 255;
  green.Blue = 0;

  RGBValue blue;
  //color.float_value = pt.rgb;
  blue.Red = 0;
  blue.Green = 0;
  blue.Blue = 255;

  int width = cloud->width;
  int height = cloud->height;

  IcpLocal algorithm(cloud, cloud);

  srand(42);

  for (int i = 0; i < width; i++)
  {
    // Select random point
    int x = (rand() % (width - 2 * 10)) + 10;
    int y = (rand() % (height - 2 * 10)) + 10;
    Point& pt = (*cloud)(x, y);
    if (pcl::hasValidXYZ(pt))
    {
      pt.rgb = red.float_value;

      for (int i = 0; i < 20; i++)
      {
        Vector3f normal;
        if (algorithm.ComputeNormalSimple(x, y, normal))
        {
          if (normal[1] > 0)
            normal = -normal;
          pcl::PointXYZRGB new_point;
          new_point.x = pt.x + 0.005 * normal[0] * i;
          new_point.y = pt.y + 0.005 * normal[1] * i;
          new_point.z = pt.z + 0.005 * normal[2] * i;
          new_point.rgb = green.float_value;
          cloud->push_back(new_point);
        }
      }
    }
  }

  cloud->width = cloud->points.size();
  cloud->height = 1;

  publisher_.publish(*cloud);

  delete cloud;
}

void IcpCore::registerCloud(const PCloud::ConstPtr& new_point_cloud)
{
  ROS_DEBUG("Received Point Cloud");

  static int i = 0;
  static PCloud* firstCloud = NULL;

  const int max_frame = 13;

  if (!cloud1_)
  {
    cloud1_ = new PCloud(*new_point_cloud);
    firstCloud = new PCloud(*new_point_cloud);

    BOOST_FOREACH(pcl::PointXYZRGB& pt, firstCloud->points)
    {
      pt.rgb = green_;
    }

    return;
  }

  if (i != max_frame)
  {
    i++;

    if (cloud2_)
    {
      delete cloud2_;
      cloud2_ = NULL;
    }

    cloud2_ = cloud1_;
    cloud1_ = new PCloud(*new_point_cloud);

    delete outCloud_;
    outCloud_ = new PCloud(*cloud2_);

    IcpLocal* tmpAlgo = new IcpLocal(cloud1_, cloud2_);

    if (algorithm_)
    {
      //tmpAlgo->SetTransformation(algorithm_->GetTransformation());
      delete algorithm_;
    }

    algorithm_ = tmpAlgo;

    algorithm_->SetMaxIterations(200);
    algorithm_->Compute();

    lastTransformation_ *= algorithm_->GetTransformation();
    cout << i << endl;
    cout << lastTransformation_ << endl;

    delete outCloud_;
    outCloud_ = new PCloud(*firstCloud);

    PCloud cloud(*cloud1_);

    BOOST_FOREACH(pcl::PointXYZRGB& pt, cloud.points)
    {
      Eigen::Vector4f pnt(pt.x, pt.y, pt.z, 1.0);
      /*if(pnt.norm()>4)
      {
        pt.rgb = blue.float_value;
      }
      else
      {*/
      pt.rgb = red_;
      //}
      pnt = lastTransformation_ * pnt;
      pt.x = pnt[0];
      pt.y = pnt[1];
      pt.z = pnt[2];
    }

    *outCloud_ += cloud;
    publisher_.publish(*outCloud_);
  }

  if (i % 10 == 0 || i == max_frame)
  {
    for (int j = 0; j < 5; j++)
    {
      PCloud cloud(*cloud1_);

      BOOST_FOREACH(pcl::PointXYZRGB& pt, cloud.points)
      {
        Eigen::Vector4f pnt(pt.x, pt.y, pt.z, 1.0);
        pnt = lastTransformation_ * pnt;
        pt.x = pnt[0];
        pt.y = pnt[1];
        pt.z = pnt[2];
        pt.rgb = red_;
      }

      IcpLocal* tmpAlgo = new IcpLocal(&cloud, firstCloud);

      tmpAlgo->SetMaxIterations(200);
      tmpAlgo->SetSelectionAmount(1000);
      tmpAlgo->Compute();

      cout << "Done" << endl;
      cout << i << endl;
      cout << tmpAlgo->GetTransformation() << endl;

      lastTransformation_ *= tmpAlgo->GetTransformation();

      cout << "All" << endl;
      cout << lastTransformation_ << endl;

      delete tmpAlgo;

      delete outCloud_;
      outCloud_ = new PCloud(*firstCloud);

      PCloud cloud_new(*cloud1_);

      BOOST_FOREACH(pcl::PointXYZRGB& pt, cloud_new.points)
      {
        Eigen::Vector4f pnt(pt.x, pt.y, pt.z, 1.0);
        /*if(pnt.norm()>4)
        {
          pt.rgb = blue.float_value;
        }
        else
        {*/
        pt.rgb = red_;
        //}
        pnt = lastTransformation_ * pnt;
        pt.x = pnt[0];
        pt.y = pnt[1];
        pt.z = pnt[2];
      }

      *outCloud_ += cloud_new;
      publisher_.publish(*outCloud_);
    }

  }
}