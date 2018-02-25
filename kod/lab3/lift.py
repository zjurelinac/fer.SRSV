from kivy.app import App
from kivy.clock import Clock
from kivy.properties import ListProperty, DictProperty
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.button import Button
# from kivy.graphics import *


class PushButton(Button):
    def __init__(self, *args, **kwargs):
        self.item_state = False
        self.on_state_changed = None
        super().__init__(*args, **kwargs)

    def on_press(self):
        self.item_state = True
        if self.on_state_changed is not None:
            self.on_state_changed(self.item_state)

    def on_release(self):
        self.item_state = False
        if self.on_state_changed is not None:
            self.on_state_changed(self.item_state)


DIRECTION_NONE = -1
DIRECTION_UP = 0
DIRECTION_DOWN = 1


class DirectionLED(Label):

    VISIBLE_COLOR = (1.0, 1.0, 1.0, 1.0)
    INVISIBLE_COLOR = (0, 0, 0, 0)

    dir_colors = DictProperty({
        DIRECTION_NONE: VISIBLE_COLOR,
        DIRECTION_UP: INVISIBLE_COLOR,
        DIRECTION_DOWN: INVISIBLE_COLOR})

    def __init__(self, *args, **kwargs):
        self._item_state = DIRECTION_NONE
        super().__init__(*args, **kwargs)

    @property
    def item_state(self):
        return self._direction

    @item_state.setter
    def item_state(self, d):
        self.dir_colors[self._item_state] = self.INVISIBLE_COLOR
        self._item_state = d
        self.dir_colors[self._item_state] = self.VISIBLE_COLOR


STATE_FAR = 0
STATE_NEAR = 1
STATE_AT_CLOSED = 2
STATE_AT_CHANGING = 3
STATE_AT_OPEN = 4


class StateLED(Label):

    st_colors = {
        STATE_FAR: (0.1, 0.1, 0.1),
        STATE_NEAR: (0.1, 0.1, 1.0),
        STATE_AT_CLOSED: (1, 0.1, 0.1),
        STATE_AT_CHANGING: (1, 1, 0.1),
        STATE_AT_OPEN: (0.1, 1, 0.1),
        None: (0.1, 0.1, 0.1)
    }

    color = ListProperty(st_colors[STATE_FAR])

    st_intervals = {
        STATE_FAR: None,
        STATE_NEAR: 0.25,
        STATE_AT_CLOSED: None,
        STATE_AT_CHANGING: 0.25,
        STATE_AT_OPEN: None,
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.state_event = None
        self.item_state = STATE_FAR

    @property
    def item_state(self):
        return self._item_state

    @item_state.setter
    def item_state(self, new):
        self._item_state = new
        self.color = self.st_colors[new]
        if self.state_event is not None:
            Clock.unschedule(self.state_event)
        if self.st_intervals[new] is not None:
            self.state_event = Clock.schedule_interval(self._toggle, self.st_intervals[new])

    def _toggle(self, _):
        self.color = (self.st_colors[self.item_state] if tuple(self.color) == self.st_colors[None]
                      else self.st_colors[None])


class LiftInterface(BoxLayout):
    pass


class Lift(App):
    def __init__(self):
        super().__init__()
        self.devices = {}
        self.handlers = {}

    def register_handler(self, id, handler):
        self.handlers[id] = handler

    def unregister_handler(self, id):
        self.handlers[id] = None

    def trigger_event(self, id):
        if id not in self.handlers:
            return
        self.handlers[id](id, self.get(id))

    def get(self, id):
        return self.devices[id].item_state

    def set(self, id, item_state):
        self.devices[id].item_state = item_state

    def update(self, id):
        def f(item_state):
            self.devices[id].item_state = item_state
            self.trigger_event(id)
        return f

    def build(self):
        self.interface = LiftInterface()

        self.devices = self.interface.ids
        for id, device in self.interface.ids.items():
            if hasattr(device, 'on_state_changed'):
                device.on_state_changed = self.update(id)

        # self.interface.ids['l1_2'].item_state = STATE_NEAR
        # self.interface.ids['l1_3'].item_state = StateLED.AT_CLOSED
        # self.interface.ids['l1_4'].item_state = StateLED.AT_CHANGING
        # self.interface.ids['l1_5'].item_state = StateLED.AT_OPEN

        return self.interface
