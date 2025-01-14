// Copyright 2017 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS), National Renewable Energy Laboratory, University of Texas Austin,
// Northwest Research Associates. Under the terms of Contract DE-NA0003525
// with NTESS, the U.S. Government retains certain rights in this software.
//
// This software is released under the BSD 3-clause license. See LICENSE file
// for more details.
//

#include "wind_energy/SyntheticLidar.h"

#include "NaluParsing.h"
#include "master_element/TensorOps.h"

#include "xfer/Transfer.h"
#include "xfer/LocalVolumeSearch.h"
#include "netcdf.h"
#include "Ioss_FileInfo.h"

#include <memory>

namespace sierra {
namespace nalu {

constexpr int dim = 3;

void
LidarLineOfSite::load(const YAML::Node& node)
{
  NaluEnv::self().naluOutputP0() << "LidarLineOfSite::load" << std::endl;

  if (node["type"]) {
    segGen = make_segment_generator(node["type"].as<std::string>());
  } else {
    segGen = make_segment_generator(SegmentType::SPINNER);
  }

  if (node["output"]) {
    const auto type = node["output"].as<std::string>();
    if (type == "text") {
      output_type_ = Output::TEXT;
    } else if (type == "netcdf") {
      output_type_ = Output::NETCDF;
    } else if ("dataprobes") {
      output_type_ = Output::DATAPROBE;
    }
  }

  get_required(node, "points_along_line", npoints_);

  if (node["name"]) {
    name_ = node["name"].as<std::string>();
  }

  if (node["time_step"] && output_type_ != Output::DATAPROBE) {
    lidar_dt_ = node["time_step"].as<double>();
  } else if (node["frequency"] && output_type_ != Output::DATAPROBE) {
    lidar_dt_ = 1.0 / node["frequency"].as<double>();
  } else {
    get_required(node, "scan_time", scanTime_);
    get_required(node, "number_of_samples", nsamples_);
    lidar_dt_ = scanTime_ / nsamples_;
  }

  if (node["predictor"]) {
    const auto spec = node["predictor"].as<std::string>();
    std::map<std::string, Predictor> valid = {
      {"forward_euler", Predictor::FORWARD_EULER},
      {"nearest", Predictor::NEAREST}};
    if (valid.find(spec) != valid.end()) {
      predictor_ = valid.at(spec);
    } else {
      std::string valid_keys = "";
      for (const auto& pair : valid) {
        valid_keys += pair.first + " ";
      }
      valid_keys = valid_keys.substr(0, valid_keys.size() - 1);
      throw std::runtime_error(
        "invalid predictor spec: " + spec + ", valid specs are: " + valid_keys);
    }
  }

  const YAML::Node fromTargets = node["from_target_part"];
  if (fromTargets) {
    if (fromTargets.Type() == YAML::NodeType::Scalar) {
      fromTargetNames_.push_back(fromTargets.as<std::string>());
    } else {
      for (const auto& target : fromTargets) {
        fromTargetNames_.push_back(target.as<std::string>());
      }
    }
  }

  if (node["scanning_lidar_specifications"]) {
    segGen->load(node["scanning_lidar_specifications"]);
  } else {
    segGen->load(node);
  }
}

bool
is_root(MPI_Comm comm, int root)
{
  int rank;
  MPI_Comm_rank(comm, &rank);
  return rank == root;
}
void
check_nc_error(int code)
{

  if (code != 0) {
    throw std::runtime_error(
      "SyntheticLidar NetCDF error: " + std::string(nc_strerror(code)));
  }
}
namespace {
std::string
determine_filename(const std::string& name, const std::string& suffix)
{
  std::string fname = name + suffix;

  Ioss::FileInfo info(fname);
  if (info.exists()) {
    // give a large, finite amount of names to check
    const int max_restarts = 2048;
    bool found_valid = false;
    for (int j = 1; j < max_restarts; ++j) {
      fname = name + "-rst-" + std::to_string(j) + suffix;
      Ioss::FileInfo info_j(fname);
      if (!info_j.exists()) {
        found_valid = true;
        break;
      }
    }
    if (!found_valid) {
      throw std::runtime_error("Too many restarts checked");
    }
  }
  return fname;
}
} // namespace

void
LidarLineOfSite::prepare_nc_file()
{
  int ncid;

  fname_ = determine_filename(name_, ".nc");

  int ierr = nc_create(fname_.c_str(), NC_CLOBBER, &ncid);
  check_nc_error(ierr);

  // Define dimensions for the NetCDF file
  int tDim;
  ierr = nc_def_dim(ncid, "num_timesteps", NC_UNLIMITED, &tDim);
  check_nc_error(ierr);

  int pDim;
  ierr = nc_def_dim(ncid, "num_points", npoints_, &pDim);
  check_nc_error(ierr);

  int vDim;
  ierr = nc_def_dim(ncid, "vec_dim", 3, &vDim);
  check_nc_error(ierr);

  const int vec_dim[3] = {tDim, pDim, vDim};

  {
    int varid;
    ierr = nc_def_var(ncid, "step", NC_INT, 1, &tDim, &varid);
    check_nc_error(ierr);
    ncVarIDs_["step"] = varid;
  }

  auto add_ncvar = [&](std::string name, int dim, const int* const dims) {
    int varid;
    ierr = nc_def_var(ncid, name.c_str(), NC_DOUBLE, dim, dims, &varid);
    check_nc_error(ierr);
    ncVarIDs_[name] = varid;
  };

  add_ncvar("time", 1, &tDim);
  add_ncvar("coordinates", 3, vec_dim);
  add_ncvar("velocity", 3, vec_dim);

  //! Indicate that we are done defining variables, ready to write data
  ierr = nc_enddef(ncid);
  check_nc_error(ierr);

  ierr = nc_close(ncid);
  check_nc_error(ierr);
}

void
LidarLineOfSite::output_nc(
  double time,
  const std::vector<std::array<double, 3>>& x,
  const std::vector<std::array<double, 3>>& u)
{
  if (internal_output_counter_ == 0) {
    prepare_nc_file();
  }

  int ncid, ierr;
  ierr = nc_open(fname_.c_str(), NC_WRITE, &ncid);
  check_nc_error(ierr);

  size_t scalar = 1;
  const size_t vector_list_start[] = {internal_output_counter_, 0, 0};
  const size_t vector_list_count[] = {1, static_cast<size_t>(npoints_), 3};

  const int step = static_cast<int>(internal_output_counter_);
  ierr = nc_put_vara_int(
    ncid, ncVarIDs_["step"], &internal_output_counter_, &scalar, &step);
  check_nc_error(ierr);

  ierr = nc_put_vara_double(
    ncid, ncVarIDs_["time"], &internal_output_counter_, &scalar, &time);
  check_nc_error(ierr);

  ierr = nc_put_vara_double(
    ncid, ncVarIDs_["coordinates"], vector_list_start, vector_list_count,
    &x[0][0]);
  check_nc_error(ierr);

  ierr = nc_put_vara_double(
    ncid, ncVarIDs_["velocity"], vector_list_start, vector_list_count,
    &u[0][0]);
  check_nc_error(ierr);

  ierr = nc_close(ncid);
  check_nc_error(ierr);

  ++internal_output_counter_;
}

void
LidarLineOfSite::output_txt(
  double time,
  const std::vector<std::array<double, 3>>& x,
  const std::vector<std::array<double, 3>>& u)
{
  if (internal_output_counter_ == 0) {
    fname_ = determine_filename(name_, ".txt");
    std::ofstream file{fname_, std::ios_base::out};
    file.open(fname_);
    file << "t,x,y,z,u,v,w" << std::endl;
    file.close();
  }

  std::ofstream file;
  file.exceptions(file.exceptions() | std::ios::failbit);
  file.open(fname_, std::ios::out | std::ios::app);
  if (file.fail()) {
    throw std::ios_base::failure(std::strerror(errno));
  }
  for (size_t j = 0; j < x.size(); ++j) {
    file << std::setprecision(15) << time << "," << x.at(j)[0] << ","
         << x.at(j)[1] << "," << x.at(j)[2] << "," << u.at(j)[0] << ","
         << u.at(j)[1] << "," << u.at(j)[2] << std::endl;
  }
  file.close();

  ++internal_output_counter_;
}

void
LidarLineOfSite::output(
  const stk::mesh::BulkData& bulk,
  const stk::mesh::Selector& active,
  const std::string& coordinates_name,
  double dtratio)
{
  if (output_type_ == Output::DATAPROBE) {
    return;
  }
  if (internal_output_counter_ == 0) {
    Ioss::FileInfo::create_path(name_);
  }

  if (!search_data_) {
    search_data_ =
      std::make_unique<LocalVolumeSearchData>(bulk, active, npoints_);
  }

  const auto seg = segGen->generate(time());
  const std::array<double, 3> dx{
    {(seg.tip_[0] - seg.tail_[0]) / (npoints_ > 1 ? (npoints_ - 1) : 1),
     (seg.tip_[1] - seg.tail_[1]) / (npoints_ > 1 ? (npoints_ - 1) : 1),
     (seg.tip_[2] - seg.tail_[2]) / (npoints_ > 1 ? (npoints_ - 1) : 1)}};

  std::vector<std::array<double, 3>> points(npoints_);
  for (int j = 0; j < npoints_; ++j) {
    points[j] = {
      {seg.tail_[0] + j * dx[0], seg.tail_[1] + j * dx[1],
       seg.tail_[2] + j * dx[2]}};
  }
  const auto& coord_field =
    *bulk.mesh_meta_data()
       .get_field<stk::mesh::Field<double, stk::mesh::Cartesian3d>>(
         stk::topology::NODE_RANK, coordinates_name);

  const auto& velocity_field =
    bulk.mesh_meta_data()
      .get_field<stk::mesh::Field<double, stk::mesh::Cartesian3d>>(
        stk::topology::NODE_RANK, "velocity")
      ->field_of_state(stk::mesh::StateNP1);

  const auto& velocity_prev =
    bulk.mesh_meta_data()
      .get_field<stk::mesh::Field<double, stk::mesh::Cartesian3d>>(
        stk::topology::NODE_RANK, "velocity")
      ->field_of_state(stk::mesh::StateN);

  const double extrap_dt = predictor_ == Predictor::NEAREST ? 0 : dtratio;
  local_field_interpolation(
    bulk, active, points, coord_field, velocity_prev, velocity_field, extrap_dt,
    *search_data_);

  const auto lcl_velocity = search_data_->interpolated_values;
  const auto lcl_ownership = search_data_->ownership;

  auto comm = bulk.parallel();
  const int root = 0;

  std::vector<std::array<double, 3>> velocity(npoints_, {0, 0, 0});
  MPI_Reduce(
    lcl_velocity.data(), velocity.data(), 3 * npoints_, MPI_DOUBLE, MPI_SUM,
    root, comm);

  // parallel reconciliation for points along processor boundaries is to
  // do an arithmetic average, assuming continuity.
  std::vector<int> degree(npoints_, 0);
  MPI_Reduce(
    lcl_ownership.data(), degree.data(), npoints_, MPI_INT, MPI_SUM, root,
    comm);

  if (is_root(comm, root)) {
    int not_found_count = 0;

    std::array<double, dim> max_unmatched{
      std::numeric_limits<double>::lowest(),
      std::numeric_limits<double>::lowest(),
      std::numeric_limits<double>::lowest()};
    std::array<double, dim> min_unmatched{
      std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
      std::numeric_limits<double>::max()};
    for (int j = 0; j < npoints_; ++j) {
      const auto degj = degree.at(j);
      if (degj == 0) {
        ++not_found_count;
        for (int d = 0; d < 3; ++d) {
          max_unmatched[d] = std::max(max_unmatched[d], points.at(j)[d]);
          min_unmatched[d] = std::min(min_unmatched[d], points.at(j)[d]);
        }
      }
      const double inv_deg =
        (degj > 0) ? 1 / static_cast<double>(degree[j]) : 0;
      for (int d = 0; d < 3; ++d) {
        velocity.at(j)[d] *= inv_deg;
      }
    }
    if (not_found_count > 0) {

      auto lidar_name_start = name_.find_last_of("/");
      auto lidar_name = name_.substr(lidar_name_start + 1);

      NaluEnv::self().naluOutputP0()
        << "LIDAR " << lidar_name << " search did not match " << not_found_count
        << " points, max individually unmatched coords: (" << max_unmatched[0]
        << ", " << max_unmatched[1] << ", " << max_unmatched[2] << ")"
        << ", min individually unmatched coords: (" << min_unmatched[0] << ", "
        << min_unmatched[1] << ", " << min_unmatched[2] << ")" << std::endl;
    }

    if (output_type_ == Output::TEXT) {
      output_txt(time(), points, velocity);
    } else if (output_type_ == Output::NETCDF) {
      output_nc(time(), points, velocity);
    }
  }
}

std::unique_ptr<DataProbeSpecInfo>
LidarLineOfSite::determine_line_of_site_info(const YAML::Node& node)
{
  load(node);

  auto lidarLOSInfo = std::make_unique<DataProbeSpecInfo>();

  lidarLOSInfo->xferName_ = "LidarSampling_xfer";
  lidarLOSInfo->fromToName_.emplace_back("velocity", "velocity_probe");
  lidarLOSInfo->fieldInfo_.emplace_back("velocity_probe", 3);
  lidarLOSInfo->fromTargetNames_ = fromTargetNames_;

  auto probeInfo = std::make_unique<DataProbeInfo>();

  probeInfo->isLineOfSite_ = true;
  probeInfo->numProbes_ = nsamples_;
  probeInfo->partName_.resize(nsamples_);
  probeInfo->processorId_.resize(nsamples_);
  probeInfo->numPoints_.resize(nsamples_);
  probeInfo->generateNewIds_.resize(nsamples_);
  probeInfo->tipCoordinates_.resize(nsamples_);
  probeInfo->tailCoordinates_.resize(nsamples_);
  probeInfo->nodeVector_.resize(nsamples_);
  probeInfo->part_.resize(nsamples_);
  probeInfo->geomType_.resize(nsamples_);

  const int numProcs = NaluEnv::self().parallel_size();
  const int divProcProbe = std::max(numProcs / nsamples_, numProcs);

  for (int ilos = 0; ilos < nsamples_; ilos++) {
    const double lidarTime = scanTime_ / (double)nsamples_ * ilos;
    Segment seg = segGen->generate(lidarTime);

    probeInfo->processorId_[ilos] = divProcProbe > 0 ? ilos % divProcProbe : 0;
    probeInfo->partName_[ilos] = name_ + "_" + std::to_string(ilos);
    probeInfo->numPoints_[ilos] = npoints_;
    probeInfo->geomType_[ilos] = DataProbeGeomType::LINEOFSITE;

    probeInfo->tipCoordinates_[ilos].x_ = seg.tip_[0];
    probeInfo->tipCoordinates_[ilos].y_ = seg.tip_[1];
    probeInfo->tipCoordinates_[ilos].z_ = seg.tip_[2];

    probeInfo->tailCoordinates_[ilos].x_ = seg.tail_[0];
    probeInfo->tailCoordinates_[ilos].y_ = seg.tail_[1];
    probeInfo->tailCoordinates_[ilos].z_ = seg.tail_[2];
  }
  lidarLOSInfo->dataProbeInfo_.push_back(probeInfo.release());

  return lidarLOSInfo;
}

} // namespace nalu
} // namespace sierra
