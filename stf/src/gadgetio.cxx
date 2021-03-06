/*! \file gadgetio.cxx
 *  \brief this file contains routines for gadget snapshot file io
 */

//-- GADGET SPECIFIC IO

#include "stf.h"

#include "gadgetitems.h"
#include "endianutils.h"

///reads a gadget file. If cosmological simulation uses cosmology (generally assuming LCDM or small deviations from this) to estimate the mean interparticle spacing
///and scales physical linking length passed by this distance. Also reads header and over rides passed cosmological parameters with ones stored in header.
void ReadGadget(Options &opt, Particle *&Part, const Int_t nbodies,Particle *&Pbaryons, Int_t nbaryons)
{
    //counters
    Int_t i,j,k,n,m,temp,count,countsph,countstar,countbh,count2,bcount,bcount2,pc,pc_new, Ntot,indark,ingas,instar,Ntotfile;
    Int_t ntot_withmasses;
    //used to read gadget data
    unsigned int dummy;
    GADGETIDTYPE idval;
    FLOAT ctemp[3],sphtemp;
    REAL dtemp;
    char buf[2000];
    char DATA[5];
    //store cosmology
    double z,aadjust,Hubble,Hubbleflow;

    fstream *Fgad;
    struct gadget_header *header;
    Double_t mscale,lscale,lvscale;
    Double_t MP_DM=MAXVALUE,LN,N_DM,MP_B=0;
    int ifirstfile=0,*ireadfile;
#ifndef USEMPI
    Int_t Ntotal;
    int ThisTask=0,NProcs=1;
    ireadfile=new int[opt.num_files];
    for (i=0;i<opt.num_files;i++) ireadfile[i]=1;
#endif

    //if MPI is used, Processor zero opens the file and loads the data into a particle buffer
    //this particle buffer is used to broadcast data to the appropriate processor
#ifdef USEMPI
    //since positions, velocities, masses are all at different points in the file,
    //to correctly assign particle to proccessor with correct velocities and mass must have several file pointers
    fstream *Fgadvel, *Fgadid, *Fgadmass, *Fgadsph;
    fstream *Fgadstar, *Fgadbh;
    FLOAT vtemp[3];
    MPI_Status status;
    Particle *Pbuf;
    Int_t chunksize=GADGETCHUNKSIZE,nchunk;
    FLOAT *ctempchunk, *vtempchunk, *sphtempchunk;
    FLOAT *startempchunk, *bhtempchunk;
    REAL *dtempchunk;
    GADGETIDTYPE *idvalchunk;
    //for parallel io
    Int_t Nlocalbuf,ibuf=0,*Nbuf, *Nreadbuf,*nreadoffset;
    Int_t *Nlocalthreadbuf,Nlocaltotalbuf;
    int *irecv, sendTask,recvTask,irecvflag, *mpi_irecvflag;
    MPI_Request *mpi_request;
    Int_t *mpi_nsend_baryon;
    if (opt.iBaryonSearch && opt.partsearchtype!=PSTALL) mpi_nsend_baryon=new Int_t[NProcs*NProcs];

    //altering io so that all tasks with task numbers less than the number of snapshots are used to read the data
    //this means that all ThisTask==0 need to be changed!
    //if (ThisTask==0) {
    Nbuf=new Int_t[NProcs];
    nreadoffset=new Int_t[opt.nsnapread];

    if (ThisTask<opt.nsnapread)
    {
        //to temporarily store data from gadget file
        Pbuf=new Particle[BufSize*NProcs];
        Nreadbuf=new Int_t[opt.num_files];
        for (int j=0;j<NProcs;j++) Nbuf[j]=0;
        for (int j=0;j<opt.num_files;j++) Nreadbuf[j]=0;

        ctempchunk=new FLOAT[3*chunksize];
        vtempchunk=new FLOAT[3*chunksize];
        dtempchunk=new REAL[chunksize];
        idvalchunk=new GADGETIDTYPE[chunksize];
        sphtempchunk=new FLOAT[NUMGADGETSPHBLOCKS*chunksize];
        startempchunk=new FLOAT[NUMGADGETSTARBLOCKS*chunksize];
        bhtempchunk=new FLOAT[NUMGADGETBHBLOCKS*chunksize];

        //to determine which files the thread should read
        ireadfile=new int[opt.num_files];
        for (i=0;i<opt.num_files;i++) ireadfile[i]=0;
        int nread=opt.num_files/opt.nsnapread;
        int niread=ThisTask*nread,nfread=(ThisTask+1)*nread;
        if (ThisTask==opt.nsnapread-1) nfread=opt.num_files;
        for (i=niread;i<nfread;i++) ireadfile[i]=1;
        ifirstfile=niread;
    }
    else {
        Nlocalthreadbuf=new Int_t[opt.nsnapread];
        irecv=new int[opt.nsnapread];
        mpi_irecvflag=new int[opt.nsnapread];
        for (i=0;i<opt.nsnapread;i++) irecv[i]=1;
        mpi_request=new MPI_Request[opt.nsnapread];
    }
    Nlocal=0;
    if (opt.iBaryonSearch) Nlocalbaryon[0]=0;

#ifndef MPIREDUCEMEM
    MPIDomainExtentGadget(opt);
    if (NProcs>1) {
    MPIDomainDecompositionGadget(opt);
    MPIInitialDomainDecomposition();
    }
    MPI_Barrier(MPI_COMM_WORLD);
#endif
//    if (ThisTask==0) {
    if (ThisTask<opt.nsnapread) {
#endif
    //opening file
#define SKIP2 Fgad[i].read((char*)&dummy, sizeof(dummy));
#define SKIPV2 Fgadvel[i].read((char*)&dummy, sizeof(dummy));
#define SKIPI2 Fgadid[i].read((char*)&dummy, sizeof(dummy));
#define SKIPM2 Fgadmass[i].read((char*)&dummy, sizeof(dummy));

    Fgad=new fstream[opt.num_files];
    header=new gadget_header[opt.num_files];
    for(i=0; i<opt.num_files; i++)
    //if(i==ThisTask)
    if(ireadfile[i])
    {
        if(opt.num_files>1) sprintf(buf,"%s.%d",opt.fname,i);
        else sprintf(buf,"%s",opt.fname);
        Fgad[i].open(buf,ios::in);
        if(!Fgad[i]) {
            cout<<"can't open file "<<buf<<endl;
            exit(0);
        }
        else cout<<"reading "<<buf<<endl;
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
#endif
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&header[i], sizeof(gadget_header));
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        //endian indep call
        header[i].Endian();
    }
    opt.p=header[ifirstfile].BoxSize;
    //if input is from a cosmological box, the following cosmological parameters have meaning
    if (opt.icosmologicalin) {
        z=header[ifirstfile].redshift;
        opt.a=1./(1.+z);
        opt.Omega_m=header[ifirstfile].Omega0;
        opt.Omega_Lambda=header[ifirstfile].OmegaLambda;
        opt.h=header[ifirstfile].HubbleParam;
        opt.Omega_cdm=opt.Omega_m-opt.Omega_b;
        //Hubble flow
        if (opt.comove) aadjust=1.0;
        else aadjust=opt.a;
        Hubble=opt.h*opt.H*sqrt((1-opt.Omega_m-opt.Omega_Lambda)*pow(aadjust,-2.0)+opt.Omega_m*pow(aadjust,-3.0)+opt.Omega_Lambda);
        opt.rhobg=3.*Hubble*Hubble/(8.0*M_PI*opt.G)*opt.Omega_m;
        //if opt.virlevel<0, then use virial overdensity based on Bryan and Norman 1998 virialization level is given by
        if (opt.virlevel<0) 
        {
            Double_t bnx=-((1-opt.Omega_m-opt.Omega_Lambda)*pow(aadjust,-2.0)+opt.Omega_Lambda)/((1-opt.Omega_m-opt.Omega_Lambda)*pow(aadjust,-2.0)+opt.Omega_m*pow(aadjust,-3.0)+opt.Omega_Lambda);
            opt.virlevel=(18.0*M_PI*M_PI+82.0*bnx-39*bnx*bnx)/opt.Omega_m;
        }
        //normally Hubbleflow=lvscale*Hubble but we only care about peculiar velocities
        //ignore hubble flow
        Hubbleflow=0.;
        cout<<"Cosmology (h,Omega_m,Omega_cdm,Omega_b,Omega_L) = ("<< opt.h<<","<<opt.Omega_m<<","<<opt.Omega_cdm<<","<<opt.Omega_b<<","<<opt.Omega_Lambda<<")"<<endl;
    }
    //otherwise, really don't have the same meaning and values are based on input configuration values
    else {
        opt.a=1.0;
        Hubbleflow=0.;
        cout<<"Non-cosmological input, using h = "<< opt.h<<endl;
    }
    mscale=opt.M/opt.h;lscale=opt.L/opt.h*aadjust;lvscale=opt.L/opt.h*opt.a;
    //for high res region find smallest mass
    Ntotal=0;
    for (int j=0;j<NGTYPE;j++)
    {
      opt.numpart[j]=header[ifirstfile].npartTotal[j];
      Ntotal+=header[ifirstfile].npartTotal[j];
    }
    ///adjust for HighWord part of gadget header
    for (int j=0;j<NGTYPE;j++)
    {
        opt.numpart[j]+=((long long)header[ifirstfile].npartTotalHW[j]<<32);
        Ntotal+=((long long)header[ifirstfile].npartTotalHW[j]<<32);
    }
    cout<<"File contains "<<Ntotal<<" particles at is at time "<<opt.a<<endl;
    cout<<"Particle system contains "<<nbodies<<" particles at is at time "<<opt.a<<" in a box of size "<<opt.p<<endl;
    //for cosmological box
    //by default the interparticle spacing is determined using GDMTYPE
    //which is particle of type 1
    if (opt.icosmologicalin) {
        N_DM=header[ifirstfile].npartTotal[GDMTYPE];
        N_DM+=((long long)header[ifirstfile].npartTotalHW[GDMTYPE]<<32);
        LN=(opt.p*lscale/pow(N_DM,1.0/3.0));
    }
    //otherwise, mean interparticle spacing 1, so input physical linking length is not scaled in any fashion
    else {
        LN=1.0;
    }

    //if no mass is stored then assume mass equal and stored in first nonzero mass value in header.mass
#ifdef NOMASS
    for(k=0;k<NGTYPE;k++)
        if(header[ifirstfile].mass[k]!=0) {opt.MassValue=header[ifirstfile].mass[k]*mscale;break;}
#endif

    count2=bcount2=0;
#ifndef USEMPI
    //now read and store data appropriately
    for(i=0,count=0,bcount=0,pc=0;i<opt.num_files; i++,pc=pc_new,count=count2,bcount=bcount2)
    {
        indark=instar=ingas=0;
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
#ifndef NOMASS
        //determine number of particles with masses that need to be read
        for(k=0, ntot_withmasses=0; k<NGTYPE; k++) if(header[i].mass[k]==0) ntot_withmasses+= header[i].npart[k];
#endif
        for(k=0, Ntotfile=0; k<NGTYPE; k++) Ntotfile+=header[i].npart[k];
        //and read positions, velocities, ids, masses, etc
        SKIP2;
        if (dummy/Ntotfile/3!=sizeof(FLOAT)) {cout<<" mismatch in position type size, file has "<<dummy/Ntotfile/3<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            for(n=0;n<header[i].npart[k];n++)
            {
                Fgad[i].read((char*)&ctemp[0], sizeof(FLOAT)*3);
                if (opt.partsearchtype==PSTALL) {
                    for (m=0;m<3;m++) Part[count2].SetPosition(m,LittleFLOAT(ctemp[m]));
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) {
                        for (m=0;m<3;m++) Part[count2].SetPosition(m,LittleFLOAT(ctemp[m]));
                        count2++;
                    }
                    else {
                        if (opt.iBaryonSearch==1 && (k==GGASTYPE || k==GSTARTYPE)) {
                            for (m=0;m<3;m++) Pbaryons[bcount2].SetPosition(m,LittleFLOAT(ctemp[m]));
                            bcount2++;
                        }
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
                        for (m=0;m<3;m++) Part[count2].SetPosition(m,LittleFLOAT(ctemp[m]));
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
                        for (m=0;m<3;m++) Part[count2].SetPosition(m,LittleFLOAT(ctemp[m]));
                        count2++;
                    }
                }
                pc_new++;
            }
        }
        SKIP2;
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/Ntotfile/3!=sizeof(FLOAT)) {cout<<" mismatch in velocity type size, file has "<<dummy/Ntotfile/3<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            for(n=0;n<header[i].npart[k];n++)
            {
                Fgad[i].read((char*)&ctemp[0], sizeof(FLOAT)*3);
                if (opt.partsearchtype==PSTALL) {
                    for (m=0;m<3;m++) Part[count2].SetVelocity(m,LittleFLOAT(ctemp[m]));
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) {
                        for (m=0;m<3;m++) Part[count2].SetVelocity(m,LittleFLOAT(ctemp[m]));
                        count2++;
                    }
                    else {
                        if (opt.iBaryonSearch==1 && (k==GGASTYPE || k==GSTARTYPE)) {
                            for (m=0;m<3;m++) Pbaryons[bcount2].SetVelocity(m,LittleFLOAT(ctemp[m]));
                            bcount2++;
                        }
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
                        for (m=0;m<3;m++) Part[count2].SetVelocity(m,LittleFLOAT(ctemp[m]));
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
                        for (m=0;m<3;m++) Part[count2].SetVelocity(m,LittleFLOAT(ctemp[m]));
                        count2++;
                    }
                }
                pc_new++;
            }
        }
        SKIP2;
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/Ntotfile!=sizeof(idval)) {cout<<" mismatch in ID type size, file has "<<dummy/Ntotfile<<" but using "<<sizeof(idval)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            for(n=0;n<header[i].npart[k];n++) {
                Fgad[i].read((char*)&idval, sizeof(idval));
#ifdef GADGETLONGID
                idval=LittleLongInt(idval);
#else
                idval=LittleInt(idval);
#endif
                if (opt.partsearchtype==PSTALL) {
                    Part[count2].SetPID(idval);
                    Part[count2].SetID(count2);
                    Part[count2].SetType(k);
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) {
                        Part[count2].SetPID(idval);
                        Part[count2].SetID(count2);
                        Part[count2].SetType(DARKTYPE);
                        count2++;
                    }
                    else {
                        if (opt.iBaryonSearch==1 && (k==GGASTYPE || k==GSTARTYPE || k==GBHTYPE)) {
                            Pbaryons[bcount2].SetPID(idval);
                            Pbaryons[bcount2].SetID(bcount2+nbodies);
                            Pbaryons[bcount2].SetType(STARTYPE*(k==GSTARTYPE)+GASTYPE*(k==GGASTYPE)+BHTYPE*(k==GBHTYPE));
                            bcount2++;
                        }
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
                        Part[count2].SetPID(idval);
                        Part[count2].SetID(count2);
                        Part[count2].SetType(STARTYPE);
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
                        Part[count2].SetPID(idval);
                        Part[count2].SetID(count2);
                        Part[count2].SetType(GASTYPE);
                        count2++;
                    }
                }
                pc_new++;
            }
        }
        SKIP2;
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
#ifndef NOMASS
        if(ntot_withmasses>0) {
        SKIP2;
        if (dummy/ntot_withmasses!=sizeof(REAL)) {cout<<" mismatch in mass type size, file has "<<dummy/ntot_withmasses<<" but using "<<sizeof(REAL)<<endl;exit(9);}
        }
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            for(n=0;n<header[i].npart[k];n++)
            {
                //if mass is read from header then does not need to
                //be altered for endian. but must be altered if read from file.
                if(header[i].mass[k]==0) {
                    Fgad[i].read((char*)&dtemp, sizeof(REAL));
                    dtemp=LittleREAL(dtemp);
                }
                else dtemp=header[i].mass[k];
                if(k!=GGASTYPE && k!=GSTARTYPE && dtemp<MP_DM&&dtemp>0) MP_DM=dtemp;
                if(k==GGASTYPE && dtemp<MP_B&&dtemp>0) MP_B=dtemp;
                if (opt.partsearchtype==PSTALL) {
                    Part[count2].SetMass(dtemp);
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) {
                        Part[count2].SetMass(dtemp);
                        count2++;
                    }
                    else {
                        if (opt.iBaryonSearch==1 && (k==GGASTYPE || k==GSTARTYPE)) {
                            Pbaryons[bcount2].SetMass(dtemp);
                            bcount2++;
                        }
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
                        Part[count2].SetMass(dtemp);
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
                        Part[count2].SetMass(dtemp);
                        count2++;
                    }
                }
                pc_new++;
            }
        }
        if(ntot_withmasses>0) SKIP2;
