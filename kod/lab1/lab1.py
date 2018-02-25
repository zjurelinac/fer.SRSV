import random
import time

from kivy.clock import Clock
from repeater import Repeater


class Game:

    IDLE = 0
    START = 1
    SHOW = 2
    INPUT = 3
    ERROR = 4

    def __init__(self):
        self.repeater = Repeater()
        self.state = Game.IDLE

        self.debug_mode = False

        self.btns = {1: 'sw6', 2: 'sw7', 3: 'sw8', 4: 'sw2', 5: 'sw3', 6: 'sw4'}
        self.rbtns = {v: k for k, v in self.btns.items()}
        self.leds = {1: 'ds3', 2: 'ds6', 3: 'ds9', 4: 'ds1', 5: 'ds4', 6: 'ds7', 7: 'ds2', 8: 'ds5', 9: 'ds8'}

        self.repeater.register_handler('sw5', self.start_handler)
        self.repeater.register_handler('sw1', self.reset_handler)

        for btn in self.rbtns:
            self.repeater.register_handler(btn, self.btn_handler)

        self.len = 0
        self.sequence = []
        self.input_pos = 0

        self.timeout_event = None
        self.btn_events = {}

        self._start_time = time.time()

        self.blinker = None

    def play(self):
        self.repeater.run()

    def start_handler(self, _, value):
        if self.state != Game.IDLE or not value:
            return

        self._schedule(lambda: self.start(1), 0, name='start')

    def reset_handler(self, _, value):
        if self.state == Game.IDLE or not value:
            return

        print('=' * 40 + '\nResetting the game.')

        self._schedule(lambda: self.start(1), 0, name='start')

    def btn_handler(self, btn, value):
        if self.state != Game.INPUT or not value:
            return

        value = self.rbtns[btn]
        print('Handling user input: %d.' % value)

        self._set_all(False)
        self._clear_btn_events()

        self._set(value, True)
        self.btn_events[value] = self._schedule(lambda: self._set_all(False), 0.5, name='clear_clicked_light')

        if self.timeout_event is not None:
            Clock.unschedule(self.timeout_event)
            self.timeout_event = None

        if self.sequence[self.input_pos] == value:
            self.input_pos += 1
            print('A correct guess <%d>!' % value)

            if self.input_pos == self.len:
                print('Correctly finished a sequence! Increasing game difficulty.')
                self._schedule(lambda: self.start(self.len + 1), 1, name='starting_next_level')
            else:
                self.timeout_event = self._schedule(lambda: self.display_error('Waited too long (over 3s), game over!'), 3, name='input_timeout_error')
        else:
            self._schedule(lambda: self.display_error('Wrong guess: %d != %d!' % (value, self.sequence[self.input_pos])),
                           0, name='wrong_input_error')

    def start(self, length):
        print('Starting a new game with len=%d.' % length)

        self.len = length
        self.sequence = [random.randint(1, 6) for _ in range(self.len)]
        self.state = Game.START

        self._clear_btn_events()
        self._disable_btns()

        self.blinker = Blinker(2, 2, self._set_all)
        self.blinker.blink()

        self._schedule(lambda: self._set_state(Game.SHOW), 3.9, name='enter_show_state')
        self._schedule(lambda: self.display_item(0), 4, name='display_first_item')

    def display_item(self, pos):
        item = self.sequence[pos]

        print('Displaying an item <%s> (%d/%d).' % (item, pos + 1, self.len))

        self._set(item, True)
        self._schedule(lambda: self._set(item, False), 0.5, name='clear_displayed_led')

        if pos < self.len - 1:
            self._schedule(lambda: self.display_item(pos + 1), 0.75, name='display_next_item')
        else:
            print('Whole sequence shown, will proceed to input.')
            self._schedule(lambda: self.init_clicks(), 2.5, name='init_clicks')

    def init_clicks(self):
        print('Initializing user input state.')

        self._schedule(lambda: self._set_all(True), 0, name='light_all')
        self._schedule(lambda: self._set_all(False), 1, name='clear_all')

        self.input_pos = 0

        self._schedule(lambda: self._set_state(Game.INPUT), 1, name='start_input_mode')
        self._schedule(lambda: self._enable_btns(), 1, name='enable_btns')

    def display_error(self, msg):
        print('An error occurred: %s.' % msg)
        self.blinker = Blinker(0.1, 20, self._set_all)
        self.blinker.blink()
        self._schedule(lambda: self.start(1), 2, name='start_from_scratch(error)')

    # =========================================================================

    def _schedule(self, fn, by, *args, name=None):
        def f(dt):
            if self.debug_mode:
                print('> Executing <%s> at %.2f (delayed by %s).' %
                      (getattr(fn, '__qualname__', None) if name is None else name,
                       time.time() - self._start_time, by))
            fn(*args)
        return Clock.schedule_once(f, by)

    def _blink_once(self, val):
        def f(dt):
            self._set_all(val)
        return f

    # ==========================================================================

    def _set(self, i, value):
        self.repeater.set(self.leds[i], value)

    def _set_all(self, value):
        for id in self.leds.values():
            self.repeater.set(id, value)

    def _set_state(self, state):
        self.state = state

    def _disable_btns(self):
        print('Disabling all buttons.')
        for btn in self.btns.values():
            self.repeater.interface.ids[btn].disabled = True

    def _enable_btns(self):
        print('Enabling all buttons.')
        for btn in self.btns.values():
            self.repeater.interface.ids[btn].disabled = False

    # ==========================================================================

    def _clear_btn_events(self):
        for evnt in self.btn_events.values():
            Clock.unschedule(evnt)

    def _dbg(self, dt):
        print('> Debug after %s :: state = %s, seq = %s, pos = %s, len = %s.' % (dt, self.state, self.sequence, self.pos, self.len))


class Blinker:

    def __init__(self, duration, repeats, action, start=False, debug_mode=False):
        self.duration = duration
        self.repeats = repeats
        self.action = action
        self.state = start
        self.debug_mode = debug_mode

    def blink(self):
        if self.debug_mode:
            print('Running a blinker!')
        self._blink()
        self.event = Clock.schedule_interval(lambda dt: self._blink(), self.duration)

    def destroy(self):
        Clock.unschedule(self.event)

    def _blink(self):
        if self.debug_mode:
            print('Blink ;)')
        self.repeats -= 1
        self.state = not self.state

        if self.repeats <= 0:
            self.action(False)
            return False
        else:
            self.action(self.state)


if __name__ == '__main__':
    Game().play()
