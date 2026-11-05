#pragma once

#include "service_run.h"

// 基于 stdin/stdout 的 JSON 行协议服务循环
// 阻塞调用，直到 stdin 关闭或收到 shutdown 指令
int runStdioServer(ServiceRun& serviceRun);