#endif
        //more information contained in sph particles and if there is sf feed back but for the moment, ignore
        //other quantities
        if (header[i].npartTotal[GGASTYPE]>0) {
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/header[i].npart[GGASTYPE]!=sizeof(FLOAT)) {cout<<" mismatch in SPH type size, file has "<<dummy/header[i].npart[GGASTYPE]<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            if (k==GGASTYPE) {
                if ((opt.partsearchtype==PSTALL || opt.partsearchtype==PSTGAS || (opt.partsearchtype==PSTDARK && opt.iBaryonSearch==1))) {
                for(n=0;n<header[i].npart[k];n++) {
                    Fgad[i].read((char*)&ctemp[0], sizeof(FLOAT));
                    if (opt.partsearchtype==PSTALL || opt.partsearchtype==PSTGAS) {
#ifdef GASON
                        Part[count2].SetU(ctemp[0]);
#endif
                        count2++;
                    }
                    else if (opt.partsearchtype==PSTDARK && opt.iBaryonSearch==1) {
#ifdef GASON
                        Pbaryons[bcount2].SetU(ctemp[0]);
#endif
                        bcount2++;
                    }
                    pc_new++;
                }
                }
                else Fgad[i].seekg(header[i].npart[GGASTYPE]*sizeof(FLOAT),ios::cur);
            }
        }
        SKIP2
