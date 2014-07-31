#include "DiagnosticScalar.h"

#include <string>
#include <iomanip>

#include "PicParams.h"
#include "DiagParams.h"
#include "SmileiMPI.h"
#include "ElectroMagn.h"

using namespace std;

// constructor
DiagnosticScalar::DiagnosticScalar(PicParams* params, DiagParams* diagParams, SmileiMPI* smpi) :
every(diagParams->scalar_every),
smpi_(smpi)
{
    smpi_=smpi;
    if (smpi_->isMaster()) {
        fout.open("scalars.txt");
        if (!fout.is_open()) ERROR("can't open scalar file");
    }
}

void DiagnosticScalar::close() {
    if (smpi_->isMaster()) {
        fout.close();
    }
}

// wrapper of the methods
void DiagnosticScalar::run(int timestep, ElectroMagn* EMfields, vector<Species*>& vecSpecies) {
    EMfields->computePoynting(smpi_);
    if (every && timestep % every == 0) {
        compute_proc_gather(EMfields,vecSpecies);
        compute();
        write(timestep);
    }
}


// it contains all to manage the communication of data. It is "transparent" to the user.
void DiagnosticScalar::compute_proc_gather (ElectroMagn* EMfields, vector<Species*>& vecSpecies) {
    // 	it fills the map on each specie
    for (unsigned int ispec=0; ispec<vecSpecies.size(); ispec++) {
        vecSpecies[ispec]->computeScalars();
    }
    // definition of the memory allocation vector
    vector<double> oneProc;
    for (unsigned int ispec=0; ispec<vecSpecies.size(); ispec++) {
        for (map<string, double>::iterator iter = vecSpecies[ispec]->scalars.begin(); iter != vecSpecies[ispec]->scalars.end(); iter++) {
            oneProc.push_back(iter->second);
        }
    }


    EMfields->computeScalars();
    
    for (map<string,map<string,vector<double> > >::iterator iterEM=EMfields->scalars.begin(); iterEM!=EMfields->scalars.end(); iterEM++) {
        for (map<string,vector<double> >::iterator iterMap=iterEM->second.begin(); iterMap!=iterEM->second.end(); iterMap++ ) {

            for (unsigned int i=0; i<iterMap->second.size(); i++) {
//				DEBUG(iterEM->first << " " << iterMap->first << " " << iterMap->second[i]);
                oneProc.push_back(iterMap->second[i]);
            }
        }
    }

    // 	it constructs the receiving structure on the master processor
    vector<double> allProcs;
    if(smpi_->isMaster()) {
        allProcs.resize(smpi_->getSize()*oneProc.size());
    }
    // gathering chars of the master processor
    MPI_Gather(&oneProc[0],oneProc.size(),MPI_DOUBLE,&allProcs[0],oneProc.size(),MPI_DOUBLE,0,MPI_COMM_WORLD);

    smpi_->barrier();

    // 	method to reconstruct the information on the master processor
    if(smpi_->isMaster()) {
        unsigned int count=0;
        mpi_spec_scalars.resize(smpi_->getSize());
        mpi_EM_scalars.resize(smpi_->getSize());
        for(int iCPU=0; iCPU<smpi_->getSize(); iCPU++) {
            mpi_spec_scalars[iCPU].resize(vecSpecies.size());
            for (unsigned int ispec=0; ispec<vecSpecies.size(); ispec++) {
                for (map<string, double>::iterator iter = vecSpecies[ispec]->scalars.begin(); iter != vecSpecies[ispec]->scalars.end(); ++iter) {
                    mpi_spec_scalars[iCPU][ispec][iter->first] = allProcs[count];
                    count++;
                }
            }

            for (map<string,map<string,vector<double> > >::iterator iterEM=EMfields->scalars.begin(); iterEM!=EMfields->scalars.end(); iterEM++) {
                for (map<string,vector<double> >::iterator iterMap=iterEM->second.begin(); iterMap!=iterEM->second.end(); iterMap++ ) {
                    mpi_EM_scalars[iCPU][iterEM->first][iterMap->first].resize(iterMap->second.size());

                    for (unsigned int i=0; i<iterMap->second.size(); i++) {
                        mpi_EM_scalars[iCPU][iterEM->first][iterMap->first][i]=allProcs[count];
                        count++;
                    }
                }
            }
        }
        if (count!=allProcs.size()) ERROR("problem here " << count << " != " << allProcs.size());
    }
}

