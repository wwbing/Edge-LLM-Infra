/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "event_loop.h"

#include <fcntl.h>
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <thread>
#include <fstream>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <list>
#include <map>
#include <vector>

#include "all.h"
#include "json.hpp"
#include "zmq_bus.h"
#include "remote_action.h"
#include <simdjson.h>
#include <glob.h>
#include "StackFlowUtil.h"

void usr_print_error(const std::string &request_id, const std::string &work_id, const std::string &error_msg,
                     int zmq_out)
{
    nlohmann::json out_body;
    out_body["request_id"] = request_id;
    out_body["work_id"]    = work_id;
    out_body["created"]    = time(NULL);
    out_body["error"]      = nlohmann::json::parse(error_msg);
    out_body["object"]     = std::string("None");
    out_body["data"]       = std::string("None");
    std::string out        = out_body.dump();
    zmq_com_send(zmq_out, out);
}

std::mutex unit_action_match_mtx;
simdjson::ondemand::parser parser;
typedef int (*sys_fun_call)(int, const nlohmann::json &);

void unit_action_match(int com_id, const std::string &json_str)
{
    std::lock_guard<std::mutex> guard(unit_action_match_mtx);
    simdjson::padded_string json_string(json_str);
    simdjson::ondemand::document doc;
    auto error = parser.iterate(json_string).get(doc);
    ALOGI("json format :%s", json_str.c_str());

    if (error) {
        ALOGE("json format error:%s", json_str.c_str());
        usr_print_error("0", "sys", "{\"code\":-2, \"message\":\"json format error\"}", com_id);
        return;
    }
    std::string_view sv;

    // std::string request_id;
    error = doc["request_id"].get_string().get(sv);
    std::string request_id(sv);
    if (error) {
        ALOGE("miss request_id, error:%s", simdjson::error_message(error));
        usr_print_error("0", "sys", "{\"code\":-2, \"message\":\"json format error\"}", com_id);
        return;
    }
    // std::string work_id;
    error = doc["work_id"].get_string().get(sv);
    std::string work_id(sv);
    if (error) {
        ALOGE("miss work_id, error:%s", simdjson::error_message(error));
        usr_print_error("0", "sys", "{\"code\":-2, \"message\":\"json format error\"}", com_id);
        return;
    }
    if (work_id.empty()) work_id = "sys";
    // std::string action;
    error = doc["action"].get_string().get(sv);
    std::string action(sv);
    if (error) {
        ALOGE("miss action, error:%s", simdjson::error_message(error));
        usr_print_error("0", "sys", "{\"code\":-2, \"message\":\"json format error\"}", com_id);
        return;
    }
    //
    std::vector<std::string> work_id_fragment;
    std::string fragment;
    for (auto c : work_id) {
        if (c != '.') {
            fragment.push_back(c);
        } else {
            work_id_fragment.push_back(fragment);
            fragment.clear();
        }
    }
    if (fragment.length()) work_id_fragment.push_back(fragment);
    if (action == "inference") {
        char zmq_push_url[128];
        int post = sprintf(zmq_push_url, zmq_c_format.c_str(), com_id);
        std::string inference_raw_data;
        inference_raw_data.resize(post + json_str.length() + 13);
        post = sprintf(inference_raw_data.data(), "{\"zmq_com\":\"");
        post += sprintf(inference_raw_data.data() + post, "%s", zmq_push_url);
        post += sprintf(inference_raw_data.data() + post, "\",");
        memcpy(inference_raw_data.data() + post, json_str.data() + 1, json_str.length() - 1);
        int ret = zmq_bus_publisher_push(work_id, inference_raw_data);
        if (ret) {
            usr_print_error(request_id, work_id, "{\"code\":-4, \"message\":\"inference data push false\"}", com_id);
        }
    } else if ((work_id_fragment.size() > 0) && (work_id_fragment[0] == "sys")) {
        std::string unit_action = "sys." + action;
        sys_fun_call call_fun   = NULL;
        SAFE_READING(call_fun, sys_fun_call, unit_action);
        if (call_fun) {
            call_fun(com_id, nlohmann::json::parse(json_str));
        } else {
            usr_print_error(request_id, work_id, "{\"code\":-3, \"message\":\"action match false\"}", com_id);
        }
    } else {
        if ((work_id_fragment[0].length() != 0) && (remote_call(com_id, json_str) != 0)) {
            usr_print_error(request_id, work_id, "{\"code\":-9, \"message\":\"unit call false\"}", com_id);
        }
    }
}