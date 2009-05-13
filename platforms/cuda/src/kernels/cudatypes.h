#ifndef CUDATYPES_H
#define CUDATYPES_H

/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2009 Stanford University and the Authors.           *
 * Authors: Scott Le Grand, Peter Eastman                                     *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

#include <stdarg.h>
#include <limits>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <builtin_types.h>
#include <vector_functions.h>

#define RTERROR(status, s) \
    if (status != cudaSuccess) { \
        printf("%s %s\n", s, cudaGetErrorString(status)); \
        exit(-1); \
    }

#define LAUNCHERROR(s) \
    { \
        cudaError_t status = cudaGetLastError(); \
        if (status != cudaSuccess) { \
            printf("Error: %s launching kernel %s\n", cudaGetErrorString(status), s); \
            exit(-1); \
        } \
    }

// Pure virtual class to define an interface for objects resident both on GPU and CPU
struct SoADeviceObject {
    virtual void Allocate() = 0;
    virtual void Deallocate() = 0;
    virtual void Upload() = 0;
    virtual void Download() = 0;
};

template <typename T>
struct CUDAStream : public SoADeviceObject
{
    unsigned int    _length;
    unsigned int    _subStreams;
    unsigned int    _stride;
    T**             _pSysStream;
    T**             _pDevStream;
    T*              _pSysData;
    T*              _pDevData;
    std::string     _name;
    CUDAStream(int length, int subStreams = 1, std::string name="");
    CUDAStream(unsigned int length, unsigned int subStreams = 1, std::string name="");
    CUDAStream(unsigned int length, int subStreams = 1, std::string name="");
    CUDAStream(int length, unsigned int subStreams = 1, std::string name="");
    virtual ~CUDAStream();
    void Allocate();
    void Deallocate();
    void Upload();
    void Download();
    void Collapse(unsigned int newstreams = 1, unsigned int interleave = 1);
    T& operator[](int index);
};

float CompareStreams(CUDAStream<float>& s1, CUDAStream<float>& s2, float tolerance, unsigned int maxindex = 0);

template <typename T>
CUDAStream<T>::CUDAStream(int length, unsigned int subStreams, std::string name) : _length(length), _subStreams(subStreams), _stride((length + 0xf) & 0xfffffff0), _name(name)
{
    Allocate();   
}

template <typename T>
CUDAStream<T>::CUDAStream(unsigned int length, int subStreams, std::string name) : _length(length), _subStreams(subStreams), _stride((length + 0xf) & 0xfffffff0), _name(name)
{
    Allocate();   
}

template <typename T>
CUDAStream<T>::CUDAStream(unsigned int length, unsigned int subStreams, std::string name) : _length(length), _subStreams(subStreams), _stride((length + 0xf) & 0xfffffff0), _name(name)
{
    Allocate();   
}

template <typename T>
CUDAStream<T>::CUDAStream(int length, int subStreams, std::string name) : _length(length), _subStreams(subStreams), _stride((length + 0xf) & 0xfffffff0), _name(name)
{
    Allocate();   
}

template <typename T>
CUDAStream<T>::~CUDAStream()
{
    Deallocate();
}

template <typename T>
void CUDAStream<T>::Allocate()
{
    cudaError_t status;
    _pSysStream =   new T*[_subStreams];
    _pDevStream =   new T*[_subStreams];
    _pSysData =     new T[_subStreams * _stride];

    status = cudaMalloc((void **) &_pDevData, _stride * _subStreams * sizeof(T));
    RTERROR(status, (_name+": cudaMalloc in CUDAStream::Allocate failed").c_str());

    for (unsigned int i = 0; i < _subStreams; i++)
    {
        _pSysStream[i] = _pSysData + i * _stride;
        _pDevStream[i] = _pDevData + i * _stride;
    }
}

template <typename T>
void CUDAStream<T>::Deallocate()
{
    cudaError_t status;
    delete[] _pSysStream;
    _pSysStream = NULL;
    delete[] _pDevStream;
    _pDevStream = NULL;
    delete[] _pSysData;
    _pSysData = NULL;
    status = cudaFree(_pDevData);
    RTERROR(status, (_name+": cudaFree in CUDAStream::Deallocate failed").c_str());
}

