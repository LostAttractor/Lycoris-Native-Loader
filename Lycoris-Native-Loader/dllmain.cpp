// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "main.h"
#include "NativeAgent.h"

using namespace std;

/* utilities */

static jmethodID getMethod = NULL, invokeMethod = NULL;
static jclass nativeAccesses = NULL, reflections = NULL;
static string* names = NULL;
static NativeAgent nativeAgent;

jclass clazztigger;

DWORD WINAPI MainThread(CONST LPVOID lpParam)
{
	nativeAgent = NativeAgent();
	ExitThread(0);
}


jbyteArray asByteArray(JNIEnv* env, const unsigned char* buf, int len) {
	jbyteArray array = env->NewByteArray(len);
	env->SetByteArrayRegion(array, 0, len, (const jbyte*)buf);
	return array;
}

unsigned char* asUnsignedCharArray(JNIEnv* env, jbyteArray array) {
	int len = env->GetArrayLength(array);
	unsigned char* buf = new unsigned char[len];
	env->GetByteArrayRegion(array, 0, len, reinterpret_cast<jbyte*>(buf));
	return buf;
}

static int classesToRedefine;
static int redefine(jvmtiEnv* jvmti, jvmtiClassDefinition* class_def) {
	if (!jvmti->RedefineClasses(classesToRedefine, class_def))
		return 0;

	return 1;
}

jobjectArray asClassArray(JNIEnv* env, jclass* buf, int len) {
	jobjectArray array = env->NewObjectArray(len, env->FindClass("java/lang/Class"), NULL);

	for (int i = 0; i < len; i++) {
		env->SetObjectArrayElement(array, i, buf[i]);
	}

	return array;
}

jclass findClass(JNIEnv* env, jvmtiEnv* jvmti, const char* name) {
	jclass* loadedClasses;
	jint loadedClassesCount = 0;
	jvmti->GetLoadedClasses(&loadedClassesCount, &loadedClasses);

	jclass findClass = NULL;
	for (jint i = 0; i < loadedClassesCount; i++)
	{
		char* signature;
		jvmti->GetClassSignature(loadedClasses[i], &signature, NULL);
		if (!strcmp(signature, name))
		{
			findClass = loadedClasses[i];
			return findClass;
		}
	}
	return NULL;
}

void JNICALL classTransformerHook
(
	jvmtiEnv* jvmti,
	JNIEnv* env,
	jclass class_being_redefined,
	jobject loader, const char* name,
	jobject protection_domain,
	jint data_len,
	const unsigned char* data,
	jint* new_data_len,
	unsigned char** new_data
) {
	if (nativeAgent.error != 0) {
		MessageBoxA(NULL, "Error: 0x00000010", "LycorisAgent", MB_OK | MB_ICONERROR); //获取JVMTI错误
		ExitThread(0);
	}
	jvmti->Allocate(data_len, new_data);
	jclass transformerClass = findClass(env, jvmti, "Lrbq/lycoris/client/transformer/TransformManager;");
	jmethodID transfrom = env->GetStaticMethodID(transformerClass, "onTransform", "(Ljava/lang/Class;[B)[B");
	jclass clazzt = class_being_redefined;
	jbyteArray classdata = asByteArray(env, data, data_len);
	jbyteArray transformedData = env->NewByteArray(0);
	clazztigger = clazzt;
	if (!class_being_redefined) {
		*new_data_len = data_len;
		memcpy(*new_data, data, data_len);
		return;
	}
	if (!clazzt || !classdata || !transformerClass || !transfrom) {
		//cout << "Unknown transform" << endl;
		*new_data_len = data_len;
		memcpy(*new_data, data, data_len);
		return;
	}
	transformedData = (jbyteArray)env->CallStaticObjectMethod(transformerClass, transfrom, clazzt, classdata);
	if (!transformedData) {
		//cout << "Unknown transform" << endl;
		*new_data_len = data_len;
		memcpy(*new_data, data, data_len);
		return;
	}

	unsigned char* newChars = asUnsignedCharArray(env, transformedData);
	const jint newLength = (jint)env->GetArrayLength(transformedData);
	jvmti->Allocate(newLength, new_data);
	*new_data_len = newLength;
	memcpy(*new_data, newChars, newLength);
}