#if defined(EXTRASPHINFO)&&defined(GASON)
        //then gas densities and softening lengths
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/header[i].npart[GGASTYPE]!=sizeof(FLOAT)) {cout<<" mismatch in SPH type size, file has "<<dummy/header[i].npart[GGASTYPE]<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            if (k==GGASTYPE) {
                if ((opt.partsearchtype==PSTALL || opt.partsearchtype==PSTGAS || (opt.partsearchtype==PSTDARK && opt.iBaryonSearch==1))) {
                for(n=0;n<header[i].npart[k];n++) {
                    Fgad[i].read((char*)&ctemp[0], sizeof(FLOAT));
                    if (opt.partsearchtype==PSTALL || opt.partsearchtype==PSTGAS) {
                        Part[count2].SetSPHDen(ctemp[0]);
                        count2++;
                    }
                    else if (opt.partsearchtype==PSTDARK && opt.iBaryonSearch==1) {
                        Pbaryons[bcount2].SetSPHDen(ctemp[0]);
                        bcount2++;
                    }
                    pc_new++;
                }
                }
                else Fgad[i].seekg(header[i].npart[GGASTYPE]*sizeof(FLOAT),ios::cur);
            }
        }
        SKIP2

        //next skip N gas blocks where, typically N is 4 for Ne, Nh, HSML, SFR. Note that if header indicates SFR block, data is kept
        for (int nsphblocks=0;nsphblocks<opt.gnsphblocks;nsphblocks++) {
        //ignored
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        if (!strcmp(DATA,"SFR ")){
        SKIP2;
        if (dummy/header[i].npart[GGASTYPE]!=sizeof(FLOAT)) {cout<<" mismatch in SPH type size, file has "<<dummy/header[i].npart[GGASTYPE]<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        Fgad[i].seekg(header[i].npart[GGASTYPE]*sizeof(FLOAT),ios::cur);
        SKIP2;
        }
        else {
        SKIP2;
        if (dummy/header[i].npart[GGASTYPE]!=sizeof(FLOAT)) {cout<<" mismatch in SPH type size, file has "<<dummy/header[i].npart[GGASTYPE]<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            if (k==GGASTYPE) {
                if ((opt.partsearchtype==PSTALL || opt.partsearchtype==PSTGAS || (opt.partsearchtype==PSTDARK && opt.iBaryonSearch==1))) {
                for(n=0;n<header[i].npart[k];n++) {
                    Fgad[i].read((char*)&ctemp[0], sizeof(FLOAT));
                    if (opt.partsearchtype==PSTALL || opt.partsearchtype==PSTGAS) {
#ifdef STARON
                        Part[count2].SetSFR(ctemp[0]);
#endif
                        count2++;
                    }
                    else if (opt.partsearchtype==PSTDARK && opt.iBaryonSearch==1) {
#ifdef STARON
                        Pbaryons[bcount2].SetSFR(ctemp[0]);
#endif
                        bcount2++;
                    }
                    pc_new++;
                }
                }
                else Fgad[i].seekg(header[i].npart[GGASTYPE]*sizeof(FLOAT),ios::cur);
            }
        }
        SKIP2;
        }
        }

#endif
        }

#ifdef EXTRASTARINFO
        //then star ages
        if (header[i].npartTotal[GSTARTYPE]>0) {
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/header[i].npart[GSTARTYPE]!=sizeof(FLOAT)) {cout<<" mismatch in Star type size, file has "<<dummy/header[i].npart[GSTARTYPE]<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            for(n=0;n<header[i].npart[k];n++) {
                if (k==GSTARTYPE) Fgad[i].read((char*)&ctemp[0], sizeof(FLOAT));
                if (opt.partsearchtype==PSTALL) {
                    if (k==GSTARTYPE) {
#ifdef STARON
                        Part[count2].SetTage(ctemp[0]);
#endif
                    }
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) {
                        count2++;
                    }
                    else {
                        if (opt.iBaryonSearch==1 && (k==GGASTYPE || k==GSTARTYPE)) {
                            if (k==GSTARTYPE) {
#ifdef STARON
                                Pbaryons[bcount2].SetTage(ctemp[0]);
#endif
                            }
                            bcount2++;
                        }
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
#ifdef STARON
                        Part[count2].SetTage(ctemp[0]);
#endif
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
                        count2++;
                    }
                }
                pc_new++;
            }
        }
        SKIP2;
        //then metallicity of gas AND stars
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/(header[i].npart[GSTARTYPE]+header[i].npart[GGASTYPE])!=sizeof(FLOAT)) {cout<<" mismatch in SPH+STAR type size, file has "<<dummy/(header[i].npart[GSTARTYPE]+header[i].npart[GGASTYPE])<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)
        {
            for(n=0;n<header[i].npart[k];n++) {
                if (k==GSTARTYPE||k==GGASTYPE) Fgad[i].read((char*)&ctemp[0], sizeof(FLOAT));
                if (opt.partsearchtype==PSTALL) {
                    if (k==GSTARTYPE||k==GGASTYPE) {
#if defined(STARON)
                        Part[count2].SetZmet(ctemp[0]);
#endif
                    }
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) {
                        count2++;
                    }
                    else {
                        if (opt.iBaryonSearch==1 && (k==GGASTYPE || k==GSTARTYPE)) {
                            if (k==GSTARTYPE||k==GGASTYPE) {
#if defined(STARON)
                                Pbaryons[bcount2].SetZmet(ctemp[0]);
#endif
                            }
                            bcount2++;
                        }
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
#if defined(STARON)
                        Part[count2].SetZmet(ctemp[0]);
#endif
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
#if defined(STARON)
                        Part[count2].SetZmet(ctemp[0]);
#endif
                        count2++;
                    }
                }
                pc_new++;
            }
        }
        SKIP2;

        //extra star blocks
        for (int nstarblocks=0;nstarblocks<opt.gnstarblocks;nstarblocks++) {
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/header[i].npart[GSTARTYPE]!=sizeof(FLOAT)) {cout<<" mismatch in STAR type size, file has "<<dummy/header[i].npart[GSTARTYPE]<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        Fgad[i].seekg(header[i].npart[GSTARTYPE]*sizeof(FLOAT),ios::cur);
        SKIP2;
        }
        }
#endif

#ifdef EXTRABHINFO
        //typical to have black hole ages, black hole scale, black hole mass perhaps
        if (header[i].npartTotal[GBHTYPE]>0) {
        for (int nbhblocks=0;nbhblocks<opt.gnbhblocks;nbhblocks++) {
#ifdef GADGET2FORMAT
        SKIP2;
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        SKIP2;
        SKIP2;
        cout<<"reading "<<DATA<<endl;
#endif
        SKIP2;
        if (dummy/header[i].npart[GBHTYPE]!=sizeof(FLOAT)) {cout<<" mismatch in BH type size, file has "<<dummy/header[i].npart[GBHTYPE]<<" but using "<<sizeof(FLOAT)<<endl;exit(9);}
        Fgad[i].seekg(header[i].npart[GBHTYPE]*sizeof(FLOAT),ios::cur);
        SKIP2;
        }
        }