template <typename T>
void CUDAStream<T>::Upload()
{
    cudaError_t status;
    status = cudaMemcpy(_pDevData, _pSysData, _stride * _subStreams * sizeof(T), cudaMemcpyHostToDevice);
    RTERROR(status, (_name+": cudaMemcpy in CUDAStream::Upload failed").c_str());
}

template <typename T>
void CUDAStream<T>::Download()
{
    cudaError_t status;
    status = cudaMemcpy(_pSysData, _pDevData, _stride * _subStreams * sizeof(T), cudaMemcpyDeviceToHost);
    RTERROR(status, (_name+": cudaMemcpy in CUDAStream::Download failed").c_str());
}

template <typename T>
void CUDAStream<T>::Collapse(unsigned int newstreams, unsigned int interleave)
{
    T* pTemp = new T[_subStreams * _stride];
    unsigned int stream = 0;
    unsigned int pos = 0;
    unsigned int newstride = _stride * _subStreams / newstreams;
    unsigned int newlength = _length * _subStreams / newstreams;

    // Copy data into new format
    for (unsigned int i = 0; i < _length; i++)
    {
        for (unsigned int j = 0; j < _subStreams; j++)
        {
            pTemp[stream * newstride + pos] = _pSysStream[j][i];
            stream++;
            if (stream == newstreams)
            {
                stream = 0;
                pos++;
            }
        }
    }

    // Remap stream pointers;
    for (unsigned int i = 0; i < newstreams; i++)
    {
        _pSysStream[i] = _pSysData + i * newstride;
        _pDevStream[i] = _pDevData + i * newstride;
    }

    // Copy data back intro original stream
    for (unsigned int i = 0; i < newlength; i++)
        for (unsigned int j = 0; j < newstreams; j++)
            _pSysStream[j][i] = pTemp[j * newstride + i];
    
    _stride = newstride;
    _length = newlength;
    _subStreams = newstreams;
    delete[] pTemp;
}

template <typename T>
T& CUDAStream<T>::operator[](int index)
{
    return _pSysData[index];
}

static const unsigned int GRID = 32;
static const unsigned int GRIDBITS = 5;
static const int G8X_NONBOND_THREADS_PER_BLOCK          = 256;
static const int GT2XX_NONBOND_THREADS_PER_BLOCK        = 320;
static const int G8X_BORNFORCE2_THREADS_PER_BLOCK       = 256;
static const int GT2XX_BORNFORCE2_THREADS_PER_BLOCK     = 320;
static const int G8X_SHAKE_THREADS_PER_BLOCK            = 128;
static const int GT2XX_SHAKE_THREADS_PER_BLOCK          = 256;
static const int G8X_UPDATE_THREADS_PER_BLOCK           = 192;
static const int GT2XX_UPDATE_THREADS_PER_BLOCK         = 384;
static const int G8X_LOCALFORCES_THREADS_PER_BLOCK      = 192;
static const int GT2XX_LOCALFORCES_THREADS_PER_BLOCK    = 384;
static const int G8X_THREADS_PER_BLOCK                  = 256;
static const int GT2XX_THREADS_PER_BLOCK                = 256;
static const int G8X_RANDOM_THREADS_PER_BLOCK           = 256;
static const int GT2XX_RANDOM_THREADS_PER_BLOCK         = 384;
static const int G8X_NONBOND_WORKUNITS_PER_SM           = 220;
static const int GT2XX_NONBOND_WORKUNITS_PER_SM         = 256;

enum CudaNonbondedMethod
{
    NO_CUTOFF,
    CUTOFF,
    PERIODIC,
    EWALD
};

