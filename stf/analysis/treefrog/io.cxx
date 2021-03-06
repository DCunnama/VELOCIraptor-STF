/*! \file io.cxx
 *  \brief this file contains routines for io
 */

#include "TreeFrog.h"

///\name Reading routines
//@{
///Reads number of halos at each snapshot, useful for mpi decomposition
#ifdef USEMPI
Int_t ReadNumberofHalos(Options &opt, Int_t *numhalos)
{
    fstream Fin;//file is list of halo data files
    string *buf=new string[opt.numsnapshots];
    Int_t tothalos=0;

    Fin.open(opt.fname);
    if (!Fin.is_open()) {
        cerr<<"file containing snapshot list can't be opened"<<endl;
#ifdef USEMPI
        MPI_Abort(MPI_COMM_WORLD,9);
#else
        exit(9);
#endif
    }
    for(int i=0; i<opt.numsnapshots; i++) 
    {
        Fin>>buf[i];
            //if (opt.ioformat==DSUSSING) HaloTree[i].Halo=ReadHaloData(&buf[i*1000],HaloTree[i].numhalos);
            //else if (opt.ioformat==DCATALOG) 
                numhalos[i]=MPIReadHaloGroupCatalogDataNum(buf[i],opt.nmpifiles, opt.ibinary,opt.ifield, opt.itypematch);
            //else if (opt.ioformat==DNIFTY) HaloTree[i].Halo=ReadNIFTYData(&buf[i*1000],HaloTree[i].numhalos, opt.idcorrectflag);
            tothalos+=numhalos[i];
    }
    Fin.close();
    delete[] buf;
    return tothalos;
}

Int_t ReadNumberofParticlesInHalos(Options &opt, Int_t *numpartinhalos)
{
    fstream Fin;//file is list of halo data files
    string *buf=new string[opt.numsnapshots];
    Int_t tothalos=0,totpart=0;

    Fin.open(opt.fname);
    if (!Fin.is_open()) {
        cerr<<"file containing snapshot list can't be opened"<<endl;
#ifdef USEMPI
        MPI_Abort(MPI_COMM_WORLD,9);
#else
        exit(9);
#endif
    }
    for(int i=0; i<opt.numsnapshots; i++) 
    {
        Fin>>buf[i];
            //if (opt.ioformat==DSUSSING) HaloTree[i].Halo=ReadHaloData(&buf[i*1000],HaloTree[i].numhalos);
            //else if (opt.ioformat==DCATALOG) 
            numpartinhalos[i]=MPIReadHaloGroupCatalogDataParticleNum(buf[i],opt.nmpifiles, opt.ibinary,opt.ifield, opt.itypematch);
            //else if (opt.ioformat==DNIFTY) HaloTree[i].Halo=ReadNIFTYData(&buf[i*1000],HaloTree[i].numhalos, opt.idcorrectflag);
            //tothalos+=numhalos[i];
            totpart+=numpartinhalos[i];
    }
    Fin.close();
    delete[] buf;
    return totpart;
}
#endif