#endif

        Fgad[i].close();
    }
    //finally adjust to appropriate units
    for (i=0;i<nbodies;i++)
    {
        Part[i].SetMass(Part[i].GetMass()*mscale);
        for (int j=0;j<3;j++) Part[i].SetVelocity(j,Part[i].GetVelocity(j)*opt.V*sqrt(opt.a)+Hubbleflow*Part[i].GetPosition(j));
        for (int j=0;j<3;j++) Part[i].SetPosition(j,Part[i].GetPosition(j)*lscale);
    }
    if (Pbaryons!=NULL && opt.iBaryonSearch==1) {
    for (i=0;i<nbaryons;i++)
    {
        Pbaryons[i].SetMass(Pbaryons[i].GetMass()*mscale);
        for (int j=0;j<3;j++) Pbaryons[i].SetVelocity(j,Pbaryons[i].GetVelocity(j)*opt.V*sqrt(opt.a)+Hubbleflow*Pbaryons[i].GetPosition(j));
        for (int j=0;j<3;j++) Pbaryons[i].SetPosition(j,Pbaryons[i].GetPosition(j)*lscale);
#ifdef GASON
        Pbaryons[i].SetU(Pbaryons[i].GetU()*opt.V*opt.V);
#endif
    }
    }

#else
    Fgadvel=new fstream[opt.num_files];
    Fgadid=new fstream[opt.num_files];
    Fgadmass=new fstream[opt.num_files];
    Fgadsph=new fstream[opt.num_files*NUMGADGETSPHBLOCKS];
    Fgadstar=new fstream[opt.num_files*NUMGADGETSTARBLOCKS];
    Fgadbh=new fstream[opt.num_files*NUMGADGETBHBLOCKS];
    for(i=0; i<opt.num_files; i++)
    if (ireadfile[i])
    {
        if(opt.num_files>1) sprintf(buf,"%s.%d",opt.fname,i);
        else sprintf(buf,"%s",opt.fname);

        count=0;for(k=0;k<NGTYPE;k++)count+=header[i].npart[k];

        //offset all the files for easy reading
        Fgadvel[i].open(buf,ios::in);
        Fgadid[i].open(buf,ios::in);
        Fgadmass[i].open(buf,ios::in);
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++) Fgadsph[i+sphblocks].open(buf,ios::in);
        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++) Fgadstar[i+starblocks].open(buf,ios::in);
        for (int bhblocks=0;bhblocks<NUMGADGETBHBLOCKS;bhblocks++) Fgadbh[i+bhblocks].open(buf,ios::in);
#ifdef GADGET2FORMAT
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));

        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));

        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&header[i], sizeof(gadget_header));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&header[i], sizeof(gadget_header));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&header[i], sizeof(gadget_header));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        //endian indep call
        header[i].Endian();

        //move to velocities
#ifdef GADGET2FORMAT
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));

        //move to ids
#ifdef GADGET2FORMAT
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadid[i].read((char*)&dummy, sizeof(dummy));

        //move to masses
#ifdef GADGET2FORMAT
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].seekg(count*sizeof(idval),ios::cur);
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));

#ifdef GASON
        //move to sph
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].seekg(count*sizeof(idval),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        for (int k=0;k<NGTYPE;k++)
            if(header[i].mass[k]==0)
              Fgadsph[i+sphblocks].seekg(header[i].npart[k]*sizeof(REAL),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        }

        //offset each sph block appropriately
        for (int sphstart=1;sphstart<NUMGADGETSPHBLOCKS;sphstart++){
        for (int  sphblocks=0;sphblocks<sphstart;sphblocks++){
#ifdef GADGET2FORMAT
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphstart].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
#endif
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
            countsph=dummy;
            Fgadsph[i+sphstart].seekg(countsph,ios::cur);
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
        }
        }
#endif

#ifdef STARON
        //move to star
        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++){
#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(count*sizeof(idval),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        for (int k=0;k<NGTYPE;k++)
            if(header[i].mass[k]==0)
              Fgadstar[i+starblocks].seekg(header[i].npart[k]*sizeof(REAL),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));

        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(header[i].npart[GGASTYPE]*sizeof(FLOAT),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        }
        }

        //offset each star block appropriately
        for (int starstart=1;starstart<NUMGADGETSTARBLOCKS;starstart++){
        for (int starblocks=0;starblocks<starstart;starblocks++){
#ifdef GADGET2FORMAT
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
            Fgadstar[i+starstart].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
#endif
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
            countstar=dummy;
            Fgadstar[i+starstart].seekg(countstar,ios::cur);
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
        }
        }
#endif

        //need black hole offset

#ifdef GADGET2FORMAT
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&dummy, sizeof(dummy));

        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));

        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));

        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));

        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
            Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
            Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        }

        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++){
            Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
            Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
            Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
            Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        }
#endif

    }
    //all fstreams now at appropriate points in the input file so data easily read
    for(i=0,count=0,pc=0;i<opt.num_files; i++,pc=pc_new,count=count2)
    if (ireadfile[i])
    {
        //determine number of particles with masses that need to be read
        for(k=0, ntot_withmasses=0; k<NGTYPE; k++) if(header[i].mass[k]==0) ntot_withmasses+= header[i].npart[k];
        for(k=0, Ntotfile=0; k<NGTYPE; k++) Ntotfile+=header[i].npart[k];
        //and read positions, velocities, masses, etc
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++) Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++) Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));

        for(k=0,count2=count,bcount2=bcount,pc_new=pc;k<NGTYPE;k++)if (header[i].npart[k]>0)
        {
            //data loaded into memory in chunks
            if (header[i].npart[k]<chunksize)nchunk=header[i].npart[k];
            else nchunk=chunksize;
            for(n=0;n<header[i].npart[k];n+=nchunk)
            {
                if (header[i].npart[k]-n<chunksize&&header[i].npart[k]-n>0)nchunk=header[i].npart[k]-n;
                Fgad[i].read((char*)ctempchunk, sizeof(FLOAT)*3*nchunk);
                Fgadvel[i].read((char*)vtempchunk, sizeof(FLOAT)*3*nchunk);
                Fgadid[i].read((char*)idvalchunk, sizeof(idval)*nchunk);
                for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
                    Fgadsph[i+sphblocks].read((char*)&sphtempchunk[sphblocks*nchunk], sizeof(FLOAT)*nchunk);
                }
                for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++){
                    Fgadstar[i+starblocks].read((char*)&startempchunk[starblocks*nchunk], sizeof(FLOAT)*nchunk);
                }
#ifndef NOMASS
                if(header[i].mass[k]==0) Fgadmass[i].read((char*)dtempchunk, sizeof(REAL)*nchunk);
#endif
                //once a block of data is in memory, start parsing it. 
                for (int nn=0;nn<nchunk;nn++) {
                ctemp[0]=ctempchunk[0+3*nn];ctemp[1]=ctempchunk[1+3*nn];ctemp[2]=ctempchunk[2+3*nn];
                vtemp[0]=vtempchunk[0+3*nn];vtemp[1]=vtempchunk[1+3*nn];vtemp[2]=vtempchunk[2+3*nn];
                idval=idvalchunk[nn];
                for (int kk=0;kk<3;kk++) {ctemp[kk]=LittleFLOAT(ctemp[kk]);vtemp[kk]=LittleFLOAT(vtemp[kk]);}
                for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++)sphtempchunk[sphblocks*nchunk+nn]=LittleFLOAT(sphtempchunk[sphblocks*nchunk+nn]);
                for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++)startempchunk[starblocks*nchunk+nn]=LittleFLOAT(startempchunk[starblocks*nchunk+nn]);
#ifdef GADGETLONGID
                idval=LittleLongInt(idval);
#else
                idval=LittleInt(idval);
#endif
#ifndef NOMASS
                if(header[i].mass[k]==0) {
                    dtemp=dtempchunk[nn];
                    dtemp=LittleREAL(dtemp);
                }
                else dtemp=header[i].mass[k];
#else
                dtemp=1.0;
