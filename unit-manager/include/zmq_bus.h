/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#if __cplusplus
extern "C"
{
#endif

    void zmq_bus_work();
    void zmq_bus_stop_work();

#if __cplusplus
}

#include "pzmq.hpp"
#include <vector>
#include "unit_data.h"

using namespace StackFlows;

int zmq_bus_publisher_push(const std::string &work_id, const std::string &json_str);
void zmq_com_send(int com_id, const std::string &out_str);

class zmq_bus_com
{
private:
    enum RAW_MSG_TYPE
    {
        RAW_NONE = 0,
        RAW_JSON = 1,
        RAW_BSON = 2,
    };

    int reace_event_;
    int raw_msg_len_;
    std::string raw_msg_buff_;

protected:
    std::string _zmq_url;
    int exit_flage;
    int err_count;
    int _port;
    std::string json_str_;
    int json_str_flage_;

public:
    std::unique_ptr<pzmq> user_chennal_;
    zmq_bus_com();
    void work(const std::string &zmq_url_format, int port);
    void stop();
    void select_json_str(const std::string &json_src, std::function<void(const std::string &)> out_fun);
    virtual void on_data(const std::string &data);
    virtual void on_raw_data(const std::string &data);
    virtual void on_bson_data(const std::string &data);
    virtual void send_data(const std::string &data);
    virtual void reace_data_event();
    virtual void send_data_event();
    ~zmq_bus_com();
};
#endif