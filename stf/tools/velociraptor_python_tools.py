import sys,os,os.path,string,time,re,struct
import math,operator
from pylab import *
import numpy as np
import h5py #import hdf5 interface
import tables as pytb #import pytables
import pandas as pd
from copy import deepcopy
from sklearn.neighbors import NearestNeighbors
import scipy.interpolate as scipyinterp
import scipy.spatial as spatial
import multiprocessing as mp

#would be good to compile these routines with cython
#try to speed up search
#cimport numpy as np

"""

Routines for reading velociraptor output
Note that this code is compatible with python2. 
For python3 please search for all print statemetns and replace them with print() style calls

"""

"""
    IO Routines
"""

def ReadPropertyFile(basefilename,ibinary=0,iseparatesubfiles=0,iverbose=0, desiredfields=[]):
    """
    VELOCIraptor/STF files in various formats
    for example ascii format contains
    a header with 
        filenumber number_of_files
        numhalos_in_file nnumhalos_in_total
    followed by a header listing the information contain. An example would be 
        ID(1) ID_mbp(2) hostHaloID(3) numSubStruct(4) npart(5) Mvir(6) Xc(7) Yc(8) Zc(9) Xcmbp(10) Ycmbp(11) Zcmbp(12) VXc(13) VYc(14) VZc(15) VXcmbp(16) VYcmbp(17) VZcmbp(18) Mass_tot(19) Mass_FOF(20) Mass_200mean(21) Mass_200crit(22) Mass_BN97(23) Efrac(24) Rvir(25) R_size(26) R_200mean(27) R_200crit(28) R_BN97(29) R_HalfMass(30) Rmax(31) Vmax(32) sigV(33) veldisp_xx(34) veldisp_xy(35) veldisp_xz(36) veldisp_yx(37) veldisp_yy(38) veldisp_yz(39) veldisp_zx(40) veldisp_zy(41) veldisp_zz(42) lambda_B(43) Lx(44) Ly(45) Lz(46) q(47) s(48) eig_xx(49) eig_xy(50) eig_xz(51) eig_yx(52) eig_yy(53) eig_yz(54) eig_zx(55) eig_zy(56) eig_zz(57) cNFW(58) Krot(59) Ekin(60) Epot(61) n_gas(62) M_gas(63) Xc_gas(64) Yc_gas(65) Zc_gas(66) VXc_gas(67) VYc_gas(68) VZc_gas(69) Efrac_gas(70) R_HalfMass_gas(71) veldisp_xx_gas(72) veldisp_xy_gas(73) veldisp_xz_gas(74) veldisp_yx_gas(75) veldisp_yy_gas(76) veldisp_yz_gas(77) veldisp_zx_gas(78) veldisp_zy_gas(79) veldisp_zz_gas(80) Lx_gas(81) Ly_gas(82) Lz_gas(83) q_gas(84) s_gas(85) eig_xx_gas(86) eig_xy_gas(87) eig_xz_gas(88) eig_yx_gas(89) eig_yy_gas(90) eig_yz_gas(91) eig_zx_gas(92) eig_zy_gas(93) eig_zz_gas(94) Krot_gas(95) T_gas(96) Zmet_gas(97) SFR_gas(98) n_star(99) M_star(100) Xc_star(101) Yc_star(102) Zc_star(103) VXc_star(104) VYc_star(105) VZc_star(106) Efrac_star(107) R_HalfMass_star(108) veldisp_xx_star(109) veldisp_xy_star(110) veldisp_xz_star(111) veldisp_yx_star(112) veldisp_yy_star(113) veldisp_yz_star(114) veldisp_zx_star(115) veldisp_zy_star(116) veldisp_zz_star(117) Lx_star(118) Ly_star(119) Lz_star(120) q_star(121) s_star(122) eig_xx_star(123) eig_xy_star(124) eig_xz_star(125) eig_yx_star(126) eig_yy_star(127) eig_yz_star(128) eig_zx_star(129) eig_zy_star(130) eig_zz_star(131) Krot_star(132) tage_star(133) Zmet_star(134) 

    then followed by data

    Note that a file will indicate how many files the total output has been split into

    Not all fields need be read in. If only want specific fields, can pass a string of desired fields like 
    ['ID', 'Mass_FOF', 'Krot']
    #todo still need checks to see if fields not present. 
    """
    #this variable is the size of the char array in binary formated data that stores the field names
    CHARSIZE=40

    start = time.clock()
    inompi=True
    if (iverbose): print "reading properties file",basefilename
    filename=basefilename+".properties"
    #load header
    if (os.path.isfile(filename)==True):
        numfiles=0
    else:
        filename=basefilename+".properties"+".0"
        inompi=False
        if (os.path.isfile(filename)==False):
            print "file not found"
            return []
    byteoffset=0
    #used to store fields, their type, etc
    fieldnames=[]
    fieldtype=[]
    fieldindex=[]

    if (ibinary==0):
        #load ascii file
        halofile = open(filename, 'r')
        #read header information
        [filenum,numfiles]=halofile.readline().split()
        filenum=int(filenum);numfiles=int(numfiles)
        [numhalos, numtothalos]= halofile.readline().split()
        numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
        names = ((halofile.readline())).split()
        #remove the brackets in ascii file names 
        fieldnames= [fieldname.split("(")[0] for fieldname in names]
        for i in np.arange(fieldnames.__len__()):
            fieldname=fieldnames[i]
            if fieldname in ["ID","hostHalo","numSubStruct","npart","n_gas","n_star"]:
                fieldtype.append(np.uint64)
            elif fieldname in ["ID_mbp"]:
                fieldtype.append(np.int64)
            else:
                fieldtype.append(np.float64)
        halofile.close()
        #if desiredfields is NULL load all fields
        #but if this is passed load only those fields
        if (len(desiredfields)>0): 
            lend=len(desiredfields)
            fieldindex=np.zeros(lend,dtype=int)
            desiredfieldtype=[[] for i in range(lend)]
            for i in range(lend):
                fieldindex[i]=fieldnames.index(desiredfields[i])
                desiredfieldtype[i]=fieldtype[fieldindex[i]]
            fieldtype=desiredfieldtype
            fieldnames=desiredfields
        #to store the string containing data format
        fieldtypestring=''
        for i in np.arange(fieldnames.__len__()):
            if fieldtype[i]==np.uint64: fieldtypestring+='u8,'
            elif fieldtype[i]==np.int64: fieldtypestring+='i8,'
            elif fieldtype[i]==np.float64: fieldtypestring+='f8,'

    elif (ibinary==1):
        #load binary file
        halofile = open(filename, 'rb')
        [filenum,numfiles]=np.fromfile(halofile,dtype=np.int32,count=2)
        [numhalos,numtothalos]=np.fromfile(halofile,dtype=np.uint64,count=2)
        headersize=np.fromfile(halofile,dtype=np.int32,count=1)[0]
        byteoffset=np.dtype(np.int32).itemsize*3+np.dtype(np.uint64).itemsize*2+4*headersize
        for i in range(headersize):
            fieldnames.append(unpack('s', halofile.read(CHARSIZE)).strip())
        for i in np.arange(fieldnames.__len__()):
            fieldname=fieldnames[i]
            if fieldname in ["ID","hostHalo","numSubStruct","npart","n_gas","n_star"]:
                fieldtype.append(np.uint64)
            elif fieldname in ["ID_mbp"]:
                fieldtype.append(np.int64)
            else:
                fieldtype.append(np.float64)
        halofile.close()
        #if desiredfields is NULL load all fields
        #but if this is passed load only those fields
        if (len(desiredfields)>0): 
            lend=len(desiredfields)
            fieldindex=np.zeros(lend,dtype=int)
            desiredfieldtype=[[] for i in range(lend)]
            for i in range(lend):
                fieldindex[i]=fieldnames.index(desiredfields[i])
                desiredfieldtype[i]=fieldtype[fieldindex[i]]
            fieldtype=desiredfieldtype
            fieldnames=desiredfields
        #to store the string containing data format
        fieldtypestring=''
        for i in np.arange(fieldnames.__len__()):
            if fieldtype[i]==np.uint64: fieldtypestring+='u8,'
            elif fieldtype[i]==np.int64: fieldtypestring+='i8,'
            elif fieldtype[i]==np.float64: fieldtypestring+='f8,'

    elif (ibinary==2):
        #load hdf file
        halofile = h5py.File(filename, 'r')
        filenum=int(halofile["File_id"][0])
        numfiles=int(halofile["Num_of_files"][0])
        numhalos=np.uint64(halofile["Num_of_groups"][0])
        numtothalos=np.uint64(halofile["Total_num_of_groups"][0])
        fieldnames=[str(n) for n in halofile.keys()]
        #clean of header info
        fieldnames.remove("File_id")
        fieldnames.remove("Num_of_files")
        fieldnames.remove("Num_of_groups")
        fieldnames.remove("Total_num_of_groups")
        fieldtype=[halofile[fieldname].dtype for fieldname in fieldnames]
        #if the desiredfields argument is passed only these fieds are loaded
        if (len(desiredfields)>0): 
            if (iverbose):print "Loading subset of all fields in property file ", len(desiredfields), " instead of ", len(fieldnames)
            fieldnames=desiredfields
            fieldtype=[halofile[fieldname].dtype for fieldname in fieldnames]
        halofile.close()

    #allocate memory that will store the halo dictionary
    catalog={fieldnames[i]:np.zeros(numtothalos,dtype=fieldtype[i]) for i in range(len(fieldnames))}
    noffset=0
    for ifile in range(numfiles):
        if (inompi==True): filename=basefilename+".properties"
        else: filename=basefilename+".properties"+"."+str(ifile)
        if (iverbose) : print "reading ",filename
        if (ibinary==0): 
            halofile = open(filename, 'r')
            halofile.readline()
            numhalos=np.uint64(halofile.readline().split()[0])
            halofile.close()
            if (numhalos>0):htemp = np.loadtxt(filename,skiprows=3, usecols=fieldindex, dtype=fieldtypestring, unpack=True)
        elif(ibinary==1):
            halofile = open(filename, 'rb')
            np.fromfile(halofile,dtype=np.int32,count=2)
            numhalos=np.fromfile(halofile,dtype=np.uint64,count=2)[0]
            #halofile.seek(byteoffset);
            if (numhalos>0):htemp=np.fromfile(halofile, usecols=fieldindex, dtype=fieldtypestring, unpack=True)
            halofile.close()
        elif(ibinary==2):
            #here convert the hdf information into a numpy array
            halofile = h5py.File(filename, 'r')
            numhalos=np.uint64(halofile["Num_of_groups"][0])
            if (numhalos>0):htemp=[np.array(halofile[catvalue]) for catvalue in fieldnames]
            halofile.close()
        #numhalos=len(htemp[0])
        for i in range(len(fieldnames)):
            catvalue=fieldnames[i]
            if (numhalos>0): catalog[catvalue][noffset:noffset+numhalos]=htemp[i]
        noffset+=numhalos
    #if subhalos are written in separate files, then read them too
    if (iseparatesubfiles==1):
        for ifile in range(numfiles):
            if (inompi==True): filename=basefilename+".sublevels"+".properties"
            else: filename=basefilename+".sublevels"+".properties"+"."+str(ifile)
            if (iverbose) : print "reading ",filename
            if (ibinary==0): 
                halofile = open(filename, 'r')
                halofile.readline()
                numhalos=np.uint64(halofile.readline().split()[0])
                halofile.close()
                if (numhalos>0):htemp = np.loadtxt(filename,skiprows=3, usecols=fieldindex, dtype=fieldtypestring, unpack=True)
            elif(ibinary==1):
                halofile = open(filename, 'rb')
                #halofile.seek(byteoffset);
                np.fromfile(halofile,dtype=np.int32,count=2)
                numhalos=np.fromfile(halofile,dtype=np.uint64,count=2)[0]
                if (numhalos>0):htemp=np.fromfile(halofile, usecols=fieldindex, dtype=fieldtypestring, unpack=True)
                halofile.close()
            elif(ibinary==2):
                halofile = h5py.File(filename, 'r')
                numhalos=np.uint64(halofile["Num_of_groups"][0])
                if (numhalos>0):htemp=[np.array(halofile[catvalue]) for catvalue in fieldnames]
                halofile.close()
            #numhalos=len(htemp[0])
            for i in range(len(fieldnames)):
                catvalue=fieldnames[i]
            if (numhalos>0): catalog[catvalue][noffset:noffset+numhalos]=htemp[i]
            noffset+=numhalos

    if (iverbose): print "done reading properties file ",time.clock()-start
    return catalog,numtothalos

