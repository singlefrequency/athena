//======================================================================================
// Athena++ astrophysical MHD code
// Copyright (C) 2014 James M. Stone  <jmstone@princeton.edu>
//
// This program is free software: you can redistribute and/or modify it under the terms
// of the GNU General Public License (GPL) as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
// PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of GNU GPL in the file LICENSE included in the code
// distribution.  If not see <http://www.gnu.org/licenses/>.
//======================================================================================
//! \file radiation.cpp
//  \brief implementation of functions in class Radiation
//======================================================================================


// Athena++ headers
#include "../athena.hpp"
#include "../athena_arrays.hpp" 
#include "radiation.hpp"
#include "../parameter_input.hpp"
#include "../mesh.hpp"

// constructor, initializes data structures and parameters

// The default opacity function.
// Do nothing. Keep the opacity as the initial value
inline void DefaultOpacity(MeshBlock *pmb, AthenaArray<Real> &prim)
{
  
}


Radiation::Radiation(MeshBlock *pmb, ParameterInput *pin)
{
  // read in the parameters
  int nmu = pin->GetInteger("radiation","nmu");
  int angle_flag = pin->GetOrAddInteger("radiation","angle_flag",0);
  prat = pin->GetReal("radiation","Prat");
  crat = pin->GetReal("radiation","Crat");
  reduced_c  = crat * pin->GetOrAddReal("radiation","reduced_factor",1.0);
  nfreq = pin->GetOrAddInteger("radiation","n_frequency",1);
  
  pmy_block = pmb;
  
  // calculate noct based on dimension
  int ndim = 1;
  if(pmb->block_size.nx2 > 1) ndim = 2;
  if(pmb->block_size.nx3 > 1) ndim = 3;
  
  int n1z = pmb->block_size.nx1 + 2*(NGHOST);
  int n2z = 1;
  int n3z = 1;
  
  
  
  int n_ang; // number of angles per octant and number of octant
  // total calculate total number of angles based on dimensions
  if(ndim == 1){
    n_ang = nmu;
    noct = 2;
  }else if(ndim == 2){
    noct = 4;
    n2z = pmb->block_size.nx2 + 2*(NGHOST);
    if(angle_flag == 0){
      n_ang = nmu * (nmu + 1)/2;
    }else if(angle_flag == 10){
      n_ang = nmu;
    }
  }else if(ndim == 3){
    noct = 8;
    n2z = pmb->block_size.nx2 + 2*(NGHOST);
    n3z = pmb->block_size.nx3 + 2*(NGHOST);
    if(angle_flag == 0){
      n_ang = nmu * (nmu + 1)/2;
    }else if(angle_flag == 10){
      n_ang = nmu * nmu/2;
    }
  }// end 3D
  
  nang = n_ang * noct;
  
  
  // allocate arrays
  ir.NewAthenaArray(nfreq,n3z,n2z,n1z,nang);
  ir1.NewAthenaArray(nfreq,n3z,n2z,n1z,nang);
  
  rad_mom.NewAthenaArray(13,n3z,n2z,n1z);
  sigma_s.NewAthenaArray(nfreq,n3z,n2z,n1z);
  sigma_a.NewAthenaArray(nfreq,n3z,n2z,n1z);
  
  mu.NewAthenaArray(3,n3z,n2z,n1z,nang);
  wmu.NewAthenaArray(nang);

  wfreq.NewAthenaArray(nfreq);
  
  AngularGrid(angle_flag, nmu);
  
  // Initialize the frequency weight
  FrequencyGrid();
  
  // set a default opacity function
  UpdateOpacity = DefaultOpacity;



}

// destructor

Radiation::~Radiation()
{
  ir.DeleteAthenaArray();
  ir1.DeleteAthenaArray();
  rad_mom.DeleteAthenaArray();
  sigma_s.DeleteAthenaArray();
  sigma_a.DeleteAthenaArray();
  mu.DeleteAthenaArray();
  wmu.DeleteAthenaArray();
  wfreq.DeleteAthenaArray();
}


//Enrol the function to update opacity

