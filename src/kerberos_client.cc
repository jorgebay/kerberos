#include "kerberos_client.h"

Nan::Persistent<v8::Function> KerberosClient::constructor;
NAN_MODULE_INIT(KerberosClient::Init) {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>();
  tpl->SetClassName(Nan::New("KerberosClient").ToLocalChecked());
  Nan::SetPrototypeMethod(tpl, "step", Step);
  Nan::SetPrototypeMethod(tpl, "wrap", WrapData);
  Nan::SetPrototypeMethod(tpl, "unwrap", UnwrapData);

  v8::Local<v8::ObjectTemplate> itpl = tpl->InstanceTemplate();
  itpl->SetInternalFieldCount(1);

  Nan::SetAccessor(itpl, Nan::New("username").ToLocalChecked(), KerberosClient::UserNameGetter);
  Nan::SetAccessor(itpl, Nan::New("response").ToLocalChecked(), KerberosClient::ResponseGetter);
  Nan::SetAccessor(itpl, Nan::New("responseConf").ToLocalChecked(), KerberosClient::ResponseConfGetter);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("KerberosClient").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

v8::Local<v8::Object> KerberosClient::NewInstance(gss_client_state* state) {
  Nan::EscapableHandleScope scope;
  v8::Local<v8::Function> ctor = Nan::New<v8::Function>(KerberosClient::constructor);
  v8::Local<v8::Object> object = Nan::NewInstance(ctor).ToLocalChecked();
  KerberosClient *class_instance = new KerberosClient(state);
  class_instance->Wrap(object);
  return scope.Escape(object);
}

KerberosClient::KerberosClient(gss_client_state* state)
  : _state(state)
{}

KerberosClient::~KerberosClient() {
  if (_state != NULL) {
    authenticate_gss_client_clean(_state);
    _state = NULL;
  }
}

gss_client_state* KerberosClient::state() const {
  return _state;
}

NAN_GETTER(KerberosClient::UserNameGetter) {
  KerberosClient* client = Nan::ObjectWrap::Unwrap<KerberosClient>(info.Holder());
  (client->_state->username == NULL) ?
    info.GetReturnValue().Set(Nan::Null()) :
    info.GetReturnValue().Set(Nan::New(client->_state->username).ToLocalChecked());
}

NAN_GETTER(KerberosClient::ResponseGetter) {
  KerberosClient* client = Nan::ObjectWrap::Unwrap<KerberosClient>(info.Holder());
  (client->_state->response == NULL) ?
    info.GetReturnValue().Set(Nan::Null()) :
    info.GetReturnValue().Set(Nan::New(client->_state->response).ToLocalChecked());
}

NAN_GETTER(KerberosClient::ResponseConfGetter) {
  KerberosClient* client = Nan::ObjectWrap::Unwrap<KerberosClient>(info.Holder());
  info.GetReturnValue().Set(Nan::New(client->_state->responseConf));
}

class ClientStepWorker : public Nan::AsyncWorker {
 public:
  ClientStepWorker(KerberosClient* client, std::string challenge, Nan::Callback *callback)
    : AsyncWorker(callback, "kerberos:ClientStepWorker"),
      _client(client),
      _challenge(challenge)
  {}

  virtual void Execute() {
    std::unique_ptr<gss_result, FreeDeleter> result(
      authenticate_gss_client_step(_client->state(), _challenge.c_str(), NULL));
    if (result->code == AUTH_GSS_ERROR) {
      SetErrorMessage(result->message);
      return;
    }
  }

 private:
  KerberosClient* _client;
  std::string _challenge;
};

NAN_METHOD(KerberosClient::Step) {
  KerberosClient* client = Nan::ObjectWrap::Unwrap<KerberosClient>(info[0]->ToObject());
  std::string challenge(*Nan::Utf8String(info[1]));
  Nan::Callback *callback = new Nan::Callback(Nan::To<v8::Function>(info[2]).ToLocalChecked());

  AsyncQueueWorker(new ClientStepWorker(client, challenge, callback));
}

class ClientUnwrapWorker : public Nan::AsyncWorker {
 public:
  ClientUnwrapWorker(KerberosClient* client, std::string challenge, Nan::Callback *callback)
    : AsyncWorker(callback, "kerberos:ClientUnwrapWorker"),
      _client(client),
      _challenge(challenge)
  {}

  virtual void Execute() {
    std::unique_ptr<gss_result, FreeDeleter> result(
      authenticate_gss_client_unwrap(_client->state(), _challenge.c_str()));
    if (result->code == AUTH_GSS_ERROR) {
      SetErrorMessage(result->message);
      return;
    }
  }

 private:
  KerberosClient* _client;
  std::string _challenge;
};

NAN_METHOD(KerberosClient::UnwrapData) {
  KerberosClient* client = Nan::ObjectWrap::Unwrap<KerberosClient>(info[0]->ToObject());
  std::string challenge(*Nan::Utf8String(info[1]));
  Nan::Callback *callback = new Nan::Callback(Nan::To<v8::Function>(info[2]).ToLocalChecked());

  AsyncQueueWorker(new ClientUnwrapWorker(client, challenge, callback));
}

class ClientWrapWorker : public Nan::AsyncWorker {
 public:
  ClientWrapWorker(KerberosClient* client, std::string challenge, std::string user, int protect, Nan::Callback *callback)
    : AsyncWorker(callback, "kerberos:ClientWrapWorker"),
      _client(client),
      _challenge(challenge),
      _user(user),
      _protect(protect)
  {}

  virtual void Execute() {
    std::unique_ptr<gss_result, FreeDeleter> result(
      authenticate_gss_client_wrap(_client->state(), _challenge.c_str(), _user.c_str(), _protect));
    if (result->code == AUTH_GSS_ERROR) {
      SetErrorMessage(result->message);
      return;
    }
  }

 private:
  KerberosClient* _client;
  std::string _challenge;
  std::string _user;
  int _protect;
};

NAN_METHOD(KerberosClient::WrapData) {
  KerberosClient* client = Nan::ObjectWrap::Unwrap<KerberosClient>(info[0]->ToObject());
  std::string challenge(*Nan::Utf8String(info[1]));
  v8::Local<v8::Object> options = Nan::To<v8::Object>(info[1]).ToLocalChecked();
  Nan::Callback *callback = new Nan::Callback(Nan::To<v8::Function>(info[2]).ToLocalChecked());

  std::string user = StringOptionValue(options, "user");
  AsyncQueueWorker(new ClientWrapWorker(client, challenge, user, 0, callback));
}