def ReadPropertyFileMultiWrapper(basefilename,index,halodata,numhalos,ibinary=0,iseparatesubfiles=0,iverbose=0,desiredfields=[]):
    """
    Wrapper for multithreaded reading
    """
    #call read routine and store the data 
    halodata[index],numhalos[index]=ReadPropertyFile(basefilename,ibinary,iseparatesubfiles,iverbose,desiredfields)

def ReadPropertyFileMultiWrapperNamespace(index,basefilename,ns,ibinary=0,iseparatesubfiles=0,iverbose=0,desiredfields=[]):
    #call read routine and store the data 
    ns.hdata[index],ns.ndata[index]=ReadPropertyFile(basefilename,ibinary,iseparatesubfiles,iverbose,desiredfields)

def ReadHaloMergerTree(treefilename,ibinary=0,iverbose=0):
    """
    VELOCIraptor/STF merger tree in ascii format contains 
    a header with 
        number_of_snapshots
        a description of how the tree was built
        total number of halos across all snapshots

    then followed by data
    for each snapshot 
        snapshotvalue numhalos
        haloid_1 numprogen_1
        progenid_1
        progenid_2
        ...
        progenid_numprogen_1
        haloid_2 numprogen_2
        .
        .
        .
    one can also have an output format that has an additional field for each progenitor, the meritvalue    

    """
    start = time.clock()
    tree=[]
    if (iverbose): print "reading Tree file",treefilename,os.path.isfile(treefilename)
    if (os.path.isfile(treefilename)==False):
        print "Error, file not found"
        return tree
    #if ascii format
    if (ibinary==0):
        treefile = open(treefilename, 'r')
        numsnap=int(treefile.readline())
        descrip=treefile.readline().strip()
        tothalos=int(treefile.readline())
        tree=[{"haloID": [], "Num_progen": [], "Progen": []} for i in range(numsnap)]
        offset=0
        totalnumprogen=0
        for i in range(numsnap-1):
            [snapval,numhalos]=treefile.readline().strip().split('\t')
            snapval=int(snapval);numhalos=int(numhalos)
            #if really verbose
            if (iverbose==2): print snapval,numhalos
            tree[i]["haloID"]=np.zeros(numhalos, dtype=np.int64)
            tree[i]["Num_progen"]=np.zeros(numhalos, dtype=np.int32)
            tree[i]["Progen"]=[[] for j in range(numhalos)]
            for j in range(numhalos):
                [hid,nprog]=treefile.readline().strip().split('\t')
                hid=np.int64(hid);nprog=int(nprog)
                tree[i]["haloID"][j]=hid
                tree[i]["Num_progen"][j]=nprog
                totalnumprogen+=nprog
                if (nprog>0):
                    tree[i]["Progen"][j]=np.zeros(nprog,dtype=np.int64)
                    for k in range(nprog):
                        tree[i]["Progen"][j][k]=np.int64(treefile.readline())
    if (iverbose): print "done reading tree file ",time.clock()-start
    return tree

def ReadHaloPropertiesAcrossSnapshots(numsnaps,snaplistfname,inputtype,iseperatefiles,iverbose=0,desiredfields=[]):
    """
    read halo data from snapshots listed in file with snaplistfname file name 
    """
    halodata=[[] for j in range(numsnaps)]
    ngtot=[0 for j in range(numsnaps)]
    start=time.clock()
    print "reading data"
    #if there are a large number of snapshots to read, read in parallel
    #only read in parallel if worthwhile, specifically if large number of snapshots and snapshots are ascii
    iparallel=(numsnaps>20 and inputtype==2)
    if (iparallel):
        #determine maximum number of threads
        nthreads=min(mp.cpu_count(),numsnaps)
        nchunks=int(np.ceil(numsnaps/float(nthreads)))
        print "Using", nthreads,"threads to parse ",numsnaps," snapshots in ",nchunks,"chunks"
        #load file names
        snapnamelist=open(snaplistfname[i],'r')
        catfilename=["" for j in range(numsnaps)]
        for j in range(numsnaps):
            catfilename[j]=snapnamelist.readline().strip()+".properties"
        #allocate a manager
        manager = mp.Manager()
        #use manager to specify the dictionary and list that can be accessed by threads
        hdata=manager.list([manager.dict() for j in range(numsnaps)])
        ndata=manager.list([0 for j in range(numsnaps)])
        #now for each chunk run a set of proceses
        for j in range(nchunks):
            offset=j*nthreads
            #if last chunk then must adjust nthreads
            if (j==nchunks-1):
                nthreads=numsnaps-offset
            #when calling a process pass manager based proxies, which then are used to copy data back
            processes=[mp.Process(target=ReadPropertyFileMultiWrapper,args=(catfilename[offset+k],k+offset,hdata,ndata,inputtype,iseparatefiles,iverbose,desiredfields)) for k in range(nthreads)]
            #start each process
            #store the state of each thread, alive or not, and whether it has finished
            activethreads=[[True,False] for k in range(nthreads)]
            count=0
            for p in processes:
                print "reading", catfilename[offset+count]
                p.start()
                #space threads apart (join's time out is 0.25 seconds
                p.join(0.2)
                count+=1
            totactivethreads=nthreads
            while(totactivethreads>0):
                count=0
                for p in processes:
                    #join thread and see if still active
                    p.join(0.5)
                    if (p.is_alive()==False):
                        #if thread nolonger active check if its been processed
                        if (activethreads[count][1]==False):
                            #make deep copy of manager constructed objects that store data
                            #halodata[i][offset+count]=deepcopy(hdata[offset+count])
                            #try instead init a dictionary
                            halodata[i][offset+count]=dict(hdata[offset+count])
                            ngtot[i][offset+count]=ndata[offset+count]
                            #effectively free the data in manager dictionary
                            hdata[offset+count]=[]
                            activethreads[count][0]=False
                            activethreads[count][1]=True
                            totactivethreads-=1
                    count+=1
            #terminate threads
            for p in processes:
                p.terminate()

    else:
        snapnamelist=open(snaplistfname[i],'r')
        for j in range(0,numsnaps):
            catfilename=snapnamelist.readline().strip()+".properties"
            print "reading ", catfilename
            halodata[i][j],ngtot[i][j] = ReadPropertyFile(catfilename,inputtype,iseparatefiles,iverbose,desiredfields)
    print "data read in ",time.clock()-start
    return halodata,ngtot

