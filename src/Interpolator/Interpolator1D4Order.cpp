#include "Interpolator1D4Order.h"

#include <cmath>
#include <iostream>

#include "ElectroMagn.h"
#include "Field1D.h"
#include "Particles.h"

using namespace std;

Interpolator1D4Order::Interpolator1D4Order(Params &params, Patch *patch) : Interpolator1D(params, patch)
{
    dx_inv_ = 1.0/params.cell_length[0];
    
    //double defined for use in coefficients
    dble_1_ov_384 = 1.0/384.0;
    dble_1_ov_48 = 1.0/48.0;
    dble_1_ov_16 = 1.0/16.0;
    dble_1_ov_12 = 1.0/12.0;
    dble_1_ov_24 = 1.0/24.0;
    dble_19_ov_96 = 19.0/96.0;
    dble_11_ov_24 = 11.0/24.0;
    dble_1_ov_4 = 1.0/4.0;
    dble_1_ov_6 = 1.0/6.0;
    dble_115_ov_192 = 115.0/192.0;
    dble_5_ov_8 = 5.0/8.0;

}

// ---------------------------------------------------------------------------------------------------------------------
// 2nd Order Interpolation of the fields at a the particle position (3 nodes are used)
// ---------------------------------------------------------------------------------------------------------------------
void Interpolator1D4Order::operator() (ElectroMagn* EMfields, Particles &particles, int ipart, int nparts, double* ELoc, double* BLoc)
{
    
    // Variable declaration
    double xjn, xjmxi2, xjmxi3, xjmxi4;
    
    // Static cast of the electromagnetic fields
    Field1D* Ex1D     = static_cast<Field1D*>(EMfields->Ex_);
    Field1D* Ey1D     = static_cast<Field1D*>(EMfields->Ey_);
    Field1D* Ez1D     = static_cast<Field1D*>(EMfields->Ez_);
    Field1D* Bx1D_m   = static_cast<Field1D*>(EMfields->Bx_m);
    Field1D* By1D_m   = static_cast<Field1D*>(EMfields->By_m);
    Field1D* Bz1D_m   = static_cast<Field1D*>(EMfields->Bz_m);
    
    
    // Particle position (in units of the spatial-step)
    xjn    = particles.position(0, ipart)*dx_inv_;
    
    // --------------------------------------------------------
    // Interpolate the fields from the Dual grid : Ex, By, Bz
    // --------------------------------------------------------
    id_      = round(xjn+0.5);  // index of the central point
    xjmxi  = xjn -(double)id_+0.5;  // normalized distance to the central node
    xjmxi2 = xjmxi*xjmxi;     // square of the normalized distance to the central node
    xjmxi3 = xjmxi2*xjmxi;    // cube of the normalized distance to the central node
    xjmxi4 = xjmxi3*xjmxi;    // 4th power of the normalized distance to the central node
    
    // coefficients for the 4th order interpolation on 5 nodes
    coeffd_[0] = dble_1_ov_384   - dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 - dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    coeffd_[1] = dble_19_ov_96   - dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  + dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffd_[2] = dble_115_ov_192 - dble_5_ov_8   * xjmxi2 + dble_1_ov_4 * xjmxi4;
    coeffd_[3] = dble_19_ov_96   + dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  - dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffd_[4] = dble_1_ov_384   + dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 + dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    
    id_ -= index_domain_begin;
    
    *(ELoc+0*nparts) = compute(coeffd_, Ex1D,   id_);  
    *(BLoc+1*nparts) = compute(coeffd_, By1D_m, id_);  
    *(BLoc+2*nparts) = compute(coeffd_, Bz1D_m, id_);  
    
    // --------------------------------------------------------
    // Interpolate the fields from the Primal grid : Ey, Ez, Bx
    // --------------------------------------------------------
    ip_      = round(xjn);      // index of the central point
    xjmxi  = xjn -(double)ip_;  // normalized distance to the central node
    xjmxi2 = xjmxi*xjmxi;     // square of the normalized distance to the central node
    xjmxi3 = xjmxi2*xjmxi;    // cube of the normalized distance to the central node
    xjmxi4 = xjmxi3*xjmxi;    // 4th power of the normalized distance to the central node
    
    // coefficients for the 4th order interpolation on 5 nodes
    coeffp_[0] = dble_1_ov_384   - dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 - dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    coeffp_[1] = dble_19_ov_96   - dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  + dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffp_[2] = dble_115_ov_192 - dble_5_ov_8   * xjmxi2 + dble_1_ov_4 * xjmxi4;
    coeffp_[3] = dble_19_ov_96   + dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  - dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffp_[4] = dble_1_ov_384   + dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 + dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    
    ip_ -= index_domain_begin;
    
    *(ELoc+1*nparts) = compute(coeffp_, Ey1D,   ip_);  
    *(ELoc+2*nparts) = compute(coeffp_, Ez1D,   ip_);  
    *(BLoc+0*nparts) = compute(coeffp_, Bx1D_m, ip_);



}//END Interpolator1D4Order

