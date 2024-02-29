#include "QNNGraph.hpp"
#ifdef DEBUGPRINT
#include "Timing.hpp"
#endif

namespace mllm {

QNNGraph::QNNGraph(const NetParameter &param, Backend *bn,
                   unordered_map<string, shared_ptr<Tensor>> &external_tensors,
                   int threadCount) :
    Graph(param, bn, external_tensors, threadCount) {
}

void QNNGraph::QNNThreadExecute() {
    // backend event hook
    this->backend_->onExecuteEnd();
}

void QNNGraph::setUpTensors(std::string name) {
    auto &graph_in_tensors = ops_input_tensors_[op_names_[0]];
    this->backend_->onSetUpStart(graph_in_tensors, name);
    for (auto &t : graph_in_tensors) { t->alloc(); }
    // set graph out tensor TensorType
    auto &graph_out_tensors = ops_output_tensors_[op_names_[op_names_.size() - 1]];
    for (auto &t : graph_out_tensors) {
        t->setTensorType(GRAPH_OUTPUT);
    }
    // set up tensors of ops
    for (const auto &op_name : op_names_) {
        if (ops_not_inputs_empty_[op_name]) {
            ops_[op_name]->setUp(ops_input_tensors_[op_name],
                                 ops_output_tensors_[op_name]);
        } else {
            // std::cout << "op_name:" << op_name << " is not do" << std::endl;
        }
    }
}

//#define SAVECHECK
const vector<shared_ptr<Tensor>> &QNNGraph::forward(bool autofree) {
    // backend event hook
    if (autoregressive_seq_pos_ % 32 == 31 || autoregressive_seq_pos_ == 0)
        this->backend_->onExecuteStart(ops_input_tensors_[op_names_[0]], ops_output_tensors_[op_names_[op_names_.size() - 1]]);

    std::cout << "QNNexecute thread start" << std::endl;

    std::thread *qnnThread = new std::thread(&QNNGraph::QNNThreadExecute, this);

    for (const auto &op_name : op_names_) {
        if (ops_not_inputs_empty_[op_name]) {
#ifdef SAVECHECK
            for (auto &t : ops_input_tensors_[op_name]) {
                t->checkData<float>();
                t->saveData<float>();
            }
#endif
#ifdef DEBUGPRINT
            uint64_t t_start = mllm_time_us();
#endif
            ops_[op_name]->execute(ops_input_tensors_[op_name],
                                   ops_output_tensors_[op_name]);

#ifdef SAVECHECK
            for (auto &t : ops_output_tensors_[op_name]) {
                t->checkData<float>();
                t->saveData<float>();
            }
#endif

#ifdef DEBUGPRINT
            uint64_t t_end = mllm_time_us();
            std::cout << "" << op_name
                      << "       exe_time:" << (t_end - t_start) / 1000.0F << " ms"
                      << std::endl;
#endif
            if (autofree) {
                ops_[op_name]->free(ops_input_tensors_[op_name],
                                    ops_output_tensors_[op_name]);
            }
        } else {
            //            std::cout<<"op_name:"<<op_name<<" is not do"<<std::endl;
        }
    }
    // backend event hook
    // We use a thread to parallel CPU AND QNN execution.
    qnnThread->join();

    autoregressive_seq_pos_ += ops_input_tensors_[op_names_[0]][0]->sequence();

    return ops_output_tensors_[op_names_[op_names_.size() - 1]];
}

} // namespace mllm
