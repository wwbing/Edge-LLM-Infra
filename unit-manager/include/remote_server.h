/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "pzmq.hpp"
#include <vector>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <iostream>

#include "all.h"
#include "unit_data.h"

using namespace StackFlows;

void unit_action_match(int com_id, const std::string &json_str);

void remote_server_work();
void remote_server_stop_work();
