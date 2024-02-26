#include "Tensor.hpp"

#include <express/ExpressBase.hpp>
#include "backends/cpu/CPUTensorFunction.hpp"

namespace mllm {

Tensor::Tensor(const int batch, const int head, const int sequence, const int dimension) :
    host_ptr_(), capacity_(0) {
    reshape(batch, head, sequence, dimension);
}
Tensor::Tensor(int batch, int head, int sequence, int dimension, Backend *bn, bool do_alloc) {
    dtype_ = MLLM_TYPE_F32;
    setBackend(bn);
    reshape(batch, head, sequence, dimension);
    if (do_alloc) {
        alloc();
    }
}

Tensor::Tensor(const vector<int> &shape) :
    host_ptr_(), capacity_(0) {
    reshape(shape);
}

bool Tensor::reshape(const int batch, const int head, const int sequence, const int dimension) {
    vector<int> shape(4);
    shape[0] = batch;
    switch (ctype_) {
    case BSHD:
        shape[1] = sequence;
        shape[2] = head;
        shape[3] = dimension;
        break;
    case BHDS:
        shape[1] = head;
        shape[2] = dimension;
        shape[3] = sequence;
        break;
    case SBHD:
        shape[0] = sequence;
        shape[1] = batch;
        shape[2] = head;
        shape[3] = dimension;
    default:
        break;
    }
    return reshape(shape);
}

void Tensor::alloc() {
    if (aggregated_) { return; }
    assert(backend_ != nullptr);
    if (masterTensor() != nullptr) {
        return;
    }
    if (!shape_offset_.empty() & !shape_master_.empty()) {
        return;
    }
    if (allocated_ != count_) {
        if (host_ptr_ != nullptr) {
            backend_->free(host_ptr_);
            host_ptr_ = nullptr;
        }
        if (count_ > 0) {
            backend_->alloc(&host_ptr_, cntSize(), 8);
        }
        allocated_ = count_;
    }
}

bool Tensor::reshape(const int batch, const int channel, const int time, const int height, const int width) {
    if (ctype_ != BTHWC) {
        ctype_ = BCTHW;
        vector<int> shape(5);
        shape[0] = batch;
        shape[1] = channel;
        shape[2] = time;
        shape[3] = height;
        shape[4] = width;
        return reshape(shape);
    } else {
        vector<int> shape(5);
        shape[0] = batch;
        shape[1] = time;
        shape[2] = height;
        shape[3] = width;
        shape[4] = channel;
        return reshape(shape);
    }
}

map<string, Tensor> Tensor::gph_;

template <typename Func>
Tensor &Tensor::binaryCompute(Func operation, string append_s, float data) {
    const std::string next_name = name_ + append_s;
    switch (status_) {
    case TENSOR_DYNAMIC: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(backend_);
            gph_[next_name].setName(next_name);
        }
        CPUbinaryFunction::reshape(gph_[name_], gph_[next_name]);
        CPUbinaryFunction::setup(gph_[name_], gph_[next_name]);
        CPUbinaryFunction::execute(gph_[name_], gph_[next_name], operation, data);
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(backend_);
            gph_[next_name].setName(next_name);
        }
        CPUbinaryFunction::reshape(gph_[name_], gph_[next_name]);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        CPUbinaryFunction::setup(gph_[name_], gph_[next_name]);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        CPUbinaryFunction::execute(gph_[name_], gph_[next_name], operation, data);
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = status_;
    return gph_[next_name];
}
Tensor &Tensor::operator+(float data) {
    return binaryCompute(std::plus<float>(), "-TDadd",  data);
}
Tensor &Tensor::operator-(float data) {
    return binaryCompute(std::minus<float>(), "-TDsub",  data);
}
Tensor &Tensor::operator*(float data) {
    return binaryCompute(std::multiplies<float>(), "-TDmul",  data);
}
Tensor &Tensor::operator/(float data) {
    return binaryCompute(std::divides<float>(), "-TDdiv",  data);
}
Tensor &Tensor::operator/(double data) {
    return binaryCompute(std::divides<float>(), "-TDdiv",  static_cast<float>(data));
}
template <typename Func>
Tensor &Tensor::binaryTwoCompute(Func operation, string append_s, Tensor& other) {
    int thread_count = 4;
    const std::string next_name = name_ + append_s;
    switch (status_) {
    case TENSOR_DYNAMIC: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(backend_);
            gph_[next_name].setName(next_name);
        }
        CPUbinaryTwoFunction::reshape(gph_[name_], gph_[other.name_], gph_[next_name]);
        CPUbinaryTwoFunction::setup(gph_[name_], gph_[other.name_], gph_[next_name]);
        CPUbinaryTwoFunction::execute(gph_[name_], gph_[other.name_], gph_[next_name], operation);
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(backend_);
            gph_[next_name].setName(next_name);
        }
        CPUbinaryTwoFunction::reshape(gph_[name_], gph_[other.name_], gph_[next_name]);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        CPUbinaryTwoFunction::setup(gph_[name_], gph_[other.name_], gph_[next_name]);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        CPUbinaryTwoFunction::execute(gph_[name_], gph_[other.name_], gph_[next_name], operation);
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = status_;
    return gph_[next_name];
}
Tensor& Tensor::operator+(Tensor& other) {
    return binaryTwoCompute(std::plus<float>(), "-TTadd", other);
}
Tensor& Tensor::operator-(Tensor& other){
    return binaryTwoCompute(std::minus<float>(), "-TTsub", other);
}
Tensor& Tensor::operator*(Tensor& other){
    return binaryTwoCompute(std::multiplies<float>(), "-TTmul", other);
}
Tensor& Tensor::operator/(Tensor& other){
    return binaryTwoCompute(std::divides<float>(), "-TTdiv", other);
}

