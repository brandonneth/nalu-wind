// Copyright 2017 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS), National Renewable Energy Laboratory, University of Texas Austin,
// Northwest Research Associates. Under the terms of Contract DE-NA0003525
// with NTESS, the U.S. Government retains certain rights in this software.
//
// This software is released under the BSD 3-clause license. See LICENSE file
// for more details.
//

#ifndef Edge32DCVFEM_h
#define Edge32DCVFEM_h

#include <master_element/MasterElement.h>

#include <AlgTraits.h>

// NGP-based includes
#include "SimdInterface.h"
#include "KokkosInterface.h"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <array>

namespace stk {
struct topology;
}

namespace sierra {
namespace nalu {

// edge 2d
class Edge32DSCS : public MasterElement
{
public:
  using AlgTraits = AlgTraitsEdge3_2D;

  KOKKOS_FUNCTION
  Edge32DSCS();
  KOKKOS_FUNCTION virtual ~Edge32DSCS() {}
  using MasterElement::determinant;
  using MasterElement::shape_fcn;
  using MasterElement::shifted_shape_fcn;

  KOKKOS_FUNCTION virtual const int* ipNodeMap(int ordinal = 0) const final;

  KOKKOS_FUNCTION virtual void determinant(
    const SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType**, DeviceShmem>& area) override;

  virtual void determinant(
    const SharedMemView<double**>& coords,
    SharedMemView<double**>& area) override;

  template <typename SCALAR, typename SHMEM>
  KOKKOS_FUNCTION void shape_fcn(SharedMemView<SCALAR**, SHMEM>& shpfc);

  template <typename SCALAR, typename SHMEM>
  KOKKOS_FUNCTION void shifted_shape_fcn(SharedMemView<SCALAR**, SHMEM>& shpfc);

  void interpolatePoint(
    const int& nComp,
    const double* isoParCoord,
    const double* field,
    double* result) override;

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
  static constexpr int nDim_ = AlgTraits::nDim_;
  static constexpr int nodesPerElement_ = AlgTraits::nodesPerElement_;
  static constexpr int numIntPoints_ = AlgTraits::numScsIp_;
  static constexpr int numQuad_ = 2;

  const double scsDist_ = std::sqrt(3.0) / 3.0;
  const double scsEndLoc_[4] = {-1.0, -scsDist_, scsDist_, +1.0};

  const double intgLocShift_[numIntPoints_] = {-1.00, -1.00, 0.00,
                                               0.00,  1.00,  1.00};

  int ipNodeMap_[numIntPoints_];
  double intgLoc_[numIntPoints_];
  double ipWeight_[numIntPoints_];
  const double gaussWeight_[numQuad_] = {0.5, 0.5};
  const double gaussAbscissae_[numQuad_] = {
    -std::sqrt(3.0) / 3.0, std::sqrt(3.0) / 3.0};

  double tensor_product_weight(const int s1Node, const int s1Ip) const;
  double gauss_point_location(
    const int nodeOrdinal, const int gaussPointOrdinal) const;

  void area_vector(
    const double* POINTER_RESTRICT coords,
    const double s,
    double* POINTER_RESTRICT areaVector) const;

  template <typename DBLTYPE, typename SHMEM>
  KOKKOS_INLINE_FUNCTION void determinant_scs(
    const SharedMemView<DBLTYPE**, SHMEM>& coords,
    SharedMemView<DBLTYPE**, SHMEM>& area) const;
};

} // namespace nalu
} // namespace sierra

#endif
