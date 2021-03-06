/**
 * Copyright 2013-2014 Axel Huebl, Felix Schmitt, Heiko Burau, Rene Widera, Felix Schmitt
 *
 * This file is part of PIConGPU. 
 * 
 * PIConGPU is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. 
 * 
 * PIConGPU is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with PIConGPU.  
 * If not, see <http://www.gnu.org/licenses/>. 
 */



#ifndef SUMCURRENTS_HPP
#define	SUMCURRENTS_HPP

#include <iostream>

#include "types.h"
#include "simulation_defines.hpp"
#include "simulation_types.hpp"

#include "simulation_classTypes.hpp"

#include "fields/FieldJ.hpp"

#include "basicOperations.hpp"
#include "dimensions/DataSpaceOperations.hpp"
#include "plugins/IPluginModule.hpp"

namespace picongpu
{
using namespace PMacc;

namespace po = boost::program_options;

typedef typename FieldJ::DataBoxType J_DataBox;

template<class Mapping>
__global__ void kernelSumCurrents(J_DataBox fieldJ, float3_X* gCurrent, Mapping mapper)
{
    typedef typename Mapping::SuperCellSize SuperCellSize;

    __shared__ float3_X sh_sumJ;
    __syncthreads(); /*wait that all shared memory is initialised*/

    const DataSpace<simDim > threadIndex(threadIdx);
    const int linearThreadIdx = DataSpaceOperations<simDim>::template map<SuperCellSize > (threadIndex);

    if (linearThreadIdx == 0)
    {
        sh_sumJ = float3_X(float_X(0.0), float_X(0.0), float_X(0.0));
    }

    __syncthreads();


    const DataSpace<simDim> superCellIdx(mapper.getSuperCellIndex(DataSpace<simDim > (blockIdx)));
    const DataSpace<simDim> cell(superCellIdx * SuperCellSize() + threadIndex);

    const float3_X J = fieldJ(cell);
    const float3_X myJ = float3_X(J.x(), J.y(), J.z());

    atomicAddWrapper(&(sh_sumJ.x()), myJ.x());
    atomicAddWrapper(&(sh_sumJ.y()), myJ.y());
    atomicAddWrapper(&(sh_sumJ.z()), myJ.z());

    __syncthreads();

    if (linearThreadIdx == 0)
    {
        atomicAddWrapper(&(gCurrent->x()), sh_sumJ.x());
        atomicAddWrapper(&(gCurrent->y()), sh_sumJ.y());
        atomicAddWrapper(&(gCurrent->z()), sh_sumJ.z());
    }
}

class SumCurrents : public ISimulationIO, public IPluginModule
{
private:
    FieldJ* fieldJ;

    MappingDesc *cellDescription;
    uint32_t notifyFrequency;

    GridBuffer<float3_X, DIM1> *sumcurrents;

public:

    SumCurrents() :
    fieldJ(NULL),
    cellDescription(NULL),
    notifyFrequency(0)
    {

        ModuleConnector::getInstance().registerModule(this);
    }

    virtual ~SumCurrents()
    {

    }

    void notify(uint32_t currentStep)
    {
        DataConnector &dc = DataConnector::getInstance();

        fieldJ = &(dc.getData<FieldJ > (FieldJ::getName(), true));


        const int rank = GridController<simDim>::getInstance().getGlobalRank();
        const float3_X gCurrent = getSumCurrents();

        //const DataSpace<simDim> nrOfGpuCells = MappingDesc::SuperCellSize::getDataSpace()
        //                                       *(cellDescription->getGridSuperCells() - 2*cellDescription->getGuardingSuperCells() );

        // gCurrent is just j
        // j = I/A
#if(SIMDIM==DIM3)
        const float3_X realCurrent(
                                   gCurrent.x() * CELL_HEIGHT * CELL_DEPTH,
                                   gCurrent.y() * CELL_WIDTH * CELL_DEPTH,
                                   gCurrent.z() * CELL_WIDTH * CELL_HEIGHT);
#elif(SIMDIM==DIM2)
        const float3_X realCurrent(
                                   gCurrent.x() * CELL_HEIGHT,
                                   gCurrent.y() * CELL_WIDTH,
                                   gCurrent.z() * CELL_WIDTH * CELL_HEIGHT);
#endif
        float3_64 realCurrent_SI(
                                 double(realCurrent.x()) * (UNIT_CHARGE / UNIT_TIME),
                                 double(realCurrent.y()) * (UNIT_CHARGE / UNIT_TIME),
                                 double(realCurrent.z()) * (UNIT_CHARGE / UNIT_TIME));

        /*FORMAT OUTPUT*/
        typedef std::numeric_limits< float_64 > dbl;

        std::cout.precision(dbl::digits10);
        if (math::abs(gCurrent.x()) + math::abs(gCurrent.y()) + math::abs(gCurrent.z()) != float_X(0.0))
            std::cout << "[ANALYSIS] [" << rank << "] [COUNTER] [SumCurrents] [" << currentStep
            << std::scientific << "] " <<
            realCurrent_SI << " Abs:" << math::abs(realCurrent_SI) << std::endl;
    }

    void moduleRegisterHelp(po::options_description& desc)
    {
        desc.add_options()
            ("sumcurr.period", po::value<uint32_t > (&notifyFrequency), "enable analyser [for each n-th step]");
    }

    std::string moduleGetName() const
    {
        return "SumCurrents";
    }

    void setMappingDescription(MappingDesc *cellDescription)
    {
        this->cellDescription = cellDescription;
    }

private:

    void moduleLoad()
    {
        if (notifyFrequency > 0)
        {
            sumcurrents = new GridBuffer<float3_X, DIM1 > (DataSpace<DIM1 > (1)); //create one int on gpu und host

            DataConnector::getInstance().registerObserver(this, notifyFrequency);
        }
    }

    void moduleUnload()
    {
        if (notifyFrequency > 0)
        {
            if (sumcurrents)
                delete sumcurrents;
        }
    }

    float3_X getSumCurrents()
    {
        sumcurrents->getDeviceBuffer().setValue(float3_X(float_X(0.0), float_X(0.0), float_X(0.0)));
        dim3 block(MappingDesc::SuperCellSize::getDataSpace());

        __picKernelArea(kernelSumCurrents, *cellDescription, CORE + BORDER)
            (block)
            (fieldJ->getDeviceDataBox(),
             sumcurrents->getDeviceBuffer().getBasePointer());
        sumcurrents->deviceToHost();
        return sumcurrents->getHostBuffer().getDataBox()[0];
    }

};

}


#endif	/* SUMCURRENTS_HPP */

