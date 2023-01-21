
#include "core.h"

void Deer::Core::register_builtins(Isolate * isolate, Local<Object> Deer) {
    Local<Object> target = Object::New(isolate);
    Loader::Init(isolate, target);
    Util::Init(isolate, target);
    Socket::Init(isolate, target);
    Console::Init(isolate, target);
    Process::Init(isolate, target);
    setObjectValue(isolate, Deer, "buildin", target);
}
