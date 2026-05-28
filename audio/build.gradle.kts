plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
}

// ONNX Runtime ships as an AAR with no prefab package, so for native linking we
// extract its headers + per-ABI libonnxruntime.so into the build directory and
// hand the path to CMake via -DORT_HOME. The same AAR is also a normal
// `implementation` dependency, which is what actually packages the .so into the
// APK; the extracted copy is only used at compile/link time.
val ortAar: Configuration by configurations.creating
val ortHome = layout.buildDirectory.dir("ort")

val extractOnnxRuntime by tasks.registering(Copy::class) {
    from(ortAar.elements.map { artifacts -> artifacts.map { zipTree(it.asFile) } })
    into(ortHome)
    // Only the C/C++ API headers and the native libs are needed.
    include("headers/**", "jni/**")
}

android {
    namespace = "com.codexsd.vocalremover.audio"
    compileSdk = libs.versions.compileSdk.get().toInt()
    ndkVersion = "26.3.11579264"

    defaultConfig {
        minSdk = libs.versions.minSdk.get().toInt()

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")

        ndk {
            // 64-bit ARM is the only production target; the others exist for
            // emulator/legacy coverage during development.
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_shared"
                arguments += "-DORT_HOME=${ortHome.get().asFile.absolutePath}"
                cppFlags += "-std=c++17"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // Oboe ships as a prefab package inside its AAR.
    buildFeatures {
        prefab = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    testOptions {
        unitTests {
            isReturnDefaultValues = true
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.oboe)

    // Packages libonnxruntime.so into the APK; the spectrogram separator
    // (Phase 2) links against the headers/lib extracted from this same AAR.
    implementation(libs.onnxruntime.android)
    ortAar(libs.onnxruntime.android)

    testImplementation(libs.junit)
    testImplementation(libs.truth)

    androidTestImplementation(libs.androidx.test.ext.junit)
    androidTestImplementation(libs.truth)
}

// Make sure the headers/lib are extracted before any native build configures.
tasks.configureEach {
    if (name.startsWith("configureCMake") || name.startsWith("buildCMake")) {
        dependsOn(extractOnnxRuntime)
    }
}
