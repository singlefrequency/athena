//========================================================================================
// Athena++ astrophysical MHD code
// Copyright(C) 2014 James M. Stone <jmstone@princeton.edu> and other code contributors
// Licensed under the 3-clause BSD License, see LICENSE file for details
//========================================================================================
//! \file gr_bondi.cpp
//! Problem generator for Bondi spherical accretion with user defined four dimensional
//  spacetime. In the current case, we used special form of spacetime line element, 
//  namely Schwarzschild wormhole spacetime with the present tidal forces:
//  ds^2 = -e^(-2*aa/r)dt^2+dr^2/(1-b(r)/r)+r^2dθ^2 + r^2 sin^2θ d^2ϕ
//  with aa being the so-called tidal parameter and b(r)=r0 is the wormhole shape function,
//  in our case it is constant and equals to the wormhole throat radius r0.

// C headers

// C++ headers
#include <cmath>   // abs(), NAN, pow(), sqrt()
#include <cstring> // strcmp()

// Athena++ headers
#include "../athena.hpp"                   // macros, enums, FaceField
#include "../athena_arrays.hpp"            // AthenaArray
#include "../bvals/bvals.hpp"              // BoundaryValues
#include "../coordinates/coordinates.hpp"  // Coordinates
#include "../eos/eos.hpp"                  // EquationOfState
#include "../field/field.hpp"              // Field
#include "../hydro/hydro.hpp"              // Hydro
#include "../mesh/mesh.hpp"
#include "../parameter_input.hpp"          // ParameterInput

// Configuration checking
#if not GENERAL_RELATIVITY
#error "This problem generator must be used with general relativity"
#endif

// Declarations

void SchildWH(Real x1, Real x2, Real x3, ParameterInput *pin,
    AthenaArray<Real> &g, AthenaArray<Real> &g_inv, AthenaArray<Real> &dg_dx1,
    AthenaArray<Real> &dg_dx2, AthenaArray<Real> &dg_dx3)
{
  // Extract inputs
  Real aa = pin->GetReal("coord", "aa");
  Real r0 = pin->GetReal("coord", "r0");

  Real r = x1;
  Real theta = x2;
  Real phi = x3;

  // Calculate intermediate quantities
  Real sth = std::sin(theta);
  Real cth = std::cos(theta);
  Real shape = r0;
  Real redshift = -aa/r;
  Real dshape = 0;
  Real dredshift =  aa/(r*r);
  
  // Set covariant components
  g(I00) = -std::exp(2*redshift);
  g(I11) = 1/(1-shape/r);
  g(I22) = r*r;
  g(I33) = r*r*sth*sth;
  g(I01) = 0.0;
  g(I02) = 0.0;
  g(I03) = 0.0;
  g(I12) = 0.0;
  g(I13) = 0.0;
  g(I23) = 0.0;

  // Set contravariant components
  g_inv(I00) = -std::exp(-2*redshift);
  g_inv(I11) = 1-shape/r;
  g_inv(I22) = 1/(r*r);
  g_inv(I33) = 1/(r*r*sth*sth);
  g_inv(I01) = 0.0;
  g_inv(I02) = 0.0;
  g_inv(I03) = 0.0;
  g_inv(I12) = 0.0;
  g_inv(I13) = 0.0;
  g_inv(I23) = 0.0;

  // Set r-derivatives of covariant components
  dg_dx1(I00) = -2*std::exp(-2*redshift)*dredshift;
  dg_dx1(I11) = (r*dshape-shape)/((r-shape)*(r-shape));
  dg_dx1(I22) = 2*r;
  dg_dx1(I33) = 2*r*sth*sth;
  dg_dx1(I01) = 0.0;
  dg_dx1(I02) = 0.0;
  dg_dx1(I03) = 0.0;
  dg_dx1(I12) = 0.0;
  dg_dx1(I13) = 0.0;
  dg_dx1(I23) = 0.0;

  // Set theta-derivatives of covariant components
  dg_dx2(I00) = 0;
  dg_dx2(I11) = 0;
  dg_dx2(I22) = 0;
  dg_dx2(I33) = 2*r*r*sth*cth;
  dg_dx2(I01) = 0.0;
  dg_dx2(I02) = 0.0;
  dg_dx2(I03) = 0.0;
  dg_dx2(I12) = 0.0;
  dg_dx2(I13) = 0.0;
  dg_dx2(I23) = 0.0;

  // Set phi-derivatives of covariant components
  for (int n = 0; n < NMETRIC; ++n) {
    dg_dx3(n) = 0.0;
  }
  return;
}


void FixedBoundary(MeshBlock *pmb, Coordinates *pcoord, AthenaArray<Real> &prim,
                   FaceField &bb, Real time, Real dt,
                   int il, int iu, int jl, int ju, int kl, int ku, int ngh);