def ReadCrossCatalogList(fname,meritlim=0.1,iverbose=0):
    """
    Reads a cross catalog produced by halomergertree, 
    also allows trimming of cross catalog using a higher merit threshold than one used to produce catalog
    """
    start = time.clock()
    if (iverbose): print "reading cross catalog"
    dfile=open(fname,"r")
    dfile.readline()
    dfile.readline()
    dataline=(dfile.readline().strip()).split('\t')
    ndata=np.int32(dataline[1])
    pdata=CrossCatalogList(ndata)
    for i in range(0,ndata):
        data=(dfile.readline().strip()).split('\t')
        nmatches=np.int32(data[1])
        for j in range(0,nmatches):
            data=(dfile.readline().strip()).split(' ')
            meritval=np.float32(data[1])
            nsharedval=np.float32(data[2])
            if(meritval>meritlim):
                nmatchid=np.int64(data[0])
                pdata.matches[i].append(nmatchid)
                pdata.matches[i].append(meritval)
                pdata.nsharedfrac[i].append(nsharedval)
                pdata.nmatches[i]+=1
    dfile.close()
    if (iverbose): print "done reading cross catalog ",time.clock()-start
    return pdata

"""
    Routines to build a hierarchy structure (both spatially and temporally)
"""

def BuildHierarchy(halodata,iverbose=0):
    """
    the halo data stored in a velociraptor .properties file should store the id of its parent halo. Here 
    this catalog is used to produce a hierarchy to quickly access the relevant subhaloes of a parent halo.
    We could 
    """
    halohierarchy=[]
    start=time.clock()
    if (iverbose): print "setting hierarchy"
    numhalos=len(halodata["npart"])
    subhaloindex=np.where(halodata["hostHaloID"]!=-1)
    lensub=len(subhaloindex[0])
    haloindex=np.where(halodata["hostHaloID"]==-1)
    lenhal=len(haloindex[0])
    halohierarchy=[[] for k in range(numhalos)]
    if (iverbose): print "prelims done ",time.clock()-start
    for k in range(lenhal):
        halohierarchy[haloindex[0][k]]=np.where(halodata["hostHaloID"]==halodata["ID"][haloindex[0][k]])
    #NOTE: IMPORTANT this is only adding the subsub halos! I need to eventually parse the hierarchy 
    #data first to deteremine the depth of the subhalo hierarchy and store how deep an object is in the hierarchy
    #then I can begin adding (sub)subhalos to parent subhalos from the bottom level up
    """
    for k in range(0,len(halodata["npart"])):
        hid=np.int32(halodata["hostHaloID"][k])
        if (hid>-1 and halohierarchy[k]!=[]):
            halohierarchy[hid]=np.append(np.int32(halohierarchy[hid]),halohierarchy[k])
    """
    if (iverbose): print "hierarchy set in read in ",time.clock()-start
    return halohierarchy

def TraceMainProgen(istart,ihalo,numsnaps,numhalos,halodata,tree,HALOIDVAL):
    """
    Follows a halo along three to identify main progenitor
    """
    #start at this snapshot
    k=istart
    #see if halo does not have a tail (descendant set).
    if (halodata[k]['Tail'][ihalo]==0):
        #if halo has not had a tail set the branch needs to be walked along the main branch
        haloid=halodata[k]['ID'][ihalo]
        #only set the head if it has not been set 
        #otherwise it should have already been set and just need to store the root head
        if (halodata[k]['Head'][ihalo]==0):
            halodata[k]['Head'][ihalo]=haloid
            halodata[k]['HeadSnap'][ihalo]=k
            halodata[k]['RootHead'][ihalo]=haloid
            halodata[k]['RootHeadSnap'][ihalo]=k
            roothead,rootsnap,rootindex=haloid,k,ihalo
        else:
            roothead=halodata[k]['RootHead'][ihalo]
            rootsnap=halodata[k]['RootHeadSnap'][ihalo]
            rootindex=int(roothead%HALOIDVAL)-1
        #now move along tree first pass to store head and tails and root heads of main branch
        while (True):
            #instead of seraching array make use of the value of the id as it should be in id order
            #wdata=np.where(tree[k]['haloID']==haloid)
            #w2data=np.where(halodata[k]['ID']==haloid)[0][0]
            wdata=w2data=int(haloid%HALOIDVAL)-1
            halodata[k]['Num_progen'][wdata]=tree[k]['Num_progen'][wdata]
            #if no more progenitors, break from search
            #if (tree[k]['Num_progen'][wdata[0][0]]==0 or len(wdata[0])==0):
            if (tree[k]['Num_progen'][wdata]==0):
                #store for current halo its tail and root tail info (also store root tail for root head)
                halodata[k]['Tail'][w2data]=haloid
                halodata[k]['TailSnap'][w2data]=k
                halodata[k]['RootTail'][w2data]=haloid
                halodata[k]['RootTailSnap'][w2data]=k
                #only set the roots tail if it has not been set before (ie: along the main branch of root halo)
                #if it has been set then we are walking along a secondary branch of the root halo's tree
                if (halodata[rootsnap]['RootTail'][rootindex]==0):
                    halodata[rootsnap]['RootTail'][rootindex]=haloid
                    halodata[rootsnap]['RootTailSnap'][rootindex]=k
                break

            #store main progenitor
            #mainprog=tree[k]['Progen'][wdata[0][0]][0]
            mainprog=tree[k]['Progen'][wdata][0]
            #calculate stepsize based on the halo ids 
            stepsize=int(((haloid-haloid%HALOIDVAL)-(mainprog-mainprog%HALOIDVAL))/HALOIDVAL)
            #store tail 
            halodata[k]['Tail'][w2data]=mainprog
            halodata[k]['TailSnap'][w2data]=k+stepsize
            k+=stepsize

            #instead of searching array make use of the value of the id as it should be in id order
            #for progid in tree[k-stepsize]['Progen'][wdata[0][0]]:
            #    wdata3=np.where(halodata[k]['ID']==progid)[0][0]
            for progid in tree[k-stepsize]['Progen'][wdata]:
                wdata3=int(progid%HALOIDVAL)-1
                halodata[k]['Head'][wdata3]=haloid
                halodata[k]['HeadSnap'][wdata3]=k-stepsize
                halodata[k]['RootHead'][wdata3]=roothead
                halodata[k]['RootHeadSnap'][wdata3]=rootsnap

            #then store next progenitor
            haloid=mainprog

def TraceMainProgenParallelChunk(istart,ihalochunk,numsnaps,numhalos,halodata,tree,HALOIDVAL):
    for ihalo in ihalochunk:
        TraceMainProgen(istart,ihalo,numsnaps,numhalos,halodata,tree,HALOIDVAL)

