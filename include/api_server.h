#pragma once

#include "service_run.h"

// 启动 HTTP REST API server
// 阻塞调用，直到收到 shutdown 信号
// port=0 则让操作系统分配随机端口
int runApiServer(ServiceRun& serviceRun, int port = 8765);
