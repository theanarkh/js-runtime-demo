#include "socket.h"      

namespace Deer {
  static void handle_read(struct io_watcher* watcher) {
    Socket::TCPHandle* handle = container_of(watcher, Socket::TCPHandle, watcher);
    Socket * socket = static_cast<Socket *>(handle->data);
    Environment* env = socket->env();
    Isolate* isolate = env->GetIsolate();
    Local<Context> context = env->GetContext();
    v8::HandleScope handle_scope(isolate);
    Context::Scope context_scope(context);
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(isolate, 1024);
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(bs));
    std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
    int bytes = read(socket->handle.watcher.fd, backing->Data(), backing->ByteLength());
    v8::Local<v8::Value> argv[] = {
      Number::New(isolate, bytes),
      Uint8Array::New(ab, 0, bytes)
    };
    socket->makeCallback("onread", 2, argv);
  }

  static void handle_write(struct io_watcher* watcher) {
    Socket::TCPHandle* handle = container_of(watcher, Socket::TCPHandle, watcher);
    Socket * socket = static_cast<Socket *>(handle->data);
    Environment* env = socket->env();
    Isolate* isolate = env->GetIsolate();
    Local<Context> context = env->GetContext();
    v8::HandleScope handle_scope(isolate);
    Context::Scope context_scope(context);
    int size = socket->handle.write_queue.size();
    for (int i = 0; i < size; i++) {
      Socket::WriteRequest* write_request = socket->handle.write_queue.front();
      int bytes;
      do {
        bytes = write(socket->handle.watcher.fd, write_request->buf.data + write_request->buf.offset, write_request->buf.len);
      } while(bytes == -1 && errno == EINTR);
      // TODO
      if (bytes == 0) {
        break;
      } else if (bytes > 0) {
        write_request->buf.offset += bytes;
        if (write_request->buf.offset == write_request->buf.len) {
          // v8::Local<v8::Value> argv[] = {
          //   Number::New(isolate, bytes),
          //   Uint8Array::New(ab, 0, bytes)
          // };
          write_request->makeCallback("oncomplete", 0, nullptr);
          socket->handle.write_queue.pop_front();
          delete[] write_request->buf.data;
        } else {
          break;
        }
      } else if (bytes == EAGAIN) {
        break;
      } else {
        printf("write error: %d\n", errno);
      }
    }
    if (socket->handle.write_queue.size() == 0) {
      socket->handle.watcher.pevent &= ~POLL_OUT;
    }
  }

  static void handle_remove(struct io_watcher* watcher) {
    printf("remove io watcher\n");
    Socket::TCPHandle* handle = container_of(watcher, Socket::TCPHandle, watcher);
    Socket * socket = static_cast<Socket *>(handle->data);
    Environment* env = socket->env();
    struct event_loop* loop = env->get_loop();
    std::list<struct io_watcher*>::iterator it;
    for(it = loop->io_watchers.begin(); it != loop->io_watchers.end(); it++) {
      if(*it == watcher) {
        loop->io_watchers.erase(it);
        break;
      }
    }
  };

  static void io_watch_handler(struct io_watcher* watcher, int events) {
    if (events == 0) {
      handle_remove(watcher);
    } else {
      if (events & POLL_IN) {
        handle_read(watcher);
      }
      if(events & POLL_OUT) {
        Socket::TCPHandle* handle = container_of(watcher, Socket::TCPHandle, watcher);
        Socket * socket = (Socket*)handle->data;
        handle_write(watcher);
      }
    }
  }

  Socket::Socket(Deer::Env::Environment* env, Local<Object> object): Deer::Async(env, object) {
      handle.data = this;
      handle.watcher.handler = io_watch_handler;
  }

  static struct sockaddr_in HandleAddrInfo(V8_ARGS) {
      V8_ISOLATE
      String::Utf8Value ip(isolate, args[0]);
      int port = args[1].As<Integer>()->Value(); 
      struct sockaddr_in serv_addr;
      memset(&serv_addr, 0, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_addr.s_addr = inet_addr(*ip);
      serv_addr.sin_port = htons(port);
      return serv_addr;
  }

  DEFIND_FUNC(Socket::Bind) {   
      V8_ISOLATE
      Socket * socket_ = Deer::BaseObject::unwrap<Socket>(args.Holder());
      int fd = socket(AF_INET, SOCK_STREAM, 0);
      ioctl(fd, FIONBIO, 1);
      socket_->handle.watcher.fd = fd;
      struct sockaddr_in server_addr_info = HandleAddrInfo(args);
      int ret = bind(fd, (struct sockaddr*)&server_addr_info, sizeof(server_addr_info));
      V8_RETURN(Integer::New(isolate, ret));
  }

  DEFIND_FUNC(Socket::Listen) {
      V8_ISOLATE
      V8_CONTEXT
      Socket * socket = Deer::BaseObject::unwrap<Socket>(args.Holder());
      int fd = socket->handle.watcher.fd; 
      int backlog = args[0].As<Integer>()->Value(); 
      int ret = listen(fd, backlog);
      if (ret == -1) {
          V8_RETURN(Integer::New(isolate, ret));
          return;
      }
      int on = 1;
      ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
      if (ret == -1) {
          V8_RETURN(Integer::New(isolate, ret));
          return;
      }
      socket->handle.watcher.pevent = POLL_IN;
      Local<String> event = newStringToLcal(isolate, "onconnection");
      socket->object()->Set(context, event, args[1].As<Function>());
      socket->handle.watcher.handler = [](struct io_watcher* watcher, int events) {
        Socket::TCPHandle* handle = container_of(watcher, Socket::TCPHandle, watcher);
        Socket * server_socket = static_cast<Socket *>(handle->data);
        Environment* env = server_socket->env();
        Isolate* isolate = env->GetIsolate();
        Local<Context> context = env->GetContext();
        v8::HandleScope handle_scope(isolate);
        Context::Scope context_scope(context);
        Local<v8::FunctionTemplate> function_template = server_socket->env()->get_tcp_template();
        Local<Function> constructor = function_template->GetFunction(context).ToLocalChecked();
        Local<Object> client = constructor->NewInstance(context).ToLocalChecked();
        Socket * client_socket = Deer::BaseObject::unwrap<Socket>(client);
        int client_fd = accept(server_socket->handle.watcher.fd, nullptr, nullptr);
        client_socket->handle.watcher.fd = client_fd; 
        v8::Local<v8::Value> client_obj = client;
        v8::Local<v8::Value> argv[] = {
          client_obj
        };
        server_socket->makeCallback("onconnection", 1, argv);
      };
      Deer::Env::Environment* env = Deer::Env::Environment::GetEnvByContext(isolate->GetCurrentContext());
      struct event_loop* loop = env->get_loop();
      loop->io_watchers.push_back(&socket->handle.watcher);
      V8_RETURN(Integer::New(isolate, 0));
  }

  DEFIND_FUNC(Socket::Read) {
    V8_ISOLATE
    V8_CONTEXT
    Socket * socket = Deer::BaseObject::unwrap<Socket>(args.Holder());
    Local<String> event = newStringToLcal(isolate, "onread");
    socket->object()->Set(context, event, args[0].As<Function>());
    socket->handle.watcher.pevent |= POLL_IN;
    if (socket->handle.watcher.pevent == socket->handle.watcher.event) {
      return;
    }
    Deer::Env::Environment* env = Deer::Env::Environment::GetEnvByContext(isolate->GetCurrentContext());
    struct event_loop* loop = env->get_loop();
    std::list<io_watcher*>::iterator it = std::find(loop->io_watchers.begin(),loop->io_watchers.end(), &socket->handle.watcher);
    if(it == loop->io_watchers.end()) {
      loop->io_watchers.push_back(&socket->handle.watcher);
    }
  }

  Socket::WriteRequest::WriteRequest(Deer::Env::Environment* env, Local<Object> object, buffer& _buf): Deer::Async(env, object), buf(_buf) {

  }

  DEFIND_FUNC(Socket::Write) { 
    V8_ISOLATE
    V8_CONTEXT
    Local<Uint8Array> uint8Array = args[0].As<Uint8Array>();
    Local<ArrayBuffer> arrayBuffer = uint8Array->Buffer();
    std::shared_ptr<BackingStore> backing = arrayBuffer->GetBackingStore();
    Local<Object> write_obj = args[1].As<Object>();
    Environment *env = Environment::GetEnvByContext(context);
    Socket * socket = Deer::BaseObject::unwrap<Socket>(args.Holder());
    int fd = socket->handle.watcher.fd;
    int len = backing->ByteLength();
    char* data = new char[len];
    memcpy(static_cast<char*>(data), backing->Data(), len);
    buffer buf = {
      data,
      0,
      len,
    };
    Socket::WriteRequest* write_request = new Socket::WriteRequest(env, write_obj, buf);
    socket->handle.write_queue.push_back(write_request);
    socket->handle.watcher.pevent |= POLL_OUT;
    if (socket->handle.watcher.pevent == socket->handle.watcher.event) {
      return;
    }
    struct event_loop* loop = env->get_loop();
    std::list<io_watcher*>::iterator it = std::find(loop->io_watchers.begin(),loop->io_watchers.end(), &socket->handle.watcher);

    if(it == loop->io_watchers.end()) {
      loop->io_watchers.push_back(&socket->handle.watcher);
    }
  }

  void Socket::New(V8_ARGS) {
    V8_ISOLATE
    Deer::Env::Environment* env = Deer::Env::Environment::GetEnvByContext(isolate->GetCurrentContext());
    new Socket(env, args.This());
  }
  
  void Socket::Init(Isolate* isolate, Local<Object> target) {
    Local<Object> socket = Object::New(isolate);
    Local<Context> context = isolate->GetCurrentContext();
    Environment* env = Environment::GetEnvByContext(context);
    Local<FunctionTemplate> t = Deer::Util::NewFunctionTemplate(isolate, New);
    env->set_tcp_template(t);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    SetProtoMethod(isolate, t, "bind", Bind);
    SetProtoMethod(isolate, t, "write", Write);
    SetProtoMethod(isolate, t, "read", Read);
    SetProtoMethod(isolate, t, "listen", Listen);
    SetConstructorFunction(context, socket, "Socket", t);

    Local<FunctionTemplate> write_request = NewFunctionTemplate(isolate);
    write_request->InstanceTemplate()->SetInternalFieldCount(1);
    SetConstructorFunction(context, socket, "WriteRequest", write_request);

    setObjectValue(isolate, target, "socket", socket);
  }
}