


enum class State { Idle, Heating, Stabilize, Processing, Fault };
enum class Event { Start, TempReached, Done, Error, Reset };

// constexpr 2D lookup table: table[state][event] = next_state
constexpr State table[5][5] = {
    // Start        TempReached    Done           Error          Reset
    { State::Heating, State::Idle, State::Idle, State::Idle,   State::Idle },      // Idle
    { State::Heating, State::Stabilize, State::Heating, State::Fault, State::Heating }, // Heating
    // ... fill remaining rows
};

constexpr State transition(State s, Event e) {
    return table[static_cast<int>(s)][static_cast<int>(e)];
}

static_assert(transition(State::Idle, Event::Start) == State::Heating);