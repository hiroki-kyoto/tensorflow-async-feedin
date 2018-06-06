# reader.py
from ctypes import *
import numpy as np
import tensorflow as tf
import time

'''
fetch a package of wave data
data in form of NHWC = [1, packages, points, channels]
'''
def reader_setup(
	offset, 
	points, 
	packages, 
	channels, 
	ratio, 
	reg0, 
	reg1, 
	reg2, 
	reg3, 
	reg4
):
	reader = cdll.LoadLibrary(b'./libreader.so')
	reader.r_alloc.restype = c_uint64
	buf = reader.r_alloc(packages*points*channels)
	reader.session.restype = c_uint64
	reader_sess = reader.session(
		b'/dev/fpga-ad', 
		c_int32(offset),
		c_int32(points), 
		c_int32(packages), 
		c_int32(channels), 
		c_float(ratio), 
		c_int(reg0), 
		c_int(reg1),
		c_int(reg2),
		c_int(reg3),
		c_int(reg4)
	)
	reader.session_dump(c_uint64(reader_sess))
	reader.start.restype = c_uint64
	reader_thread = reader.start(c_uint64(reader_sess))
	if reader_thread == 0 :
    		print("Error: failed to create thread of reader!")
    		return
	return (reader_sess, reader_thread)

def reader_shutdown(reader_sess, reader_thread):	
	reader = cdll.LoadLibrary(b'./libreader.so')	
	reader.stop(c_uint64(reader_sess), c_uint64(reader_thread))

class ModelRunner:
    def __init__(self):
	self.packages = 32
	self.points = 500
	self.channels = 3
	self.offset = 0
	self.ratio = 0.8
	self.reg = [1,1,500,20000,32]
	self.reader_sess, self.reader_thread = reader_setup(
		self.offset, 
		self.points, 
		self.packages, 
		self.channels, 
		self.ratio, 
		self.reg[0], 
		self.reg[1], 
		self.reg[2], 
		self.reg[3], 
		self.reg[4]
	)
        self.graph = tf.Graph()
        with self.graph.as_default():
            with tf.variable_scope('foo', reuse=tf.AUTO_REUSE):
		''' replace your model here '''
                self.w = tf.get_variable('w', initializer=[0.5])
                self.x = tf.placeholder(dtype=tf.float32, shape=[self.packages, self.points, self.channels])
                #self.y = self.x * self.w
		#self.y = self.x + 1
		self.y = tf.identity(self.x)

    def save_model(self, model_path):
        with self.graph.as_default():
            saver = tf.train.Saver()
            self.sess = tf.Session()
            self.sess.run(tf.global_variables_initializer())
            saver.save(self.sess, model_path);
            self.sess.close()

    def restore_model(self, model_path):
        with self.graph.as_default():
            saver = tf.train.Saver()
            self.sess = tf.Session()
            saver.restore(self.sess, model_path)

    def run(self):
        x = np.zeros([self.packages,self.points,self.channels],dtype=np.float32)
        if not x.flags['C_CONTIGUOUS']:
            x = np.ascontiguous(x, dtype=x.dtype)
        x_ptr = cast(x.ctypes.data, POINTER(c_float))
        reader = cdll.LoadLibrary('./libreader.so')
        t_beg = time.time()
	RUN_TIME = 1000
	counter = 0
        while True:
		reader.batch(c_uint64(self.reader_sess), x_ptr)
		y = self.sess.run(self.y, feed_dict={self.x:x})
		#print(y)
		if counter > RUN_TIME:
			break
		else:
			counter += 1
        t_end = time.time()
        print('speed: ' + str((t_end-t_beg)/RUN_TIME*1000) + '(ms/frame)')

    def close(self):
        self.sess.close()
	reader_shutdown(self.reader_sess, self.reader_thread)

def main():
    runner = ModelRunner()
    #runner.save_model('./model/')
    runner.restore_model('./model/')
    runner.run()
    runner.close()

main()
