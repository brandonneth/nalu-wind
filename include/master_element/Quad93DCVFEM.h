// Copyright 2017 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS), National Renewable Energy Laboratory, University of Texas Austin,
// Northwest Research Associates. Under the terms of Contract DE-NA0003525
// with NTESS, the U.S. Government retains certain rights in this software.
//
// This software is released under the BSD 3-clause license. See LICENSE file
// for more details.
//

#ifndef Quad93DCVFEM_h
#define Quad93DCVFEM_h

#include <master_element/MasterElement.h>
#include <master_element/MasterElementUtils.h>
#include <master_element/MasterElementFunctions.h>

#include <SimdInterface.h>
#include <Kokkos_Core.hpp>
#include <AlgTraits.h>

#include <stk_util/util/ReportHandler.hpp>

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <array>
#include <type_traits>

namespace sierra {
namespace nalu {

// 3D Quad 9
class Quad93DSCS : public MasterElement
{
public:
  using AlgTraits = AlgTraitsQuad9;
  using MasterElement::shape_fcn;
  using MasterElement::shifted_shape_fcn;

  KOKKOS_FUNCTION
  Quad93DSCS();
  KOKKOS_FUNCTION virtual ~Quad93DSCS() {}

  using MasterElement::determinant;

  KOKKOS_FUNCTION virtual const int* ipNodeMap(int ordinal = 0) const final;

  template <typename SCALAR, typename SHMEM>
  KOKKOS_FUNCTION void shape_fcn(SharedMemView<SCALAR**, SHMEM>& shpfc);

  template <typename SCALAR, typename SHMEM>
  KOKKOS_FUNCTION void shifted_shape_fcn(SharedMemView<SCALAR**, SHMEM>& shpfc);

  KOKKOS_FUNCTION virtual void determinant(
    const SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType**, DeviceShmem>& areav) override;

  virtual void determinant(
    const SharedMemView<double**>& coords,
    SharedMemView<double**>& areav) override;

  double isInElement(
    const double* elemNodalCoord,
    const double* pointCoord,
    double* isoParCoord) final;

  void interpolatePoint(
    const int& nComp,
    const double* isoParCoord,
    const double* field,
    double* result) final;

  void general_shape_fcn(
    const int numIp, const double* isoParCoord, double* shpfc) final;

  void general_normal(
    const double* isoParCoord, const double* coords, double* normal) final;

  virtual const double* integration_locations() const final { return intgLoc_; }
  virtual const double* integration_location_shift() const final
  {
    return intgLocShift_;
  }

protected:
  KOKKOS_FUNCTION virtual void
  shape_fcn(SharedMemView<DoubleType**, DeviceShmem>& shpfc) override;
  virtual void shape_fcn(SharedMemView<double**, HostShmem>& shpfc) override;

  KOKKOS_FUNCTION virtual void
  shifted_shape_fcn(SharedMemView<DoubleType**, DeviceShmem>& shpfc) override;
  virtual void
  shifted_shape_fcn(SharedMemView<double**, HostShmem>& shpfc) override;

private:
  static const int nDim_ = AlgTraits::nDim_;
  static const int nodesPerElement_ = AlgTraits::nodesPerElement_;
  static const int numIntPoints_ = AlgTraits::numScsIp_; // 36
  static const int nodes1D_ = 3;
  static const int numQuad_ = 2;
  static const int surfaceDimension_ = 2;

  // quadrature info
  const double gaussAbscissae_[2] = {
    -std::sqrt(3.0) / 3.0, std::sqrt(3.0) / 3.0};
  const double gaussWeight_[2] = {0.5, 0.5};
  const double gaussAbscissaeShift_[6] = {-1.0, -1.0, 0.0, 0.0, 1.0, 1.0};

  const double scsDist_ = std::sqrt(3.0) / 3.0;
  const double scsEndLoc_[4] = {-1.0, -scsDist_, scsDist_, 1.0};

  int ipNodeMap_[numIntPoints_];
  double intgLoc_[numIntPoints_ * surfaceDimension_];      // size = 72
  double intgLocShift_[numIntPoints_ * surfaceDimension_]; // size = 72

  double shapeFunctions_[numIntPoints_ * nodesPerElement_];
  double shapeFunctionsShift_[numIntPoints_ * nodesPerElement_];

  double shapeDerivs_[numIntPoints_ * nodesPerElement_ * surfaceDimension_];
  double
    shapeDerivsShift_[numIntPoints_ * nodesPerElement_ * surfaceDimension_];
  double ipWeight_[numIntPoints_];

  void eval_shape_functions_at_ips();
  void eval_shape_derivs_at_ips();
  void eval_shape_functions_at_shifted_ips();
  void eval_shape_derivs_at_shifted_ips();

  void set_interior_info();

  void area_vector(
    const double* POINTER_RESTRICT coords,
    const double* POINTER_RESTRICT shapeDerivs,
    double* POINTER_RESTRICT areaVector) const;

  double gauss_point_location(int nodeOrdinal, int gaussPointOrdinal) const;

  double
  shifted_gauss_point_location(int nodeOrdinal, int gaussPointOrdinal) const;

  double
  tensor_product_weight(int s1Node, int s2Node, int s1Ip, int s2Ip) const;

  void
  quad9_shape_fcn(int npts, const double* par_coord, double* shape_fcn) const;

  void
  quad9_shape_deriv(int npts, const double* par_coord, double* shape_fcn) const;

  void non_unit_face_normal(
    const double* isoParCoord,
    const double* elemNodalCoord,
    double* normalVector);

  double parametric_distance(const std::array<double, 3>& x);

  template <typename DBLTYPE, typename SHMEM>
  KOKKOS_INLINE_FUNCTION void determinant_scs(
    const SharedMemView<DBLTYPE**, SHMEM>& coords,
    SharedMemView<DBLTYPE**, SHMEM>& areav) const;
};

} // namespace nalu
} // namespace sierra

#endif