#endif
                //useful to store smallest mass
                if(k!=GGASTYPE && k!= GSTARTYPE && k!=GBHTYPE && dtemp<MP_DM&&dtemp>0) MP_DM=dtemp;
                if(k==GGASTYPE && dtemp<MP_B&&dtemp>0) MP_B=dtemp;

                //determine processor this particle belongs on based on its spatial position
                ibuf=MPIGetParticlesProcessor(ctemp[0],ctemp[1],ctemp[2]);
                //now depending on the type of particle and the type of search,
                //load the particle into a particle buffer. If the particle belongs on local thread, then just copy it over
                //to the Part array (or Pbaryons array if the iBaryonSearch is set). Otherwise, keep adding to the Pbuf array
                //till the array is full and then send messages. 
                if (opt.partsearchtype==PSTALL) {
                    Pbuf[ibuf*BufSize+Nbuf[ibuf]]=Particle(dtemp*mscale,
                        ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                        vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                        vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                        vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                        count2,k);
                    Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetPID(idval);
                    //assume that first sphblock is internal energy
#ifdef GASON
                    if (k==GGASTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetU(sphtempchunk[0*nchunk+nn]);
                    if (k==GGASTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetSPHDen(sphtempchunk[1*nchunk+nn]);
#endif
#ifdef STARON
                    if (k==GSTARTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetTage(startempchunk[0*nchunk+nn]);
#endif
                    if (k==GGASTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(GASTYPE);
                    else if (k==GSTARTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(STARTYPE);
                    else if (k==GBHTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(BHTYPE);
                    else Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(DARKTYPE);
                    //ensure that store number of particles to be sent to the threads involved with reading snapshot files
                    if(ibuf<opt.nsnapread&&ibuf!=ThisTask) Nreadbuf[ibuf]++;
                    Nbuf[ibuf]++;
                    if (ibuf==ThisTask) {
                        Nbuf[ibuf]--;
                        Part[Nlocal++]=Pbuf[ibuf*BufSize+Nbuf[ibuf]];
                    }
                    else {
                        //before a simple send was done because only Task zero was reading the data
                        //but now if ibuf<opt.nsnapread, care must be taken.
                        //blocking sends that are matched by non-blocking receives
                        if(Nbuf[ibuf]==BufSize&&ibuf>=opt.nsnapread) {
                            MPI_Ssend(&Nbuf[ibuf], 1, MPI_Int_t, ibuf, ibuf+NProcs, MPI_COMM_WORLD);
                            MPI_Ssend(&Pbuf[ibuf*BufSize],sizeof(Particle)*Nbuf[ibuf],MPI_BYTE,ibuf,ibuf,MPI_COMM_WORLD);
                            Nbuf[ibuf]=0;
                        }
                        else if (Nbuf[ibuf]==BufSize&&ibuf<opt.nsnapread) {
                            Nbuf[ibuf]=0;
                        }
                    }
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) {
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]]=Particle(dtemp*mscale,
                            ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                            vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                            vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                            vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                            count2,DARKTYPE);
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetPID(idval);
                        //ensure that store number of particles to be sent to other reading threads
                        if(ibuf<opt.nsnapread&&ibuf!=ThisTask) Nreadbuf[ibuf]++;
                        Nbuf[ibuf]++;
                        //now determine what to do with the particle, local or must send
                        if (ibuf==ThisTask) {
                            Nbuf[ibuf]--;
                            Part[Nlocal++]=Pbuf[ibuf*BufSize+Nbuf[ibuf]];
                        }
                        else {
                            //if belongs to another mpi thread then see if buffer is full and send with appropriate flag
                            if(Nbuf[ibuf]==BufSize&&ibuf>=opt.nsnapread) {
                                MPI_Ssend(&Nbuf[ibuf], 1, MPI_Int_t, ibuf, ibuf+NProcs, MPI_COMM_WORLD);
                                MPI_Ssend(&Pbuf[ibuf*BufSize],sizeof(Particle)*Nbuf[ibuf],MPI_BYTE,ibuf,ibuf,MPI_COMM_WORLD);
                                Nbuf[ibuf]=0;
                            }
                            else if (Nbuf[ibuf]==BufSize&&ibuf<opt.nsnapread) {
                                Nbuf[ibuf]=0;
                            }
                        }
                        count2++;
                    }
                    else if (opt.iBaryonSearch) {
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]]=Particle(dtemp*mscale,
                            ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                            vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                            vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                            vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                            count2);
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetPID(idval);
                        if (k==GGASTYPE) {
#ifdef GASON
                            Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetU(sphtempchunk[0*nchunk+nn]);
                            Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetSPHDen(sphtempchunk[1*nchunk+nn]);
#endif
                            Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(GASTYPE);
                        }
                        else if (k==GSTARTYPE) {
#ifdef STARON
                            Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetTage(startempchunk[0*nchunk+nn]);
#endif
                            Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(STARTYPE);
                        }
                        else if (k==GBHTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(BHTYPE);
                        //ensure that store number of particles to be sent to the reading threads
                        if(ibuf<opt.nsnapread&&ibuf!=ThisTask) Nreadbuf[ibuf]++;
                        Nbuf[ibuf]++;
                        if (ibuf==ThisTask) {
                            Nbuf[ibuf]--;
                            Pbaryons[Nlocalbaryon[0]++]=Pbuf[ibuf*BufSize+Nbuf[ibuf]];
                            if (k==GGASTYPE) Nlocalbaryon[1]++;
                            else if (k==GSTARTYPE) Nlocalbaryon[2]++;
                            else if (k==GBHTYPE) Nlocalbaryon[3]++;
                        }
                        else {
                            if(Nbuf[ibuf]==BufSize&&ibuf>=opt.nsnapread) {
                                MPI_Ssend(&Nbuf[ibuf], 1, MPI_Int_t, ibuf, ibuf+NProcs, MPI_COMM_WORLD);
                                MPI_Ssend(&Pbuf[ibuf*BufSize],sizeof(Particle)*Nbuf[ibuf],MPI_BYTE,ibuf,ibuf,MPI_COMM_WORLD);
                                Nbuf[ibuf]=0;
                            }
                            else if (Nbuf[ibuf]==BufSize&&ibuf<opt.nsnapread) {
                                Nbuf[ibuf]=0;
                            }
                        }
                        bcount2++;
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
                        //if using MPI, determine proccessor and place in ibuf, store particle in particle buffer and if buffer full, broadcast data
                        //unless ibuf is 0, then just store locally
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]]=Particle(dtemp*mscale,
                            ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                            vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                            vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                            vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                            count2,STARTYPE);
#ifdef STARON
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetTage(startempchunk[0*nchunk+nn]);
#endif
                        //ensure that store number of particles to be sent to the reading threads
                        if(ibuf<opt.nsnapread&&ibuf!=ThisTask) Nreadbuf[ibuf]++;
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetPID(idval);
                        Nbuf[ibuf]++;
                        if (ibuf==ThisTask) {
                            Nbuf[ibuf]--;
                            Part[Nlocal++]=Pbuf[ibuf*BufSize+Nbuf[ibuf]];
                        }
                        else {
                            if(Nbuf[ibuf]==BufSize&&ibuf>=opt.nsnapread) {
                                MPI_Ssend(&Nbuf[ibuf], 1, MPI_Int_t, ibuf, ibuf+NProcs, MPI_COMM_WORLD);
                                MPI_Ssend(&Pbuf[ibuf*BufSize],sizeof(Particle)*Nbuf[ibuf],MPI_BYTE,ibuf,ibuf,MPI_COMM_WORLD);
                                Nbuf[ibuf]=0;
                            }
                            else if (Nbuf[ibuf]==BufSize&&ibuf<opt.nsnapread) {
                                Nbuf[ibuf]=0;
                            }
                        }
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]]=Particle(dtemp*mscale,
                            ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                            vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                            vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                            vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                            count2,GASTYPE);
                        //ensure that store number of particles to be sent to the reading threads
                        if(ibuf<opt.nsnapread&&ibuf!=ThisTask) Nreadbuf[ibuf]++;
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetPID(idval);
#ifdef GASON
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetU(sphtempchunk[0*nchunk+nn]);
                        Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetSPHDen(sphtempchunk[1*nchunk+nn]);