def BuildTemporalHeadTail(numsnaps,tree,numhalos,halodata,HALOIDVAL=1000000000000, iverbose=1):
    """
    Adds for each halo its Head and Tail and stores Roothead and RootTail to the halo
    properties file
    HALOIDVAL is used to parse the halo ids and determine the step size between descendant and progenitor
    """
    print "Building Temporal catalog with head and tails"
    for k in range(numsnaps):
        halodata[k]['Head']=np.zeros(numhalos[k],dtype=np.int64)
        halodata[k]['Tail']=np.zeros(numhalos[k],dtype=np.int64)
        halodata[k]['HeadSnap']=np.zeros(numhalos[k],dtype=np.int32)
        halodata[k]['TailSnap']=np.zeros(numhalos[k],dtype=np.int32)
        halodata[k]['RootHead']=np.zeros(numhalos[k],dtype=np.int64)
        halodata[k]['RootTail']=np.zeros(numhalos[k],dtype=np.int64)
        halodata[k]['RootHeadSnap']=np.zeros(numhalos[k],dtype=np.int32)
        halodata[k]['RootTailSnap']=np.zeros(numhalos[k],dtype=np.int32)
        halodata[k]['Num_progen']=np.zeros(numhalos[k],dtype=np.uint32)
    #for each snapshot identify halos that have not had their tail set
    #for these halos, the main branch must be walked
    #allocate python manager to wrapper the tree and halo catalog so they can be altered in parallel
    manager=mp.Manager()
    chunksize=5000000 #have each thread handle this many halos at once
    #init to that at this point snapshots should be run in parallel
    if (numhalos[0]>2*chunksize): iparallel=1
    else: iparallel=-1 #no parallel at all
    iparallel=-1

    totstart=time.clock()

    if (iparallel==1):
        #need to copy halodata as this will be altered
        if (iverbose>0): print "copying halo"
        start=time.clock()
        mphalodata=manager.list([manager.dict(halodata[k]) for k in range(numsnaps)])
        if (iverbose>0): print "done",time.clock()-start

    for istart in range(numsnaps):
        if (iverbose>0): print "Starting from halos at ",istart,"with",numhalos[istart]
        if (numhalos[istart]==0): continue
        #if the number of halos is large then run in parallel
        if (numhalos[istart]>2*chunksize and iparallel==1):
            #determine maximum number of threads
            nthreads=int(min(mp.cpu_count(),ceil(numhalos[istart]/float(chunksize))))
            nchunks=int(np.ceil(numhalos[istart]/float(chunksize)/float(nthreads)))
            if (iverbose>0): print "Using", nthreads,"threads to parse ",numhalos[istart]," halos in ",nchunks,"chunks, each of size", chunksize
            #now for each chunk run a set of proceses
            for j in range(nchunks):
                start=time.clock()
                offset=j*nthreads*chunksize
                #if last chunk then must adjust nthreads
                if (j==nchunks-1):
                    nthreads=int(ceil((numhalos[istart]-offset)/float(chunksize)))

                halochunk=[range(offset+k*chunksize,offset+(k+1)*chunksize) for k in range(nthreads)]
                #adjust last chunk
                if (j==nchunks-1):
                    halochunk[-1]=range(offset+(nthreads-1)*chunksize,numhalos[istart])
                #when calling a process pass not just a work queue but the pointers to where data should be stored
                processes=[mp.Process(target=TraceMainProgenParallelChunk,args=(istart,halochunk[k],numsnaps,numhalos,mphalodata,tree,HALOIDVAL)) for k in range(nthreads)]
                count=0
                for p in processes:
                    print count+offset,k,min(halochunk[count]),max(halochunk[count])
                    p.start()
                    count+=1
                for p in processes:
                    #join thread and see if still active
                    p.join()
                if (iverbose>1): print (offset+j*nthreads*chunksize)/float(numhalos[istart])," done in",time.clock()-start
        #otherwise just single
        else :
            #if first time entering non parallel section copy data back from parallel manager based structure to original data structure
            #as parallel structures have been updated
            if (iparallel==1):
                #tree=[dict(mptree[k]) for k in range(numsnaps)]
                halodata=[dict(mphalodata[k]) for k in range(numsnaps)]
                #set the iparallel flag to 0 so that all subsequent snapshots (which should have fewer objects) not run in parallel
                #this is principly to minimize the amount of copying between manager based parallel structures and the halo/tree catalogs
                iparallel=0
            start=time.clock()
            chunksize=max(int(0.10*numhalos[istart]),10)
            for j in range(numhalos[istart]):
                #start at this snapshot
                #start=time.clock()
                TraceMainProgen(istart,j,numsnaps,numhalos,halodata,tree,HALOIDVAL)
                if (j%chunksize==0 and j>0):
                    if (iverbose>1): print "done", j/float(numhalos[istart]), "in", time.clock()-start
                    start=time.clock()
    if (iverbose>0): print "done with first bit"
    #now have walked all the main branches and set the root head, head and tail values
    #and can set the root tail of all halos. Start at end of the tree and move in reverse setting the root tail
    #of a halo's head so long as that halo's tail is the current halo (main branch)
    for istart in range(numsnaps-1,-1,-1):
        for j in range(numhalos[istart]):
            #if a halo's root tail is itself then start moving up its along to its head (if its head is not itself as well
            k=istart
            #rootheadid,rootheadsnap=halodata[k]['RootHead'][j],halodata[k]['RootHeadSnap'][j]
            roottailid,roottailsnap=halodata[k]['RootTail'][j],halodata[k]['RootTailSnap'][j]
            headid,headsnap=halodata[k]['Head'][j],halodata[k]['HeadSnap'][j]
            if (roottailid==halodata[k]['ID'][j] and headid!=halodata[k]['ID'][j]):
                #headindex=np.where(halodata[headsnap]['ID']==headid)[0][0]
                headindex=int(headid%HALOIDVAL)-1
                headtailid,headtailsnap=halodata[headsnap]['Tail'][headindex],halodata[headsnap]['TailSnap'][headindex]
                haloid=halodata[k]['ID'][j]
                #only proceed in setting root tails of a head who's tail is the same as halo (main branch) till we reach a halo who is its own head
                while (headtailid==haloid and headid!=haloid):
                    #set root tails
                    halodata[headsnap]['RootTail'][headindex]=roottailid
                    halodata[headsnap]['RootTailSnap'][headindex]=roottailsnap
                    #move to next head
                    haloid=halodata[headsnap]['ID'][headindex]
                    #haloindex=np.where(halodata[headsnap]['ID']==haloid)[0][0]
                    haloindex=int(haloid%HALOIDVAL)-1
                    halosnap=headsnap
                    headid,headsnap=halodata[halosnap]['Head'][haloindex],halodata[halosnap]['HeadSnap'][haloindex]
                    headindex=int(headid%HALOIDVAL)-1
                    #store the tail of the next head
                    headtailid,headtailsnap=halodata[headsnap]['Tail'][headindex],halodata[headsnap]['TailSnap'][headindex]
    print "Done building", time.clock()-totstart

"""
Adjust halo catalog for period, comoving coords, etc
"""

def AdjustforPeriod(numsnaps,numhalos,boxsize,hval,atime,halodata,icomove=0):
    """
    Map halo positions from 0 to box size
    """
    for i in range(numsnaps):
        if (icomove):
            boxval=boxsize/hval
        else:
            boxval=boxsize*atime[i]/hval
        wdata=np.where(halodata[i]["Xc"]<0)
        halodata[i]["Xc"][wdata]+=boxval
        wdata=np.where(halodata[i]["Yc"]<0)
        halodata[i]["Yc"][wdata]+=boxval
        wdata=np.where(halodata[i]["Zc"]<0)
        halodata[i]["Zc"][wdata]+=boxval

        wdata=np.where(halodata[i]["Xc"]>boxval)
        halodata[i]["Xc"][wdata]-=boxval
        wdata=np.where(halodata[i]["Yc"]>boxval)
        halodata[i]["Yc"][wdata]-=boxval
        wdata=np.where(halodata[i]["Zc"]>boxval)
        halodata[i]["Zc"][wdata]-=boxval

def AdjustComove(itocomovefromphysnumsnaps,numsnaps,numhalos,atime,halodata,igas=0,istar=0):
    """
    Convert distances to/from physical from/to comoving
    """
    for i in range(numsnaps):
        if (numhalos[i]==0): continue
        #converting from physical to comoving
        if (itocomovefromphysnumsnaps==1):
            fac=float(1.0/atime[i])
        #converting from comoving to physical
        else:
            fac=float(atime[i])
        if (fac==1): continue

        #convert physical distances
        halodata[i]["Xc"]*=fac
        halodata[i]["Yc"]*=fac
        halodata[i]["Zc"]*=fac
        halodata[i]["Xcmbp"]*=fac
        halodata[i]["Ycmbp"]*=fac
        halodata[i]["Zcmbp"]*=fac

        #sizes
        halodata[i]["Rvir"]*=fac
        halodata[i]["R_size"]*=fac
        halodata[i]["R_200mean"]*=fac
        halodata[i]["R_200crit"]*=fac
        halodata[i]["R_BN97"]*=fac
        halodata[i]["Rmax"]*=fac
        halodata[i]["R_HalfMass"]*=fac

        #if gas
        if (igas):
            halodata[i]["Xc_gas"]*=fac
            halodata[i]["Yc_gas"]*=fac
            halodata[i]["Zc_gas"]*=fac
            halodata[i]["R_HalfMass_gas"]*=fac

        #if stars
        if (istar):
            halodata[i]["Xc_star"]*=fac
            halodata[i]["Yc_star"]*=fac
            halodata[i]["Zc_star"]*=fac
            halodata[i]["R_HalfMass_star"]*=fac

"""
Code to use individual snapshot files and merge them together into a full unified hdf file containing information determined from the tree
"""

