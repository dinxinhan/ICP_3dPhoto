
#include "kinect_icp/icp_local.h"
#include "time.h"
#include <Eigen/Dense>
#include <iostream>

using namespace Eigen;
using namespace kinect_icp;
using namespace std;

IcpLocal::IcpLocal(PCloud* first, PCloud* second)
: first_(first)
, second_(second)
{
}
	

void IcpLocal::Compute(/*SomeMatrixClass initialTransformation*/)
{
  ROS_INFO("IcpLocal::Compute");
  float error = std::numeric_limits<float>::max();
  float old_error;
//  do
  {
    old_error = error;
    Selection();
    Matching();
    Rejecting();
    error = Minimization();
  }// while (abs(old_error-error) > ITERATION_THRESHOLD);
}

		
/*SomeMatrixClass IcpLocal::GetTransformation()
{
	return asdkasdkljas;
}
*/  
		
#define SelectionAmount 100
		
void IcpLocal::Selection()
{
  ROS_INFO("IcpLocal::Selection");
  srand(time(NULL));
  int i = 0;
  int count = first_->points.size();
  selected_.clear();
  selected_.reserve(SelectionAmount);
  MatchedPoint mp;
  while(i < SelectionAmount)
  {
    mp.first_index = rand()%count;
    float x = first_->points[mp.first_index].x;
    if(x==x){    
      selected_.push_back(mp);      
      i++;
    }
  }
}

void IcpLocal::Matching()
{
  ROS_INFO("IcpLocal::Matching");
  int imax = selected_.size();
  for (int i=0; i<imax; i++)
  {
    selected_[i].distance = numeric_limits<float>::max();

    const Point& FirstPoint = first_->points[selected_[i].first_index];
    
    int jmax = second_->height;
    int kmax = second_->width;
    for (int j=0; j<jmax; j++)
    {
      for (int k=0; k<kmax; k++)
      {
        const Point& SecondPoint = (*second_)(j,k);
        if(!pcl::hasValidXYZ((*second_)(j,k)))
        {
          k++; //next will anyway not generate valid normal
          continue;
        }
        
        float dist = pcl::squaredEuclideanDistance(SecondPoint,FirstPoint);
        
        Eigen::Vector3f normal;

        if (dist < selected_[i].distance && ComputeNormal(j,k,normal)) 
        {
          selected_[i].distance = dist;
          selected_[i].second_index = j*kmax + k;
          selected_[i].normal = normal;
        }          
      }
    }
  }
}

bool IcpLocal::ComputeNormal(int j, int k, Eigen::Vector3f& normal)
{
  return false;
  //if(second_)
}


void IcpLocal::Rejecting()
{
  ROS_INFO("IcpLocal::Rejecting");
  const double threshold = 10;

  int threshold_squared = threshold*threshold;
  int imax = selected_.size();
  for (int i=0; i<imax; i++)
  {
    selected_[i].rejected = selected_[i].distance > threshold_squared;
    /*if(selected_[i].rejected)
    {
       float dist_x = first_->points[j].x - second_->points[selected_[i].first_index].x;
      float dist_y = first_->points[j].y - second_->points[selected_[i].first_index].y;
      float dist_z = first_->points[j].z - second_->points[selected_[i].first_index].z;

     printf("%f, ", selected_[i].);
    }*/
    printf("%f, ", selected_[i].distance);
  }
}

float IcpLocal::Minimization()
{
  int N = selected_.size();
  
  Matrix<double, Dynamic, 6> A(N, 6);
  Matrix<double, Dynamic, 1> b(N, 1);

  for (int n = 0; n < N; n++) {
    // Fill in A
    Eigen::Vector3f normal = selected_[n].normal;
    Point source = first_->points[selected_[n].first_index];

    A(n, 0) = normal(2)*source.y - normal(1)*source.z;
    A(n, 1) = normal(0)*source.z - normal(2)*source.x;
    A(n, 2) = normal(1)*source.x - normal(0)*source.y;

    A(n, 3) = normal(0);
    A(n, 4) = normal(1);
    A(n, 5) = normal(2);

    // Fill in b
    Point dest = second_->points[selected_[n].second_index];
    b(n) = normal(0)*(dest.x-source.x) +
           normal(1)*(dest.y-source.y) +
           normal(2)*(dest.z-source.z);
  }

/*  Matrix2f A, b;
  A << 2, -1, -1, 3;
  b << 1, 2, 3, 1;
  cout << "Here is the matrix A:\n" << A << endl;
  cout << "Here is the right hand side b:\n" << b << endl;
  Matrix2f x = A.ldlt().solve(b);
  cout << "The solution is:\n" << x << endl;*/
  return 0.f;
}