#endif
                        Nbuf[ibuf]++;
                        if(ibuf==ThisTask){
                            Nbuf[ibuf]--;
                            Part[Nlocal++]=Pbuf[ibuf*BufSize+Nbuf[ibuf]];
                        }
                        else {
                            if(Nbuf[ibuf]==BufSize&&ibuf>=opt.nsnapread) {
                                MPI_Ssend(&Nbuf[ibuf], 1, MPI_Int_t, ibuf, ibuf+NProcs, MPI_COMM_WORLD);
                                MPI_Ssend(&Pbuf[ibuf*BufSize],sizeof(Particle)*Nbuf[ibuf],MPI_BYTE,ibuf,ibuf,MPI_COMM_WORLD);
                                Nbuf[ibuf]=0;
                            }
                            else if (Nbuf[ibuf]==BufSize&&ibuf<opt.nsnapread) {
                                Nbuf[ibuf]=0;
                            }
                        }
                        count2++;
                    }
                }
                pc_new++;
                }
            }
        }
        //more information contained in sph particles and if there is sf feed back but for the moment, ignore
        Fgad[i].close();
        Fgadvel[i].close();
        Fgadid[i].close();
        Fgadmass[i].close();
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++) Fgadsph[i+sphblocks].close();
        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++) Fgadstar[i+starblocks].close();
        for (int bhblocks=0;bhblocks<NUMGADGETBHBLOCKS;bhblocks++) Fgadbh[i+bhblocks].close();
    }
    //once finished reading the file if there are any particles left in the buffer broadcast them
    for(ibuf = opt.nsnapread; ibuf < NProcs; ibuf++)
    {
        MPI_Ssend(&Nbuf[ibuf],1,MPI_Int_t, ibuf, ibuf+NProcs, MPI_COMM_WORLD);
        if (Nbuf[ibuf]>0) {
            MPI_Ssend(&Pbuf[ibuf*BufSize], sizeof(Particle)*Nbuf[ibuf], MPI_BYTE, ibuf, ibuf, MPI_COMM_WORLD);
            Nbuf[ibuf]=0;
            //last broadcast with Nbuf[ibuf]=0 so that receiver knows no more particles are to be broadcast
            MPI_Ssend(&Nbuf[ibuf],1,MPI_Int_t,ibuf,ibuf+NProcs,MPI_COMM_WORLD);
        }
    }
    
#endif

#ifdef USEMPI
    }
    else {
        //for all threads not reading snapshots, simply receive particles as necessary from all threads involved with reading the data
        //first determine which threads are going to send information to this thread.
        for (i=0;i<opt.nsnapread;i++) if (irecv[i]) {
            mpi_irecvflag[i]=0;
            MPI_Irecv(&Nlocalthreadbuf[i], 1, MPI_Int_t, i, ThisTask+NProcs, MPI_COMM_WORLD, &mpi_request[i]);
        }
        Nlocaltotalbuf=0;
        //non-blocking receives for the number of particles one expects to receive
        do {
            irecvflag=0;
            for (i=0;i<opt.nsnapread;i++) if (irecv[i]) {
                if (mpi_irecvflag[i]==0) {
                    //test if a request has been sent for a Recv call by one of the read threads
                    MPI_Test(&mpi_request[i], &mpi_irecvflag[i], &status);
                    if (mpi_irecvflag[i]) {
                        if (Nlocalthreadbuf[i]>0) {
                            MPI_Recv(&Part[Nlocal],sizeof(Particle)*Nlocalthreadbuf[i],MPI_BYTE,i,ThisTask, MPI_COMM_WORLD,&status);
                            Nlocal+=Nlocalthreadbuf[i];
                            Nlocaltotalbuf+=Nlocalthreadbuf[i];
                            mpi_irecvflag[i]=0;
                            MPI_Irecv(&Nlocalthreadbuf[i], 1, MPI_Int_t, i, ThisTask+NProcs, MPI_COMM_WORLD, &mpi_request[i]);
                        }
                        else {
                            irecv[i]=0;
                        }
                    }
                }
            }
            for (i=0;i<opt.nsnapread;i++) irecvflag+=irecv[i];
        } while(irecvflag>0);
        //now that data is local, must adjust data iff a separate baryon search is required. 
        if (opt.iBaryonSearch && opt.partsearchtype!=PSTALL) {
            for (i=0;i<Nlocal;i++) {
                k=Part[i].GetType();
                if (!(k==GGASTYPE||k==GSTARTYPE||k==GBHTYPE)) Part[i].SetID(0);
                else {
                    Nlocalbaryon[0]++;
                    if  (k==GGASTYPE) {Part[i].SetID(1);Nlocalbaryon[1]++;}
                    else if  (k==GSTARTYPE) {Part[i].SetID(2);Nlocalbaryon[2]++;}
                    else if  (k==GBHTYPE) {Part[i].SetID(3);Nlocalbaryon[3]++;}
                }
            }
            //sorted so that dark matter particles first, baryons after
            qsort(Part,Nlocal, sizeof(Particle), IDCompare);
            Nlocal-=Nlocalbaryon[0];
            //index type separated
            for (i=0;i<Nlocal;i++) Part[i].SetID(i);
            for (i=0;i<Nlocalbaryon[0];i++) Part[i+Nlocal].SetID(i+Nlocal);
            //finally, need to move baryons forward by the Export Factor * Nlocal as need that extra buffer to copy data two and from mpi threads
            for (i=Nlocalbaryon[0]-1;i>=0;i--) Part[i+(Int_t)(Nlocal*MPIExportFac)]=Part[i+Nlocal];
        }
    }
    //finally need to send info between read threads once all threads reading data have broadcasted the data appropriately to all other threads
    //must deallocate Pbuf, reallocate it for the local Nreadbuf amount and read the files again. This ensures little memory overhead and files are only read twice

    //since Nbuf is used to determine what is going to be sent between threads in point-to-point communication
    //via an allgather, reset Nbuf
    for (i=0;i<NProcs;i++) Nbuf[i]=0;
    if (ThisTask<opt.nsnapread) {
    delete[] Pbuf;
    Nlocalbuf=0;
    for (i=0;i<opt.nsnapread;i++) Nlocalbuf+=Nreadbuf[i];
    if (Nlocalbuf>0)
    {
    Pbuf=new Particle[Nlocalbuf];
    //determine offsets
    nreadoffset[0]=0;for (i=1;i<opt.nsnapread;i++)nreadoffset[i]=nreadoffset[i-1]+Nreadbuf[i-1];

    for(i=0; i<opt.num_files; i++)
    if (ireadfile[i])
    {
        if(opt.num_files>1) sprintf(buf,"%s.%d",opt.fname,i);
        else sprintf(buf,"%s",opt.fname);
        Fgad[i].open(buf,ios::in);
        Fgadvel[i].open(buf,ios::in);
        Fgadid[i].open(buf,ios::in);
        Fgadmass[i].open(buf,ios::in);
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++) Fgadsph[i+sphblocks].open(buf,ios::in);
        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++) Fgadstar[i+starblocks].open(buf,ios::in);
        for (int bhblocks=0;bhblocks<NUMGADGETBHBLOCKS;bhblocks++) Fgadbh[i+bhblocks].open(buf,ios::in);
        if(!Fgad[i]) {
            cout<<"can't open file "<<buf<<endl;
            exit(0);
        }
        else cout<<"reading "<<buf<<endl;

        //move read points to appropriate positions
#ifdef GADGET2FORMAT
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&dummy, sizeof(dummy));

        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));

        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));

        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&header[i], sizeof(gadget_header));
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&header[i], sizeof(gadget_header));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&header[i], sizeof(gadget_header));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&header[i], sizeof(gadget_header));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        //endian indep call
        header[i].Endian();

        //move to velocities
#ifdef GADGET2FORMAT
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));

        //move to ids
#ifdef GADGET2FORMAT
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadid[i].read((char*)&dummy, sizeof(dummy));

        //move to masses
#ifdef GADGET2FORMAT
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].seekg(count*sizeof(idval),ios::cur);
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));

#ifdef GASON
        //move to sph
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].seekg(count*sizeof(idval),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        for (int k=0;k<NGTYPE;k++)
            if(header[i].mass[k]==0)
              Fgadsph[i+sphblocks].seekg(header[i].npart[k]*sizeof(REAL),ios::cur);
        Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        }

        //offset each sph block appropriately
        for (int sphstart=1;sphstart<NUMGADGETSPHBLOCKS;sphstart++){
        for (int  sphblocks=0;sphblocks<sphstart;sphblocks++){
#ifdef GADGET2FORMAT
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphstart].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
#endif
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
            countsph=dummy;
            Fgadsph[i+sphstart].seekg(countsph,ios::cur);
            Fgadsph[i+sphstart].read((char*)&dummy, sizeof(dummy));
        }
        }
