/*! \file hdfitems.h
 *  \brief this file contains definitions and routines for reading HDF5 files
 * 
 *   NOTE: the routines are based on reading Illustris HDF outputs
 */

#ifndef HDFITEMS_H
#define HDFITEMS_H


#include "H5Cpp.h"
using namespace H5;

///\name ILLUSTRIS specific constants
//@{
///convert illustris metallicty to ratio to solar
#define ILLUSTRISZMET 1.0/0.0127 
//@}

///number of particle types
#define NHDFTYPE 6
///\name define hdf particle types
//@{
#define HDFGASTYPE 0
#define HDFDMTYPE 1
#define HDFWINDTYPE 2
#define HDFTRACERTYPE 3
#define HDFSTARTYPE 4
#define HDFBHTYPE 5
#define HDFEXTRATYPE 6
//@}

///\name number of entries in various data groups 
//@{
#define HDFHEADNINFO 11
#define HDFGASNINFO 20
#define HDFDMNINFO 7
#define HDFTRACERNINFO 3
#define HDFSTARNINFO 13
#define HDFBHNINFO 21
#define HDFMAXPINFO 40
//@}

///number of luminosity bands for stars
#define NLUMBANDS 8

///how many particle properties are read from file in one go
#define HDFCHUNKSIZE 100000

///how many data blocks are of  interest. That is what data blocks do I wish to load. pos,vel,mass,pid,U,Zmet,tage?
#define NHDFDATABLOCK 10
///here number shared by all particle types
#define NHDFDATABLOCKALL 4
//Maximum dimensionality of a datablock 
///example at most one needs a dimensionality of 13 for the tracer particles in Illustris for fluid related info
#define HDFMAXPROPDIM 13

///\name labels for HDF naming conventions 
//@{

///\name Structures for the HDF5 interface, primarily used to store the strings of Groups and DataSets
//@{
#define HDFILLUSTISNAMES 0 
#define HDFGADGETXNAMES 1
//@}

///This structures stores the strings defining the groups of data in the hdf input. NOTE: HERE I show the strings for Illustris format
struct HDF_Group_Names {
    //define the strings associated with the types of structures contained in the hdf file. 
    H5std_string Header_name;
    H5std_string GASpart_name;
    H5std_string DMpart_name;
    H5std_string EXTRApart_name;
    H5std_string TRACERpart_name;
    H5std_string STARpart_name;
    H5std_string BHpart_name;
    H5std_string part_names[NHDFTYPE];
    H5std_string names[NHDFTYPE+1];

    ///constructor
    HDF_Group_Names(){
        Header_name=H5std_string("Header");
        GASpart_name=H5std_string("PartType0");
        DMpart_name=H5std_string("PartType1");
        EXTRApart_name=H5std_string("PartType2");
        TRACERpart_name=H5std_string("PartType3");
        STARpart_name=H5std_string("PartType4");
        BHpart_name=H5std_string("PartType5");
        part_names[0]=GASpart_name;
        part_names[1]=DMpart_name;
        part_names[2]=EXTRApart_name;
        part_names[3]=TRACERpart_name;
        part_names[4]=STARpart_name;
        part_names[5]=BHpart_name;

        names[0]=Header_name;
        names[1]=GASpart_name;
        names[2]=DMpart_name;
        names[3]=EXTRApart_name;
        names[4]=TRACERpart_name;
        names[5]=STARpart_name;
        names[6]=BHpart_name;
    }
};

///data stored in the header group structure in the HDF format
struct HDF_Header {

    double      BoxSize;
    int         npart[NHDFTYPE];
    int         npartTotal[NHDFTYPE];
    int         npartTotalHW[NHDFTYPE];
    double      mass[NHDFTYPE];
    double      Omega0, OmegaLambda, HubbleParam;
    double      redshift, time;
    int         num_files;

    H5std_string names[HDFHEADNINFO];
    const static int IBoxSize  =0;
    const static int IMass     =1;
    const static int INuminFile=2;
    const static int INumTot   =3;
    const static int INumTotHW =4;
    const static int IOmega0   =5;
    const static int IOmegaL   =6;
    const static int IRedshift =7;
    const static int ITime     =8;
    const static int INumFiles =9;
    const static int IHubbleParam =10;

    ///constructor
    HDF_Header() {
        int itemp=0;
        names[itemp++]=H5std_string("BoxSize");
        names[itemp++]=H5std_string("MassTable");
        names[itemp++]=H5std_string("NumPart_ThisFile");
        names[itemp++]=H5std_string("NumPart_Total");
        names[itemp++]=H5std_string("NumPart_Total_HighWord");
        names[itemp++]=H5std_string("Omega0");
        names[itemp++]=H5std_string("OmegaLambda");
        names[itemp++]=H5std_string("Redshift");
        names[itemp++]=H5std_string("Time");
        names[itemp++]=H5std_string("NumFilesPerSnapshot");
        names[itemp++]=H5std_string("HubbleParam");
    }
};

