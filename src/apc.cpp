#include "apc.h"

#define makeMethod(dst, name, body) { \
	setMethod(dst, L#name, [](const v8::FunctionCallbackInfo<v8::Value>& info) { \
		body; \
	}); \
}

#define makeMethodRet(dst, name, body) { \
	setMethod(dst, L#name, [](const v8::FunctionCallbackInfo<v8::Value>& info) { \
		info.GetReturnValue().Set(body); \
	}); \
}

namespace {
	const HANDLE heapExec{HeapCreate(
		HEAP_CREATE_ENABLE_EXECUTE | HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE,
		0,
		0)};

	const array<uint8_t, 32> bounceAsm{
		//mov rcx, 1DEADBEEFh
		0x48, 0x8B, 0xD1,

		//mov rcx, 1DEADBEEFh
		0x48, 0xB9, 0xEF, 0xBE, 0xAD, 0xDE, 0x01, 0x00, 0x00, 0x00,

		//mov rax, 2DEADBEEFh
		0x48, 0xB8, 0xEF, 0xBE, 0xAD, 0xDE, 0x02, 0x00, 0x00, 0x00,

		//jmp rax
		0xFF, 0xE0
	};

	void init(v8::Handle<v8::Object> target) {
		v8::Isolate* isolate = v8::Isolate::GetCurrent(); v8::HandleScope scope(isolate);
		class_APC::Init(target);

		makeMethodRet(target, DequeueAPCs, static_cast<uint32_t>(SleepEx(0, TRUE)));

		makeMethod(target, SleepEx, {
			switch (info.Length()) {
			case 0:
				info.GetReturnValue().Set(static_cast<uint32_t>(SleepEx(0, false)));
				return;
			case 1:
				info.GetReturnValue().Set(static_cast<uint32_t>(SleepEx(info[0]->Uint32Value(), false)));
				return;
			case 2:
			default:
				info.GetReturnValue().Set(static_cast<uint32_t>(SleepEx(info[0]->Uint32Value(), info[1]->BooleanValue())));
				return;
			}
		});

	}

	NODE_MODULE(apc, init);
};