#endif

#ifdef STARON
        //move to star
        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++){
#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(count*3*sizeof(FLOAT),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(count*sizeof(idval),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));

#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        for (int k=0;k<NGTYPE;k++)
            if(header[i].mass[k]==0)
              Fgadstar[i+starblocks].seekg(header[i].npart[k]*sizeof(REAL),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));

        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
#ifdef GADGET2FORMAT
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
#endif
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        Fgadstar[i+starblocks].seekg(header[i].npart[0]*sizeof(FLOAT),ios::cur);
        Fgadstar[i+starblocks].read((char*)&dummy, sizeof(dummy));
        }
        }

        //offset each star block appropriately
        for (int starstart=1;starstart<NUMGADGETSTARBLOCKS;starstart++){
        for (int starblocks=0;starblocks<starstart;starblocks++){
#ifdef GADGET2FORMAT
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
            Fgadstar[i+starstart].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
#endif
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
            countstar=dummy;
            Fgadstar[i+starstart].seekg(countstar,ios::cur);
            Fgadstar[i+starstart].read((char*)&dummy, sizeof(dummy));
        }
        }
#endif

        //move to black holes 
        
#ifdef GADGET2FORMAT
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgad[i].read((char*)&dummy, sizeof(dummy));

        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));

        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));

        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));

        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
            Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphblocks].read((char*)&DATA[0],sizeof(char)*4);DATA[4] = '\0';
            Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
            Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        }
#endif
    }
    for(i=0,count=0,pc=0;i<opt.num_files; i++,pc=pc_new,count=count2)
    if (ireadfile[i])
    {
        //determine number of particles with masses that need to be read
        for(k=0, ntot_withmasses=0; k<NGTYPE; k++) if(header[i].mass[k]==0) ntot_withmasses+= header[i].npart[k];
        //and read positions, velocities, masses, etc
        Fgad[i].read((char*)&dummy, sizeof(dummy));
        Fgadvel[i].read((char*)&dummy, sizeof(dummy));
        Fgadid[i].read((char*)&dummy, sizeof(dummy));
        Fgadmass[i].read((char*)&dummy, sizeof(dummy));
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++) Fgadsph[i+sphblocks].read((char*)&dummy, sizeof(dummy));
        for(k=0,count2=count,pc_new=pc;k<NGTYPE;k++)if (header[i].npart[k]>0)
        {
            if (header[i].npart[k]<chunksize)nchunk=header[i].npart[k];
            else nchunk=chunksize;
            for(n=0;n<header[i].npart[k];n+=nchunk)
            {
                if (header[i].npart[k]-n<chunksize&&header[i].npart[k]-n>0)nchunk=header[i].npart[k]-n;
                Fgad[i].read((char*)ctempchunk, sizeof(FLOAT)*3*nchunk);
                Fgadvel[i].read((char*)vtempchunk, sizeof(FLOAT)*3*nchunk);
                Fgadid[i].read((char*)idvalchunk, sizeof(idval)*nchunk);
#ifndef NOMASS
                if(header[i].mass[k]==0) Fgadmass[i].read((char*)dtempchunk, sizeof(REAL)*nchunk);
#endif
                for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++){
                    Fgadsph[i+sphblocks].read((char*)&sphtempchunk[sphblocks*nchunk], sizeof(FLOAT)*nchunk);
                }
                for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++){
                    Fgadstar[i+starblocks].read((char*)&startempchunk[starblocks*nchunk], sizeof(FLOAT)*nchunk);
                }
                for (int nn=0;nn<nchunk;nn++) {
                ctemp[0]=ctempchunk[0+3*nn];ctemp[1]=ctempchunk[1+3*nn];ctemp[2]=ctempchunk[2+3*nn];
                vtemp[0]=vtempchunk[0+3*nn];vtemp[1]=vtempchunk[1+3*nn];vtemp[2]=vtempchunk[2+3*nn];
                idval=idvalchunk[nn];
                for (int kk=0;kk<3;kk++) {ctemp[kk]=LittleFLOAT(ctemp[kk]);vtemp[kk]=LittleFLOAT(vtemp[kk]);}
                for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++)sphtempchunk[sphblocks*nchunk+nn]=LittleFLOAT(sphtempchunk[sphblocks*nchunk+nn]);
#ifdef GADGETLONGID
                idval=LittleLongInt(idval);
#else
                idval=LittleInt(idval);
#endif
#ifndef NOMASS
                if(header[i].mass[k]==0) {
                    dtemp=dtempchunk[nn];
                    dtemp=LittleREAL(dtemp);
                }
                else dtemp=header[i].mass[k];
                if(dtemp<MP_DM&&dtemp>0) MP_DM=dtemp;
#else
                dtemp=1.0;
#endif
                if(k!=GGASTYPE && k!= GSTARTYPE && k!=GBHTYPE && dtemp<MP_DM&&dtemp>0) MP_DM=dtemp;
                if(k==GGASTYPE && dtemp<MP_B&&dtemp>0) MP_B=dtemp;

                ibuf=MPIGetParticlesProcessor(ctemp[0],ctemp[1],ctemp[2]);
                if (ibuf<opt.nsnapread&&ibuf!=ThisTask) {
                if (opt.partsearchtype==PSTALL) {
                    Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]]=Particle(dtemp*mscale,
                        ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                        vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                        vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                        vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                        count2,k);
                    Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetPID(idval);
#ifdef GASON
                    if (k==GGASTYPE) Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetU(sphtempchunk[0*nchunk+nn]);
                    if (k==GGASTYPE) Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetSPHDen(sphtempchunk[1*nchunk+nn]);
#endif
#ifdef STARON
                    if (k==GSTARTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetTage(startempchunk[0*nchunk+nn]);
#endif
                    if (k==GGASTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(GASTYPE);
                    else if (k==GSTARTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(STARTYPE);
                    else if (k==GBHTYPE) Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(BHTYPE);
                    else Pbuf[ibuf*BufSize+Nbuf[ibuf]].SetType(DARKTYPE);
                    Nbuf[ibuf]++;
                    count2++;
                }
                else if (opt.partsearchtype==PSTDARK) {
                    if (!(k==GASTYPE||k==STARTYPE||k==GBHTYPE)) {
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]]=Particle(dtemp*mscale,
                            ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                            vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                            vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                            vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                            count2,DARKTYPE);
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetPID(idval);
                        if (k==GGASTYPE) {
#ifdef GASON
                            Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetU(sphtempchunk[0*nchunk+nn]);
                            Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetSPHDen(sphtempchunk[1*nchunk+nn]);
#endif
                            Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetType(GASTYPE);
                        }
                        else if (k==GSTARTYPE) {
#ifdef STARON
                            Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetTage(startempchunk[0*nchunk+nn]);
#endif
                            Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetType(STARTYPE);
                        }
                        else if (k==GBHTYPE) Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetType(BHTYPE);
                        Nbuf[ibuf]++;
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTSTAR) {
                    if (k==GSTARTYPE) {
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]]=Particle(dtemp*mscale,
                            ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                            vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                            vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                            vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                            count2,STARTYPE);
#ifdef STARON
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetTage(startempchunk[0*nchunk+nn]);
#endif
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetPID(idval);
                        Nbuf[ibuf]++;
                        count2++;
                    }
                }
                else if (opt.partsearchtype==PSTGAS) {
                    if (k==GGASTYPE) {
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]]=Particle(dtemp*mscale,
                            ctemp[0]*lscale,ctemp[1]*lscale,ctemp[2]*lscale,
                            vtemp[0]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[0],
                            vtemp[1]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[1],
                            vtemp[2]*opt.V*sqrt(opt.a)+Hubbleflow*ctemp[2],
                            count2,GASTYPE);
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetPID(idval);
#ifdef GASON
                        Pbuf[nreadoffset[ibuf]+Nbuf[ibuf]].SetU(sphtempchunk[0*nchunk+nn]);
