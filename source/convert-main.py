import os
import numpy
import tensorflow
from PositionalEncoding import PositionalEncodingLayer
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
keras_model = tensorflow.keras.models.load_model(os.path.join(os.path.dirname(os.path.abspath(__file__)), "neural-texture-mapping.keras"), custom_objects={'PositionalEncodingLayer': PositionalEncodingLayer})

# Inference
prediction_RGBs = numpy.clip(keras_model.predict(input_UVs), 0.0, 1.0).reshape(texture_height, texture_width, 3)

# Log
matplotlib.pyplot.figure(figsize=(texture_width, texture_height), dpi=1)
matplotlib.pyplot.clf()
matplotlib.pyplot.imshow(prediction_RGBs)
matplotlib.pyplot.axis("off")
matplotlib.pyplot.subplots_adjust(left=0, right=1, top=1, bottom=0)
matplotlib.pyplot.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "prediction.png"), bbox_inches='tight', pad_inches=0)
matplotlib.pyplot.close()

# Convert
tflite_model = tensorflow.lite.TFLiteConverter.from_keras_model(keras_model).convert()

# Serialization
file_tflite_model_binary = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "neural-texture-mapping.tflite"), 'wb')
file_tflite_model_binary.write(tflite_model)
file_tflite_model_binary.close()

file_tflite_model_text = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "neural-texture-mapping.inl"), 'w')
file_tflite_model_text.write(', '.join([f"0X{ubyte:02X}" for ubyte in tflite_model]))
file_tflite_model_text.close()