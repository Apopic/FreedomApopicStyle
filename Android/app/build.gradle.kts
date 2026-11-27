plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.example.freedomapopicstyle"
    compileSdk {
        version = release(37)
    }

    defaultConfig {
        applicationId = "com.example.freedomapopicstyle"
        minSdk = 28
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            signingConfig = signingConfigs.getByName("debug")
            optimization {
                enable = false
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    ndkVersion = "28.2.13676358"
    // youtubedl-android は Python・yt-dlp・ffmpeg を共有ライブラリとして同梱し、実行時に展開して使う
    packaging {
        jniLibs {
            useLegacyPackaging = true
            // libpython.zip.so などは中身が zip なので strip しない
            keepDebugSymbols += setOf("**/*.zip.so")
        }
    }
}

// 出力する APK の名前 (FreedomApopicStyle-debug.apk / FreedomApopicStyle-release.apk)
base {
    archivesName = "FreedomApopicStyle"
}

dependencies {
    // SONGLINK のダウンロード (yt-dlp の Android 版。GPL-3.0: https://github.com/yausername/youtubedl-android)
    implementation("io.github.junkfood02.youtubedl-android:library:0.18.1")
    implementation("io.github.junkfood02.youtubedl-android:ffmpeg:0.18.1")
    testImplementation(libs.junit)
    androidTestImplementation(libs.espresso.core)
    androidTestImplementation(libs.ext.junit)
}
