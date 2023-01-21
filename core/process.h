#ifndef PROCESS_H
#define PROCESS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>

#include "util.h"
#include "include/common.h"
#include "env.h"

using namespace v8;
using namespace Deer::Util;
using namespace Deer::Env;

namespace Deer {
    namespace Process {
        void Init(Isolate* isolate, Local<Object> target);
    }
}

#endif 