struct HDF_Part_Info {
    H5std_string names[HDFMAXPINFO];
    int ptype;
    int nentries;
    //the HDF naming convenction for the data blocks. By default assumes ILLUSTRIS nameing convention
    //for simplicity, all particles have basic properties listed first, x,v,ids,mass in this order
    HDF_Part_Info(int PTYPE, int hdfnametype=HDFILLUSTISNAMES) {
        ptype=PTYPE;
        int itemp=0;
        if (ptype==HDFGASTYPE) {
        names[itemp++]=H5std_string("Coordinates");
        names[itemp++]=H5std_string("Velocities");
        names[itemp++]=H5std_string("ParticleIDs");
        names[itemp++]=H5std_string("Masses");
        names[itemp++]=H5std_string("Density");
        names[itemp++]=H5std_string("InternalEnergy");
        names[itemp++]=H5std_string("StarFormationRate");
        names[itemp++]=H5std_string("Metallicity");
	//always place the metacallity at position 7 in naming array
        if (hdfnametype==HDFILLUSTISNAMES) {
            //names[itemp++]=H5std_string("GFM_Metallicity");
            names[itemp++]=H5std_string("ElectronAbundance");
            names[itemp++]=H5std_string("NeutralHydrogenAbundance");
            names[itemp++]=H5std_string("Volume");
            names[itemp++]=H5std_string("SmoothingLength");
            names[itemp++]=H5std_string("Potential");
            names[itemp++]=H5std_string("SubfindDensity");
            names[itemp++]=H5std_string("SubfindHsml");
            names[itemp++]=H5std_string("SubfindVelDisp");
            names[itemp++]=H5std_string("GFM_AGNRadiation");
            names[itemp++]=H5std_string("GFM_CoolingRate");
            names[itemp++]=H5std_string("GFM_WindDMVelDisp");
            names[itemp++]=H5std_string("NumTracers");
        }
        nentries=itemp;
        }
        if (ptype==HDFDMTYPE) {
        names[itemp++]=H5std_string("Coordinates");
        names[itemp++]=H5std_string("Velocities");
        names[itemp++]=H5std_string("ParticleIDs");
        names[itemp++]=H5std_string("Masses");
	if (hdfnametype==HDFILLUSTISNAMES) {
            names[itemp++]=H5std_string("Potential");
            names[itemp++]=H5std_string("SubfindDensity");
            names[itemp++]=H5std_string("SubfindHsml");
            names[itemp++]=H5std_string("SubfindVelDisp");
        }
        nentries=itemp;
        }
        if (ptype==HDFTRACERTYPE) {
        names[itemp++]=H5std_string("FluidQuantities");
        names[itemp++]=H5std_string("ParentID");
        names[itemp++]=H5std_string("TracerID");
        nentries=itemp;
        }
        if (ptype==HDFSTARTYPE) {
        names[itemp++]=H5std_string("Coordinates");
        names[itemp++]=H5std_string("Velocities");
        names[itemp++]=H5std_string("ParticleIDs");
        names[itemp++]=H5std_string("Masses");
	names[itemp++]=H5std_string("Metallicity");
	names[itemp++]=H5std_string("StellarFormationTime");
        //for stars assume star formation and metallicy are position 4, 5 in name array
        if (hdfnametype==HDFILLUSTISNAMES) {
            //names[itemp++]=H5std_string("GFM_StellarFormationTime");
            //names[itemp++]=H5std_string("GFM_Metallicity");
            names[itemp++]=H5std_string("Potential");
            names[itemp++]=H5std_string("SubfindDensity");
            names[itemp++]=H5std_string("SubfindHsml");
            names[itemp++]=H5std_string("SubfindVelDisp");
            names[itemp++]=H5std_string("GFM_InitialMass");
            names[itemp++]=H5std_string("GFM_StellarPhotometrics");
            names[itemp++]=H5std_string("NumTracers");
        }
        nentries=itemp;
        }
        if (ptype==HDFBHTYPE) {
        names[itemp++]=H5std_string("Coordinates");
        names[itemp++]=H5std_string("Velocities");
        names[itemp++]=H5std_string("ParticleIDs");
        names[itemp++]=H5std_string("Masses");
        if (hdfnametype==HDFILLUSTISNAMES) {
            names[itemp++]=H5std_string("HostHaloMass");
            names[itemp++]=H5std_string("Potential");
            names[itemp++]=H5std_string("SubfindDensity");
            names[itemp++]=H5std_string("SubfindHsml");
            names[itemp++]=H5std_string("SubfindVelDisp");
            names[itemp++]=H5std_string("BH_CumEgyInjection_QM");
            names[itemp++]=H5std_string("BH_CumMassGrowth_QM");
            names[itemp++]=H5std_string("BH_Density");
            names[itemp++]=H5std_string("BH_Hsml");
            names[itemp++]=H5std_string("BH_Mass");
            names[itemp++]=H5std_string("BH_Mass_bubbles");
            names[itemp++]=H5std_string("BH_Mass_ini");
            names[itemp++]=H5std_string("BH_Mdot");
            names[itemp++]=H5std_string("BH_Pressure");
            names[itemp++]=H5std_string("BH_Progs");
            names[itemp++]=H5std_string("BH_U");
            names[itemp++]=H5std_string("NumTracers");
        }
        nentries=itemp;
        }
    }
};
//@}

