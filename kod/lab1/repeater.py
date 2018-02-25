from kivy.app import App
from kivy.properties import NumericProperty, ReferenceListProperty
from kivy.uix.widget import Widget
from kivy.uix.label import Label
from kivy.uix.button import Button


class RepeaterInterface(Widget):
    pass


class SwitchButton(Button):
    def __init__(self, *args, **kwargs):
        self.pin_state = False
        self.on_state_changed = None
        super().__init__(*args, **kwargs)

    def on_press(self):
        self.pin_state = True
        if self.on_state_changed is not None:
            self.on_state_changed(self.pin_state)

    def on_release(self):
        self.pin_state = False
        if self.on_state_changed is not None:
            self.on_state_changed(self.pin_state)


class LEDLight(Label):
    COLOR_OFF = (0.1, 0.1, 0.1)
    COLOR_ON = (1, 1, 0)
    color_r = NumericProperty(0)
    color_g = NumericProperty(0)
    color_b = NumericProperty(0)
    color = ReferenceListProperty(color_r, color_g, color_b)

    def __init__(self, *args, **kwargs):
        self.pin_state = False
        super().__init__(*args, **kwargs)

    @property
    def pin_state(self):
        return self._pin_state

    @pin_state.setter
    def pin_state(self, new):
        self._pin_state = new
        self.color = self.COLOR_ON if new else self.COLOR_OFF


class Repeater(App):

    def __init__(self):
        super().__init__()
        self.devices = {}
        self.handlers = {}

    def update(self, id):
        def f(pin_state):
            self.devices[id].pin_state = pin_state
            self.trigger_event(id)
        return f

    def register_handler(self, id, handler):
        self.handlers[id] = handler

    def unregister_handler(self, id):
        self.handlers[id] = None

    def trigger_event(self, id):
        if id not in self.handlers:
            return
        self.handlers[id](id, self.get(id))

    def get(self, id):
        return self.devices[id].pin_state

    def set(self, id, pin_state):
        self.devices[id].pin_state = pin_state

    def build(self):
        self.interface = RepeaterInterface()

        self.devices = self.interface.ids
        for id, device in self.interface.ids.items():
            if hasattr(device, 'on_state_changed'):
                device.on_state_changed = self.update(id)

        return self.interface


if __name__ == '__main__':
    r = Repeater()
    r.register_handler('sw2', lambda y, x: r.set('ds1', x))
    r.register_handler('sw3', lambda y, x: r.set('ds4', x))
    r.register_handler('sw4', lambda y, x: r.set('ds7', x))
    r.register_handler('sw6', lambda y, x: r.set('ds3', x))
    r.register_handler('sw7', lambda y, x: r.set('ds6', x))
    r.register_handler('sw8', lambda y, x: r.set('ds9', x))
    r.run()
