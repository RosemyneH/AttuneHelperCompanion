plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.attunehelper.companion"
    compileSdk = 34
    val canSignRelease: Boolean = run {
        val path = System.getenv("ANDROID_KEYSTORE_FILE")?.trim()
        if (path.isNullOrEmpty()) {
            return@run false
        }
        if (!file(path).isFile) {
            return@run false
        }
        if (System.getenv("ANDROID_KEYSTORE_PASSWORD").isNullOrEmpty()) {
            return@run false
        }
        if (System.getenv("ANDROID_KEY_ALIAS").isNullOrEmpty()) {
            return@run false
        }
        if (System.getenv("ANDROID_KEY_PASSWORD").isNullOrEmpty()) {
            return@run false
        }
        true
    }
    signingConfigs {
        if (canSignRelease) {
            create("release") {
                storeFile = file(System.getenv("ANDROID_KEYSTORE_FILE")!!.trim())
                storePassword = System.getenv("ANDROID_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")
                keyPassword = System.getenv("ANDROID_KEY_PASSWORD")
            }
        }
    }
    buildFeatures {
        buildConfig = true
    }
    defaultConfig {
        applicationId = "com.attunehelper.companion"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }
    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildTypes {
        getByName("debug") { }
        getByName("release") {
            isMinifyEnabled = false
            if (canSignRelease) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
    sourceSets {
        getByName("main") {
            assets {
                srcDir(
                    file("${rootProject.projectDir}/../manifest").also {
                        require(it.isDirectory) { "Expected manifest/ at ${it.path}" }
                    }
                )
            }
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.0")
    implementation("androidx.activity:activity-ktx:1.8.2")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("com.google.zxing:core:3.5.3")
    implementation("com.journeyapps:zxing-android-embedded:4.3.0")
    implementation("androidx.documentfile:documentfile:1.0.1")
    testImplementation("junit:junit:4.13.2")
}
