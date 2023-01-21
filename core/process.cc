#include "process.h"

void Deer::Process::Init(Isolate* isolate, Local<Object> target) {
  Local<Context> context = isolate->GetCurrentContext();
  Environment * env = Environment::GetEnvByContext(context);
  Local<ObjectTemplate> process = ObjectTemplate::New(isolate);
  char ** argv = env->getArgv();
  int argc = env->getArgc();
  Local<Array> arr = Array::New(isolate, argc);
  for (int i = 0; i < argc; i++) {
      arr->Set(context, Number::New(isolate, i), newStringToLcal(isolate, argv[i]));
  }
  Local<Object> obj = process->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
  setObjectValue(isolate, obj, "argv", arr);
  setObjectValue(isolate, target, "process", obj);
}
