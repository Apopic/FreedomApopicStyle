#pragma once
#ifdef __ANDROID__
#include <jni.h>
#include <optional>
#include <string>

// 既に接続されていたゲームスレッドを切り離さない。ローカル参照は呼び出しごとに破棄する。
struct AndroidJava {
    const ANativeActivity* Activity = GetNativeActivity();
    JNIEnv* Env = nullptr;
    jclass Class = nullptr;
    bool Attached = false;
    AndroidJava() {
        if (!Activity) { return; }
        if (Activity->vm->GetEnv((void**)&Env, JNI_VERSION_1_6) == JNI_EDETACHED) {
            Attached = Activity->vm->AttachCurrentThread(&Env, nullptr) == JNI_OK;
            if (!Attached) { Env = nullptr; return; }
        }
        if (Env) { Class = Env->GetObjectClass(Activity->clazz); }
    }
    ~AndroidJava() {
        if (Env) { if (Env->ExceptionCheck()) { Env->ExceptionClear(); } if (Class) { Env->DeleteLocalRef(Class); } }
        if (Attached) { Activity->vm->DetachCurrentThread(); }
    }
    jmethodID Method(const char* name, const char* signature) {
        return Env && Class ? Env->GetMethodID(Class, name, signature) : nullptr;
    }
};

inline void AndroidConfigureExternalInput() {
    AndroidJava j;
    if (auto m = j.Method("ConfigureExternalInput", "()V")) { j.Env->CallVoidMethod(j.Activity->clazz, m); }
}
inline int AndroidPollExternalInput() {
    AndroidJava j;
    if (auto m = j.Method("PollExternalInput", "()I")) { return j.Env->CallIntMethod(j.Activity->clazz, m); }
    return 0;
}
inline bool AndroidCallTwoStrings(const char* method, const std::string& mime, const std::string& directory) {
    AndroidJava j;
    auto m = j.Method(method, "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!m) { return false; }
    jstring type = j.Env->NewStringUTF(mime.c_str()), dir = j.Env->NewStringUTF(directory.c_str());
    j.Env->CallVoidMethod(j.Activity->clazz, m, type, dir);
    j.Env->DeleteLocalRef(type); j.Env->DeleteLocalRef(dir);
    return !j.Env->ExceptionCheck();
}
inline bool AndroidStartEditorPicker(const std::string& mime, const std::string& directory) { return AndroidCallTwoStrings("StartEditorFilePicker", mime, directory); }
inline std::optional<std::string> AndroidPollString(const char* method) {
    AndroidJava j;
    auto m = j.Method(method, "()Ljava/lang/String;");
    if (!m) { return std::string(); }
    auto result = (jstring)j.Env->CallObjectMethod(j.Activity->clazz, m);
    if (j.Env->ExceptionCheck()) { return std::string(); }
    if (!result) { return std::nullopt; }
    const char* text = j.Env->GetStringUTFChars(result, nullptr);
    std::string value = text ? text : "";
    if (text) { j.Env->ReleaseStringUTFChars(result, text); }
    j.Env->DeleteLocalRef(result);
    return value;
}
inline std::optional<std::string> AndroidPollEditorPicker() { return AndroidPollString("PollEditorFilePicker"); }
#endif
