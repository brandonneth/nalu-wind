// Copyright 2017 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS), National Renewable Energy Laboratory, University of Texas Austin,
// Northwest Research Associates. Under the terms of Contract DE-NA0003525
// with NTESS, the U.S. Government retains certain rights in this software.
//
// This software is released under the BSD 3-clause license. See LICENSE file
// for more details.
//

#ifndef TensorProductCVFEMVolumeMetric_h
#define TensorProductCVFEMVolumeMetric_h

#include <master_element/CVFEMCoefficientMatrices.h>
#include <master_element/TensorProductCVFEMOperators.h>
#include <master_element/DirectionMacros.h>
#include <master_element/TensorOps.h>
#include <master_element/Hex8GeometryFunctions.h>

#include <AlgTraits.h>

#include <stk_util/util/ReportHandler.hpp>

namespace sierra {
namespace nalu {
namespace high_order_metrics {
template <int p, typename Scalar>
void
compute_volume_metric_linear(
  const CVFEMOperators<p, Scalar>& ops,
  const nodal_vector_view<p, Scalar>& xc,
  nodal_scalar_view<p, Scalar>& vol)
{
  enum { LEFT = 0, RIGHT = 1 };
  const auto& nodalInterp = ops.mat_.linearNodalInterp;

  NALU_ALIGNED Scalar base_box[3][8];
  hex_vertex_coordinates<p, Scalar>(xc, base_box);

  for (int k = 0; k < p + 1; ++k) {
    NALU_ALIGNED const Scalar interpk[2] = {
      nodalInterp(LEFT, k), nodalInterp(RIGHT, k)};
    for (int j = 0; j < p + 1; ++j) {
      NALU_ALIGNED const Scalar interpj[2] = {
        nodalInterp(LEFT, j), nodalInterp(RIGHT, j)};
      for (int i = 0; i < p + 1; ++i) {
        NALU_ALIGNED const Scalar interpi[2] = {
          nodalInterp(LEFT, i), nodalInterp(RIGHT, i)};
        NALU_ALIGNED Scalar jac[3][3];
        hex_jacobian(base_box, interpi, interpj, interpk, jac);
        vol(k, j, i) = determinant33(&jac[0][0]);
      }
    }
  }
}
} // namespace high_order_metrics
} // namespace nalu
} // namespace sierra

#endif