struct cudaGmxSimulation {
    // Constants
    unsigned int    atoms;                          // Number of atoms
    unsigned int    paddedNumberOfAtoms;            // Padded number of atoms
    unsigned int    blocks;                         // Number of blocks to launch across linear kernels
    unsigned int    nonbond_blocks;                 // Number of blocks to launch across CDLJ and Born Force Part1
    unsigned int    bornForce2_blocks;              // Number of blocks to launch across Born Force 2
    unsigned int    interaction_blocks;             // Number of blocks to launch when identifying interacting tiles
    unsigned int    threads_per_block;              // Threads per block to launch
    unsigned int    nonbond_threads_per_block;      // Threads per block in nonbond kernel calls
    unsigned int    bornForce2_threads_per_block;   // Threads per block in nonbond kernel calls
    unsigned int    max_update_threads_per_block;   // Maximum threads per block in update kernel calls
    unsigned int    update_threads_per_block;       // Threads per block in update kernel calls
    unsigned int    bf_reduce_threads_per_block;    // Threads per block in Born Force reduction calls
    unsigned int    bsf_reduce_threads_per_block;   // Threads per block in Born Sum And Forces reduction calls
    unsigned int    max_shake_threads_per_block;    // Maximum threads per block in shake kernel calls
    unsigned int    shake_threads_per_block;        // Threads per block in shake kernel calls
    unsigned int    settle_threads_per_block;       // Threads per block in SETTLE kernel calls
    unsigned int    lincs_threads_per_block;        // Threads per block in LINCS kernel calls
    unsigned int    nonshake_threads_per_block;     // Threads per block in nonshaking kernel call
    unsigned int    max_localForces_threads_per_block;  // Threads per block in local forces kernel calls
    unsigned int    localForces_threads_per_block;  // Threads per block in local forces kernel calls
    unsigned int    random_threads_per_block;       // Threads per block in RNG kernel calls
    unsigned int    interaction_threads_per_block;  // Threads per block when identifying interacting tiles
    unsigned int    workUnits;                      // Number of work units
    unsigned int*   pWorkUnit;                      // Pointer to work units
    unsigned int*   pInteractingWorkUnit;           // Pointer to work units that have interactions
    unsigned int*   pInteractionFlag;               // Flags for which work units have interactions
    size_t*         pInteractionCount;              // A count of the number of work units which have interactions
    unsigned int    nonbond_workBlock;              // Number of work units running simultaneously per block in CDLJ and Born Force Part 1
    unsigned int    bornForce2_workBlock;           // Number of work units running second half of Born Forces calculation
    unsigned int    workUnitsPerSM;                 // Number of workblocks per SM
    unsigned int    nbWorkUnitsPerBlock;            // Number of work units assigned to each nonbond block
    unsigned int    nbWorkUnitsPerBlockRemainder;   // Remainder of work units to assign across lower numbered nonbond blocks
    unsigned int    bf2WorkUnitsPerBlock;           // Number of work units assigned to each bornForce2 block
    unsigned int    bf2WorkUnitsPerBlockRemainder;  // Remainder of work units to assign across lower numbered bornForce2 blocks


