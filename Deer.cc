#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include "libplatform/libplatform.h"
#include "v8.h"
#include "core/env.h"
#include "core/core.h"
#include "core/util.h"
#include "core/loop.h"

using namespace v8;
using namespace Deer::Util;
using namespace Deer::Core;
using namespace Deer::Env;

int main(int argc, char* argv[]) {
  // 不需要输出缓冲，可以实时看到代码里的输出
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  // V8 的一些通用初始化逻辑
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  // 创建 Isolate 时传入的参数
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
  // 创建一个 Isolate，V8 的对象
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    // 创建一个 HandleScope，用于下面分配 Handle
    HandleScope handle_scope(isolate);
    // 创建一个对象模版，用于创建全局对象
    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
    // 创建一个上下文
    Local<Context> context = Context::New(isolate, nullptr, global);
    // 创建一个 Environment 保存运行时的一些公共数据
    Environment * env = new Environment(context);
    // 保存命令行参数
    env->setArgv(argv);
    env->setArgc(argc);
    Context::Scope context_scope(context);
    //  创建一个自定义的对象，我们可以在这个对象里挂载一些 C++ 层实现的功能
    Local<Object> Deer = Object::New(isolate);
    // 注册 C++ 模块，把 C++ 层实现的功能挂载到 Deer 中
    register_builtins(isolate, Deer);
    // 获取 JS 全局对象
    Local<Object> globalInstance = context->Global();
    // 设置全局变量 Deer，这样在 JS 里就可以直接访问 Deer 变量了
    globalInstance->Set(context, String::NewFromUtf8Literal(isolate, "Deer", NewStringType::kNormal), Deer);
    // 设置全局属性 global 指向全局对象
    globalInstance->Set(context, String::NewFromUtf8Literal(isolate, "global", NewStringType::kNormal), globalInstance).Check();
    {
      // 初始化事件循环
      Deer::Loop::init_event_system(env->get_loop());
      // 打开文件
      int fd = open("Deer.js", 0, O_RDONLY);
      struct stat info;
      // 取得文件信息
      fstat(fd, &info);
      // 分配内存保存文件内容
      char *ptr = (char *)malloc(info.st_size + 1);
      read(fd, (void *)ptr, info.st_size);
      ptr[info.st_size] = '\0';
      // 要执行的 JS 代码
      Local<String> source = String::NewFromUtf8(isolate, ptr, NewStringType::kNormal, info.st_size).ToLocalChecked();
      // 编译
      Local<Script> script = Script::Compile(context, source).ToLocalChecked();
      // 解析完应该没用了，释放内存
      free(ptr);
      // 执行 JS
      Local<Value> result = script->Run(context).ToLocalChecked();
      // 进入事件循环
      Deer::Loop::run_event_system(env->get_loop());
    }
  }

  isolate->Dispose();
  v8::V8::Dispose();
  delete create_params.array_buffer_allocator;
  return 0;
}