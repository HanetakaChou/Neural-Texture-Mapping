# Build TensorFlow Lite  

We follow the [Build TensorFlow Lite Python Wheel Package](https://www.tensorflow.org/lite/guide/build_cmake_pip).  

We modify the **path/to/tensorflow/lite/tools/pip_package/build_pip_package_with_cmake.sh**.  

We add the option **-DTFLITE_ENABLE_GPU=ON -DTFLITE_ENABLE_NNAPI=OFF -DTFLITE_ENABLE_XNNPACK=OFF**.  

We modify the **path/to/tensorflow/lite/lite/python/interpreter_wrapper/interpreter_wrapper.cc**.  

We replace the **TfLiteXNNPackDelegate\*** by **TfLiteGpuDelegateV2\***.  

```bash
export CC=clang
export CXX=clang++
export PYTHON=python3
export BUILD_NUM_JOBS=$(nproc)
/path/to/tensorflow/lite/tools/pip_package/build_pip_package_with_cmake.sh native
```  

```bash
cmake --build . -j -t _pywrap_tensorflow_interpreter_wrapper
```

## Build C API on Linux  

Add **TfLiteGpuDelegateOptionsV2Default** **TfLiteGpuDelegateV2Create** **TfLiteGpuDelegateV2Delete** to **/path/to/tensorflow/lite/c/version_script.lds**.

Add the following **unused_reference_symbols** to **/path/to/tensorflow/lite/core/c/c_api.cc**.

```c++
__attribute__((used)) int volatile* reference_symbols() {
  int volatile* p = (int*)&TfLiteGpuDelegateOptionsV2Default +
                    (intptr_t)&TfLiteGpuDelegateV2Create +
                    (intptr_t)&TfLiteGpuDelegateV2Delete;
  return p;
}
```

```bash
export CC=clang
export CXX=clang++
cmake -DTFLITE_ENABLE_GPU=ON -DTFLITE_ENABLE_NNAPI=OFF -DTFLITE_ENABLE_XNNPACK=OFF -DTFLITE_C_BUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=release /path/to/tensorflow/lite/c
```

## Build C API on Windows  

We should remove the following from **/path/to/tensorflow/lite/CMakeLists.txt**  
```cmake
# if (NOT BUILD_SHARED_LIBS)
#   list(APPEND TFLITE_TARGET_PUBLIC_OPTIONS "-DTFL_STATIC_LIBRARY_BUILD")
# endif()
```

"-DCMAKE_CXX_STANDARD=20" and "-DCMAKE_CXX_STANDARD_REQUIRED=YES" does NOT work on Windows. We still need to manually change to "/std:c++20". 

We add the prefix "__declspec(dllexport)" to **TfLiteGpuDelegateOptionsV2Default** **TfLiteGpuDelegateV2Create** **TfLiteGpuDelegateV2Delete** manually.  

And we need the following DEF file to export these three symbols.  
```DEF
LIBRARY
EXPORTS
    TfLiteGpuDelegateOptionsV2Default
    TfLiteGpuDelegateV2Create
    TfLiteGpuDelegateV2Delete
```
