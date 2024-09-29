import numpy
import tensorflow

# Positional Encoding
# [keras_nlp.layers.SinePositionEncoding](https://github.com/keras-team/keras-hub/blob/master/keras_hub/src/layers/modeling/sine_position_encoding.py)
class PositionalEncodingLayer(tensorflow.keras.layers.Layer):
    def __init__(self, num_frequencies, **kwargs):
        super(PositionalEncodingLayer, self).__init__(**kwargs)
        self.num_frequencies = num_frequencies

    def call(self, inputs):
        encoded = []
        for i in range(self.num_frequencies):
            encoded.append(tensorflow.math.sin((1 << i) * tensorflow.constant(numpy.pi) * inputs))
            encoded.append(tensorflow.math.cos((1 << i) * tensorflow.constant(numpy.pi) * inputs))
        return tensorflow.concat(encoded, axis=-1)

    def get_config(self):
        config = super(PositionalEncodingLayer, self).get_config()
        config.update({"num_frequencies": self.num_frequencies})
        return config
