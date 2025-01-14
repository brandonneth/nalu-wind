// Copyright 2017 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS), National Renewable Energy Laboratory, University of Texas Austin,
// Northwest Research Associates. Under the terms of Contract DE-NA0003525
// with NTESS, the U.S. Government retains certain rights in this software.
//
// This software is released under the BSD 3-clause license. See LICENSE file
// for more details.
//

#ifndef Quad92DCVFEM_h
#define Quad92DCVFEM_h

#include <master_element/MasterElement.h>

#include <AlgTraits.h>

// NGP-based includes
#include "SimdInterface.h"
#include "KokkosInterface.h"

#include <vector>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <array>

namespace sierra {
namespace nalu {

class QuadrilateralP2Element : public MasterElement
{
public:
  using AlgTraits = AlgTraitsQuad9_2D;

  KOKKOS_FUNCTION
  QuadrilateralP2Element();
  KOKKOS_FUNCTION virtual ~QuadrilateralP2Element() {}

protected:
  struct ContourData
  {
    Jacobian::Direction direction;
    double weight;
  };

  void GLLGLL_quadrature_weights();

  int tensor_product_node_map(int i, int j) const;

  double gauss_point_location(int nodeOrdinal, int gaussPointOrdinal) const;

  double
  shifted_gauss_point_location(int nodeOrdinal, int gaussPointOrdinal) const;

  double
  tensor_product_weight(int s1Node, int s2Node, int s1Ip, int s2Ip) const;

  double tensor_product_weight(int s1Node, int s1Ip) const;

  double parametric_distance(const std::array<double, 2>& x);

  virtual void interpolatePoint(
    const int& nComp,
    const double* isoParCoord,
    const double* field,
    double* result);

  virtual double isInElement(
    const double* elemNodalCoord,
    const double* pointCoord,
    double* isoParCoord);

  virtual void sidePcoords_to_elemPcoords(
    const int& side_ordinal,
    const int& npoints,
    const double* side_pcoords,
    double* elem_pcoords);

  void eval_shape_derivs_at_ips(const double*);

  void eval_shape_derivs_at_face_ips();

  // quadrature info

  const int sideNodeOrdinals_[12] = {0, 1, 4, 1, 2, 5, 2, 3, 6, 3, 0, 7};

protected:
  static const int nDim_ = AlgTraits::nDim_;
  static const int nodesPerElement_ = AlgTraits::nodesPerElement_;
  static const int nodes1D_ = 3;
  static const int numQuad_ = 2;

  // map the standard stk (refinement consistent) node numbering
  // to a tensor-product style node numbering (i.e. node (m,l,k) ->
  // m+npe*l+npe^2*k)
  const int stkNodeMap_[nodes1D_][nodes1D_] = {
    {0, 4, 1}, // bottom row of nodes
    {7, 8, 5}, // middle row of nodes
    {3, 6, 2}  // top row of nodes
  };

  const double scsDist_ = std::sqrt(3.0) / 3.0;
  const double scsEndLoc_[4] = {-1.0, -scsDist_, scsDist_, +1.0};

  const double gaussAbscissaeShift_[nodes1D_][numQuad_] = {
    {-1.0, -1.0}, {0.0, 0.0}, {+1.0, +1.0}};

  const double gaussAbscissae_[numQuad_] = {
    -std::sqrt(3.0) / 3.0, std::sqrt(3.0) / 3.0};
  const double gaussWeight_[numQuad_] = {0.5, 0.5};

  void
  quad9_shape_fcn(int npts, const double* par_coord, double* shape_fcn) const;

  void
  quad9_shape_deriv(int npts, const double* par_coord, double* shape_fcn) const;
};

// 3D Quad 27 subcontrol volume
class Quad92DSCV : public QuadrilateralP2Element
{
public:
  using MasterElement::determinant;
  using MasterElement::grad_op;
  using MasterElement::shape_fcn;
  using MasterElement::shifted_grad_op;
  using MasterElement::shifted_shape_fcn;

