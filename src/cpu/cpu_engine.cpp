/*******************************************************************************
* Copyright 2016 Intel Corporation
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
*******************************************************************************/

#include <assert.h>

#include "cpu_engine.hpp"
#include "cpu_memory.hpp"
#include "type_helpers.hpp"

#include "cpu_concat.hpp"

#include "cpu/jit_avx2_convolution.hpp"
#include "cpu/ref_convolution.hpp"
#include "cpu/jit_avx2_relu.hpp"
#include "cpu/ref_relu.hpp"
#include "cpu/jit_avx2_pooling.hpp"
#include "cpu/ref_pooling.hpp"
#include "cpu/jit_avx2_lrn.hpp"
#include "cpu/ref_lrn.hpp"
#include "cpu/jit_avx2_batch_normalization.hpp"
#include "cpu/ref_batch_normalization.hpp"
#include "cpu/ref_inner_product.hpp"
#include "cpu/gemm_inner_product.hpp"

#include "cpu/simple_reorder.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

using namespace mkldnn::impl::status;

status_t cpu_engine_t::memory_primitive_desc_create(memory_pd_t **pd,
        const memory_desc_t *desc) {
    return safe_ptr_assign<memory_pd_t>(*pd,
            new cpu_memory_t::pd_t(this, desc));
}

status_t cpu_engine_t::view_primitive_desc_create(view_pd_t **view_pd,
            const memory_pd_t *memory_pd, const dims_t dims,
            const dims_t offsets) {
    assert(memory_pd->engine() == this);
    auto mpd = (const cpu_memory_t::pd_t *)memory_pd;
    /* FIXME: what if failed? */
    return safe_ptr_assign<view_pd_t>(*view_pd,
            new cpu_view_t::pd_t(this, mpd, dims, offsets));
}

status_t cpu_engine_t::concat_primitive_desc_create(concat_pd_t **concat_pd,
        const memory_desc_t *output_d, int n, int concat_dim,
        const memory_pd_t **input_pds) {
    assert(input_pds[0]->engine() == this);
    auto i_pds = (const cpu_memory_t::pd_t **)input_pds;
    return safe_ptr_assign<concat_pd_t>(*concat_pd,
            new cpu_concat_t::pd_t(this, output_d, n, concat_dim, i_pds));
}
using rpd_create_f = mkldnn::impl::engine_t::reorder_primitive_desc_create_f;
using pd_create_f = mkldnn::impl::engine_t::primitive_desc_create_f;

namespace {
using namespace mkldnn::impl::data_type;
using namespace mkldnn::impl::memory_format;

static const rpd_create_f cpu_reorder_impl_list[] = {
    simple_reorder_t<f32, any, f32, any, fmt_order::any, spec::direct_copy>::pd_t::create,
    simple_reorder_t<f32, any, f32, any, fmt_order::any, spec::direct_copy_except_dim_0>::pd_t::create,
    simple_reorder_t<f32, nchw, f32, nChw8c, fmt_order::keep>::pd_t::create,
    simple_reorder_t<f32, nchw, f32, nChw8c, fmt_order::reverse>::pd_t::create,
    simple_reorder_t<f32, nchw, f32, nhwc, fmt_order::keep>::pd_t::create,
    simple_reorder_t<f32, nchw, f32, nhwc, fmt_order::reverse>::pd_t::create,
    simple_reorder_t<f32, oihw, f32, OIhw8i8o, fmt_order::keep>::pd_t::create,
    simple_reorder_t<f32, oihw, f32, OIhw8i8o, fmt_order::reverse>::pd_t::create,
    simple_reorder_t<f32, goihw, f32, gOIhw8i8o, fmt_order::keep>::pd_t::create,
    simple_reorder_t<f32, goihw, f32, gOIhw8i8o, fmt_order::reverse>::pd_t::create,
    simple_reorder_t<f32, any, f32, any, fmt_order::any, spec::reference>::pd_t::create,
    nullptr,
};

#define INSTANCE(inst) &primitive_desc_t::create<inst::pd_t>
static const pd_create_f cpu_impl_list[] = {
    /* conv */
    INSTANCE(jit_avx2_convolution_t),
    INSTANCE(ref_convolution_t<data_type::f32>),
    /* relu */
    INSTANCE(jit_avx2_relu_fwd_t),
    INSTANCE(ref_relu_fwd_t<data_type::f32>),
    /* pool */
    INSTANCE(jit_avx2_pooling_fwd_t),
    INSTANCE(ref_pooling_fwd_t<data_type::f32>),
    /* lrn */
    INSTANCE(jit_avx2_lrn_fwd_t),
    INSTANCE(ref_lrn_fwd_t<data_type::f32>),
    /* batch normalization */
    INSTANCE(jit_avx2_batch_normalization_fwd_t),
    INSTANCE(ref_batch_normalization_fwd_t<data_type::f32>),
    /* inner product */
    INSTANCE(gemm_inner_product_fwd_t<data_type::f32>),
    INSTANCE(ref_inner_product_fwd_t<data_type::f32>),
    /* conv_relu */
    INSTANCE(jit_avx2_convolution_relu_t),
    INSTANCE(ref_convolution_relu_t<data_type::f32>),
    nullptr,
};
#undef INSTANCE
}

const rpd_create_f* cpu_engine_t::get_reorder_implementation_list() const {
    return cpu_reorder_impl_list;
}

const pd_create_f* cpu_engine_t::get_implementation_list() const {
    return cpu_impl_list;
}

cpu_engine_factory_t engine_factory;

status_t cpu_engine_t::submit(primitive_t *p, event_t *e,
        event_vector &prerequisites) {
    p->execute(e);
    return success;
}

}
}
}

// vim: et ts=4 sw=4 cindent cino^=l0,\:0,N-s