def ProduceUnifiedTreeandHaloCatalog(fname,numsnaps,tree,numhalos,halodata,atime,
    descripdata={'Title':'Tree and Halo catalog of sim', 'VELOCIraptor_version':1.15, 'Tree_version':1.1, 'Particle_num_threshold':20, 'Temporal_linking_length':1, 'Flag_gas':False, 'Flag_star':False, 'Flag_bh':False},
    cosmodata={'Omega_m':1.0, 'Omega_b':0., 'Omega_Lambda':0., 'Hubble_param':1.0,'BoxSize':1.0, 'Sigma8':1.0},
    unitdata={'UnitLength_in_Mpc':1.0, 'UnitVelocity_in_kms':1.0,'UnitMass_in_Msol':1.0, 'Flag_physical_comoving':True,'Flag_hubble_flow':False},
    partdata={'Flag_gas':False, 'Flag_star':False, 'Flag_bh':False},
    ibuildheadtail=0, icombinefile=1):

    """

    produces a unifed HDF5 formatted file containing the full catalog plus information to walk the tree
    \ref BuildTemporalHeadTail must have been called before otherwise it is called.
    Code produces a file for each snapshot
    The keys are the same as that contained in the halo catalog dictionary with the addition of 
    Num_of_snaps, and similar header info contain in the VELOCIraptor hdf files, ie Num_of_groups, Total_num_of_groups

    \todo don't know if I should use multiprocessing here to write files in parallel. IO might not be ideal

    """
    if (ibuildheadtail==1): 
        BuildTemporalHeadTail(numsnaps,tree,numhalos,halodata)
    totnumhalos=sum(numhalos)
    if (icombinefile==1):
        hdffile=h5py.File(fname+".snap.hdf.data",'w')
        headergrp=hdffile.create_group("Header")
        #store useful information such as number of snapshots, halos, 
        #cosmology (Omega_m,Omega_b,Hubble_param,Omega_Lambda, Box size)
        #units (Physical [1/0] for physical/comoving flag, length in Mpc, km/s, solar masses, Gravity
        #and HALOIDVAL used to traverse tree information (converting halo ids to haloindex or snapshot), Reverse_order [1/0] for last snap listed first)
        #set the attributes of the header
        headergrp.attrs["NSnaps"]=numsnaps
        #overall description
        #simulation box size

        #cosmological params
        cosmogrp=headergrp.create_group("Cosmology")
        for key in cosmodata.keys():
            cosmogrp.attrs[key]=cosmodata[key]
        #unit params
        unitgrp=headergrp.create_group("Units")
        for key in unitdata.keys():
            unitgrp.attrs[key]=unitdata[key]
        #particle types 
        partgrp=headergrp.create_group("Parttypes")
        partgrp.attrs["Flag_gas"]=descripdata["Flag_gas"]
        partgrp.attrs["Flag_star"]=descripdata["Flag_star"]
        partgrp.attrs["Flag_bh"]=descripdata["Flag_bh"]

        for i in range(numsnaps):
            snapgrp=hdffile.create_group("Snap_%03d"%(numsnaps-1-i))
            snapgrp.attrs["Snapnum"]=i
            snapgrp.attrs["NHalos"]=numhalos[i]
            snapgrp.attrs["scalefactor"]=atime[i]
            for key in halodata[i].keys():
                snapgrp.create_dataset(key,data=halodata[i][key])
        hdffile.close()
    else:
        for i in range(numsnaps):
            hdffile=h5py.File(fname+".snap_%03d.hdf.data"%(numsnaps-1-i),'w')
            hdffile.create_dataset("Snap_value",data=np.array([i],dtype=np.uint32))
            hdffile.create_dataset("Num_of_snaps",data=np.array([numsnaps],dtype=np.uint32))
            hdffile.create_dataset("Num_of_groups",data=np.array([numhalos[i]],dtype=np.uint64))
            hdffile.create_dataset("Total_num_of_groups",data=np.array([totnumhalos],dtype=np.uint64))
            hdffile.create_dataset("scalefactor",data=np.array([atime[i]],dtype=np.float64))
            for key in halodata[i].keys():
                hdffile.create_dataset(key,data=halodata[i][key])
            hdffile.close()

    hdffile=h5py.File(fname+".tree.hdf.data",'w')
    hdffile.create_dataset("Num_of_snaps",data=np.array([numsnaps],dtype=np.uint32))
    hdffile.create_dataset("Total_num_of_groups",data=np.array([totnumhalos],dtype=np.uint64))
    hdffile.create_dataset("Num_of_groups",data=np.array([numhalos],dtype=np.uint64))
    for i in range(numsnaps):
        snapgrp=hdffile.create_group("Snap_%03d"%(numsnaps-1-i))
        for key in tree[i].keys():
            """
            #to be completed for progenitor list
            if (key=="Progen"):
                for j in range(numhalos[i]):
                    halogrp=snapgrp.create_group("Halo"+str(j))
                    halogrp.create_dataset(key,data=tree[i][key][j])
            else:
                snapgrp.create_dataset(key,data=tree[i][key])
            """
            if (key=="Progen"): continue
            snapgrp.create_dataset(key,data=tree[i][key])
    hdffile.close()