  KOKKOS_FUNCTION
  Quad92DSCV();
  KOKKOS_FUNCTION virtual ~Quad92DSCV() {}

  KOKKOS_FUNCTION virtual const int* ipNodeMap(int ordinal = 0) const final;

  template <typename SCALAR, typename SHMEM>
  KOKKOS_FUNCTION void shape_fcn(SharedMemView<SCALAR**, SHMEM>& shpfc);

  template <typename SCALAR, typename SHMEM>
  KOKKOS_FUNCTION void shifted_shape_fcn(SharedMemView<SCALAR**, SHMEM>& shpfc);

  KOKKOS_FUNCTION virtual void determinant(
    const SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType*, DeviceShmem>& vol) override;

  virtual void determinant(
    const SharedMemView<double**>& coords,
    SharedMemView<double*>& vol) override;

  KOKKOS_FUNCTION void grad_op(
    const SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& gradop,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) override;

  KOKKOS_FUNCTION void shifted_grad_op(
    SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& gradop,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) override;

  KOKKOS_FUNCTION void Mij(
    SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& metric,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) override;

  void Mij(const double* coords, double* metric, double* deriv) override;

  virtual const double* integration_locations() const final { return intgLoc_; }

protected:
  KOKKOS_FUNCTION virtual void
  shape_fcn(SharedMemView<DoubleType**, DeviceShmem>& shpfc) override;
  virtual void shape_fcn(SharedMemView<double**, HostShmem>& shpfc) override;

  KOKKOS_FUNCTION virtual void
  shifted_shape_fcn(SharedMemView<DoubleType**, DeviceShmem>& shpfc) override;
  virtual void
  shifted_shape_fcn(SharedMemView<double**, HostShmem>& shpfc) override;

private:
  static constexpr int numIntPoints_ = AlgTraits::numScvIp_;

  int ipNodeMap_[nodes1D_][nodes1D_][numQuad_][numQuad_]; //[numIntPoints_];
  double intgLoc_[numIntPoints_ * nDim_];
  double shapeFunctions_[numIntPoints_ * nodesPerElement_];
  double shapeFunctionsShift_[numIntPoints_ * nodesPerElement_];
  double intgLocShift_[numIntPoints_ * nDim_];
  double shapeDerivsShift_[numIntPoints_ * nodesPerElement_ * nDim_];
  double shapeDerivs_[numIntPoints_ * nodesPerElement_ * nDim_];
  // double expFaceShapeDerivs_[numIntPoints_*nodesPerElement_*nDim_];
  double ipWeight_[numIntPoints_];

  void set_interior_info();

  template <typename DBLTYPE, typename SHMEM>
  KOKKOS_FUNCTION DBLTYPE jacobian_determinant(
    const SharedMemView<DBLTYPE**, SHMEM>& coords,
    const double* POINTER_RESTRICT shapeDerivs) const;

  template <typename DBLTYPE>
  KOKKOS_FUNCTION DBLTYPE jacobian_determinant(
    const DBLTYPE* POINTER_RESTRICT elemNodalCoords,
    const DBLTYPE* POINTER_RESTRICT shapeDerivs) const;

  template <typename DBLTYPE, typename SHMEM>
  KOKKOS_INLINE_FUNCTION void determinant_scv(
    const SharedMemView<DBLTYPE**, SHMEM>& coords,
    SharedMemView<DBLTYPE*, SHMEM>& volume) const;
};

// 3D Hex 27 subcontrol surface
class Quad92DSCS : public QuadrilateralP2Element
{
public:
  using MasterElement::adjacentNodes;
  using MasterElement::determinant;
  using MasterElement::shape_fcn;
  using MasterElement::shifted_shape_fcn;

  KOKKOS_FUNCTION
  Quad92DSCS();
  KOKKOS_FUNCTION virtual ~Quad92DSCS() {}

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

  KOKKOS_FUNCTION void grad_op(
    const SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& gradop,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) override;

