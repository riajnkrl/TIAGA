# ProGuard rules for TensorFlow Lite

# Keep all TensorFlow Lite classes (prevents R8 from removing native bridge classes)
-keep class org.tensorflow.lite.** { *; }
-keepclassmembers class org.tensorflow.lite.** { *; }

# Keep GPU delegate related classes
-keep class org.tensorflow.lite.gpu.** { *; }
-keepclassmembers class org.tensorflow.lite.gpu.** { *; }

# Keep any native loader / delegate factories
-keep class org.tensorflow.lite.*Delegate* { *; }

# Keep flatbuffer and schema classes often used by TFLite
-keep class com.google.flatbuffers.** { *; }

# Keep protobuf or other generated classes referenced by tflite
-keep class com.google.protobuf.** { *; }

# Keep Kotlin metadata and synthetic members
-keep class kotlin.Metadata { *; }

# Suppress warnings for generated GPU factory options (added from missing_rules.txt)
-dontwarn org.tensorflow.lite.gpu.GpuDelegateFactory$Options