def ProduceCombinedUnifiedTreeandHaloCatalog(fname,numsnaps,tree,numhalos,halodata,atime,
    descripdata={'Title':'Tree and Halo catalog of sim', 'VELOCIraptor_version':1.15, 'Tree_version':1.1, 'Particle_num_threshold':20, 'Temporal_linking_length':1, 'Flag_gas':False, 'Flag_star':False, 'Flag_bh':False},
    cosmodata={'Omega_m':1.0, 'Omega_b':0., 'Omega_Lambda':0., 'Hubble_param':1.0,'BoxSize':1.0, 'Sigma8':1.0},
    unitdata={'UnitLength_in_Mpc':1.0, 'UnitVelocity_in_kms':1.0,'UnitMass_in_Msol':1.0, 'Flag_physical_comoving':True,'Flag_hubble_flow':False},
    partdata={'Flag_gas':False, 'Flag_star':False, 'Flag_bh':False},
    ibuildheadtail=0,ibuildmajormergers=0, HALOIDVAL=1000000000000):

    """
    produces a unifed HDF5 formatted file containing the full catalog plus information to walk the tree
    \ref BuildTemporalHeadTail must have been called before otherwise it is called.
    Code produces a file for each snapshot
    The keys are the same as that contained in the halo catalog dictionary with the addition of 
    Num_of_snaps, and similar header info contain in the VELOCIraptor hdf files, ie Num_of_groups, Total_num_of_groups

    \todo don't know if I should use multiprocessing here to write files in parallel. IO might not be ideal

    Here the halodata is the dictionary contains the information

    """

    if (ibuildheadtail==1): 
        BuildTemporalHeadTail(numsnaps,tree,numhalos,halodata)
    if (ibuildmajormergers==1): 
        IdentifyMergers(numsnaps,tree,numhalos,halodata,boxsize,hval,atime)
    hdffile=h5py.File(fname+".snap.hdf.data",'w')
    headergrp=hdffile.create_group("Header")
    #store useful information such as number of snapshots, halos, 
    #cosmology (Omega_m,Omega_b,Hubble_param,Omega_Lambda, Box size)
    #units (Physical [1/0] for physical/comoving flag, length in Mpc, km/s, solar masses, Gravity
    #and HALOIDVAL used to traverse tree information (converting halo ids to haloindex or snapshot), Reverse_order [1/0] for last snap listed first)
    #set the attributes of the header
    headergrp.attrs["NSnaps"]=numsnaps
    #overall description
    headergrp.attrs["Title"]=descripdata["Title"]
    #simulation box size
    headergrp.attrs["BoxSize"]=cosmodata["BoxSize"]
    findergrp=headergrp.create_group("HaloFinder")
    findergrp.attrs["Name"]="VELOCIraptor"
    findergrp.attrs["Version"]=descripdata["VELOCIraptor_version"]
    findergrp.attrs["Particle_num_threshold"]=descripdata["Particle_num_threshold"]
    
    treebuildergrp=headergrp.create_group("TreeBuilder")
    treebuildergrp.attrs["Name"]="VELOCIraptor-Tree"
    treebuildergrp.attrs["Version"]=descripdata["Tree_version"]
    treebuildergrp.attrs["Temporal_linking_length"]=descripdata["Temporal_linking_length"]
    
    #cosmological params
    cosmogrp=headergrp.create_group("Cosmology")
    for key in cosmodata.keys():
        if (key!='BoxSize'): cosmogrp.attrs[key]=cosmodata[key]
    #unit params
    unitgrp=headergrp.create_group("Units")
    for key in unitdata.keys():
        unitgrp.attrs[key]=unitdata[key]
    #particle types 
    partgrp=headergrp.create_group("Parttypes")
    partgrp.attrs["Flag_gas"]=descripdata["Flag_gas"]
    partgrp.attrs["Flag_star"]=descripdata["Flag_star"]
    partgrp.attrs["Flag_bh"]=descripdata["Flag_bh"]

    #now have finished with header

    #now need to create groups for halos and then a group containing tree information
    snapsgrp=hdffile.create_group("Snapshots")
    #internal tree keys
    treekeys=["RootHead", "RootHeadSnap", "Head", "HeadSnap", "Tail", "TailSnap", "RootTail", "RootTailSnap", "Num_progen"]

    for i in range(numsnaps):
        #note that I normally have information in reverse order so that might be something in the units
        snapgrp=snapsgrp.create_group("Snap_%03d"%(numsnaps-1-i))
        snapgrp.attrs["Snapnum"]=i
        snapgrp.attrs["NHalos"]=numhalos[i]
        snapgrp.attrs["scalefactor"]=atime[i]
    #now close file and use the pytables interface so as to write the table
    hdffile.close()
    #now write tables using pandas interface
    for i in range(numsnaps):
        #lets see if we can alter the code to write a table
        keys=halodata[i].keys()
        #remove tree keys
        for tkey in treekeys: keys.remove(tkey)
        #make temp dict
        dictval=dict()
        for key in keys:
            dictval[key]=halodata[i][key]
        #make a pandas DataFrame using halo dictionary
        df=pd.DataFrame.from_dict(dictval)
        df.to_hdf(fname+".snap.hdf.data","Snapshots/Snap_%03d/Halos"%(numsnaps-1-i), format='table', mode='a')

    #reopen with h5py interface
    hdffile=h5py.File(fname+".snap.hdf.data",'a')
    #then write tree information in separate group
    treegrp=hdffile.create_group("MergerTree")
    #Tree group should contain  
    """
        HaloSnapID
        HaloSnapNum
        HaloSnapIndex
        ProgenitorIndex
        ProgenitorSnapnum
        ProgenitorID
        DescendantIndex
        ..
        ..
        RootProgenitorIndex
        ..
        ..
        RootDescendantIndex
    """
    #to save on memory, allocate each block separately
    #store halo information
    tothalos=sum(numhalos)
    tdata=np.zeros(tothalos,dtype=halodata[0]["ID"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["ID"]
        count+=int(numhalos[i])
    treegrp.create_dataset("HaloSnapID",data=tdata)        
    tdata=np.zeros(tothalos,dtype=np.uint32)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=i
        count+=int(numhalos[i])
    treegrp.create_dataset("HaloSnapNum",data=tdata)
    tdata=np.zeros(tothalos,dtype=np.uint64)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=range(int(numhalos[i]))
        count+=int(numhalos[i])
    treegrp.create_dataset("HaloSnapIndex",data=tdata)
    #store progenitors
    tdata=np.zeros(tothalos,dtype=halodata[0]["Tail"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["Tail"]
        count+=int(numhalos[i])
    treegrp.create_dataset("ProgenitorID",data=tdata)
    tdata=np.zeros(tothalos,dtype=halodata[0]["TailSnap"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["TailSnap"]
        count+=int(numhalos[i])
    treegrp.create_dataset("ProgenitorSnapnum",data=tdata)
    tdata=np.zeros(tothalos,dtype=np.uint64)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=(halodata[i]["Tail"]%HALOIDVAL-1)
        count+=int(numhalos[i])
    treegrp.create_dataset("ProgenitorIndex",data=tdata)
    #store descendants
    tdata=np.zeros(tothalos,dtype=halodata[0]["Head"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["Head"]
        count+=int(numhalos[i])
    treegrp.create_dataset("DescendantID",data=tdata)
    tdata=np.zeros(tothalos,dtype=halodata[0]["HeadSnap"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["HeadSnap"]
        count+=int(numhalos[i])
    treegrp.create_dataset("DescendantSnapnum",data=tdata)
    tdata=np.zeros(tothalos,dtype=np.uint64)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=(halodata[i]["Head"]%HALOIDVAL-1)
        count+=int(numhalos[i])
    treegrp.create_dataset("DescendantIndex",data=tdata)
    #store progenitors
    tdata=np.zeros(tothalos,dtype=halodata[0]["RootTail"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["RootTail"]
        count+=int(numhalos[i])
    treegrp.create_dataset("RootProgenitorID",data=tdata)
    tdata=np.zeros(tothalos,dtype=halodata[0]["RootTailSnap"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["RootTailSnap"]
        count+=int(numhalos[i])
    treegrp.create_dataset("RootProgenitorSnapnum",data=tdata)
    tdata=np.zeros(tothalos,dtype=np.uint64)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=(halodata[i]["RootTail"]%HALOIDVAL-1)
        count+=int(numhalos[i])
    treegrp.create_dataset("RootProgenitorIndex",data=tdata)
    #store descendants
    tdata=np.zeros(tothalos,dtype=halodata[0]["RootHead"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["RootHead"]
        count+=int(numhalos[i])
    treegrp.create_dataset("RootDescendantID",data=tdata)
    tdata=np.zeros(tothalos,dtype=halodata[0]["RootHeadSnap"].dtype)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["RootHeadSnap"]
        count+=int(numhalos[i])
    treegrp.create_dataset("RootDescendantSnapnum",data=tdata)
    tdata=np.zeros(tothalos,dtype=np.uint64)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=(halodata[i]["RootHead"]%HALOIDVAL-1)
        count+=int(numhalos[i])
    treegrp.create_dataset("RootDescendantIndex",data=tdata)
    #store number of progenitors
    tdata=np.zeros(tothalos,dtype=np.uint32)
    count=0
    for i in range(numsnaps):
        tdata[count:int(numhalos[i])+count]=halodata[i]["Num_progen"]
        count+=int(numhalos[i])
    treegrp.create_dataset("NProgen",data=tdata)

    hdffile.close()

def ReadUnifiedTreeandHaloCatalog(fname, icombinedfile=1):
    """
    Read Unified Tree and halo catalog from HDF file with base filename fname
    """
    if (icombinedfile):
        hdffile=h5py.File(fname+".snap.hdf.data",'r')

        #load data sets containing number of snaps
        headergrpname="Header/"
        numsnaps=hdffile[headergrpname].attrs["Num_of_snaps"]

        #allocate memory
        halodata=[dict() for i in range(numsnaps)]
        numhalos=[0 for i in range(numsnaps)]
        atime=[0 for i in range(numsnaps)]
        tree=[[] for i in range(numsnaps)]
        cosmodata=dict()
        unitdata=dict()

        #load cosmology data
        cosmogrpname="Cosmology/"
        fieldnames=[str(n) for n in hdffile[headergrpname+cosmogrpname].attrs.keys()]
        for fieldname in fieldnames:
            cosmodata[fieldname]=hdffile[headergrpname+cosmogrpname].attrs[fieldname]

        #load unit data
        unitgrpname="Units/"
        fieldnames=[str(n) for n in hdffile[headergrpname+unitgrpname].attrs.keys()]
        for fieldname in fieldnames:
            unitdata[fieldname]=hdffile[headergrpname+unitgrpname].attrs[fieldname]

        #for each snap load the appropriate group 
        start=time.clock()
        for i in range(numsnaps):
            snapgrpname="Snap_%03d/"%(numsnaps-1-i)
            isnap=hdffile[snapgrpname].attrs["Snapnum"]
            atime[isnap]=hdffile[snapgrpname].attrs["scalefactor"]
            numhalos[isnap]=hdffile[snapgrpname].attrs["NHalos"]
            fieldnames=[str(n) for n in hdffile[snapgrpname].keys()]
            for catvalue in fieldnames:
                halodata[isnap][catvalue]=np.array(hdffile[snapgrpname+catvalue])
        hdffile.close()
        print "read halo data ",time.clock()-start
    else :
        hdffile=h5py.File(fname+".snap_000.hdf.data",'r')
        numsnaps=int(hdffile["Num_of_snaps"][0])
        #get field names
        fieldnames=[str(n) for n in hdffile.keys()]
        #clean of header info
        fieldnames.remove("Snap_value")
        fieldnames.remove("Num_of_snaps")
        fieldnames.remove("NHalos")
        fieldnames.remove("Total_num_of_groups")
        fieldnames.remove("scalefactor")
        hdffile.close()
        halodata=[[] for i in range(numsnaps)]
        numhalos=[0 for i in range(numsnaps)]
        atime=[0 for i in range(numsnaps)]
        tree=[[] for i in range(numsnaps)]
        start=time.clock()
        for i in range(numsnaps):
            hdffile=h5py.File(fname+".snap_%03d.hdf.data"%(numsnaps-1-i),'r')
            atime[i]=(hdffile["scalefactor"])[0]
            numhalos[i]=(hdffile["NHalos"])[0]
            halodata[i]=dict()
            for catvalue in fieldnames:
                halodata[i][catvalue]=np.array(hdffile[catvalue])
            hdffile.close()
        print "read halo data ",time.clock()-start
    if (icombinedfile==1):
        hdffile=h5py.File(fname+".tree.hdf.data",'r')
        treefields=["haloID", "Num_progen"]
        #do be completed for Progenitor list although information is contained in the halo catalog by searching for things with the same head 
        #treefields=["haloID", "Num_progen", "Progen"]
        for i in range(numsnaps):
            snapgrpname="Snap_%03d/"%(numsnaps-1-i)
            tree[i]=dict()
            for catvalue in treefields:
                """
                if (catvalue==treefields[-1]):
                    tree[i][catvalue]=[[]for j in range(numhalos[i])]
                    for j in range(numhalos[i]):
                        halogrpname=snapgrpname+"/Halo"+str(j)
                        tree[i][catvalue]=np.array(hdffile[halogrpname+catvalue])
                else:
                    tree[i][catvalue]=np.array(hdffile[snapgrpname+catvalue])
                """
                tree[i][catvalue]=np.array(hdffile[snapgrpname+catvalue])
        hdffile.close()
    return atime,tree,numhalos,halodata,cosmodata,unitdata

"""
    Conversion Tools
"""


def ConvertASCIIPropertyFileToHDF(basefilename,iseparatesubfiles=0,iverbose=0):
    """
    Reads an ASCII file and converts it to the HDF format for VELOCIraptor properties files

    """
    inompi=True
    if (iverbose): print "reading properties file and converting to hdf",basefilename,os.path.isfile(basefilename)
    filename=basefilename+".properties"
    #load header
    if (os.path.isfile(basefilename)==True):
        numfiles=0
    else:
        filename=basefilename+".properties"+".0"
        inompi=False
        if (os.path.isfile(filename)==False):
            print "file not found"
            return []
    byteoffset=0
    #load ascii file
    halofile = open(filename, 'r')
    #read header information
    [filenum,numfiles]=halofile.readline().split()
    filenum=int(filenum);numfiles=int(numfiles)
    [numhalos, numtothalos]= halofile.readline().split()
    numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
    names = ((halofile.readline())).split()
    #remove the brackets in ascii file names 
    fieldnames= [fieldname.split("(")[0] for fieldname in names]
    halofile.close()

    for ifile in range(numfiles):
        if (inompi==True): 
            filename=basefilename+".properties"
            hdffilename=basefilename+".hdf.properties"
        else: 
            filename=basefilename+".properties"+"."+str(ifile)
            hdffilename=basefilename+".hdf.properties"+"."+str(ifile)
        if (iverbose) : print "reading ",filename
        halofile = open(filename, 'r')
        hdffile=h5py.File(hdffilename,'w')
        [filenum,numfiles]=halofile.readline().split()
        [numhalos, numtothalos]= halofile.readline().split()
        filenum=int(filenum);numfiles=int(numfiles)
        numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
        #write header info
        hdffile.create_dataset("File_id",data=np.array([filenum]))
        hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
        hdffile.create_dataset("Num_of_groups",data=np.array([numhalos]));
        hdffile.create_dataset("Total_num_of_groups",data=np.array([numtothalos]));
        halofile.close()
        if (numhalos>0): htemp = np.loadtxt(filename,skiprows=3).transpose()
        else: htemp=[[]for ikeys in range(len(fieldnames))]
        for ikeys in range(len(fieldnames)):
            if (fieldnames[ikeys]=="ID"):
                hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys],dtype=np.uint64))
            elif (fieldnames[ikeys]=="ID_mbp"):
                hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys],dtype=np.int64))
            elif (fieldnames[ikeys]=="hostHaloID"):
                hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys],dtype=np.int64))
            elif fieldnames[ikeys] in ["numSubStruct","npart","n_gas","n_star"]:
                hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys], dtype=np.uint64))
            else:
                hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys], dtype=np.float64))

        hdffile.close()
    #if subhalos are written in separate files, then read them too
    if (iseparatesubfiles==1):
        for ifile in range(numfiles):
            if (inompi==True): 
                filename=basefilename+".sublevels"+".properties"
                hdffilename=basefilename+".hdf"+".sublevels"+".properties"
            else: 
                filename=basefilename+".sublevels"+".properties"+"."+str(ifile)
                hdffilename=basefilename+".hdf"+".sublevels"+".properties"+"."+str(ifile)
            if (iverbose) : print "reading ",filename
            halofile = open(filename, 'r')
            hdffile=h5py.File(hdffilename,'w')
            [filenum,numfiles]=halofile.readline().split()
            [numhalos, numtothalos]= halofile.readline().split()
            filenum=int(filenum);numfiles=int(numfiles)
            numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
            #write header info
            hdffile.create_dataset("File_id",data=np.array([filenum]))
            hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
            hdffile.create_dataset("Num_of_groups",data=np.array([numhalos]));
            hdffile.create_dataset("Total_num_of_groups",data=np.array([numtothalos]));
            halofile.close()
            if (numhalos>0): htemp = np.loadtxt(filename,skiprows=3).transpose()
            else: htemp=[[]for ikeys in range(len(fieldnames))]
            for ikeys in range(len(fieldnames)):
                if (fieldnames[ikeys]=="ID"):
                    hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys],dtype=np.uint64))
                elif (fieldnames[ikeys]=="ID_mbp"):
                    hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys],dtype=np.int64))
                elif (fieldnames[ikeys]=="hostHaloID"):
                    hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys],dtype=np.int64))
                elif fieldnames[ikeys] in ["numSubStruct","npart","n_gas","n_star"]:
                    hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys], dtype=np.uint64))
                else:
                    hdffile.create_dataset(fieldnames[ikeys],data=np.array(htemp[ikeys], dtype=np.float64))
            hdffile.close()

