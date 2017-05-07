/*
 * Copyright (c) 2017 Ari Aosved
 * http://github.com/devaos/node-icmp-ping/blob/master/LICENSE
 */

#include <stdlib.h>
#include <node.h>

/* ========================================================================== */

namespace ping {

extern "C" {
  #include "ping.h"
}

using namespace v8;

/* ========================================================================== */

struct PersistPingContext {
  Persistent<Function> on_startup;
  Persistent<Function> on_receipt;
  Persistent<Function> on_complete;
};

/* ========================================================================== */

void cleanup(PersistPingContext* persist) {
  persist->on_startup.Reset();
  persist->on_receipt.Reset();
  persist->on_complete.Reset();
  delete persist;
}

/* ========================================================================== */

void on_startup(ping_state_t *context) {
  PersistPingContext* persist = (PersistPingContext*)context->options->data;
  Isolate * isolate = Isolate::GetCurrent();

  Locker locker(isolate);
  HandleScope scope(isolate);

  if (persist->on_startup.IsEmpty()) {
    return;
  }

  Local<Object> obj = Object::New(isolate);
  obj->Set(String::NewFromUtf8(isolate, "target"), String::NewFromUtf8(isolate, context->target));
  obj->Set(String::NewFromUtf8(isolate, "ip"), String::NewFromUtf8(isolate, context->toip));
  obj->Set(String::NewFromUtf8(isolate, "payloadBytes"), Number::New(isolate, context->osize - 8));
  obj->Set(String::NewFromUtf8(isolate, "totalBytes"), Number::New(isolate, context->osize));

  Handle<Value> argv[1] = { obj };
  Local<Function> cb = Local<Function>::New(isolate, persist->on_startup);
  cb->Call(isolate->GetCurrentContext()->Global(), 1, argv);
}

/* ========================================================================== */

void on_receipt(ping_state_t *context, float triptime, struct timeval sent, struct timeval received, u_short seq) {
  PersistPingContext* persist = (PersistPingContext*)context->options->data;
  Isolate * isolate = Isolate::GetCurrent();

  Locker locker(isolate);
  HandleScope scope(isolate);

  if (persist->on_receipt.IsEmpty()) {
    return;
  }

  Local<Object> obj = Object::New(isolate);
  obj->Set(String::NewFromUtf8(isolate, "target"), String::NewFromUtf8(isolate, context->target));
  obj->Set(String::NewFromUtf8(isolate, "ip"), String::NewFromUtf8(isolate, context->toip));
  obj->Set(String::NewFromUtf8(isolate, "payloadBytes"), Number::New(isolate, context->osize - 8));
  obj->Set(String::NewFromUtf8(isolate, "totalBytes"), Number::New(isolate, context->osize));

  obj->Set(String::NewFromUtf8(isolate, "receivedBytes"), Number::New(isolate, context->isize));
  obj->Set(String::NewFromUtf8(isolate, "received"), Number::New(isolate, context->received));
  obj->Set(String::NewFromUtf8(isolate, "sequence"), Number::New(isolate, seq));
  obj->Set(String::NewFromUtf8(isolate, "triptime"), Number::New(isolate, triptime));

  Handle<Value> argv[1] = { obj };
  Local<Function> cb = Local<Function>::New(isolate, persist->on_receipt);
  cb->Call(isolate->GetCurrentContext()->Global(), 1, argv);
}

/* ========================================================================== */

void on_complete(ping_state_t *context, int runtime) {
  PersistPingContext* persist;
  Isolate * isolate = Isolate::GetCurrent();

  Locker locker(isolate);
  HandleScope scope(isolate);

  if (!context) {
    return;
  }

  persist = (PersistPingContext*)context->options->data;

  if (persist->on_complete.IsEmpty()) {
    cleanup(persist);
    return;
  }

  Local<Function> cb = Local<Function>::New(isolate, persist->on_complete);

  if (context->err) {
    Handle<Value> argv[2] = {
      Exception::Error(String::NewFromUtf8(isolate, context->errmsg)),
      Undefined(isolate)
    };
    cb->Call(isolate->GetCurrentContext()->Global(), 2, argv);

    cleanup(persist);
    return;
  }

  Local<Object> obj = Object::New(isolate);
  obj->Set(String::NewFromUtf8(isolate, "target"), String::NewFromUtf8(isolate, context->target));
  obj->Set(String::NewFromUtf8(isolate, "ip"), String::NewFromUtf8(isolate, context->toip));
  obj->Set(String::NewFromUtf8(isolate, "payloadBytes"), Number::New(isolate, context->osize - 8));
  obj->Set(String::NewFromUtf8(isolate, "totalBytes"), Number::New(isolate, context->osize));

  obj->Set(String::NewFromUtf8(isolate, "transmitted"), Number::New(isolate, context->transmitted));
  obj->Set(String::NewFromUtf8(isolate, "received"), Number::New(isolate, context->received));
  obj->Set(String::NewFromUtf8(isolate, "tmin"), Number::New(isolate, context->tmin));
  obj->Set(String::NewFromUtf8(isolate, "tmax"), Number::New(isolate, context->tmax));
  obj->Set(String::NewFromUtf8(isolate, "tsum"), Number::New(isolate, context->tsum));
  obj->Set(String::NewFromUtf8(isolate, "runtime"), Number::New(isolate, runtime));

  Handle<Value> argv[2] = { Null(isolate), obj };
  cb->Call(isolate->GetCurrentContext()->Global(), 2, argv);

  cleanup(persist);
}

/* ========================================================================== */

void RunProbe(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PersistPingContext* persist = new PersistPingContext();
  Local<Function> cb;

  if (args.Length() >= 4 && args[4]->IsFunction()) {
    persist->on_startup.Reset(isolate, Local<Function>::Cast(args[4]));
  } else {
    persist->on_startup.Reset();
  }

  if (args.Length() >= 5 && args[5]->IsFunction()) {
    persist->on_receipt.Reset(isolate, Local<Function>::Cast(args[5]));
  } else {
    persist->on_receipt.Reset();
  }

  if (args.Length() >= 6 && args[6]->IsFunction()) {
    cb = Local<Function>::Cast(args[6]);
    persist->on_complete.Reset(isolate, cb);
  } else {
    persist->on_complete.Reset();
  }

  if (args.Length() < 1 || !args[0]->IsString()) {
    if (persist->on_complete.IsEmpty()) {
      cleanup(persist);
      return;
    }

    Local<Value> argv[2] = {
      Exception::Error(String::NewFromUtf8(isolate, "Invalid arguments to ping")),
      Undefined(isolate)
    };
    cb->Call(Null(isolate), 2, argv);

    cleanup(persist);
    return;
  }

  v8::String::Utf8Value target(args[0]->ToString());
  ping_options_t options = {
    .probes = args[1]->Int32Value(),
    .timeout = args[2]->Int32Value(),
    .interval = args[3]->Int32Value(),
    .cb_startup = on_startup,
    .cb_receipt = on_receipt,
    .cb_complete = on_complete,
    .data = (char *)persist
  };

  ping_state_t* context = ping(*target, &options);

  if (!context) {
    if (persist->on_complete.IsEmpty()) {
      cleanup(persist);
      return;
    }

    Local<Value> argv[2] = {
      Exception::Error(String::NewFromUtf8(isolate, "Ping failed to allocate memory")),
      Undefined(isolate)
    };
    cb->Call(Null(isolate), 2, argv);

    cleanup(persist);
    return;
  }
}

/* ========================================================================== */

void Init(Local<Object> exports, Local<Object> module) {
  NODE_SET_METHOD(exports, "ping", RunProbe);
}

NODE_MODULE(icmp_ping, Init)
}