    unsigned int    stride;                         // Atomic attributes stride
    unsigned int    stride2;                        // Atomic attributes stride x 2
    unsigned int    stride3;                        // Atomic attributes stride x 3
    unsigned int    stride4;                        // Atomic attributes stride x 4
    unsigned int	nonbondOutputBuffers;           // Nonbond output buffers per nonbond call
    unsigned int    totalNonbondOutputBuffers;      // Total nonbond output buffers
    unsigned int    outputBuffers;                  // Number of output buffers
    float           bigFloat;                       // Floating point value used as a flag for Shaken atoms 
    float           epsfac;                         // Epsilon factor for CDLJ calculations
    CudaNonbondedMethod nonbondedMethod;            // How to handle nonbonded interactions
    float           nonbondedCutoffSqr;             // Cutoff distance for CDLJ calculations
    float           periodicBoxSizeX;               // The X dimension of the periodic box
    float           periodicBoxSizeY;               // The Y dimension of the periodic box
    float           periodicBoxSizeZ;               // The Z dimension of the periodic box
    float           reactionFieldK;                 // Constant for reaction field correction
    float           probeRadius;                    // SASA probe radius
    float           surfaceAreaFactor;              // ACE approximation surface area factor
    float           electricConstant;               // ACE approximation electric constant
    float           forceConversionFactor;          // kJ to kcal force conversion factor
    float           preFactor;                      // Born electrostatic pre-factor
    float			dielectricOffset;		        // Born dielectric offset
    float			alphaOBC;				        // OBC alpha factor
    float			betaOBC;				        // OBC beta factor
    float			gammaOBC;				        // OBC gamma factor
    float           deltaT;                         // Molecular dynamics deltaT constant
    float           oneOverDeltaT;                  // 1/deltaT
    float           B;                              // Molecular dynamics B constant
    float           C;                              // Molecular dynamics C constant
    float           D;                              // Molecular dynamics D constant
    float           EPH;                            // Molecular dynamics EPH constant
    float           EMH;                            // Molecular dynamics EMH constant
    float           EM;                             // Molecular dynamics EM constant
    float           EP;                             // Molecular dynamics EP constant
    float           GDT;                            // Molecular dynamics GDT constant
    float           OneMinusEM;                     // Molecular dynamics OneMinusEM constant
    float           TauOneMinusEM;                  // Molecular dynamics TauOneMinusEM constant
    float           TauDOverEMMinusOne;             // Molecular dynamics TauDOverEMMinusOne constant
    float           T;                              // Molecular dynamics T constant
    float           kT;                             // Boltzmann's constant times T
    float           V;                              // Molecular dynamics V constant
    float           X;                              // Molecular dynamics X constant
    float           Yv;                             // Molecular dynamics Yv constant
    float           Yx;                             // Molecular dynamics Yx constant
    float           tau;                            // Molecular dynamics tau constant
    float           fix1;                           // Molecular dynamics fix1 constant
    float           oneOverFix1;                    // Molecular dynamics reciprocal of fix1 constant
    float           DOverTauC;                      // Molecular dynamics DOverTauC constant
    float           collisionProbability;           // Collision probability for Andersen thermostat
    float2*         pObcData;                       // Pointer to fixed Born data
    float2*         pAttr;                          // Pointer to additional atom attributes (sig, eps)
    unsigned int    bonds;                          // Number of bonds
    int4*           pBondID;                        // Bond atom and output buffer IDs
    float2*         pBondParameter;                 // Bond parameters
    unsigned int    bond_angles;                    // Number of bond angles
    int4*           pBondAngleID1;                  // Bond angle atom and first output buffer IDs
    int2*           pBondAngleID2;                  // Bond angle output buffer IDs
    float2*         pBondAngleParameter;            // Bond angle parameters
    unsigned int    dihedrals;                      // Number of dihedrals
    int4*           pDihedralID1;                   // Dihedral IDs
    int4*           pDihedralID2;                   // Dihedral output buffer IDs
    float4*         pDihedralParameter;             // Dihedral parameters
    unsigned int    rb_dihedrals;                   // Number of Ryckaert Bellemans dihedrals
    int4*           pRbDihedralID1;                 // Ryckaert Bellemans Dihedral IDs
    int4*           pRbDihedralID2;                 // Ryckaert Bellemans Dihedral output buffer IDs
    float4*         pRbDihedralParameter1;          // Ryckaert Bellemans Dihedral parameters
    float2*         pRbDihedralParameter2;          // Ryckaert Bellemans Dihedral parameters
    unsigned int    LJ14s;                          // Number of Lennard Jones 1-4 interactions
    int4*           pLJ14ID;                        // Lennard Jones 1-4 atom and output buffer IDs
    float4*         pLJ14Parameter;                 // Lennard Jones 1-4 parameters
    float           inverseTotalMass;               // Used in linear momentum removal
    unsigned int    ShakeConstraints;               // Total number of Shake constraints
    unsigned int    settleConstraints;              // Total number of Settle constraints
    unsigned int    lincsConstraints;               // Total number of LINCS constraints.
    unsigned int    rigidClusters;                  // Total number of rigid clusters
    unsigned int    maxRigidClusterSize;            // The size of the largest rigid cluster
    unsigned int    clusterShakeBlockSize;          // The number of threads to process each rigid cluster
    unsigned int    NonShakeConstraints;            // Total number of NonShake atoms
    unsigned int    maxShakeIterations;             // Maximum shake iterations
    unsigned int    degreesOfFreedom;               // Number of degrees of freedom in system
    float           shakeTolerance;                 // Shake tolerance
    unsigned int    lincsTerms;                     // Number of terms in the matrix expansion for LINCS
    float           InvMassJ;                       // Shake inverse mass for hydrogens
    int*            pNonShakeID;                    // Not Shaking atoms
    int4*           pShakeID;                       // Shake atoms and phase
    float4*         pShakeParameter;                // Shake parameters
    int4*           pSettleID;                      // Settle atoms
    float2*         pSettleParameter;               // Settle parameters
    unsigned int*   pExclusion;                     // Nonbond exclusion data
    unsigned int*   pExclusionIndex;                // Index of exclusion data for each work unit
    unsigned int    bond_offset;                    // Offset to end of bonds
    unsigned int    bond_angle_offset;              // Offset to end of bond angles
    unsigned int    dihedral_offset;                // Offset to end of dihedrals
    unsigned int    rb_dihedral_offset;             // Offset to end of Ryckaert Bellemans dihedrals
    unsigned int    LJ14_offset;                    // Offset to end of Lennard Jones 1-4 parameters
    int*            pAtomIndex;                     // The original index of each atom
    float4*         pGridBoundingBox;               // The size of each grid cell
    float4*         pGridCenter;                    // The center of each grid cell
    int2*           pLincsAtoms;                    // The atoms connected by each LINCS constraint
    float4*         pLincsDistance;                 // The displacement vector (x, y, z) and constraint distance (w) for each LINCS constraint
    int*            pLincsConnections;              // The indices of constraints that other constraints are connected to
    int*            pLincsNumConnections;           // The number of other constraints that each constraint is linked to
    float*          pLincsS;                        // S matrix for LINCS
    float*          pLincsCoupling;                 // Coupling matrix for LINCS
    float*          pLincsRhs1;                     // Workspace for LINCS
    float*          pLincsRhs2;                     // Workspace for LINCS
    float*          pLincsSolution;                 // Workspace for LINCS
    int*            pLincsAtomConstraints;          // The indices of constraints involving each atom
    int*            pLincsNumAtomConstraints;       // The number of constraints involving each atom
    short*          pSyncCounter;                   // Used for global thread synchronization
    unsigned int*   pRequiredIterations;            // Used by SHAKE to communicate whether iteration has converged
    float*          pShakeReducedMass;              // The reduced mass for each SHAKE constraint
    int*            pRigidClusterConstraints;       // The constraints in each rigid cluster
    float*          pRigidClusterMatrix;            // The inverse constraint matrix for each rigid cluster
    unsigned int*   pRigidClusterConstraintIndex;   // The index of each cluster in the stream containing cluster constraints.
    unsigned int*   pRigidClusterMatrixIndex;       // The index of each cluster in the stream containing cluster matrices.

