/**
 * Copyright 2013 Rene Widera
 *
 * This file is part of libPMacc. 
 * 
 * libPMacc is free software: you can redistribute it and/or modify 
 * it under the terms of of either the GNU General Public License or 
 * the GNU Lesser General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. 
 * libPMacc is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License and the GNU Lesser General Public License 
 * for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * and the GNU Lesser General Public License along with libPMacc. 
 * If not, see <http://www.gnu.org/licenses/>. 
 */


#pragma once

#include <boost/mpl/vector.hpp>
#include <boost/mpl/copy.hpp>
#include <boost/mpl/back_inserter.hpp>
#include <boost/mpl/front_inserter.hpp>
#include "compileTime/conversion/ToSeq.hpp"

namespace PMacc
{

namespace bmpl = boost::mpl;

/** Join both input types to one boost mpl sequence
 * 
 * @tparam T_1 a boost mpl sequence or single type
 * @tparam T_2 a boost mpl sequence or single type
 */

template<class T_1, class T_2 = bmpl::vector<> >
struct JoinToSeq
{
private:
    typedef typename ToSeq<T_1 >::type Seq1;
    typedef typename ToSeq<T_2 >::type Seq2;
public:
    typedef typename bmpl::copy<
    Seq2,
    bmpl::back_inserter< Seq1>
    >::type type;
};

} //namespace PMacc
