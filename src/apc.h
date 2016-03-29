#pragma once

#include <windows.h>
#include <v8.h>
#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <safeint.h>

namespace {
	using v8::Eternal;
	using v8::Function;
	using v8::FunctionCallback;
	using v8::FunctionCallbackInfo;
	using v8::FunctionTemplate;
	using v8::Handle;
	using v8::HandleScope;
	using v8::Isolate;
	using v8::Local;
	using v8::Number;
	using v8::Object;
	using v8::String;
	using v8::Value;
	using std::array;
	using std::wstring;
	using msl::utilities::SafeInt;

	Eternal<String> es_APC;
	Eternal<String> es_addrOfAPC;
	Eternal<String> es_threadId;

	template<class T>
	void setMethod(Handle<Object>& classObj, wstring&& funcName, const T&& callback) {
		Isolate* isolate = Isolate::GetCurrent(); HandleScope scope(isolate);
		auto funcTemplate = FunctionTemplate::New(isolate, static_cast<FunctionCallback>(callback));
		auto fn_name = String::NewFromTwoByte(isolate, reinterpret_cast<const uint16_t *>(funcName.data()), String::kInternalizedString, funcName.size());
		funcTemplate->SetClassName(fn_name);
		auto func = funcTemplate->GetFunction();
		func->SetName(fn_name);
		classObj->Set(fn_name, func);
	}

	extern const array<uint8_t, 32> bounceAsm;
	extern const HANDLE heapExec;

	template<typename T>
	class Bouncer {
	private:
		typedef VOID(NTAPI *PBOUNCEER)(T *that, ULONG_PTR dwParam);
	public:
		const HANDLE apcThreadHandle{ nullptr };
		uint8_t *heapExecAllocation{ nullptr };
		const PAPCFUNC nativeFunc{ nullptr };
		Bouncer(DWORD apcThreadID, intptr_t nonlocalFuncCB) :
			apcThreadHandle{ OpenThread(THREAD_SET_CONTEXT, false, apcThreadID) },
			heapExecAllocation{ nullptr },
			nativeFunc{ reinterpret_cast<PAPCFUNC>(nonlocalFuncCB) } {

		}
		Bouncer(T *that, PBOUNCEER localFuncCB) :
			apcThreadHandle{ GetCurrentThread() },
			heapExecAllocation{ reinterpret_cast<uint8_t *>(HeapAlloc(heapExec, 0, bounceAsm.size())) },
			nativeFunc{ reinterpret_cast<PAPCFUNC>(heapExecAllocation) } {
			__movsq(reinterpret_cast<PDWORD64>(heapExecAllocation), reinterpret_cast<const DWORD64 *>(bounceAsm.data()), 4);
			*reinterpret_cast<T **>(heapExecAllocation + 5) = that;
			*reinterpret_cast<PBOUNCEER *>(heapExecAllocation + 15) = localFuncCB;
		}
		~Bouncer() {
			if (heapExecAllocation) {
				HeapFree(heapExec, 0, reinterpret_cast<const LPVOID>(heapExecAllocation));
			};
			CloseHandle(apcThreadHandle);
		}
		DWORD queue(ULONG_PTR dwData = 0) {
			return QueueUserAPC(nativeFunc, apcThreadHandle, dwData);
		}
	};

	template<typename T = Function>
	class EternalV2 {
	private:
		Isolate *isolate{ nullptr };
		Eternal<T> funcCB;
	public:
		EternalV2() { }

		EternalV2(Handle<T> localFuncCB) :isolate{ Isolate::GetCurrent() }, funcCB{ isolate , localFuncCB } { }

		Local<Value> Call(Local<Value> recv, int argc, Local<Value> argv[]) {
			return funcCB.Get(isolate)->Call(recv, argc, argv);
		}

	};

	class class_APC: public node::ObjectWrap {
	private:
		// /*const */Eternal<Function> funcCB; /* Get should be marked const maybe? */
		EternalV2<Function> funcCB;
	public:
		Bouncer<class_APC> bouncer;

		static void Init(Handle<Object> target) {
			Isolate* isolate = target->GetIsolate();
			HandleScope scope(isolate);

			es_APC.Set(isolate, String::NewFromUtf8(isolate, "APC", String::kInternalizedString));
			es_addrOfAPC.Set(isolate, String::NewFromUtf8(isolate, "addrOfAPC", String::kInternalizedString));
			es_threadId.Set(isolate, String::NewFromUtf8(isolate, "threadId", String::kInternalizedString));


			Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
			tpl->SetClassName(es_APC.Get(isolate));
			tpl->InstanceTemplate()->SetInternalFieldCount(1);

			NODE_SET_PROTOTYPE_METHOD(tpl, "queue", queue);

			target->Set(es_APC.Get(isolate), tpl->GetFunction());
		}

		class_APC(DWORD apcThreadID, intptr_t nonlocalFuncCB) : bouncer{ apcThreadID, nonlocalFuncCB } {
		}

		class_APC(Handle<Function> localFuncCB): funcCB{ localFuncCB }, bouncer{ this, staticNativeCallback } {
		}

		static void staticNativeCallback(class_APC *that, ULONG_PTR dwParam) {
			that->nativeCallback(dwParam);
		}

		void nativeCallback(ULONG_PTR dwParam) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope scope(isolate);

			Local<Number> num_dwParam;
			array<Local<Value>, 1> argv{ Number::New(isolate, static_cast<double_t>(static_cast<intptr_t>(dwParam))) };
			funcCB.Call(Null(isolate), SafeInt<int>(argv.size()), argv.data());
		}

		static void queue(const FunctionCallbackInfo<Value>& info) {
			info.GetReturnValue().Set(static_cast<uint32_t>(node::ObjectWrap::Unwrap<class_APC>(info.This())->bouncer.queue((info.Length() > 0) ? static_cast<ULONG_PTR>(info[0]->NumberValue()) : 0)));
		}

		static void New(const FunctionCallbackInfo<Value>& info) {
			if (!info.IsConstructCall()) {
				return;
			}
			Isolate* isolate = info.GetIsolate();
			if (info.Length() == 1 && info[0]->IsFunction()) {
				auto obj = new class_APC(Handle<Function>::Cast(info[0]));
				obj->Wrap(info.This());
				info.This()->Set(es_threadId.Get(isolate), Number::New(isolate, static_cast<double_t>(GetCurrentThreadId())));
				info.This()->Set(es_addrOfAPC.Get(isolate), Number::New(isolate, static_cast<double_t>(reinterpret_cast<intptr_t>(obj->bouncer.nativeFunc))));
				info.GetReturnValue().Set(info.This());
			}
			else if (info.Length() == 2 && info[0]->IsNumber() && info[1]->IsNumber()) {
				auto obj = new class_APC(static_cast<DWORD>(info[0]->NumberValue()), static_cast<intptr_t>(info[1]->NumberValue()));
				obj->Wrap(info.This());
				info.This()->Set(es_threadId.Get(isolate), info[0]);
				info.This()->Set(es_addrOfAPC.Get(isolate), info[1]);
				info.GetReturnValue().Set(info.This());
			}
		}
	};
};