namespace {
void GetBoyerLindquistCoordinates(Real x1, Real x2, Real x3, Real *pr,
                                  Real *ptheta, Real *pphi);
void TransformVector(Real a0_bl, Real a1_bl, Real a2_bl, Real a3_bl, Real r,
                     Real theta, Real phi,
                     Real *pa0, Real *pa1, Real *pa2, Real *pa3);
void CalculatePrimitives(Real r, Real temp_min, Real temp_max, Real *prho,
                         Real *ppgas, Real *put, Real *pur);
Real TemperatureMin(Real r, Real t_min, Real t_max);
Real TemperatureBisect(Real r, Real t_min, Real t_max);
Real TemperatureResidual(Real t, Real r);

// Global variables
Real r0, aa;          // wormhole throat radius and tidal parameter
Real n_adi, k_adi;  // hydro parameters
Real r_crit;        // sonic point radius
Real c1, c2;        // useful constants
Real bsq_over_rho;  // b^2/rho at inner radius
} 



void Mesh::InitUserMeshData(ParameterInput *pin) {
  // Read problem parameters
  EnrollUserMetric(SchildWH);
  k_adi = pin->GetReal("hydro", "k_adi");
  r_crit = pin->GetReal("problem", "r_crit");
  bsq_over_rho = 0.0;
  if (MAGNETIC_FIELDS_ENABLED) {
    bsq_over_rho = pin->GetReal("problem", "bsq_over_rho");
  }

  // Enroll boundary functions
  EnrollUserBoundaryFunction(BoundaryFace::inner_x1, FixedBoundary);
  EnrollUserBoundaryFunction(BoundaryFace::outer_x1, FixedBoundary);
  return;
}


void MeshBlock::ProblemGenerator(ParameterInput *pin) {
  // Parameters
  const Real temp_min = 1.0e-2;  // lesser temperature root must be greater than this
  const Real temp_max = 1.0e1;   // greater temperature root must be less than this

  // Prepare index bounds
  int il = is - NGHOST;
  int iu = ie + NGHOST;
  int jl = js;
  int ju = je;
  if (block_size.nx2 > 1) {
    jl -= NGHOST;
    ju += NGHOST;
  }
  int kl = ks;
  int ku = ke;
  if (block_size.nx3 > 1) {
    kl -= NGHOST;
    ku += NGHOST;
  }


  // Get ratio of specific heats
  Real gamma_adi = peos->GetGamma();
  n_adi = 1.0/(gamma_adi-1.0);

  // Prepare scratch arrays
  AthenaArray<Real> g, gi;
  g.NewAthenaArray(NMETRIC, iu+1);
  gi.NewAthenaArray(NMETRIC, iu+1);

  // Prepare various constants for determining primitives
  Real redcrit = -aa/r_crit;
  Real u_crit_sq = 1/4*(1-std::exp(2*redcrit));
  Real u_crit = -std::sqrt(u_crit_sq);
  Real t_crit = -(n_adi*(std::exp(2*redcrit)-1))/((n_adi+1)*(n_adi*std::exp(2*redcrit)-n_adi+3*std::exp(2*redcrit)+1));  // (HSW 74)
  c1 = std::pow(t_crit, n_adi) * u_crit * SQR(r_crit);                      // (HSW 68)
  c2 = SQR(1.0 + (n_adi+1.0) * t_crit) * (std::exp(2*redcrit)+u_crit_sq);        // (HSW 69)

  // Initialize primitive values
  for (int k=kl; k<=ku; ++k) {
    for (int j=jl; j<=ju; ++j) {
      pcoord->CellMetric(k, j, il, iu, g, gi);
      for (int i=il; i<=iu; ++i) {
        Real r(0.0), theta(0.0), phi(0.0);
        GetBoyerLindquistCoordinates(pcoord->x1v(i), pcoord->x2v(j), pcoord->x3v(k), &r,
                                     &theta, &phi);
        Real rho, pgas, ut, ur;
        CalculatePrimitives(r, temp_min, temp_max, &rho, &pgas, &ut, &ur);
        Real u0(0.0), u1(0.0), u2(0.0), u3(0.0);
        TransformVector(ut, ur, 0.0, 0.0, r, theta, phi, &u0, &u1, &u2, &u3);
        Real uu1 = u1 - gi(I01,i)/gi(I00,i) * u0;
        Real uu2 = u2 - gi(I02,i)/gi(I00,i) * u0;
        Real uu3 = u3 - gi(I03,i)/gi(I00,i) * u0;
        phydro->w(IDN,k,j,i) = phydro->w1(IDN,k,j,i) = rho;
        phydro->w(IPR,k,j,i) = phydro->w1(IPR,k,j,i) = pgas;
        phydro->w(IM1,k,j,i) = phydro->w1(IM1,k,j,i) = uu1;
        phydro->w(IM2,k,j,i) = phydro->w1(IM2,k,j,i) = uu2;
        phydro->w(IM3,k,j,i) = phydro->w1(IM3,k,j,i) = uu3;
      }
    }
  }

  // Initialize conserved variables
  peos->PrimitiveToConserved(phydro->w, pfield->bcc, phydro->u, pcoord, il, iu, jl, ju,
                             kl, ku);

  // Free scratch arrays
  return;
}

