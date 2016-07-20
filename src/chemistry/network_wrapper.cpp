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
//! \file network_wrapper.cpp
//  \brief implementation of functions in class NetworkWrapper 
//======================================================================================

// this class header
#include "network/network.hpp"

NetworkWrapper::NetworkWrapper() {}

NetworkWrapper::~NetworkWrapper() {}

int NetworkWrapper::WrapJacobian(const long int n, const realtype t,
                          const N_Vector y, const N_Vector fy, 
                          DlsMat jac, void *user_data,
                          N_Vector tmp1, N_Vector tmp2, N_Vector tmp3) {
  Real t1 = t;
  //copy y and fy values
  Real y1[NSPECIES];
  Real fy1[NSPECIES];
  for (int i=0; i<NSPECIES; i++) {
		y1[i] = NV_Ith_S(y, i);
		fy1[i] = NV_Ith_S(fy, i);
  }
  //temporary storage for return
  Real jac1[NSPECIES][NSPECIES];
  Real tmp11[NSPECIES];
  Real tmp21[NSPECIES];
  Real tmp31[NSPECIES];
  NetworkWrapper *meptr = (NetworkWrapper*) user_data;
  meptr->Jacobian(t1, y1, fy1, jac1, tmp11, tmp21, tmp31);
  //set J, tmp1, tmp2, tmp3 to return
  for (int i=0; i<NSPECIES; i++) {
		NV_Ith_S(tmp1, i) = tmp11[i];
		NV_Ith_S(tmp2, i) = tmp21[i];
		NV_Ith_S(tmp3, i) = tmp31[i];
  }
  for (int i=0; i<NSPECIES; i++) {
    for (int j=0; j<NSPECIES; j++) {
      DENSE_ELEM(jac, i, j) = jac1[i][j];
    }
  }
  return 0;
}

int NetworkWrapper::WrapRHS(const realtype t, const N_Vector y,
                     N_Vector ydot, void *user_data) {
  Real t1 = t;
  Real y1[NSPECIES];
  //set y1 to y
  for (int i=0; i<NSPECIES; i++) {
		y1[i] = NV_Ith_S(y, i);
  }
  //temporary storage for return
  Real ydot1[NSPECIES];
  NetworkWrapper *meptr = (NetworkWrapper*) user_data;
  meptr->RHS(t1, y1, ydot1);
  //set ydot to return
  for (int i=0; i<NSPECIES; i++) {
		NV_Ith_S(ydot, i) = ydot1[i];
  }
  return 0;
}