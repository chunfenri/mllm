#ifndef MLLM_CPUADD_H
#define MLLM_CPUADD_H

#include "Op.hpp"
#include "CPUBackend.hpp"

namespace mllm {

class CPUAdd : public Op {
public:
    CPUAdd(Backend *bn, bool multiThread);
    virtual ~CPUAdd() = default;
    virtual ErrorCode Reshape(vector<shared_ptr<Tensor>> &inputs, vector<shared_ptr<Tensor>> &outputs) override;
    virtual ErrorCode Setup(vector<shared_ptr<Tensor>> &inputs, vector<shared_ptr<Tensor>> &outputs) override;
    virtual ErrorCode Execute(vector<shared_ptr<Tensor>> &inputs, vector<shared_ptr<Tensor>> &outputs) override;

    virtual ErrorCode Load(ParamLoader &loader) override;

private:
    bool support_multi_thread_ = false;
};

class CPUAddCreator : public CPUBackend::Creator {
public:
    virtual Op *Create(OpParam op_param, Backend *bn) const {
        return new CPUAdd(bn, false);
    }
};

} // namespace mllm

#endif // MLLM_CPUADD_H