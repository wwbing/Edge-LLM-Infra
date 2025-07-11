
/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "zmq_bus.h"

#include "all.h"
#include <stdbool.h>
#include <functional>
#include <cstring>
#include <StackFlowUtil.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#ifdef ENABLE_BSON
#include <bson/bson.h>
#endif

using namespace StackFlows;

zmq_bus_com::zmq_bus_com()
{
    exit_flage      = 1;
    err_count       = 0;
    json_str_flage_ = 0;
}

void zmq_bus_com::work(const std::string &zmq_url_format, int port)
{
    _port             = port;
    exit_flage        = 1;
    std::string ports = std::to_string(port);
    std::vector<char> buff(zmq_url_format.length() + ports.length(), 0);
    sprintf((char *)buff.data(), zmq_url_format.c_str(), port);
    _zmq_url      = std::string((char *)buff.data());
    user_chennal_ = std::make_unique<pzmq>(
        _zmq_url, ZMQ_PULL,
        [this](pzmq *_pzmq, const std::shared_ptr<pzmq_data> &data) { this->send_data(data->string()); });
}

void zmq_bus_com::stop()
{
    exit_flage = 0;
    user_chennal_.reset();
}

void zmq_bus_com::on_data(const std::string &data)
{
    unit_action_match(_port, data);
}

void zmq_bus_com::send_data(const std::string &data)
{
}

zmq_bus_com::~zmq_bus_com()
{
    if (exit_flage) {
        stop();
    }
}

int zmq_bus_publisher_push(const std::string &work_id, const std::string &json_str)
{
    ALOGW("zmq_bus_publisher_push json_str:%s", json_str.c_str());

    if (work_id.empty()) {
        ALOGW("work_id is empty");
        return -1;
    }
    unit_data *unit_p = NULL;
    SAFE_READING(unit_p, unit_data *, work_id);
    if (unit_p) unit_p->send_msg(json_str);
    ALOGW("zmq_bus_publisher_push work_id:%s", work_id.c_str());

    else
    {
        ALOGW("zmq_bus_publisher_push failed, not have work_id:%s", work_id.c_str());
        return -1;
    }
    return 0;
}

void *usr_context;

void zmq_com_send(int com_id, const std::string &out_str)
{
    char zmq_push_url[128];
    sprintf(zmq_push_url, zmq_c_format.c_str(), com_id);
    pzmq _zmq(zmq_push_url, ZMQ_PUSH);
    std::string out = out_str + "\n";
    _zmq.send_data(out);
}

void zmq_bus_com::select_json_str(const std::string &json_src, std::function<void(const std::string &)> out_fun)
{
    const char *data = json_src.c_str();
    // json_str_.reserve(json_str_.length() + json_src.length());

    const auto length = json_src.length();
    json_str_.reserve(json_str_.capacity() + length);

    for (int i = 0; i < json_src.length(); i++) {
        // ARM NEON指令集优化（检测大括号）
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        if (json_src.length() - i >= 16) {
            uint8x16_t target_open  = vdupq_n_u8('{');
            uint8x16_t target_close = vdupq_n_u8('}');
            uint8x16_t input_vector = vld1q_u8((const uint8_t *)&data[i]);

            uint8x16_t result_open  = vceqq_u8(input_vector, target_open);
            uint8x16_t result_close = vceqq_u8(input_vector, target_close);
            uint8x16_t result_mask  = vorrq_u8(result_open, result_close);

            __uint128_t jflage;
            vst1q_u8((uint8_t *)&jflage, result_mask);
            if (jflage == 0) {
                json_str_.append(data + i, 16);
                i += 15;
                continue;
            }
        }
#endif

        // 逐字符处理
        json_str_ += data[i];
        int last_index = (i == 0) ? 0 : (i - 1);

        if ((data[i] == '{') && (data[last_index] != '\\')) json_str_flage_++;
        if ((data[i] == '}') && (data[last_index] != '\\')) json_str_flage_--;

        if (json_str_flage_ == 0) {
            if ((json_str_[0] == '{') && (json_str_.back() == '}')) {
                out_fun(json_str_);
            }
            json_str_.clear();
        }

        if (json_str_flage_ < 0) {
            json_str_flage_ = 0;
            json_str_.clear();
            throw std::runtime_error("JSON括号不匹配");
        }
    }
}