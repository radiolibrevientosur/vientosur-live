plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.radiosuite.pro"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.radiosuite.pro"
        minSdk = 26          // Oboe AAudio requiere API 26+
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        externalNativeBuild {
            cmake {
                cppFlags("-std=c++17 -fno-exceptions -fno-rtti -ffast-math")
                arguments("-DANDROID_STL=c++_static")
                // ABIs objetivo: arm64 (modernos), armeabi-v7a (legacy), x86_64 (emulador)
                abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.14"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
}

dependencies {
    // Compose
    implementation(platform("androidx.compose:compose-bom:2024.09.02"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.5")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.5")

    // Oboe (audio baja latencia)
    implementation("com.google.oboe:oboe:1.9.0")

    // ExoPlayer (decodificación MP3/OGG/Opus/WAV)
    implementation("androidx.media3:media3-exoplayer:1.4.1")
    implementation("androidx.media3:media3-common:1.4.1")

    // HTTP para Icecast/Shoutcast
    implementation("com.squareup.okhttp3:okhttp:4.12.0")

    // Coroutines
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")

    // Supabase (persistencia: playlists, perfiles de emisión, ajustes DSP)
    implementation("io.github.jan-tennert.supabase:postgrest-kt:2.5.4")
    implementation("io.ktor:ktor-client-okhttp:2.3.12")
}