// Each scalar diagnostic should be calculated here
void DiagnosticScalar::compute() {
    if(smpi_->isMaster()) {
        out_list.clear();
        for(unsigned int ispec=0; ispec<mpi_spec_scalars[0].size(); ++ispec) {
            double charge_tot=0;
            unsigned int part_tot=0;
            double ener_tot=0;
            for(int iCPU=0; iCPU<smpi_->getSize(); iCPU++) {
                charge_tot+=mpi_spec_scalars[iCPU][ispec]["charge_tot"];
                part_tot+=mpi_spec_scalars[iCPU][ispec]["part_number"];
                ener_tot+=mpi_spec_scalars[iCPU][ispec]["energy_tot"];

            }
            if (part_tot) charge_tot/=part_tot;
			ostringstream name(""); 
			name << ispec;
			
            out_list.push_back(make_pair("charge_tot-"+name.str(),charge_tot));
            out_list.push_back(make_pair("part_tot-"+name.str(),part_tot));
            out_list.push_back(make_pair("energy_tot-"+name.str(),ener_tot));
        }

        for (map<string,map<string,vector<double> > >::iterator iterEM=mpi_EM_scalars[0].begin(); iterEM!=mpi_EM_scalars[0].end(); iterEM++) {
            string nameEm=iterEM->first;
            for (map<string,vector<double> >::iterator iterMap=iterEM->second.begin(); iterMap!=iterEM->second.end(); iterMap++ ) {
                string nameType=iterMap->first;

                unsigned iCPUval=0;
                if (nameType=="min") {
                    double val=mpi_EM_scalars[iCPUval][nameEm][nameType][0];
                    unsigned int ival=mpi_EM_scalars[iCPUval][nameEm][nameType][1];
                    for(int iCPU=0; iCPU<smpi_->getSize(); iCPU++) {
                        if (mpi_EM_scalars[iCPU][nameEm][nameType][0]<val) {
                            iCPUval=iCPU;
                            val=mpi_EM_scalars[iCPUval][nameEm][nameType][0];
                            ival=mpi_EM_scalars[iCPUval][nameEm][nameType][1];
                        }
                    }
                    out_list.push_back(make_pair(nameEm+"_"+nameType,val));
//                    out_list.push_back(make_pair(nameEm+"_"+nameType+"_i",ival));
//                    out_list.push_back(make_pair(nameEm+"_"+nameType+"_cpu",iCPUval));
                } else if (nameType=="max") {
                    double val=mpi_EM_scalars[iCPUval][nameEm][nameType][0];
                    unsigned int ival=mpi_EM_scalars[iCPUval][nameEm][nameType][1];
                    for(int iCPU=0; iCPU<smpi_->getSize(); iCPU++) {
                        if (mpi_EM_scalars[iCPU][nameEm][nameType][0]>val) {
                            iCPUval=iCPU;
                            val=mpi_EM_scalars[iCPUval][nameEm][nameType][0];
                            ival=mpi_EM_scalars[iCPUval][nameEm][nameType][1];
                        }
                    }
                    out_list.push_back(make_pair(nameEm+"_"+nameType,val));
//                    out_list.push_back(make_pair(nameEm+"_"+nameType+"_i",ival));
//                    out_list.push_back(make_pair(nameEm+"_"+nameType+"_cpu",iCPUval));
                } else if (nameType=="sum") {
                    double val=0;
                    for(int iCPU=0; iCPU<smpi_->getSize(); iCPU++) {
                        val+=mpi_EM_scalars[iCPU][nameEm][nameType][0];
                    }
                    out_list.push_back(make_pair(nameEm+"_"+nameType,val));
                } else {
                    DEBUG("Don't know what to do with " << nameType);
                }
            }

        }



    }
}

void DiagnosticScalar::write(int itime) {
    const unsigned int precision=10;
    if(smpi_->isMaster()) {
        fout << std::scientific;
        fout.precision(precision);
        if (fout.tellp()==ifstream::pos_type(0)) {
            fout << "# " << 1 << " time" << endl;
            unsigned int i=2;
            for(vector<pair<string,double> >::iterator iter = out_list.begin(); iter !=out_list.end(); iter++) {
                fout << "# " << i << " " << (*iter).first << endl;
                i++;
            }

            fout << "#\n#" << setw(precision+8) << "time";
            for(vector<pair<string,double> >::iterator iter = out_list.begin(); iter !=out_list.end(); iter++) {
                fout << setw(precision+8) << (*iter).first;
            }
            fout << endl;
        }
        fout << setw(precision+8) << itime;
        for(vector<pair<string,double> >::iterator iter = out_list.begin(); iter !=out_list.end(); iter++) {
            fout << setw(precision+8) << (*iter).second;
        }
        fout << endl;
    }
}