Tensor& Tensor::view(int b, int h, int s, int d) {
    const std::string next_name = name_ + "-view";
    switch (status_) {
    case TENSOR_DYNAMIC: {
        std::cout<<"[TODO] not support dynamic tensor view"<<std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor( backend_);
            gph_[next_name].setName(next_name);
        }
        CPUviewFunction::reshape(gph_[name_], gph_[next_name], b, h, s, d);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        CPUviewFunction::setup(gph_[name_], gph_[next_name], b, h, s, d);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        CPUviewFunction::execute(gph_[name_], gph_[next_name]);
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = status_;
    return gph_[next_name];
}

Tensor& Tensor::flatten(Chl axis_start, Chl axis_end) {
    const std::string next_name = name_ + "-flatten";
    switch (status_) {
    case TENSOR_DYNAMIC: {
        std::cout << "[TODO] not support dynamic tensor view" << std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        // reshape
        // int dim_b = gph_[name_].batch();
        // int dim_h = 0;
        // int dim_s = 0;
        // int dim_d = 0;
        // if (gph_[name_].ctype() == BSHD || gph_[name_].ctype() == BHDS) {
        //     dim_h = gph_[name_].head();
        //     dim_s = gph_[name_].sequence();
        //     dim_d = gph_[name_].dimension();
        //     if (axis_start == BATCH & axis_end == SEQUENCE) {
        //         // data_dims = {-1, HEAD, BATCH + SEQUENCE, DIMENSION};
        //         dim_b = 1;
        //         dim_s = gph_[name_].sequence() * gph_[name_].batch();
        //     } else if (axis_start == HEAD & axis_end == SEQUENCE) {
        //         // data_dims = {BATCH, -1, HEAD + SEQUENCE, DIMENSION};
        //         dim_h = 1;
        //         dim_s = gph_[name_].sequence() * gph_[name_].head();
        //     } else if (axis_start == HEAD & axis_end == DIMENSION) {
        //         // data_dims = {BATCH, HEAD, -1, SEQUENCE + DIMENSION};
        //         dim_h = 1;
        //         dim_d = gph_[name_].dimension() * gph_[name_].head();
        //     } else {
        //         std::cout << "ERROR:  flatten  " << axis_start << "&" << axis_end << std::endl;
        //     }
        // } else {
        //     if (axis_start == TIME & axis_end == CHANNLE) {
        //         // data_dims = {BATCH, -1, TIME + HEIGHT + WIDTH, CHANNLE};
        //         if (gph_[name_].ctype() == BTHWC) {
        //             dim_h = 1;
        //             dim_s = gph_[name_].time() * gph_[name_].height() * gph_[name_].width();
        //             dim_d = gph_[name_].channel();
        //         } else if (gph_[name_].ctype() == BCTHW) {
        //             dim_h = 1;
        //             dim_s = gph_[name_].time() * gph_[name_].height() * gph_[name_].channel();
        //             dim_d = gph_[name_].width();
        //         } else {
        //             std::cout << "ERROR: flatten  " << axis_start << "&" << axis_end << std::endl;
        //         }
        //     }
        // }
        // assert(dim_d+dim_s+dim_h > 0);
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(backend_);
            gph_[next_name].setName(next_name);
        }
        // gph_[next_name].reshape(dim_b, dim_h, dim_s, dim_d);
        CPUflattenFunction::reshape(gph_[name_], gph_[next_name], axis_start, axis_end);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        //alloc
        // if (   (axis_start == TIME & axis_end == CHANNLE && gph_[name_].ctype()!=BSHD)
        //     || (axis_start == BATCH & axis_end == SEQUENCE && gph_[name_].ctype()!=BCTHW)
        //     || (axis_start == HEAD & axis_end == SEQUENCE && gph_[name_].ctype()==BSHD)
        //     || (axis_start == HEAD & axis_end == DIMENSION && gph_[name_].ctype()==BSHD)
        // ){
        //     if(gph_[name_].masterTensor() == nullptr) {
        //         gph_[name_].free();
        //     }
        //     gph_[next_name].setDtype(gph_[name_].dtype());
        //     gph_[next_name].alloc();
        //     gph_[name_].deepCopyFrom(gph_[next_name], false);
        // }else {
        //     std::cout<<"[TODO]Tensor.Flatten not support!!!!"<<std::endl;
        // }
        CPUflattenFunction::setup(gph_[name_], gph_[next_name], axis_start, axis_end);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        CPUflattenFunction::execute(gph_[name_], gph_[next_name]);
        // Tensor::gph_[next_name].saveData<float>();
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = status_;
    return gph_[next_name];
}
/*
Tensor& Tensor::transpose(Chl axis0, Chl axis1) {
    const std::string next_name = name_ + "-transpose";
    switch (status_) {
    case TENSOR_DYNAMIC: {
        std::cout << "[TODO] not support dynamic tensor view" << std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        // reshape
        if (gph_[name_].ctype() == BSHD) {
            int dim_b = 0;
            int dim_h = 0;
            int dim_s = 0;
            int dim_d = 0;
            if (axis0 == SEQUENCE && axis1 == DIMENSION) {
                // outputs[0]->reshape(inputs[0]->batch(), inputs[0]->head(), inputs[0]->dimension(), inputs[0]->sequence());
                dim_b = gph_[name_].batch();
                dim_h = gph_[name_].head();
                dim_s = gph_[name_].dimension();
                dim_d = gph_[name_].sequence();
            } else if (axis0 == BATCH && axis1 == SEQUENCE) {
                // outputs[0]->reshape(inputs[0]->sequence(), inputs[0]->head(), inputs[0]->batch(), inputs[0]->dimension());
                dim_b = gph_[name_].sequence();
                dim_h = gph_[name_].head();
                dim_s = gph_[name_].batch();
                dim_d = gph_[name_].dimension();
            }
            if (gph_.find(next_name) == gph_.end()) {
                gph_[next_name] = Tensor(backend_);
                gph_[next_name].reshape(dim_b, dim_h, dim_s, dim_d);
                gph_[next_name].setName(next_name);
            } else {
                gph_[next_name].reshape(dim_b, dim_h, dim_s, dim_d);
            }
        } else if (axis0 == THW && axis1 == CHANNLE && gph_[name_].ctype() == BCTHW) {
            // outputs[0]->reshape(inputs[0]->batch(), inputs[0]->time(), inputs[0]->height(), inputs[0]->width(), inputs[0]->channel());
            int dim_0 = gph_[name_].batch();
            int dim_1 = gph_[name_].time();
            int dim_2 = gph_[name_].height();
            int dim_3 = gph_[name_].width();
            int dim_4 = gph_[name_].channel();
            if (gph_.find(next_name) == gph_.end()) {
                gph_[next_name] = Tensor(backend_);
                gph_[next_name].reshape(dim_0, dim_1, dim_2, dim_3, dim_4);
                gph_[next_name].setName(next_name);
            } else {
                gph_[next_name].reshape(dim_0, dim_1, dim_2, dim_3, dim_4);
            }
        }else {
            std::cout<<"[TODO]Tensor.Transpose not support!!!!"<<std::endl;
        }
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        //alloc
        if(gph_[name_].masterTensor() == nullptr) {
            gph_[name_].free();
        }
        gph_[next_name].setDtype(gph_[name_].dtype());
        gph_[next_name].alloc();
        gph_[name_].deepCopyFrom(gph_[next_name], false);
        gph_[name_].transShape(axis0, axis1, true);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        // Tensor::gph_[next_name].saveData<float>();
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = status_;
    return gph_[next_name];
}
*/


Tensor& Tensor::transpose(Chl axis0, Chl axis1) {
    switch (status_) {
    case TENSOR_DYNAMIC: {
        std::cout << "[TODO] not support dynamic tensor view" << std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        gph_[name_].transShape(axis0, axis1, true);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        break;
    }
    default: {
    }
    }
    return gph_[name_];
}

Tensor &Tensor::clip(vector<int> b, vector<int> h, vector<int> s, vector<int> d) {
    const std::string next_name = name_ + "-clip";
    switch (status_) {
    case TENSOR_DYNAMIC: {
        std::cout << "[TODO] not support dynamic tensor view" << std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        // // reshape
        // int dim_b = gph_[name_].batch();
        // int dim_h = gph_[name_].head();
        // int dim_s = gph_[name_].sequence();
        // int dim_d = gph_[name_].dimension();
        // std::vector<std::pair<std::vector<int>, int*>> data = {{b, &dim_b}, {h, &dim_h}, {s, &dim_s}, {d, &dim_d}};
        // for (auto& pair : data) {
        //     if (pair.first.size() == 2) {
        //         *pair.second = pair.first[1] - pair.first[0];
        //     } else if (pair.first.size() == 1) {
        //         *pair.second = 1;
        //     }
        // }
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(backend_);
            gph_[next_name].setName(next_name);
        }
        // gph_[next_name].reshape(dim_b, dim_h, dim_s, dim_d);
        CPUclipFunction::reshape(gph_[name_], gph_[next_name], b, h, s, d);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        //alloc
        // gph_[next_name].setDtype(gph_[name_].dtype());
        // gph_[next_name].alloc();
        CPUclipFunction::setup(gph_[name_], gph_[next_name], b, h, s, d);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        // exe
        // if (s.size() == 2) {
        //     for (int b = 0; b < gph_[name_].batch(); ++b) {
        //         memcpy(gph_[next_name].hostPtr<float>() + gph_[next_name].offset(b, 0, 0, 0),
        //                gph_[name_].hostPtr<float>() + gph_[name_].offset(b, 0, s[0], 0),
        //                gph_[name_].head() * (s[1] - s[0]) * gph_[name_].dimension() * sizeof(float));
        //     }
        // } else if (s.size() == 1) {
        //     int seq_idx = s[0];
        //     if (seq_idx < 0) {
        //         seq_idx = gph_[name_].sequence() + seq_idx;
        //     }
        //     for (int b = 0; b < gph_[name_].batch(); ++b) {
        //         memcpy(gph_[next_name].hostPtr<float>() + gph_[next_name].offset(b, 0, 0, 0),
        //                gph_[name_].hostPtr<float>() + gph_[name_].offset(b, 0, seq_idx, 0),
        //                gph_[name_].head() * 1 * gph_[name_].dimension() * sizeof(float));
        //     }
        // }else {
        //     std::cout<<"[TODO]Tensor.CLip not support!!!!"<<std::endl;
        // }
        CPUclipFunction::execute(gph_[name_], gph_[next_name], b, h, s, d);
        // Tensor::gph_[next_name].saveData<float>();
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = status_;
    return gph_[next_name];
}

Tensor &Tensor::cat(vector<Tensor> input_tensors, Chl axis) {
    const std::string next_name = input_tensors[0].name() + "-cat";
    int expd_batch_ = input_tensors[0].batch();
    int expd_batch_input_idx = 0;
    for (int ii = 0; ii < input_tensors.size(); ++ii) {
        auto input = input_tensors[ii];
        if (input.batch() > expd_batch_) {
            expd_batch_ = input.batch();
            expd_batch_input_idx = ii;
        }
    }
    vector<Tensor*> inputs = {};
    for (const auto& input_tensor : input_tensors) {
        inputs.push_back(&gph_[input_tensor.name()]);
    }
    switch (input_tensors[0].status()) {
    case TENSOR_DYNAMIC: {
        std::cout << "[TODO] not support dynamic tensor view" << std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        // int dim_b = expd_batch_;
        // int dim_h = gph_[input_tensors[0].name()].head();
        // int dim_s = gph_[input_tensors[0].name()].sequence();
        // int dim_d = gph_[input_tensors[0].name()].dimension();
        // int sizes[] = {0, 0, 0, 0};
        // Chl axes[] = {BATCH, HEAD, SEQUENCE, DIMENSION};
        // int* dims[] = {&dim_b, &dim_h, &dim_s, &dim_d};
        // for (int i = 0; i < 4; i++) {
        //     if (axis == axes[i]) {
        //         for (auto input : input_tensors) {
        //             sizes[i] += (i == 0) ? input.batch() : (i == 1) ? input.head() : (i == 2) ? input.sequence() : input.dimension();
        //         }
        //         *dims[i] = sizes[i];
        //         break;
        //     }
        // }
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(input_tensors[0].backend());
            gph_[next_name].setName(next_name);
        }
        // gph_[next_name].reshape(dim_b, dim_h, dim_s, dim_d);
        CPUcatFunction::reshape(inputs, gph_[next_name], axis, expd_batch_, expd_batch_input_idx);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        // alloc
        // gph_[next_name].setDtype(gph_[input_tensors[0].name()].dtype());
        // gph_[next_name].alloc();
        // if (axis == SEQUENCE && gph_[input_tensors[0].name()].head() != 1) {
        //     int cbatch = 0;
        //     int chead = 0;
        //     int cseq = 0;
        //     int cdim = 0;
        //     for (int idx = 0; idx < input_tensors.size(); idx++) {
        //         if (gph_[input_tensors[idx].name()].masterTensor() == nullptr) {
        //             gph_[input_tensors[idx].name()].free();
        //         }
        //         if (idx > 0) {
        //             cseq += gph_[input_tensors[idx - 1].name()].sequence();
        //         }
        //         gph_[input_tensors[idx].name()].deepCopyFrom(gph_[next_name], false, {cbatch, chead, cseq, cdim}); // b,h,s,d
        //     }
        // } else {
        //     // std::cout << "[TODO]Tensor.Cat not support!!!!" << std::endl;
        // }
        CPUcatFunction::setup(inputs, gph_[next_name], axis, expd_batch_, expd_batch_input_idx);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        CPUcatFunction::execute(inputs, gph_[next_name], axis, expd_batch_, expd_batch_input_idx);
        // exe
        // if (axis == BATCH) {
        //     for (int n = 0; n < input_tensors.size(); ++n) {
        //         auto copysize = gph_[input_tensors[0].name()].batch() * gph_[input_tensors[0].name()].head() * gph_[input_tensors[0].name()].sequence() * gph_[input_tensors[0].name()].dimension();
        //         memcpy(gph_[next_name].ptrAt<float>(n * gph_[input_tensors[0].name()].batch(), 0, 0, 0),
        //                gph_[input_tensors[n].name()].ptrAt<float>(0, 0, 0, 0),
        //                sizeof(float) * copysize);
        //     }
        // } else if (axis == DIMENSION) {
        //     for (int n = 0; n < expd_batch_; ++n) {
        //         for (int c = 0; c < gph_[input_tensors[0].name()].head(); ++c) {
        //             for (int h = 0; h < gph_[input_tensors[0].name()].sequence(); ++h) {
        //                 int w = 0;
        //                 for (int idx = 0; idx < input_tensors.size(); idx++) {
        //                     int dim_size = gph_[input_tensors[idx].name()].dimension();
        //                     auto n_ = n;
        //                     if (idx != expd_batch_input_idx) {
        //                         n_ = 0;
        //                     }
        //                     memcpy(gph_[next_name].ptrAt<float>(n, c, h, w),
        //                            gph_[input_tensors[idx].name()].ptrAt<float>(n_, c, h, 0),
        //                            sizeof(float) * (dim_size));
        //                     w += dim_size;
        //                 }
        //             }
        //         }
        //     }
        // } else if ((axis == SEQUENCE) && gph_[input_tensors[0].name()].head() != 1) {
        // } else if ((axis == SEQUENCE) && gph_[input_tensors[0].name()].head() == 1) {
        //     for (int n = 0; n < expd_batch_; ++n) {
        //         int h = 0;
        //         for (int idx = 0; idx < input_tensors.size(); idx++) {
        //             auto n_ = n;
        //             if (idx != expd_batch_input_idx) {
        //                 n_ = 0;
        //             }
        //             memcpy(gph_[next_name].ptrAt<float>(n, 0, h, 0),
        //                    gph_[input_tensors[idx].name()].ptrAt<float>(n_, 0, 0, 0),
        //                    sizeof(float) * (gph_[input_tensors[idx].name()].sequence() * gph_[input_tensors[idx].name()].dimension()));
        //             h += gph_[input_tensors[idx].name()].sequence();
        //         }
        //     }
        // }
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = input_tensors[0].status();
    return gph_[next_name];
}

Tensor &Tensor::mm(Tensor& input0, Tensor& input1) {
    const std::string next_name = input0.name() + "-mm-" + input1.name();
    switch (input0.status()) {
    case TENSOR_DYNAMIC: {
        std::cout << "[TODO] not support dynamic tensor view" << std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(input0.backend());
            gph_[next_name].setName(next_name);
        }
        CPUmmFunction::reshape(gph_[input0.name()], gph_[input1.name()], gph_[next_name]);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        CPUmmFunction::setup(gph_[input0.name()], gph_[input1.name()], gph_[next_name]);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        CPUmmFunction::execute(gph_[input0.name()], gph_[input1.name()], gph_[next_name]);
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = input0.status();
    return gph_[next_name];
}

Tensor& Tensor::norm(int L_n) {
    // int thread_count = 4;
    assert(L_n ==1 || L_n ==2);
    const std::string next_name = name_ + "-norm";
    switch (status_) {
    case TENSOR_DYNAMIC: {
        std::cout << "[TODO] not support dynamic tensor view" << std::endl;
        break;
    }
    case TENSOR_STATIC_INIT: {
        if (gph_.find(name_) == gph_.end()) {
            gph_[name_] = *this;
            gph_[name_].status() = status_;
        }
        // reshape
        // int dim_b = gph_[name_].batch();
        // int dim_h = gph_[name_].head();
        // int dim_s = gph_[name_].sequence();
        // int dim_d = gph_[name_].dimension();
        if (gph_.find(next_name) == gph_.end()) {
            gph_[next_name] = Tensor(backend_);
            gph_[next_name].setName(next_name);
        }
        // gph_[next_name].reshape(dim_b, dim_h, dim_s, dim_d);
        CPUnormFunction::reshape(gph_[name_], gph_[next_name], L_n);
        break;
    }
    case TENSOR_STATIC_SHAPED: {
        //alloc
        // gph_[next_name].setDtype(gph_[name_].dtype());
        // gph_[next_name].alloc();
        CPUnormFunction::setup(gph_[name_], gph_[next_name], L_n);
        break;
    }
    case TENSOR_STATIC_ALLOCED: {
        // exe
//         for (int h = 0; h < gph_[name_].head(); h++) {
//             for (int n = 0; n < gph_[name_].batch(); n++) {
//                 for (int s = 0; s < gph_[name_].sequence(); s++) {
//                     if (L_n == 2) {
//                         float sum_of_squares = 0.0f;
//                         for (int d = 0; d < gph_[name_].dimension(); ++d) {
//                             sum_of_squares += gph_[name_].dataAt<float>(n, h, s,d) * gph_[name_].dataAt<float>(n, h, s,d);
//                         }
//                         float l2_norm = std::sqrt(sum_of_squares);
// #pragma omp parallel for num_threads(thread_count)
//                         for (int d = 0; d < gph_[name_].dimension(); d++) {
//                             gph_[next_name].setDataAt<float>(n, h, s,d, l2_norm);
//                         }
//                     } else {
//                         float sum_of_abs_values = 0.0f;
//                         for (int d = 0; d < gph_[name_].dimension(); ++d) {
//                             sum_of_abs_values += std::abs(gph_[name_].dataAt<float>(n, h, s,d));
//                         }
// #pragma omp parallel for num_threads(thread_count)
//                         for (int d = 0; d < gph_[name_].dimension(); d++) {
//                              gph_[next_name].setDataAt<float>(n, h, s,d, sum_of_abs_values);
//                         }
//
//                     }
//                 }
//             }
//         }
        CPUnormFunction::execute(gph_[name_], gph_[next_name], L_n);
        break;
    }
    default: {
    }
    }
    gph_[next_name].status() = status_;
    return gph_[next_name];
}
} // namespace mllm