#endif
                        Nbuf[ibuf]++;
                        count2++;
                    }
                }
                }
                pc_new++;
                }
            }
        }
        Fgad[i].close();
        Fgadvel[i].close();
        Fgadid[i].close();
        Fgadmass[i].close();
        for (int sphblocks=0;sphblocks<NUMGADGETSPHBLOCKS;sphblocks++) Fgadsph[i+sphblocks].close();
        for (int starblocks=0;starblocks<NUMGADGETSTARBLOCKS;starblocks++) Fgadstar[i+starblocks].close();
        for (int bhblocks=0;bhblocks<NUMGADGETBHBLOCKS;bhblocks++) Fgadbh[i+bhblocks].close();
    }
    }
    }
    //gather all the items that must be sent.
    MPI_Allgather(Nbuf, NProcs, MPI_Int_t, mpi_nsend, NProcs, MPI_Int_t, MPI_COMM_WORLD);
    //if separate baryon search then sort the Pbuf array so that it is separated by type 
    if (opt.iBaryonSearch && opt.partsearchtype!=PSTALL) {
        for(ibuf = 0; ibuf < opt.nsnapread; ibuf++) if (mpi_nsend[ThisTask * NProcs + ibuf] > 0)
        {
            Nbuf[ibuf]=0;
            for (i=0;i<mpi_nsend[ThisTask * NProcs + ibuf];i++) {
                k=Pbuf[nreadoffset[ibuf]+i].GetType();
                if (!(k==GASTYPE||k==STARTYPE||k==BHTYPE)) Pbuf[nreadoffset[ibuf]+i].SetID(0);
                else {
                    if  (k==GASTYPE) Pbuf[nreadoffset[ibuf]+i].SetID(1);
                    else if  (k==STARTYPE) Pbuf[nreadoffset[ibuf]+i].SetID(2);
                    else if  (k==BHTYPE) Pbuf[nreadoffset[ibuf]+i].SetID(3);
                    Nbuf[ibuf]++;
                }
            }
            qsort(&Pbuf[nreadoffset[ibuf]],mpi_nsend[ThisTask*NProcs+ibuf], sizeof(Particle), IDCompare);
        }
        MPI_Allgather(Nbuf, NProcs, MPI_Int_t, mpi_nsend_baryon, NProcs, MPI_Int_t, MPI_COMM_WORLD);
	    for (ibuf=0;ibuf<NProcs*NProcs;ibuf++) mpi_nsend[ibuf]-=mpi_nsend_baryon[ibuf];
    }
    //and then send all the data between the read threads
    if (ThisTask<opt.nsnapread) {
    for(ibuf = 0; ibuf < opt.nsnapread; ibuf++)
    {
        if (ibuf!=ThisTask)
        {
            sendTask = ThisTask;
            recvTask = ibuf;
            if(mpi_nsend[ThisTask * NProcs + recvTask] > 0 || mpi_nsend[recvTask * NProcs + ThisTask] > 0)
            {
                //blocking point-to-point send and receive. Here must determine the appropriate offset point in the local export buffer
                //for sending data and also the local appropriate offset in the local the receive buffer for information sent from the local receiving buffer
                MPI_Sendrecv(&Pbuf[nreadoffset[recvTask]],sizeof(Particle)*mpi_nsend[ThisTask * NProcs + recvTask], MPI_BYTE, recvTask, TAG_IO_A,
                    &Part[Nlocal],sizeof(Particle)*mpi_nsend[recvTask * NProcs + ThisTask], MPI_BYTE, recvTask, TAG_IO_A, MPI_COMM_WORLD, &status);
                Nlocal+=mpi_nsend[recvTask * NProcs + ThisTask];
            }
            //if separate baryon search, send baryons too
            if (opt.iBaryonSearch && opt.partsearchtype!=PSTALL) {
                if(mpi_nsend_baryon[ThisTask * NProcs + recvTask] > 0 || mpi_nsend_baryon[recvTask * NProcs + ThisTask] > 0) 
                MPI_Sendrecv(&Pbuf[nreadoffset[recvTask]+mpi_nsend[ThisTask * NProcs + recvTask]],sizeof(Particle)*mpi_nsend_baryon[ThisTask * NProcs + recvTask], MPI_BYTE, recvTask, TAG_IO_B,
                    &Pbaryons[Nlocalbaryon[0]],sizeof(Particle)*mpi_nsend_baryon[recvTask * NProcs + ThisTask], MPI_BYTE, recvTask, TAG_IO_B, MPI_COMM_WORLD, &status);
                Nlocalbaryon[0]+=mpi_nsend_baryon[recvTask * NProcs + ThisTask];
            }
        }
    }
    if (opt.iBaryonSearch && opt.partsearchtype!=PSTALL) delete[] mpi_nsend_baryon;
    //set IDS
    for (i=0;i<Nlocal;i++) Part[i].SetID(i);
    if (opt.iBaryonSearch) for (i=0;i<Nlocalbaryon[0];i++) Pbaryons[i].SetID(i+Nlocal);
    }
#endif

#ifdef USEMPI
    if (ThisTask==0) {
#endif
    ///if gas found and Omega_b not set correctly (ie: ==0), assumes that
    ///lowest mass gas particle found corresponds to Omega_b
    ///Note that if there is mass evolution this WILL NOT WORK!
    if (opt.Omega_b==0 && MP_B==MAXVALUE){
        opt.Omega_b=MP_B/(MP_DM+MP_B)*opt.Omega_m;
        opt.Omega_cdm=opt.Omega_m-opt.Omega_b;
    }
    //adjust period
    if (opt.comove) opt.p*=opt.L/opt.h;
    else opt.p*=opt.L/opt.h*opt.a;
#ifdef USEMPI
    }
#endif
#ifdef HIGHRES
    opt.zoomlowmassdm=MP_DM*mscale;
#endif
#ifdef USEMPI
    MPI_Barrier(MPI_COMM_WORLD);
    //update cosmological data and boundary in code units
    MPI_Bcast(&(opt.p),sizeof(opt.p),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.a),sizeof(opt.a),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.Omega_cdm),sizeof(opt.Omega_cdm),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.Omega_b),sizeof(opt.Omega_b),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.Omega_m),sizeof(opt.Omega_m),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.Omega_Lambda),sizeof(opt.Omega_Lambda),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.h),sizeof(opt.h),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.rhobg),sizeof(opt.rhobg),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&(opt.virlevel),sizeof(opt.virlevel),MPI_BYTE,0,MPI_COMM_WORLD);
#ifdef NOMASS
    MPI_Bcast(&(opt.MassValue),sizeof(opt.MassValue),MPI_BYTE,0,MPI_COMM_WORLD);
#endif
    MPI_Bcast(&(Ntotal),sizeof(Ntotal),MPI_BYTE,0,MPI_COMM_WORLD);
    MPI_Bcast(&opt.zoomlowmassdm,sizeof(opt.zoomlowmassdm),MPI_BYTE,0,MPI_COMM_WORLD);
#endif
    ///If compiled with HIGHRES, the code assumes that the gadget data is a multi-resolution simulation
    ///with the lowest mass dark matter particle corresponding to the highest resolution and
    ///thus the physical linking length is assumed to be in fraction of interparticle spacing
    ///and is adjusted to a physical distance. Note that if high res and neff is not passed
    ///code assumes that lowest mass gas particle can be used to determine Omega_b and thus can be used
    ///to calculate the mean interparticle spacing.
    ///but one can also pass opt.Neff to adjust what the code thinks is the average inter particle spacing
#ifdef HIGHRES

    if (opt.Neff==-1) {
        //Once smallest mass particle is found (which should correspond to highest resolution area,
        if (opt.Omega_b==0) MP_B=0;
        LN=pow(((MP_DM+MP_B)*opt.M/opt.h)/(opt.Omega_m*3.0*opt.H*opt.h*opt.H*opt.h/(8.0*M_PI*opt.G)),1./3.)*opt.a;
    }
    else {
        LN=opt.p/(Double_t)opt.Neff;
    }
#endif
#ifdef USEMPI
    MPI_Bcast(&LN, 1, MPI_Real_t, 0, MPI_COMM_WORLD);
#endif
    ///if not an individual halo, assume cosmological and store scale of the highest resolution interparticle spacing to scale the physical FOF linking length 
    if (opt.iSingleHalo==0 || opt.icosmologicalin==1) 
    {
        opt.ellxscale=LN;
        opt.uinfo.eps*=LN;
    }
}
