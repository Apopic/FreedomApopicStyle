#include "SystemCommon.hpp"

#ifndef __ANDROID__

const fs::path& GetExecutablePath() {
    // スコア・設定・譜面読み込みなどで頻繁に呼ばれるため、起動後 1 回だけ求める
    static const fs::path path = [] {
        std::vector<wchar_t> buffer(MAX_PATH);
        while (true) {
            DWORD len = GetModuleFileNameW(NULL, buffer.data(), (DWORD)buffer.size());
            if (len == 0) return fs::path();
            if (len < buffer.size()) {
                return fs::path(buffer.data()).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
    }();
    return path;
}

#else

const fs::path& GetExecutablePath() {
    static const fs::path dir = [] {
        fs::path d = "/storage/emulated/0/Documents/FAS";
        std::error_code ec;
        fs::create_directories(d, ec);
        return d;
    }();
    return dir;
}

void ShowToast(const std::string& text) {

    JNIEnv* env;
    const ANativeActivity* NativeActivity;
    NativeActivity = GetNativeActivity();

    if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
        return;
    }

    jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
    jmethodID jmethodID_ShowToast = env->GetMethodID(jclass_MainActivity, "ShowToast", "(Ljava/lang/String;)V");
    jstring jstr = env->NewStringUTF(text.c_str());
    env->CallVoidMethod(NativeActivity->clazz, jmethodID_ShowToast, jstr);
    env->DeleteLocalRef(jstr);
    env->DeleteLocalRef(jclass_MainActivity);
    NativeActivity->vm->DetachCurrentThread();
}

std::string GetInputText(std::string theme, std::string initial, bool* accepted) {
    if (accepted) { *accepted = false; }

    JNIEnv* env;
    const ANativeActivity* NativeActivity;
    int InputEnd;
    char InputString[1024] = {};

    NativeActivity = GetNativeActivity();

    {
        if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
            return "";
        }

        jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
        jmethodID jmethodID_StartInputDialog = env->GetMethodID(jclass_MainActivity, "StartInputStringDialog", "(Ljava/lang/String;Ljava/lang/String;)V");
        jstring jtheme = env->NewStringUTF(theme.c_str());
        jstring jinitial = env->NewStringUTF(initial.c_str());
        env->CallVoidMethod(NativeActivity->clazz, jmethodID_StartInputDialog, jtheme, jinitial);
        env->DeleteLocalRef(jinitial);
        env->DeleteLocalRef(jtheme);
        env->DeleteLocalRef(jclass_MainActivity);
        NativeActivity->vm->DetachCurrentThread();
    }

    InputEnd = 0;

    while (sys::PumpEvents() && InputEnd == 0) {

        if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
            return "";
        }

        jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
        jfieldID jfieldID_InputEnd = env->GetFieldID(jclass_MainActivity, "InputEnd", "I");
        InputEnd = env->GetIntField(NativeActivity->clazz, jfieldID_InputEnd);

        if (InputEnd == 1) {
            jfieldID jfieldID_InputString = env->GetFieldID(jclass_MainActivity, "InputString",
                "Ljava/lang/String;");
            jstring jstring_InputString = (jstring)env->GetObjectField(NativeActivity->clazz,
                jfieldID_InputString);
            const char* chars_InputString = jstring_InputString ? env->GetStringUTFChars(jstring_InputString, NULL) : nullptr;
            if(chars_InputString) strncpy(InputString, chars_InputString, sizeof(InputString) - 1);
            if(chars_InputString) env->ReleaseStringUTFChars(jstring_InputString, chars_InputString);
            env->DeleteLocalRef(jstring_InputString);
        }
        env->DeleteLocalRef(jclass_MainActivity);
        NativeActivity->vm->DetachCurrentThread();
    }
    if (accepted) { *accepted = InputEnd == 1; }
    return InputString;
}

// はい / いいえ のダイアログ (Java の StartYesNoDialog。はいなら InputString が "1")
bool AskYesNo(const std::string& title, const std::string& message) {
    JNIEnv* env;
    const ANativeActivity* NativeActivity = GetNativeActivity();
    if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) { return false; }
    {
        jclass cls = env->GetObjectClass(NativeActivity->clazz);
        jmethodID method = env->GetMethodID(cls, "StartYesNoDialog", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (method == nullptr) { env->ExceptionClear(); env->DeleteLocalRef(cls); NativeActivity->vm->DetachCurrentThread(); return false; }
        jstring jt = env->NewStringUTF(title.c_str()), jm = env->NewStringUTF(message.c_str());
        env->CallVoidMethod(NativeActivity->clazz, method, jt, jm);
        env->DeleteLocalRef(jt); env->DeleteLocalRef(jm); env->DeleteLocalRef(cls);
        NativeActivity->vm->DetachCurrentThread();
    }
    bool yes = false;
    int end = 0;
    while (sys::PumpEvents() && end == 0) {
        if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) { return false; }
        jclass cls = env->GetObjectClass(NativeActivity->clazz);
        end = env->GetIntField(NativeActivity->clazz, env->GetFieldID(cls, "InputEnd", "I"));
        if (end == 1) {
            jstring js = (jstring)env->GetObjectField(NativeActivity->clazz, env->GetFieldID(cls, "InputString", "Ljava/lang/String;"));
            if (js != nullptr) {
                const char* c = env->GetStringUTFChars(js, NULL);
                yes = c != nullptr && std::string(c) == "1";
                if(c) env->ReleaseStringUTFChars(js, c);
                env->DeleteLocalRef(js);
            }
        }
        env->DeleteLocalRef(cls);
        NativeActivity->vm->DetachCurrentThread();
    }
    return yes;
}
#endif

// エラーの内容をダイアログで出す (Android はトースト)
void ShowErrorDialog(const std::string& text) {
#ifndef __ANDROID__
    MessageBox(((HWND)sys::NativeWindow()), text.c_str(), "エラー", MB_ICONERROR);
#else
    ShowToast(text);
#endif
}
// 例外の内容を日本語の説明にする (読めない文字は置き換え)
std::string DescribeException(const std::exception& e) {
    if (dynamic_cast<const std::bad_alloc*>(&e)) { return "メモリが足りません"; }
    if (const auto* se = dynamic_cast<const std::system_error*>(&e)) { return std::string("システムのエラー: ") + se->what() + " (コード " + std::to_string(se->code().value()) + ")"; }
    if (dynamic_cast<const std::out_of_range*>(&e)) { return std::string("値が範囲外です (") + e.what() + ")"; }
    if (dynamic_cast<const std::invalid_argument*>(&e)) { return std::string("値が読めません (") + e.what() + ")"; }
    return e.what();
}