void Interpolator1D4Order::operator() (ElectroMagn* EMfields, Particles &particles, int ipart, LocalFields* ELoc, LocalFields* BLoc, LocalFields* JLoc, double* RhoLoc){
    
    // Interpolate E, B
    // Compute coefficient for ipart position    (*this)(EMfields, particles, ipart, ELoc, BLoc);
   // Variable declaration
    double xjn, xjmxi2, xjmxi3, xjmxi4;
    
    // Static cast of the electromagnetic fields
    Field1D* Ex1D     = static_cast<Field1D*>(EMfields->Ex_);
    Field1D* Ey1D     = static_cast<Field1D*>(EMfields->Ey_);
    Field1D* Ez1D     = static_cast<Field1D*>(EMfields->Ez_);
    Field1D* Bx1D_m   = static_cast<Field1D*>(EMfields->Bx_m);
    Field1D* By1D_m   = static_cast<Field1D*>(EMfields->By_m);
    Field1D* Bz1D_m   = static_cast<Field1D*>(EMfields->Bz_m);
    
    
    // Particle position (in units of the spatial-step)
    xjn    = particles.position(0, ipart)*dx_inv_;
    
    // --------------------------------------------------------
    // Interpolate the fields from the Dual grid : Ex, By, Bz
    // --------------------------------------------------------
    id_      = round(xjn+0.5);  // index of the central point
    xjmxi  = xjn -(double)id_+0.5;  // normalized distance to the central node
    xjmxi2 = xjmxi*xjmxi;     // square of the normalized distance to the central node
    xjmxi3 = xjmxi2*xjmxi;    // cube of the normalized distance to the central node
    xjmxi4 = xjmxi3*xjmxi;    // 4th power of the normalized distance to the central node
    
    // coefficients for the 4th order interpolation on 5 nodes
    coeffd_[0] = dble_1_ov_384   - dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 - dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    coeffd_[1] = dble_19_ov_96   - dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  + dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffd_[2] = dble_115_ov_192 - dble_5_ov_8   * xjmxi2 + dble_1_ov_4 * xjmxi4;
    coeffd_[3] = dble_19_ov_96   + dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  - dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffd_[4] = dble_1_ov_384   + dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 + dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    
    id_ -= index_domain_begin;
    
    (*ELoc).x = compute(coeffd_, Ex1D,   id_);  
    (*BLoc).y = compute(coeffd_, By1D_m, id_);  
    (*BLoc).z = compute(coeffd_, Bz1D_m, id_);  
    
    // --------------------------------------------------------
    // Interpolate the fields from the Primal grid : Ey, Ez, Bx
    // --------------------------------------------------------
    ip_      = round(xjn);      // index of the central point
    xjmxi  = xjn -(double)ip_;  // normalized distance to the central node
    xjmxi2 = xjmxi*xjmxi;     // square of the normalized distance to the central node
    xjmxi3 = xjmxi2*xjmxi;    // cube of the normalized distance to the central node
    xjmxi4 = xjmxi3*xjmxi;    // 4th power of the normalized distance to the central node
    
    // coefficients for the 4th order interpolation on 5 nodes
    coeffp_[0] = dble_1_ov_384   - dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 - dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    coeffp_[1] = dble_19_ov_96   - dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  + dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffp_[2] = dble_115_ov_192 - dble_5_ov_8   * xjmxi2 + dble_1_ov_4 * xjmxi4;
    coeffp_[3] = dble_19_ov_96   + dble_11_ov_24 * xjmxi  + dble_1_ov_4 * xjmxi2  - dble_1_ov_6  * xjmxi3 - dble_1_ov_6  * xjmxi4;
    coeffp_[4] = dble_1_ov_384   + dble_1_ov_48  * xjmxi  + dble_1_ov_16 * xjmxi2 + dble_1_ov_12 * xjmxi3 + dble_1_ov_24 * xjmxi4;
    
    ip_ -= index_domain_begin;
    
    (*ELoc).y = compute(coeffp_, Ey1D,   ip_);  
    (*ELoc).z = compute(coeffp_, Ez1D,   ip_);  
    (*BLoc).x = compute(coeffp_, Bx1D_m, ip_);
    
    // Static cast of the electromagnetic fields
    Field1D* Jx1D     = static_cast<Field1D*>(EMfields->Jx_);
    Field1D* Jy1D     = static_cast<Field1D*>(EMfields->Jy_);
    Field1D* Jz1D     = static_cast<Field1D*>(EMfields->Jz_);
    Field1D* Rho1D    = static_cast<Field1D*>(EMfields->rho_);
    
    // --------------------------------------------------------
    // Interpolate the fields from the Primal grid : Jy, Jz, Rho
    // --------------------------------------------------------
    (*JLoc).y = compute(coeffp_, Jy1D,  ip_);  
    (*JLoc).z = compute(coeffp_, Jz1D,  ip_);  
    (*RhoLoc) = compute(coeffp_, Rho1D, ip_);     
    
    // --------------------------------------------------------
    // Interpolate the fields from the Dual grid : Jx
    // --------------------------------------------------------
    (*JLoc).x = compute(coeffd_, Jx1D,  id_);  
    
}
void Interpolator1D4Order::operator() (ElectroMagn* EMfields, Particles &particles, SmileiMPI* smpi, int *istart, int *iend, int ithread)
{
    std::vector<double> *Epart = &(smpi->dynamics_Epart[ithread]);
    std::vector<double> *Bpart = &(smpi->dynamics_Bpart[ithread]);
    std::vector<int> *iold = &(smpi->dynamics_iold[ithread]);
    std::vector<double> *delta = &(smpi->dynamics_deltaold[ithread]);
    
    //Loop on bin particles
    int npart_tot = particles.size();
    for (int ipart=*istart ; ipart<*iend; ipart++ ) {
        //Interpolation on current particle
        (*this)(EMfields, particles, ipart, npart_tot, &(*Epart)[ipart], &(*Bpart)[ipart]);
        //Buffering of iol and delta
        (*iold)[ipart] = ip_;
        (*delta)[ipart] = xjmxi;
    }

}


// Interpolator specific to tracked particles. A selection of particles may be provided
void Interpolator1D4Order::operator() (ElectroMagn* EMfields, Particles &particles, double *buffer, int offset, vector<unsigned int> * selection)
{
    if( selection ) {
        
        int nsel_tot = selection->size();
        for (int isel=0 ; isel<nsel_tot; isel++ ) {
            (*this)(EMfields, particles, (*selection)[isel], offset, buffer+isel, buffer+isel+3*offset);
        }
        
    } else {
        
        int npart_tot = particles.size();
        for (int ipart=0 ; ipart<npart_tot; ipart++ ) {
            (*this)(EMfields, particles, ipart, offset, buffer+ipart, buffer+ipart+3*offset);
        }
        
    }
}


