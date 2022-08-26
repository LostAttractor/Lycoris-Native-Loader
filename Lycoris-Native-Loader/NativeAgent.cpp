#include "pch.h"
#include "NativeAgent.h"

jvmtiEnv* jvmti;
JavaVM* jvm;
JNIEnv* env;
int code = -1;

NativeAgent::NativeAgent() {
	HMODULE jvmDll = GetModuleHandleA("jvm.dll");
	if (!jvmDll)
	{
		DWORD lastError = GetLastError();
		//MessageBoxA(NULL, "Error: 0x00000001", "Lycoris Loader", MB_OK | MB_ICONERROR);
		code = 1;
		return;
	}

	FARPROC getJvmsVoidPtr = GetProcAddress(jvmDll, "JNI_GetCreatedJavaVMs");

	if (!getJvmsVoidPtr)
	{
		DWORD lastError = GetLastError();
		//MessageBoxA(NULL, "Error: 0x00000002", "Lycoris Loader", MB_OK | MB_ICONERROR);
		code = 2;
		return;
	}

	typedef jint(JNICALL* GetCreatedJavaVMs)(JavaVM**, jsize, jsize*);

	GetCreatedJavaVMs jni_GetCreatedJavaVMs = (GetCreatedJavaVMs)getJvmsVoidPtr;

	jsize count;
	if (jni_GetCreatedJavaVMs((JavaVM**)&jvm, 1, &count) != JNI_OK || count == 0) { //获取JVM
		//MessageBoxA(nullptr, "Error: 0x00000003", "LycorisAgent", MB_OK | MB_ICONERROR); //无法获取JVM
		code = 3;
		return;
	}

	jint res = jvm->GetEnv((void**)&env, JNI_VERSION_1_6); //获取JNI
	if (res == JNI_EDETACHED) {
		res = jvm->AttachCurrentThread((void**)&env, nullptr);
	}

	if (res != JNI_OK) {
		//MessageBoxA(nullptr, "Error: 0x00000004", "LycorisAgent", MB_OK | MB_ICONERROR); //无法获取JNI
		code = 4;
		return;
	}

	res = jvm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_0);
	if (res != JNI_OK) {
		//MessageBoxA(nullptr, "Error: 0x00000005", "LycorisAgent", MB_OK | MB_ICONERROR); //无法获取JVMTI ENV
		code = 5;
		return;
	}
	//Setting up JVMTI Env
	jrawMonitorID vmtrace_lock;
	jlong start_time;
	jvmti->CreateRawMonitor("vmtrace_lock", &vmtrace_lock);
	jvmti->GetTime(&start_time);

	if (env->ExceptionOccurred()) {
		env->ExceptionDescribe();
		code = 6;
		return;
	}

	jvmtiCapabilities capabilities = { 0 };
	jvmti->GetPotentialCapabilities(&capabilities);
	capabilities.can_generate_all_class_hook_events = 1;
	capabilities.can_retransform_any_class = 1;
	capabilities.can_retransform_classes = 1;
	capabilities.can_redefine_any_class = 1;
	capabilities.can_redefine_classes = 1;
	int error = jvmti->AddCapabilities(&capabilities);
	if (env->ExceptionOccurred()) {
		env->ExceptionDescribe();
		//MessageBoxA(nullptr, "Error: 0x00000006", "LycorisAgent", MB_OK | MB_ICONERROR); //获取JVMTI错误
		//ExitThread(0);
		code = 7;
		return;
	}
	//cout << "[Lycoris Agent] JVMTI was set!" << endl;
	code = 0;
}