/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "StackFlow.h"
#include "sample_log.h"
#include <iostream>

using namespace StackFlows;

StackFlow::StackFlow::StackFlow(const std::string &unit_name)
    : work_id_num_cout_(1000), unit_name_(unit_name), rpc_ctx_(std::make_unique<pzmq>(unit_name))
{
    event_queue_.appendListener(EVENT_NONE, std::bind(&StackFlow::_none_event, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_PAUSE, std::bind(&StackFlow::_pause, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_WORK, std::bind(&StackFlow::_work, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_EXIT, std::bind(&StackFlow::_exit, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_SETUP, std::bind(&StackFlow::_setup, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_LINK, std::bind(&StackFlow::_link, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_UNLINK, std::bind(&StackFlow::_unlink, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_TASKINFO, std::bind(&StackFlow::_taskinfo, this, std::placeholders::_1));
    event_queue_.appendListener(EVENT_SYS_INIT, std::bind(&StackFlow::_sys_init, this, std::placeholders::_1));
    rpc_ctx_->register_rpc_action(
        "setup", std::bind(&StackFlow::_rpc_setup, this, std::placeholders::_1, std::placeholders::_2));
    rpc_ctx_->register_rpc_action(
        "pause", std::bind(&StackFlow::_rpc_pause, this, std::placeholders::_1, std::placeholders::_2));
    rpc_ctx_->register_rpc_action("work",
                                  std::bind(&StackFlow::_rpc_work, this, std::placeholders::_1, std::placeholders::_2));
    rpc_ctx_->register_rpc_action("exit",
                                  std::bind(&StackFlow::_rpc_exit, this, std::placeholders::_1, std::placeholders::_2));
    rpc_ctx_->register_rpc_action("link",
                                  std::bind(&StackFlow::_rpc_link, this, std::placeholders::_1, std::placeholders::_2));
    rpc_ctx_->register_rpc_action(
        "unlink", std::bind(&StackFlow::_rpc_unlink, this, std::placeholders::_1, std::placeholders::_2));
    rpc_ctx_->register_rpc_action(
        "taskinfo", std::bind(&StackFlow::_rpc_taskinfo, this, std::placeholders::_1, std::placeholders::_2));

    status_.store(0);
    exit_flage_.store(false);
    even_loop_thread_ = std::make_unique<std::thread>(std::bind(&StackFlow::even_loop, this));
    llm_channel_obj::uart_push_url = std::string("ipc:///tmp/llm/5556.sock");
    status_.store(1);
}

StackFlow::~StackFlow()
{
    while (1)
    {
        auto iteam = llm_task_channel_.begin();
        if (iteam == llm_task_channel_.end())
        {
            break;
        }
        sys_release_unit(iteam->first, "");
        iteam->second.reset();
        llm_task_channel_.erase(iteam->first);
    }
    exit_flage_.store(true);
    event_queue_.enqueue(EVENT_NONE, nullptr);
    even_loop_thread_->join();
}

void StackFlow::even_loop()
{
    pthread_setname_np(pthread_self(), "even_loop"); 

    while (!exit_flage_.load())
    {
        event_queue_.wait();
        event_queue_.process();
    }
}

void StackFlow::_none_event(const std::shared_ptr<void> &arg)
{
    // std::shared_ptr<stackflow_data> originalPtr = std::static_pointer_cast<stackflow_data>(arg);
}

void StackFlow::_sys_init(const std::shared_ptr<void> &arg)
{
    // todo:...
}

std::string StackFlow::_rpc_setup(pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data)
{
    event_queue_.enqueue(EVENT_SETUP, std::make_shared<stackflow_data>(data->get_param(0), data->get_param(1)));
    return std::string("None");
}

int StackFlow::setup(const std::string &zmq_url, const std::string &raw)
{
    ALOGI("StackFlow::setup raw zmq_url:%s raw:%s", zmq_url.c_str(), raw.c_str());
    int workid_num = sys_register_unit(unit_name_);
    std::string work_id = unit_name_ + "." + std::to_string(workid_num);
    auto task_channel = get_channel(workid_num);
    task_channel->set_push_url(zmq_url);
    task_channel->request_id_ = sample_json_str_get(raw, "request_id");
    task_channel->work_id_ = work_id;
    if (setup(work_id, sample_json_str_get(raw, "object"), sample_json_str_get(raw, "data")))
    {
        sys_release_unit(workid_num, work_id);
    }
    return 0;
}

int StackFlow::setup(const std::string &work_id, const std::string &object, const std::string &data)
{
    ALOGI("StackFlow::setup");
    if (_setup_)
    {
        return _setup_(work_id, object, data);
    }
    nlohmann::json error_body;
    error_body["code"] = -18;
    error_body["message"] = "not have unit action!";
    send("None", "None", error_body, work_id);
    return -1;
}

std::string StackFlow::_rpc_link(pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data)
{
    event_queue_.enqueue(EVENT_LINK, std::make_shared<stackflow_data>(data->get_param(0), data->get_param(1)));
    return std::string("None");
}

void StackFlow::link(const std::string &zmq_url, const std::string &raw)
{
    ALOGI("StackFlow::link raw");
    std::string work_id = sample_json_str_get(raw, "work_id");
    try
    {
        auto task_channel = get_channel(sample_get_work_id_num(work_id));
        task_channel->set_push_url(zmq_url);
    }
    catch (...)
    {
    }
    link(work_id, sample_json_str_get(raw, "object"), sample_json_str_get(raw, "data"));
}

void StackFlow::link(const std::string &work_id, const std::string &object, const std::string &data)
{
    ALOGI("StackFlow::link");
    if (_link_)
    {
        _link_(work_id, object, data);
        return;
    }
    nlohmann::json error_body;
    error_body["code"] = -18;
    error_body["message"] = "not have unit action!";
    send("None", "None", error_body, work_id);
}

std::string StackFlow::_rpc_unlink(pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data)
{
    event_queue_.enqueue(EVENT_UNLINK, std::make_shared<stackflow_data>(data->get_param(0), data->get_param(1)));
    return std::string("None");
}

void StackFlow::unlink(const std::string &zmq_url, const std::string &raw)
{
    ALOGI("StackFlow::unlink raw");
    std::string work_id = sample_json_str_get(raw, "work_id");
    try
    {
        auto task_channel = get_channel(sample_get_work_id_num(work_id));
        task_channel->set_push_url(zmq_url);
    }
    catch (...)
    {
    }
    unlink(work_id, sample_json_str_get(raw, "object"), sample_json_str_get(raw, "data"));
}

void StackFlow::unlink(const std::string &work_id, const std::string &object, const std::string &data)
{
    ALOGI("StackFlow::unlink");
    if (_unlink_)
    {
        _unlink_(work_id, object, data);
        return;
    }
    nlohmann::json error_body;
    error_body["code"] = -18;
    error_body["message"] = "not have unit action!";
    send("None", "None", error_body, work_id);
}

std::string StackFlow::_rpc_work(pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data)
{
    event_queue_.enqueue(EVENT_WORK, std::make_shared<stackflow_data>(data->get_param(0), data->get_param(1)));
    return std::string("None");
}

void StackFlow::work(const std::string &zmq_url, const std::string &raw)
{
    ALOGI("StackFlow::work raw");
    std::string work_id = sample_json_str_get(raw, "work_id");
    try
    {
        auto task_channel = get_channel(sample_get_work_id_num(work_id));
        task_channel->set_push_url(zmq_url);
    }
    catch (...)
    {
    }
    work(work_id, sample_json_str_get(raw, "object"), sample_json_str_get(raw, "data"));
}

void StackFlow::work(const std::string &work_id, const std::string &object, const std::string &data)
{
    ALOGI("StackFlow::work");
    if (_work_)
    {
        _work_(work_id, object, data);
        return;
    }
    nlohmann::json error_body;
    error_body["code"] = -18;
    error_body["message"] = "not have unit action!";
    send("None", "None", error_body, work_id);
}

std::string StackFlow::_rpc_exit(pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data)
{
    event_queue_.enqueue(EVENT_EXIT, std::make_shared<stackflow_data>(data->get_param(0), data->get_param(1)));
    return std::string("None");
}

int StackFlow::exit(const std::string &zmq_url, const std::string &raw)
{
    ALOGI("StackFlow::exit raw");
    std::string work_id = sample_json_str_get(raw, "work_id");
    try
    {
        auto task_channel = get_channel(sample_get_work_id_num(work_id));
        task_channel->set_push_url(zmq_url);
    }
    catch (...)
    {
    }
    if (exit(work_id, sample_json_str_get(raw, "object"), sample_json_str_get(raw, "data")) == 0)
    {
        return (int)sys_release_unit(-1, work_id);
    }
    return 0;
}

int StackFlow::exit(const std::string &work_id, const std::string &object, const std::string &data)
{
    ALOGI("StackFlow::exit");
    if (_exit_)
    {
        return _exit_(work_id, object, data);
    }
    nlohmann::json error_body;
    error_body["code"] = -18;
    error_body["message"] = "not have unit action!";
    send("None", "None", error_body, work_id);
    return 0;
}

std::string StackFlow::_rpc_pause(pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data)
{
    event_queue_.enqueue(EVENT_PAUSE, std::make_shared<stackflow_data>(data->get_param(0), data->get_param(1)));
    return std::string("None");
}

void StackFlow::pause(const std::string &zmq_url, const std::string &raw)
{
    ALOGI("StackFlow::pause raw");
    std::string work_id = sample_json_str_get(raw, "work_id");
    try
    {
        auto task_channel = get_channel(sample_get_work_id_num(work_id));
        task_channel->set_push_url(zmq_url);
    }
    catch (...)
    {
    }
    pause(work_id, sample_json_str_get(raw, "object"), sample_json_str_get(raw, "data"));
}

void StackFlow::pause(const std::string &work_id, const std::string &object, const std::string &data)
{
    ALOGI("StackFlow::pause");
    if (_pause_)
    {
        _pause_(work_id, object, data);
        return;
    }
    nlohmann::json error_body;
    error_body["code"] = -18;
    error_body["message"] = "not have unit action!";
    send("None", "None", error_body, work_id);
}

std::string StackFlow::_rpc_taskinfo(pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data)
{
    event_queue_.enqueue(EVENT_TASKINFO, std::make_shared<stackflow_data>(data->get_param(0), data->get_param(1)));
    return std::string("None");
}

void StackFlow::taskinfo(const std::string &zmq_url, const std::string &raw)
{
    // ALOGI("StackFlow::taskinfo raw");
    std::string work_id = sample_json_str_get(raw, "work_id");
    try
    {
        auto task_channel = get_channel(sample_get_work_id_num(work_id));
        task_channel->set_push_url(zmq_url);
    }
    catch (...)
    {
    }
    taskinfo(work_id, sample_json_str_get(raw, "object"), sample_json_str_get(raw, "data"));
}

void StackFlow::taskinfo(const std::string &work_id, const std::string &object, const std::string &data)
{
    // ALOGI("StackFlow::taskinfo");
    if (_taskinfo_)
    {
        _taskinfo_(work_id, object, data);
        return;
    }
    nlohmann::json error_body;
    error_body["code"] = -18;
    error_body["message"] = "not have unit action!";
    send("None", "None", error_body, work_id);
}

int StackFlow::sys_register_unit(const std::string &unit_name)
{
    int work_id_number;
    std::string str_port;
    std::string out_port;
    std::string inference_port;

    unit_call("sys", "register_unit", unit_name, [&](const std::shared_ptr<StackFlows::pzmq_data> &pzmg_msg)
              {
        str_port       = pzmg_msg->get_param(1);
        out_port       = pzmg_msg->get_param(0, str_port);
        inference_port = pzmg_msg->get_param(1, str_port);
        str_port       = pzmg_msg->get_param(0); });
    work_id_number = std::stoi(str_port);
    ALOGI("work_id_number:%d, out_port:%s, inference_port:%s ", work_id_number, out_port.c_str(),
          inference_port.c_str());
    llm_task_channel_[work_id_number] = std::make_shared<llm_channel_obj>(out_port, inference_port, unit_name_);
    return work_id_number;
}

bool StackFlow::sys_release_unit(int work_id_num, const std::string &work_id)
{
    std::string _work_id;
    int _work_id_num;
    if (work_id.empty())
    {
        _work_id = sample_get_work_id(work_id_num, unit_name_);
        _work_id_num = work_id_num;
    }
    else
    {
        _work_id = work_id;
        _work_id_num = sample_get_work_id_num(work_id);
    }
    unit_call("sys", "release_unit", _work_id);
    llm_task_channel_[_work_id_num].reset();
    llm_task_channel_.erase(_work_id_num);
    ALOGI("release work_id %s success", _work_id.c_str());
    return false;
}

std::string StackFlow::sys_sql_select(const std::string &key)
{
    return sample_unescapeString(unit_call("sys", "sql_select", key));
}

void StackFlow::sys_sql_set(const std::string &key, const std::string &val)
{
    nlohmann::json out_body;
    out_body["key"] = key;
    out_body["val"] = val;
    unit_call("sys", "sql_set", out_body.dump());
}

void StackFlow::sys_sql_unset(const std::string &key)
{
    unit_call("sys", "sql_unset", key);
}