///Read data from a number of snapshot files
///\todo need to figure out the best way to optimize the openmp reading as it can be unstable as it stands
HaloTreeData *ReadData(Options &opt)
{
    HaloTreeData *HaloTree;
    fstream Fin;//file is list of halo data files
    long unsigned j,nparts,haloid;
    HaloTree=new HaloTreeData[opt.numsnapshots];
    string *buf=new string[opt.numsnapshots];
    Int_t tothalos=0;
    Double_t t0;
    t0=MyGetTime();
#ifdef USEMPI
    Int_t mpi_tothalos;
#endif
    int i,nthreads;
    nthreads=1;
#ifdef USEOPENMP
#pragma omp parallel 
    {
            if (omp_get_thread_num()==0) nthreads=omp_get_num_threads();
    }
#endif

#ifdef USEMPI
    if (ThisTask==0) {
        Fin.open(opt.fname);
        if (!Fin.is_open()) {
            cerr<<"file containing snapshot list can't be opened"<<endl;
            MPI_Abort(MPI_COMM_WORLD,9);
        }
        Fin.close();
    }
    MPI_Barrier(MPI_COMM_WORLD);
    for (int itask=0;itask<NProcs;itask++) {
        if (ThisTask==itask) {
            Fin.open(opt.fname);
            for(i=0; i<opt.numsnapshots; i++) Fin>>buf[i];
            Fin.close();
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
    if (opt.iverbose==1&&ThisTask==0) cout<<"Reading data"<<endl;
#else
    Fin.open(opt.fname);
    if (!Fin.is_open()) {
        cerr<<"file containing snapshot list can't be opened"<<endl;
        exit(9);
    }
    for(i=0; i<opt.numsnapshots; i++) Fin>>buf[i];
    Fin.close();
    if (opt.iverbose==1) cout<<"Reading data"<<endl;
#endif

#if (defined(USEOPENMP) && !defined(USEMPI))
#pragma omp parallel default(shared) \
private(i)
{
#pragma omp for reduction(+:tothalos)
#endif
    for(i=0; i<opt.numsnapshots; i++)
    {
#ifdef USEMPI
        //if mpi only read relavant data
        if (i>=StartSnap && i<EndSnap) {
#endif
            if (opt.ioformat==DSUSSING) HaloTree[i].Halo=ReadHaloData(buf[i],HaloTree[i].numhalos);
            else if (opt.ioformat==DCATALOG) HaloTree[i].Halo=ReadHaloGroupCatalogData(buf[i],HaloTree[i].numhalos, opt.nmpifiles, opt.ibinary,opt.ifield, opt.itypematch,opt.iverbose);
            else if (opt.ioformat==DNIFTY) HaloTree[i].Halo=ReadNIFTYData(buf[i],HaloTree[i].numhalos, opt.idcorrectflag);
            else if (opt.ioformat==DVOID) HaloTree[i].Halo=ReadVoidData(buf[i],HaloTree[i].numhalos, opt.idcorrectflag);
#ifdef USEMPI
            //if mpi then there is data overlap so only add to total if no overlap
            if (ThisTask<NProcs-1 && NProcs>1) {
                if (i<EndSnap-opt.numsteps) tothalos+=HaloTree[i].numhalos;
            }
            else tothalos+=HaloTree[i].numhalos;
#else
            tothalos+=HaloTree[i].numhalos;
#endif


#ifdef USEMPI
        }
#endif

    }
#if (defined(USEOPENMP) && !defined(USEMPI))
}
#endif

#ifdef USEMPI
    cout<<ThisTask<<" has ---- "<<tothalos<<endl;
    mpi_tothalos=tothalos;
    MPI_Allreduce(&mpi_tothalos, &tothalos, 1, MPI_Int_t, MPI_SUM, MPI_COMM_WORLD);
    if (ThisTask==0) cout<<" all tasks have "<<tothalos<<endl;
#endif
    opt.TotalNumberofHalos=tothalos;
    if (opt.iverbose==1) cout<<"Finished reading data "<<MyGetTime()-t0<<endl;

    return HaloTree;
}
//@}

///\name Write routines
//@{
void WriteHaloMergerTree(Options &opt, ProgenitorData **p, HaloTreeData *h) {
    fstream Fout;
    char fname[1000];
    int istep;
    sprintf(fname,"%s",opt.outname);
    Double_t time1=MyGetTime();
#ifndef USEMPI
    int ThisTask=0, NProcs=1;
#endif
    if (ThisTask==0) cout<<"Writing to "<<fname<<endl;
    if (NProcs>1) {
#ifdef USEMPI
    //now if mpi then last task writes header 
    //all tasks starting from last and moves backwards till it reaches its
    //startpoint+numofsteps used to produce links
    //save first task which goes all the way to its StartSnap
    int istart,iend;
    iend=EndSnap;
    istart=StartSnap+opt.numsteps;
    if (ThisTask==0) istart=0;
    for (int itask=NProcs-1;itask>=0;itask--) {
        if (ThisTask==itask) {
            if (ThisTask==NProcs-1) {
                Fout.open(fname,ios::out);
                Fout<<opt.numsnapshots<<endl;
                Fout<<opt.description<<endl;
                Fout<<opt.TotalNumberofHalos<<endl;
            }
            else {
                Fout.open(fname,ios::out | ios::app);
            }
            if (opt.iverbose)cout<<ThisTask<<" starting to write "<<fname<<" for "<<iend<<" down to "<<istart<<flush<<endl;
            if (opt.outputformat==0) {
            for (int i=opt.numsnapshots-1;i>0;i--) if (i>=istart && i<iend) {
                Fout<<i+opt.snapshotvaloffset<<"\t"<<h[i].numhalos<<endl;
                for (int j=0;j<h[i].numhalos;j++) {
                    Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<endl;
                    for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                        Fout<<h[i-p[i][j].istep].Halo[p[i][j].ProgenitorList[k]-1].haloID<<endl;
                    }
                }
            }
            }
            else {
            for (int i=opt.numsnapshots-1;i>0;i--) if (i>=istart && i<iend) {
                Fout<<i+opt.snapshotvaloffset<<"\t"<<h[i].numhalos<<endl;
                for (int j=0;j<h[i].numhalos;j++) {
                    Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<endl;
                    for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                        Fout<<h[i-p[i][j].istep].Halo[p[i][j].ProgenitorList[k]-1].haloID<<" "<<p[i][j].Merit[k]<<endl;
                    }
                }
            }
            }
            Fout.close();
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
    if (ThisTask==0) {
    Fout.open(fname,ios::out | ios::app);
    ///last file has no connections
    Fout<<0+opt.snapshotvaloffset<<"\t"<<h[0].numhalos<<endl;
    for (int j=0;j<h[0].numhalos;j++) {
        Fout<<h[0].Halo[j].haloID<<"\t"<<0<<endl;
    }
    Fout<<"END"<<endl;
    Fout.close();
    }
#endif
    }
    else{
    Fout.open(fname,ios::out);
    Fout<<opt.numsnapshots<<endl;
    Fout<<opt.description<<endl;
    Fout<<opt.TotalNumberofHalos<<endl;
    if (opt.outputformat==0) {
    for (int i=opt.numsnapshots-1;i>0;i--) {
        Fout<<i+opt.snapshotvaloffset<<"\t"<<h[i].numhalos<<endl;
        for (int j=0;j<h[i].numhalos;j++) {
            Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<endl;
            for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                Fout<<h[i-p[i][j].istep].Halo[p[i][j].ProgenitorList[k]-1].haloID<<endl;
            }
        }
    }
    }
    else {
    for (int i=opt.numsnapshots-1;i>0;i--) {
        Fout<<i+opt.snapshotvaloffset<<"\t"<<h[i].numhalos<<endl;
        for (int j=0;j<h[i].numhalos;j++) {
            Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<endl;
            for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                Fout<<h[i-p[i][j].istep].Halo[p[i][j].ProgenitorList[k]-1].haloID<<" "<<p[i][j].Merit[k]<<endl;
            }
        }
    }
    }
    Fout<<0+opt.snapshotvaloffset<<"\t"<<h[0].numhalos<<endl;
    ///last file has no connections
    for (int j=0;j<h[0].numhalos;j++) {
        Fout<<h[0].Halo[j].haloID<<"\t"<<0<<endl;
    }
    Fout<<"END"<<endl;
    Fout.close();
    }
    if (ThisTask==0) cout<<"Done writing to "<<fname<<" "<<MyGetTime()-time1<<endl;
}

