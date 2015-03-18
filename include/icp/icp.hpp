//  This file is part of the Icp Library,
//
//  Copyright (C) 2014 by Arnaud TANGUY <arn.tanguy@NOSPAM.gmail.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.


#ifndef ICP_H
#define ICP_H

#include <Eigen/Dense>

#include <glog/logging.h>

#include <pcl/common/transforms.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/kdtree/kdtree_flann.h>

#include "linear_algebra.hpp"
#include "eigentools.hpp"

#include <fstream>

namespace icp {

/**
 * @brief Optimisation parameters for ICP
 */
template<typename Dtype>
struct IcpParameters_ {
  //! Rate of convergence
  Dtype lambda;
  //! Maximum number of allowed iterations
  int max_iter;
  //! Stopping condition
  /*! ICP stops when the error variation between two iteration is under
    min_variation. */
  Dtype min_variation;
  //! Maximum search distance for correspondances
  /*! Do not look further than this for the kdtree search */
  Dtype max_correspondance_distance;
  //! Twist representing the initial guess for the registration
  Eigen::Matrix<Dtype, 6, 1> initial_guess;

  IcpParameters_() : lambda(1), max_iter(10), min_variation(10e-5),
    max_correspondance_distance(std::numeric_limits<Dtype>::max()) {
    initial_guess = Eigen::Matrix<Dtype, Eigen::Dynamic, Eigen::Dynamic>::Zero(6,
                    1);
  }
};

typedef IcpParameters_<float> IcpParametersf;
typedef IcpParameters_<double> IcpParametersd;

template<typename Dtype>
std::ostream &operator<<(std::ostream &s, const IcpParameters_<Dtype> &p) {
  s << "Lambda: "  << p.lambda
    << "\nMax iterations: " << p.max_iter
    << "\nMin variation: " << p.min_variation
    << "\nInitial guess (twist):\n" << p.initial_guess;
  return s;
}



/**
 * @brief Results for the ICP
 */
template<typename Dtype, typename Point>
struct IcpResults_ {
  typedef pcl::PointCloud<Point> Pc;
  typedef typename pcl::PointCloud<Point>::Ptr PcPtr;

  //! Point cloud of the registered points
  PcPtr registeredPointCloud;

  //! History of previous registration errors
  /*!
    - First value is the initial error before ICP,
    - Last value is the final error after ICP. */
  std::vector<Dtype> registrationError;

  //! Transformation (SE3) of the final registration transformation
  Eigen::Matrix<Dtype, 4, 4> transformation;

  Dtype getFinalError() const {
    return registrationError[registrationError.size() - 1];
  }

  void clear() {
    registrationError.clear();
    transformation = Eigen::Matrix<Dtype, 4, 4>::Zero(
                       4, 4);
  }
};

typedef IcpResults_<float, pcl::PointXYZ> IcpResultsf;

template<typename Dtype, typename Point>
std::ostream &operator<<(std::ostream &s, const IcpResults_<Dtype, Point> &r) {
  if (!r.registrationError.empty()) {
    s << "Initial error: " << r.registrationError[0]
      << "\nFinal error: " << r.registrationError[r.registrationError.size() - 1]
      << "\nFinal transformation: \n"
      << r.transformation
      << "\nError history: ";
    for (int i = 0; i < r.registrationError.size(); ++i) {
      s << r.registrationError[i]  << ", ";
    }
  } else {
    s << "Icp: No Results!";
  }
  return s;
}

/**
 * @brief Iterative Closest Point Algorithm
 */
template<typename Dtype, typename PointSource, typename PointTarget, typename Error_, typename MEstimator>
class Icp {
  public:
    typedef typename pcl::PointCloud<PointSource> Pcs;
    typedef typename pcl::PointCloud<PointTarget> Pct;
    typedef typename pcl::PointCloud<PointSource>::Ptr PcsPtr;
    typedef typename pcl::PointCloud<PointTarget>::Ptr PctPtr;
    typedef IcpParameters_<Dtype> IcpParameters;
    typedef IcpResults_<Dtype, PointSource> IcpResults;
    typedef typename Eigen::Matrix<Dtype, 6, 1> Vector6;

  protected:
    // Reference (model) point cloud. This is the fixed point cloud to be registered against.
    PctPtr target_;
    // kd-tree of the model point cloud
    pcl::KdTreeFLANN<PointTarget> kdtree_;
    // Data point cloud. This is the one needing registration
    PcsPtr source_;

    // Instance of an error kernel used to compute the error vector, Jacobian...
    Error_ err_;
    // MEstimator instance, used to improve statistical robusteness against outliers.
    MEstimator mestimator_;

    // Parameters of the algorithm (rate of convergence, stopping condition...)
    IcpParameters param_;

    // Results of the ICP
    IcpResults r_;

  protected:
    void initialize(const PctPtr &model, const PcsPtr &data,
                    const IcpParameters &param);


    void findNearestNeighbors(const PcsPtr &src,
                              const Dtype max_correspondance_distance,
                              std::vector<int> &indices_src,
                              std::vector<int> &indices_target,
                              std::vector<Dtype> &distances);

  public:
    Icp() {
    }

    /**
     * \brief Runs the ICP algorithm with given parameters.
     *
     * Runs the ICP according to the templated \c MEstimator and \c Error_ function,
     * and optimisation parameters \c IcpParameters_
     *
     * \retval void You can get a structure containing the results of the ICP (error, registered point cloud...)
     * by using \c getResults()
    **/
    void run();

    /**
     * @brief Sets the parameters for the optimisation.
     *
     * All parameters are defined within the \c IcpParameters_ structure.
     *
     * @param param
     *  Parameters to the minimisation
     */
    void setParameters(const IcpParameters &param) {
      param_ = param;
    }

    IcpParameters getParameters() const {
      return param_;
    }
    /** \brief Provide a pointer to the input target (e.g., the point cloud that we want to align the input source to).
    * \param[in] cloud the input point cloud target
    */
    void setInputTarget(const PctPtr &in) {
      target_ = in;
      kdtree_.setInputCloud(target_);
    }
    /**
     * @brief Provide a pointer to the input source (e.g., the point cloud that we want to align to the target)
     *
     * @param[in] cloud	the input point cloud source
     */
    void setInputSource(const PcsPtr &in) {
      source_ = in;
    }
    /**
     * @brief Gets the result of the ICP.
     *
     *
     * @return
     * Results of the ICP (call \c run() to run the ICP and generate results)
     */
    IcpResults getResults() const {
      return r_;
    }
};

}  // namespace icp

#endif /* ICP_H */
