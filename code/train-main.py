import os
import numpy
import tensorflow
from PIL import Image
import matplotlib.pyplot
from PositionalEncoding import PositionalEncodingLayer

NUM_FREQUENCIES = 16
NUM_NEURONS_PER_TEXTURE_MAPPING_LAYER = 64
NUM_TEXTURE_MAPPING_LAYERS = 5

# Learning Rate
alpha = 0.001

iteration_count = 5000

# Cost Change Threshold
gamma = 1e-4

# Data
pil_image = Image.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../assets/target.png")).convert("RGB")
texture_width, texture_height = pil_image.size
target_RGBs = numpy.asarray(pil_image).astype(numpy.float32).reshape(-1, 3)
target_RGBs /= 255.0
del pil_image

assert target_RGBs.shape == (texture_width * texture_height, 3)

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
assert PositionalEncodingLayer(num_frequencies=NUM_FREQUENCIES)(tensorflow.random.uniform((7, 2))).shape == (7, NUM_FREQUENCIES * 4)

keras_model = tensorflow.keras.models.Sequential()
keras_model.add(tensorflow.keras.layers.InputLayer(shape=(2,)))
keras_model.add(PositionalEncodingLayer(num_frequencies=NUM_FREQUENCIES))
for i in range(NUM_TEXTURE_MAPPING_LAYERS):
    keras_model.add(tensorflow.keras.layers.Dense(NUM_NEURONS_PER_TEXTURE_MAPPING_LAYER, activation="relu"))
keras_model.add(tensorflow.keras.layers.Dense(3, activation="linear"))

keras_model.compile(optimizer=tensorflow.keras.optimizers.Adam(learning_rate=alpha), loss=tensorflow.keras.losses.MeanSquaredError())

# Gradient Descent Loop
keras_history = keras_model.fit(input_UVs, target_RGBs, epochs=iteration_count, callbacks=[ tensorflow.keras.callbacks.EarlyStopping(monitor='loss', min_delta=gamma, patience=7, restore_best_weights=True)])

# Log
cost_history = keras_history.history['loss']
matplotlib.pyplot.clf()
matplotlib.pyplot.plot(cost_history)
matplotlib.pyplot.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "cost-history.png"))

# Serialization
keras_model.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "neural-texture-mapping.keras"), overwrite=True)
