import avango.script
from avango.script import field_has_changed

import avango.gua

from . import device
import time


class DefaultViews(avango.script.Script):

    OutTransform = avango.gua.SFMatrix4()
    Keyboard = device.KeyboardDevice()

    rotation_axis = avango.gua.Vec3(0.0, 0.0, 0.0)
    cam_offset    = avango.gua.Vec3(0.0, 0.0, 0.0)

    logging_enabled = False

    def __init__(self):
        self.super(DefaultViews).__init__()
        self.always_evaluate(True)

 
    def evaluate(self):
        if self.Keyboard.KeyW.value:
            self.rotation_axis = avango.gua.Vec3(1.0, 0.0, 0.0)
            self.cam_offset    = avango.gua.Vec3(0.0, 0.0, -20.0)

        if self.Keyboard.KeyS.value:
            self.rotation_axis = avango.gua.Vec3(0.0, 0.0, 0.0)
            self.cam_offset    = avango.gua.Vec3(0.0, 0.0, 0.0)
        if self.Keyboard.KeyA.value:
            self.rotation_axis = avango.gua.Vec3(0.0, 1.0, 0.0)
            self.cam_offset    = avango.gua.Vec3(0.0, 0.0, 0.0)
        if self.Keyboard.KeyD.value:
            self.rotation_axis = avango.gua.Vec3(0.0, -1.0, 0.0)
            self.cam_offset    = avango.gua.Vec3(0.0, 0.0, 0.0)
        if self.Keyboard.KeyL.value:
            self.logging_enabled = True
        if self.Keyboard.KeyO.value:
            self.logging_enabled = False

        self.OutTransform.value = avango.gua.make_trans_mat(self.cam_offset[0], self.cam_offset[1], self.cam_offset[2]) * avango.gua.make_rot_mat(90.0, self.rotation_axis[0], self.rotation_axis[1], self.rotation_axis[2])
