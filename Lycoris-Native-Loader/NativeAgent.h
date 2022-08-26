#pragma once
class NativeAgent
{
	public:
		/* functions */

		NativeAgent();

		/* variables */

		jvmtiEnv* jvmti;
		JavaVM* jvm;
		JNIEnv* env;
		int error;
};

