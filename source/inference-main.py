import os
import numpy
import tensorflow.lite
import matplotlib.pyplot

# Data
texture_width = 512
texture_height = 512

input_Us = numpy.linspace(0.0, 1.0, texture_width, endpoint=False, dtype=numpy.float32) + 0.5 / texture_width
input_Vs = numpy.linspace(0.0, 1.0, texture_height, endpoint=False, dtype=numpy.float32) + 0.5 / texture_height
input_UVs_u, input_UVs_v = numpy.meshgrid(input_Us, input_Vs)
input_UVs = numpy.stack([input_UVs_u, input_UVs_v], axis=-1).reshape(-1, 2)
del input_Us
del input_Vs
del input_UVs_u
del input_UVs_v

assert input_UVs.shape == (texture_width * texture_height, 2)

# Model
tflite_interpreter = tensorflow.lite.Interpreter(model_path=os.path.join(os.path.dirname(os.path.abspath(__file__)), "neural-texture-mapping.tflite"))

tflite_input_details = tflite_interpreter.get_input_details()
tflite_output_details = tflite_interpreter.get_output_details()

tflite_input_index = tflite_input_details[0]["index"]
tflite_ouput_index = tflite_output_details[0]["index"]

tflite_interpreter.resize_tensor_input(tflite_input_index, [texture_width * texture_height, 2])

tflite_interpreter.allocate_tensors()

# Inference
tflite_interpreter.set_tensor(tflite_input_index, input_UVs)
tflite_interpreter.invoke()   
prediction_RGBs = numpy.clip(tflite_interpreter.get_tensor(tflite_ouput_index), 0.0, 1.0).reshape(texture_height, texture_width, 3)

# Log
matplotlib.pyplot.figure(figsize=(texture_width, texture_height), dpi=1)
matplotlib.pyplot.clf()
matplotlib.pyplot.imshow(prediction_RGBs)
matplotlib.pyplot.axis("off")
matplotlib.pyplot.subplots_adjust(left=0, right=1, top=1, bottom=0)
matplotlib.pyplot.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "prediction.png"), bbox_inches='tight', pad_inches=0)
matplotlib.pyplot.close()