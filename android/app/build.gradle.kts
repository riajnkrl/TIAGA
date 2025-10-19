plugins {
    id("com.android.application")
    id("kotlin-android")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

import java.util.Properties
import java.io.FileInputStream

android {
    namespace = "com.example.plantbot_care"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
        isCoreLibraryDesugaringEnabled = true
    }

    dependencies {
        coreLibraryDesugaring("com.android.tools:desugar_jdk_libs:2.1.4")
    }

    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_11.toString()
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "com.example.plantbot_care"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    // Load signing properties from android/key.properties if present
    val keystorePropertiesFile = rootProject.file("key.properties")
    val keystoreProperties = Properties()
    if (keystorePropertiesFile.exists()) {
        keystoreProperties.load(FileInputStream(keystorePropertiesFile))
    }

    signingConfigs {
        create("release") {
            // Read the storeFile property and normalize it. Some workflows write
            // "android/app/tiaga_upload_key.jks" into android/key.properties which
            // results in duplicated segments when passed to rootProject.file(...).
            // Strip a leading "android/" or any leading './' or '/' so the path is
            // resolved relative to the Android project root correctly.
            val storeFileProp = keystoreProperties.getProperty("storeFile", "app/tiaga_upload_key.jks")
            val normalizedStoreFile = storeFileProp
                .removePrefix("android/")
                .removePrefix("./")
                .removePrefix("/")

            // Resolve the keystore path relative to the directory containing key.properties
            // (usually the Android project root). This avoids duplicating the 'android/'
            // segment when rootProject.file(...) is used by the Gradle invocation.
            storeFile = File(keystorePropertiesFile.parentFile, normalizedStoreFile)
            storePassword = keystoreProperties.getProperty("storePassword", "")
            keyAlias = keystoreProperties.getProperty("keyAlias", "tiaga_key")
            keyPassword = keystoreProperties.getProperty("keyPassword", "")
        }
    }

    buildTypes {
        release {
            // Use the provided signing config if key.properties exists, otherwise fall back to debug signing
            signingConfig = if (keystorePropertiesFile.exists()) signingConfigs.getByName("release") else signingConfigs.getByName("debug")
            isMinifyEnabled = true
            // Use the module's proguard-rules.pro (android/app/proguard-rules.pro)
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
}

flutter {
    source = "../.."
}

dependencies {
    // Ensure the TensorFlow Lite GPU runtime is available to satisfy R8's references
    implementation("org.tensorflow:tensorflow-lite-gpu:2.11.0")
}
