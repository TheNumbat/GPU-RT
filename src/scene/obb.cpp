
#include "obb.h"

#undef info

#pragma warning (push)
#pragma warning (disable : 4127)
#pragma warning (disable : 5054)

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#pragma warning (pop)

OBB OBB::fitPCA(const std::vector<Vec3>& points) {
    
    if(!points.size()) return OBB();

    Eigen::Vector3f center;
    for(const auto& v : points) {
        center += Eigen::Vector3f(v.x, v.y, v.z);
    }
    center /= (float)points.size();
    
    // adjust for mean and compute covariance
    Eigen::Matrix3f covariance;
    covariance.setZero();
    for(const auto& _v : points) {
        Eigen::Vector3f v(_v.x, _v.y, _v.z);
        Eigen::Vector3f pAdg = v - center;
        covariance += pAdg * pAdg.transpose();
    }
    covariance /= (float)points.size();

    // compute eigenvectors for the covariance matrix
    Eigen::EigenSolver<Eigen::Matrix3f> solver(covariance);
    Eigen::Matrix3f eigenVectors = solver.eigenvectors().real();

    // project min and max points on each principal axis
    float min1 = INFINITY, max1 = -INFINITY;
    float min2 = INFINITY, max2 = -INFINITY;
    float min3 = INFINITY, max3 = -INFINITY;
    float d = 0.0;
    eigenVectors.transposeInPlace();
    for(const auto& _v : points) {
        Eigen::Vector3f v(_v.x, _v.y, _v.z);
        d = eigenVectors.row(0).dot(v);
        if (min1 > d) min1 = d;
        if (max1 < d) max1 = d;
        
        d = eigenVectors.row(1).dot(v);
        if (min2 > d) min2 = d;
        if (max2 < d) max2 = d;
        
        d = eigenVectors.row(2).dot(v);
        if (min3 > d) min3 = d;
        if (max3 < d) max3 = d;
    }
    
    OBB ret;

    ret.ext.x = (max1 - min1) / 2.0f;
    ret.ext.y = (max2 - min2) / 2.0f;
    ret.ext.z = (max3 - min3) / 2.0f;

    Vec3 c = (Vec3(min1,min2,min3) + Vec3(max1,max2,max3)) / 2.0f;

    ret.T.cols[0] = Vec4(eigenVectors.col(0).x(), eigenVectors.col(0).y(), eigenVectors.col(0).z(), 0.0f);
    ret.T.cols[1] = Vec4(eigenVectors.col(1).x(), eigenVectors.col(1).y(), eigenVectors.col(1).z(), 0.0f);
    ret.T.cols[2] = Vec4(eigenVectors.col(2).x(), eigenVectors.col(2).y(), eigenVectors.col(2).z(), 0.0f);
    ret.T.cols[3] = Vec4(-c, 1.0f);
    return ret;
}