/// \name Get the number of particles in the hdf files
//@{
inline Int_t HDF_get_nbodies(char *fname, int ptype, Options &opt) 
{
    char buf[2000],buf1[2000],buf2[2000];
    sprintf(buf1,"%s.0.hdf5",fname);
    sprintf(buf2,"%s.hdf5",fname);
    if (FileExists(buf1)) sprintf(buf,"%s",buf1);
    else if (FileExists(buf2)) sprintf(buf,"%s",buf2);
    else {
        printf("Error. Can't find snapshot!\nneither as `%s'\nnor as `%s'\n\n", buf1, buf2);
        exit(9);
    }

    H5File Fhdf;
    HDF_Group_Names hdf_gnames;
    //to store the groups, data sets and their associated data spaces
    Group headergroup;
    Attribute headerattribs;
    HDF_Header hdf_header_info;
    //buffers to load data
    int intbuff[NHDFTYPE];
    long long longbuff[NHDFTYPE];
    int j,k,ireaderror=0;
    Int_t nbodies=0;
    IntType inttype;

    int nusetypes,usetypes[NHDFTYPE];

    if (ptype==PSTALL) {
        //lets assume there are dm/stars/gas.
        nusetypes=3;
        usetypes[0]=HDFGASTYPE;usetypes[1]=HDFDMTYPE;usetypes[2]=HDFSTARTYPE;
        //now if also blackholes/sink particles increase number of types
        if (opt.iusesinkparticles) usetypes[nusetypes++]=HDFBHTYPE;
        if (opt.iusewindparticles) usetypes[nusetypes++]=HDFWINDTYPE;
    }
    else if (ptype==PSTDARK) {nusetypes=1;usetypes[0]=HDFDMTYPE;}
    else if (ptype==PSTGAS) {nusetypes=1;usetypes[0]=HDFGASTYPE;}
    else if (ptype==PSTSTAR) {nusetypes=1;usetypes[0]=HDFSTARTYPE;}
    else if (ptype==PSTBH) {nusetypes=1;usetypes[0]=HDFBHTYPE;}
    //else if (ptype==PSTNOBH) {nusetypes=3;usetypes[0]=0;usetypes[1]=1;usetypes[2]=4;}

    //Try block to detect exceptions raised by any of the calls inside it
    try
    {
        //turn off the auto-printing when failure occurs so that we can
        //handle the errors appropriately
        Exception::dontPrint();

        //Open the specified file and the specified dataset in the file.
        Fhdf.openFile(buf, H5F_ACC_RDONLY);
        cout<<"Loading HDF header info in header group: "<<hdf_gnames.Header_name<<endl;
        //get header group 
        headergroup=Fhdf.openGroup(hdf_gnames.Header_name);

        headerattribs=headergroup.openAttribute(hdf_header_info.names[hdf_header_info.INumTot]);
        inttype=headerattribs.getIntType();
        if (inttype.getSize()==sizeof(int)) {
            headerattribs.read(PredType::NATIVE_INT,&intbuff);
            for (j=0;j<NHDFTYPE;j++) hdf_header_info.npartTotal[j]=intbuff[j];
        }
        if (inttype.getSize()==sizeof(long long)) {
            headerattribs.read(PredType::NATIVE_LONG,&longbuff);
            for (j=0;j<NHDFTYPE;j++) hdf_header_info.npartTotal[j]=longbuff[j];
        }

        headerattribs=headergroup.openAttribute(hdf_header_info.names[hdf_header_info.INumTotHW]);
        inttype=headerattribs.getIntType();
        if (inttype.getSize()==sizeof(int)) {
            headerattribs.read(PredType::NATIVE_INT,&intbuff);
            for (j=0;j<NHDFTYPE;j++) hdf_header_info.npartTotalHW[j]=intbuff[j];
        }
        if (inttype.getSize()==sizeof(long long)) {
            headerattribs.read(PredType::NATIVE_LONG,&longbuff);
            for (j=0;j<NHDFTYPE;j++) hdf_header_info.npartTotalHW[j]=longbuff[j];
        }
    }
    catch(GroupIException error)
    {
        error.printError();
    }
    // catch failure caused by the H5File operations
    catch( FileIException error )
    {
        error.printError();

    }
    // catch failure caused by the DataSet operations
    catch( DataSetIException error )
    {
        error.printError();
        ireaderror=1;
    }
    // catch failure caused by the DataSpace operations
    catch( DataSpaceIException error )
    {
        error.printError();
        ireaderror=1;
    }
    // catch failure caused by the DataSpace operations
    catch( DataTypeIException error )
    {
        error.printError();
        ireaderror=1;
    }
    Fhdf.close();

    for(j=0, nbodies=0; j<nusetypes; j++) {
        k=usetypes[j];
        nbodies+=hdf_header_info.npartTotal[k];
        nbodies+=((long long)(hdf_header_info.npartTotalHW[k]) << 32);
    }
    return nbodies;

}//@}

#endif 