void WriteHaloGraph(Options &opt, ProgenitorData **p, DescendantData **d, HaloTreeData *h) {
    fstream Fout;
    char fname[1000];
    sprintf(fname,"%s",opt.outname);
    cout<<"saving halo merger tree to "<<fname<<endl;
    Fout.open(fname,ios::out);
    Fout<<opt.numsnapshots<<endl;
    Fout<<opt.description<<endl;
    Fout<<opt.TotalNumberofHalos<<endl;
    if (opt.outputformat==0) {
    for (int i=opt.numsnapshots-1;i>=0;i--) {
        Fout<<i+opt.snapshotvaloffset<<"\t"<<h[i].numhalos<<endl;
        for (int j=0;j<h[i].numhalos;j++) {
            Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<"\t"<<d[i][j].NumberofDescendants<<endl;
            for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                Fout<<h[i-p[i][j].istep].Halo[p[i][j].ProgenitorList[k]-1].haloID<<endl;
            }
            for (int k=0;k<d[i][j].NumberofDescendants;k++) {
                Fout<<h[i+d[i][j].istep].Halo[d[i][j].DescendantList[k]-1].haloID<<endl;
            }
        }
    }
    }
    else {
    for (int i=opt.numsnapshots-1;i>=0;i--) {
        Fout<<i+opt.snapshotvaloffset<<"\t"<<h[i].numhalos<<endl;
        for (int j=0;j<h[i].numhalos;j++) {
            Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<"\t"<<d[i][j].NumberofDescendants<<endl;
            for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                Fout<<h[i-p[i][j].istep].Halo[p[i][j].ProgenitorList[k]-1].haloID<<" "<<p[i][j].Merit[k]<<endl;
            }
            for (int k=0;k<d[i][j].NumberofDescendants;k++) {
                Fout<<h[i+d[i][j].istep].Halo[d[i][j].DescendantList[k]-1].haloID<<" "<<d[i][j].Merit[k]<<endl;
            }
        }
    }
    }
    Fout<<"END"<<endl;
    Fout.close();
}

void WriteCrossComp(Options &opt, ProgenitorData **p, HaloTreeData *h) {
    fstream Fout;
    char fname[1000];
    sprintf(fname,"%s",opt.outname);
    cout<<"saving halo merger tree to "<<fname<<endl;
    Fout.open(fname,ios::out);
    Fout<<opt.description<<endl;
    Fout<<opt.TotalNumberofHalos<<endl;
    if (opt.outputformat==0) {
    for (int i=opt.numsnapshots-1;i>0;i--) {
        Fout<<i<<"\t"<<h[i].numhalos<<endl;
        for (int j=0;j<h[i].numhalos;j++) {
            Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<endl;
            for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                Fout<<h[i-1].Halo[p[i][j].ProgenitorList[k]-1].haloID<<" "<<p[i][j].Merit[k]<<endl;
            }
        }
    }
    }
    else if (opt.outputformat==1) {
    for (int i=opt.numsnapshots-1;i>0;i--) {
        Fout<<i<<"\t"<<h[i].numhalos<<endl;
        for (int j=0;j<h[i].numhalos;j++) {
            Fout<<h[i].Halo[j].haloID<<"\t"<<p[i][j].NumberofProgenitors<<endl;
            for (int k=0;k<p[i][j].NumberofProgenitors;k++) {
                Fout<<h[i-1].Halo[p[i][j].ProgenitorList[k]-1].haloID<<" "<<p[i][j].Merit[k]<<" "<<p[i][j].nsharedfrac[k]<<" ";
            }
        }
    }
    }
    Fout<<"END"<<endl;
    Fout.close();
}
//@}
