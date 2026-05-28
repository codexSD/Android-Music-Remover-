# Native methods are resolved by JNI signature, so the AudioEngine class and its
# native method declarations must survive R8/ProGuard renaming.
-keep class com.codexsd.vocalremover.audio.AudioEngine { *; }
