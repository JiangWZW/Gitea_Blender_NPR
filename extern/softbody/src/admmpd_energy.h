// Copyright Matt Overby 2020.
// Distributed under the MIT License.

#ifndef ADMMPD_ENERGY_H_
#define ADMMPD_ENERGY_H_ 1

#include <Eigen/Sparse>
#include <Eigen/Geometry>
#include <vector>
#include "admmpd_types.h"

namespace admmpd {

class Lame {
public:
	double m_mu;
	double m_lambda;
	double m_bulk_mod;
	void set_from_youngs_poisson(double youngs, double poisson);
	Lame();
};

class EnergyTerm {
public:

	void signed_svd(
		const Eigen::Matrix<double,3,3> &A, 
		Eigen::Matrix<double,3,3> &U, 
		Eigen::Matrix<double,3,1> &S, 
		Eigen::Matrix<double,3,3> &V);

	// Updates the z and u variables for an element energy.
	void update(
		const Options *options,
		int index,
		int energyterm_type,
		double rest_volume,
		double weight,
		const Eigen::MatrixXd *x,
		const Eigen::MatrixXd *Dx,
		Eigen::MatrixXd *z,
		Eigen::MatrixXd *u);

	// Updates the z and u variables for a tet
	void update_tet(
		const Options *options,
		int index,
		double rest_volume,
		double weight,
		const Eigen::MatrixXd *x,
		const Eigen::MatrixXd *Dx,
		Eigen::MatrixXd *z,
		Eigen::MatrixXd *u);

	// Updates the z and u variables for a tri
	void update_tri(
		const Options *options,
		int index,
		double rest_area,
		double weight,
		const Eigen::MatrixXd *x,
		const Eigen::MatrixXd *Dx,
		Eigen::MatrixXd *z,
		Eigen::MatrixXd *u);

	// Initializes tet energy, returns num rows for D
	int init_tet(
		const Options *options,
		int index,
		const Eigen::RowVector4i &prim,
		const Eigen::MatrixXd *x,
		double &volume,
		double &weight,
		std::vector< Eigen::Triplet<double> > &triplets);

	// Initializes facet energy, returns num rows for D
	int init_triangle(
		const Options *options,
		int index,
		const Eigen::RowVector3i &prim,
		const Eigen::MatrixXd *x,
		double &area,
		double &weight,
		std::vector< Eigen::Triplet<double> > &triplets);

	// Solves proximal energy function
	void solve_prox(
		const Options *options,
		int index,
		const Lame &lame,
		const Eigen::Vector3d &s0,
		Eigen::Vector3d &s);

	// Returns gradient and energy (+ADMM penalty)
	// of a material evaluated at s (singular values).
	double energy_density(
		const Options *options,
		const Lame &lame,
		bool add_admm_penalty,
		const Eigen::Vector3d &s0,
		const Eigen::Vector3d &s,
		Eigen::Vector3d *g=nullptr);

	// Returns the Hessian of a material at S
	// projected to nearest SPD
	void hessian_spd(
		const Options *options,
		const Lame &lame,
		bool add_admm_penalty,
		const Eigen::Vector3d &s,
		Eigen::Matrix3d &H);

};

} // end namespace admmpd

#endif // ADMMPD_ENERGY_H_