//----------------------------------------------------------------------------------------
// Fixed boundary condition
// Inputs:
//   pmb: pointer to MeshBlock
//   pcoord: pointer to Coordinates
//   time,dt: current time and timestep of simulation
//   is,ie,js,je,ks,ke: indices demarkating active region
// Outputs:
//   prim: primitives set in ghost zones
//   bb: face-centered magnetic field set in ghost zones
// Notes:
//   does nothing

void FixedBoundary(MeshBlock *pmb, Coordinates *pcoord, AthenaArray<Real> &prim,
                   FaceField &bb, Real time, Real dt,
                   int il, int iu, int jl, int ju, int kl, int ku, int ngh) {
  return;
}

namespace {
//----------------------------------------------------------------------------------------
// Function for returning corresponding Boyer-Lindquist coordinates of point
// Inputs:
//   x1,x2,x3: global coordinates to be converted
// Outputs:
//   pr,ptheta,pphi: variables pointed to set to Boyer-Lindquist coordinates
// Notes:
//   conversion is trivial in all currently implemented coordinate systems

void GetBoyerLindquistCoordinates(Real x1, Real x2, Real x3, Real *pr,
                                  Real *ptheta, Real *pphi) {
  if (std::strcmp(COORDINATE_SYSTEM, "schwarzschild") == 0 ||
      std::strcmp(COORDINATE_SYSTEM, "gr_user") == 0) {
    *pr = x1;
    *ptheta = x2;
    *pphi = x3;
  }
  return;
}

//----------------------------------------------------------------------------------------
// Function for transforming 4-vector from Boyer-Lindquist to desired coordinates
// Inputs:
//   a0_bl,a1_bl,a2_bl,a3_bl: upper 4-vector components in Boyer-Lindquist coordinates
//   r,theta,phi: Boyer-Lindquist coordinates of point
// Outputs:
//   pa0,pa1,pa2,pa3: pointers to upper 4-vector components in desired coordinates
// Notes:
//   Schwarzschild coordinates match Boyer-Lindquist when a = 0

void TransformVector(Real a0_bl, Real a1_bl, Real a2_bl, Real a3_bl, Real r,
                     Real theta, Real phi,
                     Real *pa0, Real *pa1, Real *pa2, Real *pa3) {
  if (std::strcmp(COORDINATE_SYSTEM, "schwarzschild") == 0) {
    *pa0 = a0_bl;
    *pa1 = a1_bl;
    *pa2 = a2_bl;
    *pa3 = a3_bl;
  }
  else if (std::strcmp(COORDINATE_SYSTEM, "gr_user") == 0) {
    *pa0 = a0_bl;
    *pa1 = a1_bl;
    *pa2 = a2_bl;
    *pa3 = a3_bl;
  }
  
  return;
}

//----------------------------------------------------------------------------------------
// Function for calculating primitives given radius
// Inputs:
//   r: Schwarzschild radius
//   temp_min,temp_max: bounds on temperature
// Outputs:
//   prho: value set to density
//   ppgas: value set to gas pressure
//   put: value set to u^t in Schwarzschild coordinates
//   pur: value set to u^r in Schwarzschild coordinates
// Notes:
//   references Hawley, Smarr, & Wilson 1984, ApJ 277 296 (HSW)

void CalculatePrimitives(Real r, Real temp_min, Real temp_max, Real *prho,
                         Real *ppgas, Real *put, Real *pur) {

  // Calculate intermediate quantities
  Real shape = r0;
  Real redshift = -aa/r;
  Real dshape = 0;
  Real dredshift =  aa/(r*r);
  // Calculate solution to (HSW 76)
  Real temp_neg_res = TemperatureMin(r, temp_min, temp_max);
  Real temp;
  if (r <= r_crit) {  // use lesser of two roots
    temp = TemperatureBisect(r, temp_min, temp_neg_res);
  } else {  // user greater of two roots
    temp = TemperatureBisect(r, temp_neg_res, temp_max);
  }

  // Calculate primitives
  Real rho = std::pow(temp/k_adi, n_adi);             // not same K as HSW
  Real pgas = temp * rho;
  Real ur = c1 / (SQR(r) * std::pow(temp, n_adi));    // (HSW 75)
  Real ut = (std::pow(temp, -n_adi)*std::exp(-redshift)*std::sqrt(r*r*r*(r-shape)*std::pow(temp, 2*n_adi)+c1*c1))/(std::pow(r, 3/2)*std::sqrt(r-shape));

  // Set primitives
  *prho = rho;
  *ppgas = pgas;
  *put = ut;
  *pur = ur;
  return;
}

//----------------------------------------------------------------------------------------
// Function for finding temperature at which residual is minimized
// Inputs:
//   r: Schwarzschild radius
//   t_min,t_max: bounds between which minimum must occur
// Outputs:
//   returned value: some temperature for which residual of (HSW 76) is negative
// Notes:
//   references Hawley, Smarr, & Wilson 1984, ApJ 277 296 (HSW)
//   performs golden section search (cf. Numerical Recipes, 3rd ed., 10.2)

Real TemperatureMin(Real r, Real t_min, Real t_max) {
  // Parameters
  const Real ratio = 0.3819660112501051;  // (3+\sqrt{5})/2
  const int max_iterations = 30;          // maximum number of iterations

  // Initialize values
  Real t_mid = t_min + ratio * (t_max - t_min);
  Real res_mid = TemperatureResidual(t_mid, r);

  // Apply golden section method
  bool larger_to_right = true;  // flag indicating larger subinterval is on right
  for (int n = 0; n < max_iterations; ++n) {
    if (res_mid < 0.0) {
      return t_mid;
    }
    Real t_new;
    if (larger_to_right) {
      t_new = t_mid + ratio * (t_max - t_mid);
      Real res_new = TemperatureResidual(t_new, r);
      if (res_new < res_mid) {
        t_min = t_mid;
        t_mid = t_new;
        res_mid = res_new;
      } else {
        t_max = t_new;
        larger_to_right = false;
      }
    } else {
      t_new = t_mid - ratio * (t_mid - t_min);
      Real res_new = TemperatureResidual(t_new, r);
      if (res_new < res_mid) {
        t_max = t_mid;
        t_mid = t_new;
        res_mid = res_new;
      } else {
        t_min = t_new;
        larger_to_right = true;
      }
    }
  }
  return NAN;
}

//----------------------------------------------------------------------------------------
// Bisection root finder
// Inputs:
//   r: Schwarzschild radius
//   t_min,t_max: bounds between which root must occur
// Outputs:
//   returned value: temperature that satisfies (HSW 76)
// Notes:
//   references Hawley, Smarr, & Wilson 1984, ApJ 277 296 (HSW)
//   performs bisection search

Real TemperatureBisect(Real r, Real t_min, Real t_max) {
  // Parameters
  const int max_iterations = 20;
  const Real tol_residual = 1.0e-6;
  const Real tol_temperature = 1.0e-6;

  // Find initial residuals
  Real res_min = TemperatureResidual(t_min, r);
  Real res_max = TemperatureResidual(t_max, r);
  if (std::abs(res_min) < tol_residual) {
    return t_min;
  }
  if (std::abs(res_max) < tol_residual) {
    return t_max;
  }
  if ((res_min < 0.0 && res_max < 0.0) || (res_min > 0.0 && res_max > 0.0)) {
    return NAN;
  }

  // Iterate to find root
  Real t_mid;
  for (int i = 0; i < max_iterations; ++i) {
    t_mid = (t_min + t_max) / 2.0;
    if (t_max - t_min < tol_temperature) {
      return t_mid;
    }
    Real res_mid = TemperatureResidual(t_mid, r);
    if (std::abs(res_mid) < tol_residual) {
      return t_mid;
    }
    if ((res_mid < 0.0 && res_min < 0.0) || (res_mid > 0.0 && res_min > 0.0)) {
      t_min = t_mid;
      res_min = res_mid;
    } else {
      t_max = t_mid;
      res_max = res_mid;
    }
  }
  return t_mid;
}

//----------------------------------------------------------------------------------------
// Function whose value vanishes for correct temperature
// Inputs:
//   t: temperature
//   r: Schwarzschild radius
// Outputs:
//   returned value: residual that should vanish for correct temperature
// Notes:
//   implements (76) from Hawley, Smarr, & Wilson 1984, ApJ 277 296

Real TemperatureResidual(Real t, Real r) {
  Real shape = r0;
  Real redshift = -aa/r;
  Real dshape = 0;
  Real dredshift =  aa/(r*r);
  return SQR(1.0 + (n_adi+1.0) * t)
      * (std::exp(-2*redshift) + SQR(c1) / (SQR(SQR(r)) * std::pow(t, 2.0*n_adi))) - c2;
}
} // namespace

