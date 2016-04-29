/**
@file   zmp_optimizer.cc
@author Alexander Winkler (winklera@ethz.ch)
@date   Oct 21,  2014
@brief  Dynamic Walking using Zero-Moment-Point (ZMP) Criteria
 */

#include <xpp/zmp/qp_optimizer.h>

#include <xpp/zmp/continuous_spline_container.h>
#include <xpp/hyq/support_polygon_container.h>

#include <xpp/zmp/zmp_constraint.h>
#include <xpp/zmp/spline_constraints.h>
#include <xpp/zmp/cost_function.h>
#include <xpp/zmp/eigen_quadprog-inl.h>

#include <cmath>      // std::numeric_limits

#include <ros/console.h>

namespace xpp {
namespace zmp {



QpOptimizer::VecSpline
QpOptimizer::SolveQp(const State& initial_state,
                     const State& final_state,
                     const VecFoothold& start_stance,
                     const VecFoothold& steps,
                     double swing_time,
                     double stance_time,
                     double stance_time_initial,
                     double stance_time_final,
                     double robot_height)
{
  ContinuousSplineContainer spline_structure;
  std::vector<xpp::hyq::LegID> leg_ids;
  for (Foothold f : steps)
    leg_ids.push_back(f.leg);
  spline_structure.Init(initial_state.p, initial_state.v ,leg_ids, stance_time, swing_time, stance_time_initial,stance_time_final);
  cost_function_ = CostFunction::CreateMinAccCostFunction(spline_structure);

  SplineConstraints spline_constraint(spline_structure);
  equality_constraints_ = spline_constraint.CreateAllSplineConstraints(initial_state.a, final_state);

  xpp::hyq::SupportPolygonContainer supp_polygon_container;
  supp_polygon_container.Init(start_stance, steps, hyq::SupportPolygon::GetDefaultMargins());
  ZmpConstraint zmp_constraint(spline_structure, robot_height);
  inequality_constraints_ = zmp_constraint.CreateLineConstraints(supp_polygon_container);

  ROS_INFO_STREAM("Initial state: " << initial_state);
  ROS_INFO_STREAM("Final state: " << final_state);

  Eigen::VectorXd opt_abcd = EigenSolveQuadprog();
  spline_structure.AddOptimizedCoefficients(opt_abcd, spline_structure.splines_);

  return spline_structure.splines_;
}



Eigen::VectorXd QpOptimizer::EigenSolveQuadprog()
{
  Eigen::VectorXd opt_spline_coeff_xy;
  ROS_INFO("QP optimizer running...");

  double cost = Eigen::solve_quadprog(
      cost_function_.M, cost_function_.v,
      equality_constraints_.M.transpose(), equality_constraints_.v,
      inequality_constraints_.M.transpose(), inequality_constraints_.v,
      opt_spline_coeff_xy);

  ROS_INFO("QP optimizer done.");

  if (cost == std::numeric_limits<double>::infinity())
    throw std::length_error("Eigen::quadprog did not find a solution");

  return opt_spline_coeff_xy;
}


} // namespace zmp
} // namespace xpp