extern "C" __declspec(dllexport) jobjectArray Java_rbq_lycoris_agent_instrument_impl_InstrumentationImpl_getAllLoadedClasses(JNIEnv * jnienv) {
	if (nativeAgent.error != 0) {
		MessageBoxA(NULL, "Error: 0x00000011", "LycorisAgent", MB_OK | MB_ICONERROR); //获取JVMTI错误
		ExitThread(0);
	}

	jclass* jvmClasses;
	jint classCount;

	jint err = (jint)nativeAgent.jvmti->GetLoadedClasses(&classCount, &jvmClasses);
	if (err) {
		return asClassArray(jnienv, jvmClasses, classCount);
	}

	return asClassArray(jnienv, jvmClasses, classCount);
}
extern "C" __declspec(dllexport) jobjectArray JNICALL Java_rbq_lycoris_agent_instrument_impl_InstrumentationImpl_getLoadedClasses(JNIEnv * env, jobject instrumentationInstance, jobject classLoader) {
	jclass* jvmClasses;
	jint classCount;
	const jint err = nativeAgent.jvmti->GetClassLoaderClasses(classLoader, &classCount, &jvmClasses);
	if (err) {
		return asClassArray(env, jvmClasses, classCount);
	}

	return asClassArray(env, jvmClasses, classCount);
}

extern "C" __declspec(dllexport) jint JNICALL Java_rbq_lycoris_agent_instrument_impl_InstrumentationImpl_retransformClasses(JNIEnv * env, jobject instrumentationInstance, jobjectArray classes) {
	if (nativeAgent.error != 0) {
		MessageBoxA(NULL, "Error: 0x00000010", "LycorisAgent", MB_OK | MB_ICONERROR); //获取JVMTI错误
		ExitThread(0);
	}
	const jclass stringCls = env->FindClass("java/lang/String");
	const jmethodID stringReplace = env->GetMethodID(stringCls, "replace", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Ljava/lang/String;");
	const jstring dotString = env->NewStringUTF(".");
	const jstring slashString = env->NewStringUTF("/");

	const jclass javaClass = env->FindClass("java/lang/Class");
	const jmethodID getName = env->GetMethodID(javaClass, "getName", "()Ljava/lang/String;");

	jint size = env->GetArrayLength(classes);
	jclass* jvmClasses = new jclass[size];
	names = new string[size];

	for (int index = 0; index < size; index++) {
		jvmClasses[index] = (jclass)env->GetObjectArrayElement(classes, index);
		names[index] = env->GetStringUTFChars((jstring)env->CallObjectMethod((jstring)env->CallObjectMethod(jvmClasses[index], getName), stringReplace, dotString, slashString), JNI_FALSE);
	}

	classesToRedefine = size;
	jvmtiEventCallbacks callbacks;
	callbacks.ClassFileLoadHook = classTransformerHook;
	nativeAgent.jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
	nativeAgent.jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);

	int error = nativeAgent.jvmti->RetransformClasses(size, jvmClasses);
	char* sig;
	nativeAgent.jvmti->GetClassSignature(clazztigger, &sig, NULL);
	if (error) {
		MessageBoxA(NULL, "Trasnform Error", "LycorisAgent", MB_OK | MB_ICONERROR); //Transform Error
	}
	return error;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	DisableThreadLibraryCalls(hModule);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		//HANDLE hdlWrite = GetStdHandle(STD_OUTPUT_HANDLE);
		//FILE* stream;
		//freopen_s(&stream ,"CONOUT$", "w+t", stdout);
		//freopen_s(&stream, "CONIN$", "r+t", stdin);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&MainThread, NULL, 0, NULL);
		break;
    }
    return TRUE;
}

