/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file asum.h
 * @brief BLAS Level 1 asum template function implementation.
 *
 * This file is part of XF BLAS Library.
 */

#ifndef XF_BLAS_ASUM_H
#define XF_BLAS_ASUM_H


#ifndef __cplusplus
#error "BLAS Library only works with C++."
#endif

#include "ap_int.h"
#include "hls_stream.h"
#include "xf_blas/utility.h"

namespace xf {
namespace linear_algebra {
namespace blas {

  namespace{
    template<typename t_DataType, 
      unsigned int t_LogParEntries, 
      unsigned int t_DataWidth = sizeof(t_DataType) << 3, 
      typename t_IndexType = unsigned int>
        void preProcess(
            unsigned int p_numElems,
            hls::stream<WideType<t_DataType, 1<<t_LogParEntries, t_DataWidth> > & p_x,
            hls::stream<t_DataType> & p_data
            ) {
          const unsigned int l_ParEntries = 1 << t_LogParEntries;
          for(t_IndexType i=0;i<p_numElems;i++){
            #pragma HLS PIPELINE
            WideType<t_DataType, 1<<t_LogParEntries, t_DataWidth> l_x = p_x.read();
            t_DataType l_input[l_ParEntries];
            #pragma HLS ARRAY_PARTITION variable=l_input complete dim=1
            for(t_IndexType j=0; j<l_ParEntries;j++){
              #pragma HLS UNROLL
              t_DataType l_data = l_x[j];
              l_input[j]=hls::abs(l_data);
            }
            t_DataType l_sum;
            l_sum = BinarySum<t_DataType, l_ParEntries>::sum(l_input);
            p_data.write(l_sum);
          }
        }
    template<typename t_DataType, 
      unsigned int t_LogDelays, 
      typename t_IndexType = unsigned int>
        void padding(
            unsigned int p_numElems,
            hls::stream<t_DataType> & p_data,
            hls::stream<t_DataType> & p_pad
            ) {
          const unsigned int l_Delays = 1 << t_LogDelays;
          const unsigned int l_numIter = (p_numElems + l_Delays -1) >> t_LogDelays;
          const unsigned int l_numExtra = (l_numIter << t_LogDelays) - p_numElems;
          hls::stream<t_DataType> l_inner;
          #pragma HLS stream variable=l_inner depth=2
          for(t_IndexType i=0;i<p_numElems;i++){
            #pragma HLS PIPELINE
            p_pad.write(p_data.read());
          }
          for(t_IndexType i=0;i<l_numExtra;i++){
            #pragma HLS PIPELINE
            p_pad.write(0);
          }
        }

    template<typename t_DataType, 
      unsigned int t_LogDelays, 
      typename t_IndexType = unsigned int>
        void postProcess(
            unsigned int p_numElems,
            hls::stream<t_DataType> & p_pad,
            t_DataType & p_sum
            ) {
          const unsigned int l_Delays = 1 << t_LogDelays;
          const unsigned int l_numIter = (p_numElems + l_Delays -1) >> t_LogDelays;
          t_DataType l_finalSum = 0;
          for(t_IndexType i=0;i<l_numIter;i++){
            #pragma HLS PIPELINE II=l_Delays
            t_DataType l_input[l_Delays];
            #pragma HLS ARRAY_PARTITION variable=l_input complete dim=1
            for(t_IndexType j=0; j<l_Delays;j++){
              #pragma HLS UNROLL
              l_input[j]=p_pad.read();
            }
            l_finalSum += BinarySum<t_DataType, l_Delays>::sum(l_input);
          }
          p_sum = l_finalSum;
        }
  }

  /**
   * @brief asum function that returns the sum of the magnitude of vector elements.
   *
   * @tparam t_DataType the data type of the vector entries
   * @tparam t_DataWidth the datawidth of the datatype t_DataType of the input vector 
   * @tparam t_LogParEntries log2 of the number of parallelly processed entries in the input vector 
   * @tparam t_IndexType the datatype of the index 
   *
   * @param p_n the number of entries in the input vector p_x, p_n % l_ParEntries == 0
   * @param p_x the input stream of packed vector entries
   * @param p_sum the sum, which is 0 if p_n <= 0
   */

  template<typename t_DataType, 
    unsigned int t_LogParEntries, 
    unsigned int t_DataWidth = sizeof(t_DataType) << 3, 
    typename t_IndexType=unsigned int>
      void asum(
          unsigned int p_n,
          hls::stream<WideType<t_DataType, 1<<t_LogParEntries, t_DataWidth> > & p_x,
          t_DataType &p_sum
          ) {
        #ifndef __SYNTHESIS__
        assert(p_n % ( 1 << t_LogParEntries) == 0);
        #endif
        const unsigned int l_LogDelays = AdderDelay<t_DataType>::m_logDelays;
        #pragma HLS DATAFLOW
        hls::stream<t_DataType> l_data, l_pad;
        #pragma HLS stream variable=l_data depth=2
        #pragma HLS stream variable=l_pad depth=2
        unsigned int l_numElem = p_n >> t_LogParEntries;
        preProcess<t_DataType, t_LogParEntries, t_DataWidth, t_IndexType>(l_numElem, p_x, l_data);
        padding<t_DataType, l_LogDelays, t_IndexType>(l_numElem, l_data, l_pad);
        postProcess<t_DataType, l_LogDelays, t_IndexType>(l_numElem, l_pad, p_sum);
      }


} //end namespace blas
} //end namspace linear_algebra
} //end namespace xf

#endif