def ConvertASCIICatalogGroupsFileToHDF(basefilename,iseparatesubfiles=0,iverbose=0):
    """
    Reads an ASCII file and converts it to the HDF format for VELOCIraptor files

    """
    inompi=True
    if (iverbose): print "reading properties file and converting to hdf",basefilename,os.path.isfile(basefilename)
    filename=basefilename+".catalog_groups"
    #load header
    if (os.path.isfile(basefilename)==True):
        numfiles=0
    else:
        filename=basefilename+".catalog_groups"+".0"
        inompi=False
        if (os.path.isfile(filename)==False):
            print "file not found"
            return []
    byteoffset=0
    #load ascii file
    halofile = open(filename, 'r')
    #read header information
    [filenum,numfiles]=halofile.readline().split()
    filenum=int(filenum);numfiles=int(numfiles)
    [numhalos, numtothalos]= halofile.readline().split()
    numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
    halofile.close()

    fieldnames=["Group_Size","Offset","Offset_unbound","Number_of_substructures_in_halo","Parent_halo_ID"]
    fieldtype=[np.uint32,np.uint64,np.uint64,np.uint32,np.int64]

    for ifile in range(numfiles):
        if (inompi==True): 
            filename=basefilename+".catalog_groups"
            hdffilename=basefilename+".hdf.catalog_groups"
        else: 
            filename=basefilename+".catalog_groups"+"."+str(ifile)
            hdffilename=basefilename+".hdf.catalog_groups"+"."+str(ifile)
        if (iverbose) : print "reading ",filename
        halofile = open(filename, 'r')
        hdffile=h5py.File(hdffilename,'w')
        [filenum,numfiles]=halofile.readline().split()
        [numhalos, numtothalos]= halofile.readline().split()
        filenum=int(filenum);numfiles=int(numfiles)
        numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
        #write header info
        hdffile.create_dataset("File_id",data=np.array([filenum]))
        hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
        hdffile.create_dataset("Num_of_groups",data=np.array([numhalos]));
        hdffile.create_dataset("Total_num_of_groups",data=np.array([numtothalos]));
        halofile.close()
        if (numhalos>0):
            #will look like one dimensional array of values split into 
            #"Group_Size"
            #"Offset"
            #"Offset_unbound"
            #"Number_of_substructures_in_halo"
            #"Parent_halo_ID"
            #each of size numhalos
            cattemp = np.loadtxt(filename,skiprows=2).transpose()
            for ikeys in range(len(fieldnames)):
                hdffile.create_dataset(fieldnames[ikeys],data=np.array(cattemp[ikeys*numhalos:(ikeys+1)*numhalos],dtype=fieldtype[ikeys]))
        else: 
            cattemp=[]
            for ikeys in range(len(fieldnames)):
                hdffile.create_dataset(fieldnames[ikeys],data=np.array([],dtype=fieldtype[ikeys]))
        hdffile.close()
    #if subhalos are written in separate files, then read them too
    if (iseparatesubfiles==1):
        for ifile in range(numfiles):
            if (inompi==True): 
                filename=basefilename+".sublevels"+".catalog_groups"
                hdffilename=basefilename+".hdf"+".sublevels"+".catalog_groups"
            else: 
                filename=basefilename+".sublevels"+".catalog_groups"+"."+str(ifile)
                hdffilename=basefilename+".hdf"+".sublevels"+".catalog_groups"+"."+str(ifile)
            if (iverbose) : print "reading ",filename
            halofile = open(filename, 'r')
            hdffile=h5py.File(hdffilename,'w')
            [filenum,numfiles]=halofile.readline().split()
            [numhalos, numtothalos]= halofile.readline().split()
            filenum=int(filenum);numfiles=int(numfiles)
            numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
            #write header info
            hdffile.create_dataset("File_id",data=np.array([filenum]))
            hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
            hdffile.create_dataset("Num_of_groups",data=np.array([numhalos]));
            hdffile.create_dataset("Total_num_of_groups",data=np.array([numtothalos]));
            halofile.close()
            if (numhalos>0):
                cattemp = np.loadtxt(filename,skiprows=2).transpose()
                for ikeys in range(len(fieldnames)):
                    hdffile.create_dataset(fieldnames[ikeys],data=np.array(cattemp[ikeys*numhalos:(ikeys+1)*numhalos],dtype=fieldtype[ikeys]))
            else: 
                cattemp=[]
                for ikeys in range(len(fieldnames)):
                    hdffile.create_dataset(fieldnames[ikeys],data=np.array([],dtype=fieldtype[ikeys]))
            hdffile.close()

