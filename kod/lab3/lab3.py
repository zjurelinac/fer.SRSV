import sys
import time

from kivy.clock import Clock

from lift import *


DEBUG_MODE = False
_START_TIME = None

OO = 100000

N = 5
M = 2

CONTROL_INTERVAL = 0.1

LIFT_STOPPED = 0
LIFT_MOVING = 1

DOORS_CLOSED = 1
DOORS_OPENING = 2
DOORS_OPEN = 3
DOORS_CLOSING = 4

OTHER_DIR = {DIRECTION_UP: DIRECTION_DOWN, DIRECTION_DOWN: DIRECTION_UP}
MOVEMENT = {DIRECTION_UP: +0.5, DIRECTION_DOWN: -0.5}
ACTION_TIMES = {
    'OPENING': 2,    # 3
    'CLOSING': 2,    # 3
    'OPEN': 5,       # 10
    'MOVE': 2        # 5
}

################################################################################


def integer(x):
    return x == int(x)


def other(x):
    return OTHER_DIR[x]


################################################################################


class Elevator:

    def __init__(self, i, global_requests):
        self.id = i
        self.state = LIFT_STOPPED
        self.direction = DIRECTION_NONE
        self.doors = DOORS_CLOSED
        self.position = 0
        self.idle = True

        self.stops = [False] * N
        self.internal_requests = [False] * N
        self.global_requests = global_requests
        self.assigned_requests = [[False] * N for _ in range(2)]

        self.next_event_time = None
        self.next_event = None

    # Actions

    def action_move(self):
        if self.position < 0 or self.position >= N:
            self._die('Moving to impossible position!')

        self.position += MOVEMENT[self.direction]

        if integer(self.position):
            self.position = int(self.position)

        if integer(self.position) and self.stops[self.position]:
            self.set_state(LIFT_STOPPED, self.direction, DOORS_OPENING, self.action_open, ACTION_TIMES['OPENING'])
        else:
            self.set_state(LIFT_MOVING, self.direction, DOORS_CLOSED, self.action_move, ACTION_TIMES['MOVE'])

    def action_open(self):
        direction = self.direction
        if self.assigned_requests[DIRECTION_UP][self.position]:
            self.assigned_requests[DIRECTION_UP][self.position] = False
            self.global_requests[DIRECTION_UP][self.position] = False
            direction = DIRECTION_UP
        elif self.assigned_requests[DIRECTION_DOWN][self.position]:
            self.assigned_requests[DIRECTION_DOWN][self.position] = False
            self.global_requests[DIRECTION_DOWN][self.position] = False
            direction = DIRECTION_DOWN

        self.stops[self.position] = False
        self.internal_requests[self.position] = False
        self.set_state(LIFT_STOPPED, direction, DOORS_OPEN, self.action_close, ACTION_TIMES['OPEN'])

    def action_close(self):
        self.set_state(LIFT_STOPPED, self.direction, DOORS_CLOSING,
                       self.action_proceed, ACTION_TIMES['CLOSING'])

    def action_proceed(self):
        if self.stops[self.position]:
            self.set_state(LIFT_STOPPED, self.direction, DOORS_OPENING, self.action_open, ACTION_TIMES['OPENING'])
        elif self.has_direction_stops():
            self.set_state(LIFT_MOVING, self.direction, DOORS_CLOSED, self.action_move, ACTION_TIMES['MOVE'])
        else:
            self.set_state(LIFT_STOPPED, DIRECTION_NONE, DOORS_CLOSED, None, None, idle=True)

    # Auxilliaries

    def set_state(self, state, direction, doors, next_event=None, next_event_time=None, idle=False):
        self.state = state
        self.direction = direction
        self.doors = doors
        self.next_event = next_event
        self.next_event_time = next_event_time
        self.idle = idle

    def send_idle_to(self, floor):
        if floor < self.position:
            direction = DIRECTION_DOWN
        elif floor > self.position:
            direction = DIRECTION_UP
        else:
            direction = DIRECTION_NONE

        if self.position == floor:
            self.set_state(LIFT_STOPPED, direction, DOORS_OPENING, next_event=self.action_open, next_event_time=ACTION_TIMES['OPENING'])
        else:
            self.set_state(LIFT_MOVING, direction, DOORS_CLOSED, next_event=self.action_move, next_event_time=ACTION_TIMES['MOVE'])

    def has_stops(self):
        return any(self.stops)

    def has_direction_stops(self):
        if self.direction == DIRECTION_UP:
            return any(self.stops[i] for i in range(self.position, N))
        elif self.direction == DIRECTION_DOWN:
            return any(self.stops[i] for i in range(0, self.position))
        else:
            return False

    def closest_stop(self):
        return min((i for i, s in enumerate(self.stops) if s), key=lambda x: self.distance(x))

    def distance(self, floor):
        return abs(floor - self.position)

    def next_floor(self):
        if self.state == LIFT_MOVING:
            return int(self.direction + 1) if self.direction == DIRECTION_UP else int(self.direction - 1)
        else:
            return self.position

    def reachable(self, floor):
        """A floor is reachable if:

            a) The lift is at that same floor, it is stopped and it's doors
            are either still opening or are already closed

            b) The lift is moving UP and the floor is above the lift

            c) The lift is moving DOWN and the floor is below the lift
            """
        if self.position == floor and self.state == LIFT_STOPPED and self.doors in (DOORS_OPENING, DOORS_CLOSED):
            return True
        elif self.direction == DIRECTION_UP:
            return floor > self.position
        elif self.direction == DIRECTION_DOWN:
            return floor < self.position
        else:
            return False

    def __str__(self):
        return ('Lift %d@(%.1f) [%s] :: %s%s, stops = %s, internals = %s, assigned = %s'
                % (self.id, self.position,
                   {DOORS_OPEN: 'open', DOORS_CLOSED: 'closed', DOORS_OPENING: 'opening', DOORS_CLOSING: 'closing'}[self.doors],
                   {LIFT_MOVING: 'MOVING', LIFT_STOPPED: 'STOPPED'}[self.state],
                   {DIRECTION_UP: ' UP', DIRECTION_DOWN: ' DOWN', DIRECTION_NONE: ''}[self.direction],
                   [i for i in range(N) if self.stops[i]],
                   [i for i in range(N) if self.internal_requests[i]],
                   [[i for i in range(N) if self.assigned_requests[d][i]] for d in range(2)]))

    def __repr__(self):
        return str(self)

