#include "pch.h"
#include "NativeAgent.h"

NativeAgent::NativeAgent() {
	error = -1;

	HMODULE jvmDll = GetModuleHandleA("jvm.dll");
	if (!jvmDll)
	{
		DWORD lastError = GetLastError();
		error = 1; //Can't find jvm.dll module handle
		return;
	}

	FARPROC getJvmsVoidPtr = GetProcAddress(jvmDll, "JNI_GetCreatedJavaVMs");

	if (!getJvmsVoidPtr) {
		DWORD lastError = GetLastError();
		error = 2;
		return;
	}

	typedef jint(JNICALL* GetCreatedJavaVMs)(JavaVM**, jsize, jsize*);
	GetCreatedJavaVMs jni_GetCreatedJavaVMs = (GetCreatedJavaVMs)getJvmsVoidPtr;

	jsize count;
	if (jni_GetCreatedJavaVMs((JavaVM**)&jvm, 1, &count) != JNI_OK || count == 0) { //��ȡJVM
		error = 3;
		return;
	}

	jint res = jvm->GetEnv((void**)&jnienv, JNI_VERSION_1_6); //��ȡJNI
	if (res == JNI_EDETACHED) {
		res = jvm->AttachCurrentThread((void**)&jnienv, nullptr);
	}

	if (res != JNI_OK) {
		error = 4; //�޷���ȡJNI
		return;
	}

	res = jvm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_0);
	if (res != JNI_OK) {
		error = 5; //�޷���ȡJVMTI ENV
		return;
	}
	//Setting up JVMTI Env
	jrawMonitorID vmtrace_lock;
	jlong start_time;
	jvmti->CreateRawMonitor("vmtrace_lock", &vmtrace_lock);
	jvmti->GetTime(&start_time);

	if (jnienv->ExceptionOccurred()) {
		jnienv->ExceptionDescribe();
		error = 6; //�޷���JVM����
		return;
	}

	// �޸�JVMTI Capabilities
	jvmtiCapabilities capabilities = { 0 };
	jvmti->GetPotentialCapabilities(&capabilities);
	capabilities.can_generate_all_class_hook_events = 1;
	capabilities.can_retransform_any_class = 1;
	capabilities.can_retransform_classes = 1;
	capabilities.can_redefine_any_class = 1;
	capabilities.can_redefine_classes = 1;
	if (jvmti->AddCapabilities(&capabilities) != 0) {
		error = 7; //�޸�JVMTI Capabilitiesʧ��
		return;
	}
	if (jnienv->ExceptionOccurred()) {
		jnienv->ExceptionDescribe();
		error = 8; //�޸�JVMTI Capabilities����JNIEnv����
		return;
	}
	error = 0;
}