def ConvertASCIICatalogParticleFileToHDF(basefilename,iunbound=0,iseparatesubfiles=0,iverbose=0):
    """
    Reads an ASCII file and converts it to the HDF format for VELOCIraptor files
    """
    inompi=True
    if (iverbose): print "reading properties file and converting to hdf",basefilename,os.path.isfile(basefilename)
    filename=basefilename+".catalog_particles"
    if (iunbound>0): filename+=".unbound"
    #load header
    if (os.path.isfile(basefilename)==True):
        numfiles=0
    else:
        filename=basefilename+".catalog_particles"
        if (iunbound>0): filename+=".unbound"
        filename+=".0"
        inompi=False
        if (os.path.isfile(filename)==False):
            print "file not found"
            return []
    byteoffset=0
    #load ascii file
    halofile = open(filename, 'r')
    #read header information
    [filenum,numfiles]=halofile.readline().split()
    filenum=int(filenum);numfiles=int(numfiles)
    [numhalos, numtothalos]= halofile.readline().split()
    numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
    halofile.close()

    for ifile in range(numfiles):
        if (inompi==True): 
            filename=basefilename+".catalog_particles"
            hdffilename=basefilename+".hdf.catalog_particles"
            if (iunbound>0): 
                filename+=".unbound"
                hdffilename+=".unbound"
        else: 
            filename=basefilename+".catalog_particles"
            hdffilename=basefilename+".hdf.catalog_particles"
            if (iunbound>0): 
                filename+=".unbound"
                hdffilename+=".unbound"
            filename+="."+str(ifile)
            hdffilename+="."+str(ifile)
        if (iverbose) : print "reading ",filename
        halofile = open(filename, 'r')
        hdffile=h5py.File(hdffilename,'w')
        [filenum,numfiles]=halofile.readline().split()
        [numhalos, numtothalos]= halofile.readline().split()
        filenum=int(filenum);numfiles=int(numfiles)
        numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
        #write header info
        hdffile.create_dataset("File_id",data=np.array([filenum]))
        hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
        hdffile.create_dataset("Num_of_particles_in_groups",data=np.array([numhalos]));
        hdffile.create_dataset("Total_num_of_particles_in_all_groups",data=np.array([numtothalos]));
        halofile.close()
        if (numhalos>0): cattemp = np.loadtxt(filename,skiprows=2).transpose()
        else: cattemp=[]
        hdffile.create_dataset("Particle_IDs",data=np.array(cattemp,dtype=np.int64))
        hdffile.close()
    #if subhalos are written in separate files, then read them too
    if (iseparatesubfiles==1):
        for ifile in range(numfiles):
            if (inompi==True): 
                filename=basefilename+".sublevels"+".catalog_particles"
                hdffilename=basefilename+".hdf"+".sublevels"+".catalog_particles"
            else: 
                filename=basefilename+".sublevels"+".catalog_particles"+"."+str(ifile)
                hdffilename=basefilename+".hdf"+".sublevels"+".catalog_particles"+"."+str(ifile)
            if (iverbose) : print "reading ",filename
            halofile = open(filename, 'r')
            hdffile=h5py.File(hdffilename,'w')
            [filenum,numfiles]=halofile.readline().split()
            [numhalos, numtothalos]= halofile.readline().split()
            filenum=int(filenum);numfiles=int(numfiles)
            numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
            #write header info
            hdffile.create_dataset("File_id",data=np.array([filenum]))
            hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
            hdffile.create_dataset("Num_of_particles_in_groups",data=np.array([numhalos]));
            hdffile.create_dataset("Total_num_of_particles_in_all_groups",data=np.array([numtothalos]));
            halofile.close()
            if (numhalos>0): cattemp = np.loadtxt(filename,skiprows=2).transpose()
            else: cattemp=[]
            hdffile.create_dataset("Particle_IDs",data=np.array(cattemp,dtype=np.int64))
            hdffile.close()

def ConvertASCIICatalogParticleTypeFileToHDF(basefilename,iunbound=0,iseparatesubfiles=0,iverbose=0):
    """
    Reads an ASCII file and converts it to the HDF format for VELOCIraptor files
    """
    inompi=True
    if (iverbose): print "reading properties file and converting to hdf",basefilename,os.path.isfile(basefilename)
    filename=basefilename+".catalog_parttypes"
    if (iunbound>0): filename+=".unbound"
    #load header
    if (os.path.isfile(basefilename)==True):
        numfiles=0
    else:
        filename=basefilename+".catalog_parttypes"
        if (iunbound>0): filename+=".unbound"
        filename+=".0"
        inompi=False
        if (os.path.isfile(filename)==False):
            print "file not found"
            return []
    byteoffset=0
    #load ascii file
    halofile = open(filename, 'r')
    #read header information
    [filenum,numfiles]=halofile.readline().split()
    filenum=int(filenum);numfiles=int(numfiles)
    [numhalos, numtothalos]= halofile.readline().split()
    numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
    halofile.close()

    for ifile in range(numfiles):
        if (inompi==True): 
            filename=basefilename+".catalog_parttypes"
            hdffilename=basefilename+".hdf.catalog_parttypes"
            if (iunbound>0): 
                filename+=".unbound"
                hdffilename+=".unbound"
        else: 
            filename=basefilename+".catalog_parttypes"
            hdffilename=basefilename+".hdf.catalog_parttypes"
            if (iunbound>0): 
                filename+=".unbound"
                hdffilename+=".unbound"
            filename+="."+str(ifile)
            hdffilename+="."+str(ifile)
        if (iverbose) : print "reading ",filename
        halofile = open(filename, 'r')
        hdffile=h5py.File(hdffilename,'w')
        [filenum,numfiles]=halofile.readline().split()
        [numhalos, numtothalos]= halofile.readline().split()
        filenum=int(filenum);numfiles=int(numfiles)
        numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
        #write header info
        hdffile.create_dataset("File_id",data=np.array([filenum]))
        hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
        hdffile.create_dataset("Num_of_particles_in_groups",data=np.array([numhalos]));
        hdffile.create_dataset("Total_num_of_particles_in_all_groups",data=np.array([numtothalos]));
        halofile.close()
        if (numhalos>0): cattemp = np.loadtxt(filename,skiprows=2).transpose()
        else: cattemp=[]
        hdffile.create_dataset("Particle_Types",data=np.array(cattemp,dtype=np.int64))
        hdffile.close()
    #if subhalos are written in separate files, then read them too
    if (iseparatesubfiles==1):
        for ifile in range(numfiles):
            if (inompi==True): 
                filename=basefilename+".sublevels"+".catalog_parttypes"
                hdffilename=basefilename+".hdf"+".sublevels"+".catalog_parttypes"
            else: 
                filename=basefilename+".sublevels"+".catalog_parttypes"+"."+str(ifile)
                hdffilename=basefilename+".hdf"+".sublevels"+".catalog_parttypes"+"."+str(ifile)
            if (iverbose) : print "reading ",filename
            halofile = open(filename, 'r')
            hdffile=h5py.File(hdffilename,'w')
            [filenum,numfiles]=halofile.readline().split()
            [numhalos, numtothalos]= halofile.readline().split()
            filenum=int(filenum);numfiles=int(numfiles)
            numhalos=np.uint64(numhalos);numtothalos=np.uint64(numtothalos)
            #write header info
            hdffile.create_dataset("File_id",data=np.array([filenum]))
            hdffile.create_dataset("Num_of_files",data=np.array([numfiles]))
            hdffile.create_dataset("Num_of_particles_in_groups",data=np.array([numhalos]));
            hdffile.create_dataset("Total_num_of_particles_in_all_groups",data=np.array([numtothalos]));
            halofile.close()
            if (numhalos>0): cattemp = np.loadtxt(filename,skiprows=2).transpose()
            else: cattemp=[]
            hdffile.create_dataset("Particle_Types",data=np.array(cattemp,dtype=np.int64))
            hdffile.close()

def ConvertASCIIToHDF(basefilename,iseparatesubfiles=0,itype=0,iverbose=0):
    ConvertASCIIPropertyFileToHDF(basefilename,iseparatesubfiles,iverbose)
    ConvertASCIICatalogGroupsFileToHDF(basefilename,iseparatesubfiles,iverbose)
    ConvertASCIICatalogParticleFileToHDF(basefilename,0,iseparatesubfiles,iverbose)
    ConvertASCIICatalogParticleFileToHDF(basefilename,1,iseparatesubfiles,iverbose)
    if (itype==1):
        ConvertASCIICatalogParticleTypeFileToHDF(basefilename,0,iseparatesubfiles,iverbose)
        ConvertASCIICatalogParticleTypeFileToHDF(basefilename,1,iseparatesubfiles,iverbose)