################################################################################


class LiftSimulator:

    def __init__(self):
        self.lift = Lift()

        for i in range(N):
            self.lift.register_handler('f_%d' % (i + 1), self.floor_btn_handler)

        for n in range(M):
            self.lift.register_handler('l%d_p' % (n + 1), self.lift_btn_handler)

        self.ctrl_loop_count = 0
        self.requests = [[False] * N for _ in range(2)]
        self.elevators = [Elevator(i, self.requests) for i in range(M)]

        for elevator in self.elevators:
            elevator._die = lambda: self._die()

        self.floor_last_pressed = [None] * N
        self.lift_last_pressed = [None] * M
        self.lift_clicks = [0] * M
        self.lift_events = [None] * M

        self.requests_count = [0] * N

    def simulate(self):
        global _START_TIME
        _START_TIME = time.time()
        schedule_interval(self.control_loop, CONTROL_INTERVAL, name='CONTROL_LOOP', debug=False)
        self.lift.run()

    # Handlers

    def floor_btn_handler(self, id, value):
        floor = int(id[2]) - 1
        if value:
            self.floor_last_pressed[floor] = time.time()
        else:
            dt = time.time() - self.floor_last_pressed[floor]
            self.floor_last_pressed[floor] = None
            self.requests_count[floor] += 1
            if dt < 0.5:
                self.requests[DIRECTION_UP][floor] = True
            else:
                self.requests[DIRECTION_DOWN][floor] = True

    def lift_btn_handler(self, id, value):
        elevator = int(id[1]) - 1
        if value:
            self.lift_last_pressed[elevator] = time.time()
        else:
            dt = time.time() - self.lift_last_pressed[elevator]
            self.lift_last_pressed[elevator] = None
            if dt < 1:
                self.lift_clicks[elevator] += 1
                if self.lift_events[elevator] is not None:
                    Clock.unschedule(self.lift_events[elevator])
                self.lift_events[elevator] = schedule_event(lambda: self.finalize_lift_click(elevator), 1)
            else:   # A stop!
                pass

    def finalize_lift_click(self, elevator):
        floor = self.lift_clicks[elevator] - 1
        self.lift_clicks[elevator] = 0
        self.elevators[elevator].internal_requests[floor] = True

    # Internals

    def control_loop(self):
        self.ctrl_loop_count += 1
        # print('<LOOP %d> :: %s' % (self.ctrl_loop_count, self.elevators))

        for elevator in self.elevators:
            if elevator.next_event_time is not None:
                elevator.next_event_time -= CONTROL_INTERVAL
                if elevator.next_event_time <= 0:
                    print('Performing %d::%s' % (elevator.id, elevator.next_event.__name__))
                    elevator.next_event()

        for elevator in self.elevators:
            elevator.stops = [elevator.stops[i] | elevator.internal_requests[i] for i in range(N)]

        # Serving all possible requests
        #   Selecting closest request first
        assigning = True
        while assigning:
            assigning = False
            best_request, best_lift, best_dist = None, None, OO

            for d, requests in enumerate(self.requests):
                for floor, _ in filter(lambda x: x[1], enumerate(requests)):

                    if any(self.elevators[i].assigned_requests[d][floor] for i in range(M)):
                        continue

                    for elevator in self.elevators:
                        if elevator.direction == d and elevator.reachable(floor) or elevator.idle:
                            current_dist = elevator.distance(floor)
                            if current_dist < best_dist:
                                best_dist = current_dist
                                best_lift = elevator
                                best_request = (d, floor)

            if best_request is not None:
                best_lift.assigned_requests[best_request[0]][best_request[1]] = True
                best_lift.stops[best_request[1]] = True
                assigning = True

                if best_lift.idle:
                    best_lift.send_idle_to(best_request[1])

        for elevator in self.elevators:
            if elevator.state == LIFT_MOVING and not elevator.has_stops():
                # Either stop the lift on the next floor, or send it to the most frequent floor
                elevator.stops[elevator.next_floor()] = True

            elif elevator.idle and elevator.has_stops():
                elevator.send_idle_to(elevator.closest_stop())
            elif elevator.idle:
                pass

        self.update_ui()

    def update_ui(self):
        for i, elevator in enumerate(self.elevators):
            updated = set()
            if elevator.state == LIFT_STOPPED:
                if elevator.doors == DOORS_CLOSED:
                    self._set_state_led(i, elevator.position, STATE_AT_CLOSED)
                elif elevator.doors in (DOORS_CLOSING, DOORS_OPENING):
                    self._set_state_led(i, elevator.position, STATE_AT_CHANGING)
                elif elevator.doors == DOORS_OPEN:
                    self._set_state_led(i, elevator.position, STATE_AT_OPEN)
                updated.add(elevator.position)
            else:
                if elevator.direction == DIRECTION_UP or elevator.position != int(elevator.position):
                    position_lower, position_upper = int(elevator.position), int(elevator.position + 1)
                elif elevator.direction == DIRECTION_DOWN and elevator.position == int(elevator.position):
                    position_upper, position_lower = int(elevator.position), int(elevator.position - 1)
                else:
                    self._die('Impossible lift state for UI update!')

                if position_lower < 0 or position_upper > N - 1:
                    self._die('Impossible position for NEAR!')

                self._set_state_led(i, position_upper, STATE_NEAR)
                self._set_state_led(i, position_lower, STATE_NEAR)
                updated.add(position_upper)
                updated.add(position_lower)
            for f in range(N):
                if f not in updated:
                    self._set_state_led(i, f, STATE_FAR)
            self._set_dir_led(i, elevator.direction)

    # Auxilliaries

    def _set_state_led(self, elevator, floor, value):
        lid = 'l%d_%d' % (elevator + 1, floor + 1)
        if self.lift.get(lid) != value:
            # print('Setting %s from %s to %s' % (lid, self.lift.get(lid), value))
            self.lift.set(lid, value)

    def _set_dir_led(self, elevator, value):
        self.lift.set('l%d_d' % (elevator + 1), value)

    def _die(self, msg):
        print(self, ' :: ', msg)
        sys.exit(1)


################################################################################


def schedule_event(fn, by, *args, name=None, debug=True):
    def f(dt):
        if DEBUG_MODE and debug:
            print('> Executing <%s> at %.2f (delayed by %s).' %
                  (getattr(fn, '__qualname__', None) if name is None else name,
                   time.time() - _START_TIME, by))
        fn(*args)
    return Clock.schedule_once(f, by)


def schedule_interval(fn, interval, *args, name=None, debug=True):
    def f(dt):
        if DEBUG_MODE and debug:
            print('> Executing <%s> at %.2f (delayed by %s).' %
                  (getattr(fn, '__qualname__', None) if name is None else name,
                   time.time() - _START_TIME, interval))
        fn(*args)
    return Clock.schedule_interval(f, interval)


################################################################################


if __name__ == '__main__':
    ls = LiftSimulator()
    ls.simulate()