  void grad_op(
    const SharedMemView<double**>& coords,
    SharedMemView<double***>& gradop,
    SharedMemView<double***>& deriv) override;

  KOKKOS_FUNCTION void shifted_grad_op(
    SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& gradop,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) override;

  KOKKOS_FUNCTION void face_grad_op(
    int face_ordinal,
    SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& gradop,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) final;

  KOKKOS_FUNCTION void gij(
    const SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& gupper,
    SharedMemView<DoubleType***, DeviceShmem>& glower,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) override;

  KOKKOS_FUNCTION void Mij(
    SharedMemView<DoubleType**, DeviceShmem>& coords,
    SharedMemView<DoubleType***, DeviceShmem>& metric,
    SharedMemView<DoubleType***, DeviceShmem>& deriv) override;

  void Mij(const double* coords, double* metric, double* deriv) override;

  KOKKOS_FUNCTION virtual const int* adjacentNodes() final;

  KOKKOS_FUNCTION virtual const int* ipNodeMap(int ordinal = 0) const final;

  KOKKOS_FUNCTION int opposingNodes(const int ordinal, const int node) override;

  KOKKOS_FUNCTION int opposingFace(const int ordinal, const int node) override;

  KOKKOS_FUNCTION const int* side_node_ordinals(int sideOrdinal) const final;

  virtual const double* integration_locations() const final { return intgLoc_; }

protected:
  KOKKOS_FUNCTION virtual void
  shape_fcn(SharedMemView<DoubleType**, DeviceShmem>& shpfc) override;
  virtual void shape_fcn(SharedMemView<double**, HostShmem>& shpfc) override;

  KOKKOS_FUNCTION virtual void
  shifted_shape_fcn(SharedMemView<DoubleType**, DeviceShmem>& shpfc) override;
  virtual void
  shifted_shape_fcn(SharedMemView<double**, HostShmem>& shpfc) override;

private:
  static constexpr int numIntPoints_ = AlgTraits::numScsIp_;
  static constexpr int ipsPerFace_ = nodes1D_ * numQuad_;
  static constexpr int numFaces_ = 2 * nDim_;

  int ipNodeMap_[numFaces_][nodes1D_][numQuad_]; //[numIntPoints_];
  int oppNode_[numIntPoints_];
  int oppFace_[numIntPoints_];
  double intgLoc_[numIntPoints_ * nDim_];

  double shapeFunctions_[numIntPoints_ * nodesPerElement_];
  double shapeFunctionsShift_[numIntPoints_ * nodesPerElement_];
  double intgLocShift_[numIntPoints_ * nDim_];
  double shapeDerivsShift_[numIntPoints_ * nodesPerElement_ * nDim_];
  double shapeDerivs_[numIntPoints_ * nodesPerElement_ * nDim_];
  double expFaceShapeDerivs_[numIntPoints_ * nodesPerElement_ * nDim_];
  double intgExpFace_[numIntPoints_ * nDim_];

  int lrscv_[2 * numIntPoints_];

  ContourData ipInfo_[numIntPoints_];

  void set_interior_info();
  void set_boundary_info();

  template <Jacobian::Direction direction, typename DBLTYPE, typename SHMEM>
  void KOKKOS_FUNCTION area_vector(
    const SharedMemView<DBLTYPE**, SHMEM>& elemNodalCoords,
    const double* POINTER_RESTRICT shapeDeriv,
    DBLTYPE* POINTER_RESTRICT areaVector) const;

  template <Jacobian::Direction direction>
  void KOKKOS_FUNCTION area_vector(
    const double* POINTER_RESTRICT elemNodalCoords,
    const double* POINTER_RESTRICT shapeDeriv,
    double* POINTER_RESTRICT areaVector) const;

  template <typename DBLTYPE, typename SHMEM>
  KOKKOS_INLINE_FUNCTION void determinant_scs(
    const SharedMemView<DBLTYPE**, SHMEM>& coords,
    SharedMemView<DBLTYPE**, SHMEM>& areav) const;
};

} // namespace nalu
} // namespace sierra

#endif