    // Mutable stuff
    float4*         pPosq;                          // Pointer to atom positions and charges
    float4*         pPosqP;                         // Pointer to mid-integration atom positions
    float4*         pOldPosq;                       // Pointer to old atom positions
    float4*         pVelm4;                         // Pointer to atom velocity and inverse mass
    float4*         pvVector4;                      // Pointer to atom v Vector
    float4*         pxVector4;                      // Pointer to atom x Vector
    float4*         pForce4;                        // Pointer to all force4 data
    float4*         pForce4a;                       // Pointer to first set of force4 data
    float4*         pForce4b;                       // Pointer to second set of force4 data
    float4*         pOutForce4;                     // Pointer to output float4 force
    float*          pBornForce;                     // Pointer to Born force data
    float*			pBornSum;                       // Pointer to Born Radii calculation output buffers
    float*			pBornRadii;				        // Pointer to Born Radii
    float*          pObcChain;                      // Pointer to OBC chain data
    float4*         pLinearMomentum;                // Pointer to linear momentum
    
    // Random numbers
    float4*         pRandom4a;                      // Pointer to first set of 4 random numbers
    float4*         pRandom4b;                      // Pointer to second set of 4 random numbers
    float2*         pRandom2a;                      // Pointer to first set of 2 random numbers
    float2*         pRandom2b;                      // Pointer to second set of 2 random numbers
    uint4*          pRandomSeed;                    // Pointer to random seeds
    int*            pRandomPosition;                // Pointer to random number positions
    unsigned int    randoms;                        // Number of randoms
    unsigned int    totalRandoms;                   // Number of randoms plus overflow.
    unsigned int    totalRandomsTimesTwo;           // Used for generating randoms
    unsigned int    randomIterations;               // Number of iterations before regenerating randoms
    unsigned int    randomFrames;                   // Number of frames of random numbers
};

struct Vectors {
    float3 v0;
    float3 v1;
    float3 v2;
};

#endif
