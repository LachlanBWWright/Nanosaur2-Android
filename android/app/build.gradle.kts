plugins {
    id("com.android.application")
}

android {
    namespace = "io.jor.nanosaur2"
    compileSdk = 34
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "io.jor.nanosaur2"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "2.1.1"

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_shared",
                    "-DBUILD_SDL_FROM_SOURCE=ON",
                    "-DSDL_STATIC=OFF",
                    "-DANDROID=TRUE"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDirs("../../Data")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}