void Radiation::EnrollOpacityFunction(Opacity_t MyOpacityFunction)
{
  UpdateOpacity = MyOpacityFunction;
  
}

// calculate the frequency integrated moments of the radiation field
// including the ghost zones
void Radiation::CalculateMoment()
{
  Real er, frx, fry, frz, prxx, pryy, przz, prxy, prxz, pryz;
  int n1z = pmy_block->block_size.nx1 + 2*(NGHOST);
  int n2z = pmy_block->block_size.nx2;
  int n3z = pmy_block->block_size.nx3;
  if(n2z > 1) n2z += (2*(NGHOST));
  if(n3z > 1) n3z += (2*(NGHOST));
  
  Real *weight = &(wmu(0));
  
  AthenaArray<Real> i_mom;
  
  i_mom.InitWithShallowCopy(rad_mom);

  
  // reset the moment arrays to be zero
  // There are 12 3D arrays
  for(int n=0; n<13; ++n)
    for(int k=0; k<n3z; ++k)
      for(int j=0; j<n2z; ++j)
#pragma simd
        for(int i=0; i<n1z; ++i){
          i_mom(n,k,j,i) = 0.0;
        }
  
  
  for(int ifr=0; ifr<nfreq; ++ifr){
    for(int k=0; k<n3z; ++k){
      for(int j=0; j<n2z; ++j){
        for(int i=0; i<n1z; ++i){
          er=0.0; frx=0.0; fry=0.0; frz=0.0;
          prxx=0.0; pryy=0.0; przz=0.0; prxy=0.0;
          prxz=0.0; pryz=0.0;
          Real *intensity = &(ir(ifr,k,j,i,0));
          Real *cosx = &(mu(0,k,j,i,0));
          Real *cosy = &(mu(1,k,j,i,0));
          Real *cosz = &(mu(2,k,j,i,0));
#pragma simd
          for(int n=0; n<nang; ++n){
            er   += weight[n] * intensity[n];
            frx  += weight[n] * intensity[n] * cosx[n];
            fry  += weight[n] * intensity[n] * cosy[n];
            frz  += weight[n] * intensity[n] * cosz[n];
            prxx += weight[n] * intensity[n] * cosx[n] * cosx[n];
            pryy += weight[n] * intensity[n] * cosy[n] * cosy[n];
            przz += weight[n] * intensity[n] * cosz[n] * cosz[n];
            prxy += weight[n] * intensity[n] * cosx[n] * cosy[n];
            prxz += weight[n] * intensity[n] * cosx[n] * cosz[n];
            pryz += weight[n] * intensity[n] * cosy[n] * cosz[n];
          }
          //multiply the frequency weight
          er *= wfreq(ifr);
          frx *= wfreq(ifr);
          fry *= wfreq(ifr);
          frz *= wfreq(ifr);
          prxx *= wfreq(ifr);
          pryy *= wfreq(ifr);
          przz *= wfreq(ifr);
          prxy *= wfreq(ifr);
          prxz *= wfreq(ifr);
          pryz *= wfreq(ifr);
          //assign the moments
          i_mom(ER,k,j,i) += er * 4.0 * PI;
          i_mom(FR1,k,j,i) += frx * 4.0 * PI;
          i_mom(FR2,k,j,i) += fry * 4.0 * PI;
          i_mom(FR3,k,j,i) += frz * 4.0 * PI;
          i_mom(PR11,k,j,i) += prxx * 4.0 * PI;
          i_mom(PR12,k,j,i) += prxy * 4.0 * PI;
          i_mom(PR13,k,j,i) += prxz * 4.0 * PI;
          i_mom(PR21,k,j,i) += prxy * 4.0 * PI;
          i_mom(PR22,k,j,i) += pryy * 4.0 * PI;
          i_mom(PR23,k,j,i) += pryz * 4.0 * PI;
          i_mom(PR31,k,j,i) += prxz * 4.0 * PI;
          i_mom(PR32,k,j,i) += pryz * 4.0 * PI;
          i_mom(PR33,k,j,i) += przz * 4.0 * PI;
          
        }
      }
    }
    
  }
  
  
}


