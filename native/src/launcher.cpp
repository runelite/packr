/*******************************************************************************
 * Copyright 2011 See AUTHORS file.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#ifdef WINDOWS
#include <windows.h>
#endif

#include <launcher.h>
#include <stdio.h>
#include <jni.h>
#include <string>
#include <iostream>
#include <fstream>
#include <picojson.h>
#include <dlfcn.h>

#ifdef MACOSX
#include <unistd.h>
#endif

extern std::string getExecutableDir();
extern int g_argc;
extern char** g_argv;

typedef jint (JNICALL *PtrCreateJavaVM)(JavaVM **, void **, void *);

void* launchVM(void* params) {
    std::string execDir = getExecutableDir();
    std::ifstream configFile;
    configFile.open((execDir + std::string("/config.json")).c_str());
    printf("config file: %s\n", (execDir + std::string("/config.json")).c_str());
    
    picojson::value json;
    configFile >> json;
    std::string err = picojson::get_last_error();
    if(!err.empty()) {
        printf("Couldn't parse json: %s\n", err.c_str());
    }
    
    std::string jarFile = execDir + std::string("/") + json.get<picojson::object>()["jar"].to_str();
    std::string main = json.get<picojson::object>()["mainClass"].to_str();
    std::string classPath = std::string("-Djava.class.path=") + jarFile;
    picojson::array vmArgs = json.get<picojson::object>()["vmArgs"].get<picojson::array>();
    printf("jar: %s\n", jarFile.c_str());
    printf("mainClass: %s\n", main.c_str());
    
    JavaVMOption* options = (JavaVMOption*)malloc(sizeof(JavaVMOption) * (1 + vmArgs.size()));
    options[0].optionString = (char*)classPath.c_str();
    for(unsigned i = 0; i < vmArgs.size(); i++) {
        options[i+1].optionString = (char*)vmArgs[i].to_str().c_str();
        printf("vmArg %d: %s\n", i, options[i+1].optionString);
    }
    
    JavaVMInitArgs args;
    args.version = JNI_VERSION_1_6;
    args.nOptions = 1 + vmArgs.size();
    args.options = options;
    args.ignoreUnrecognized = JNI_FALSE;
    
    JavaVM* jvm = 0;
    JNIEnv* env = 0;

    
#ifndef WINDOWS
    #ifdef MACOSX
        std::string jre = execDir + std::string("/jre/lib/server/libjvm.dylib");
    #else
        std::string jre = execDir + std::string("/jre/lib/amd64/server/libjvm.so");
    #endif
    
    void* handle = dlopen(jre.c_str(), RTLD_LAZY);
    PtrCreateJavaVM ptrCreateJavaVM = (PtrCreateJavaVM)dlsym(handle, "JNI_CreateJavaVM");
#else
	HINSTANCE hinstLib = LoadLibrary(TEXT("jre\\bin\\server\\jvm.dll"));
	PtrCreateJavaVM ptrCreateJavaVM = (PtrCreateJavaVM)GetProcAddress(hinstLib,"JNI_CreateJavaVM");
#endif
    
#ifdef MACOSX
    chdir(execDir.c_str());
#endif

    jint res = ptrCreateJavaVM(&jvm, (void**)&env, &args);

    jobjectArray appArgs = env->NewObjectArray(g_argc, env->FindClass("java/lang/String"), NULL);
    for(int i = 0; i < g_argc; i++) {
        jstring arg = env->NewStringUTF(g_argv[i]);
        env->SetObjectArrayElement(appArgs, i, arg);
    }
    
    jclass mainClass = env->FindClass(main.c_str());
    jmethodID mainMethod = env->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
    env->CallStaticVoidMethod(mainClass, mainMethod, appArgs);
    jvm->DestroyJavaVM();
    return 0;
}
